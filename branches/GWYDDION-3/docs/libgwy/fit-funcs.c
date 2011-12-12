/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "libgwy/libgwy.h"

#define id "libgwy-builtin-fit-func"
#define generated "GENERATED"

static const gchar file_prologue[] =
"<?xml version='1.0' encoding='utf-8'?>\n"
"<!DOCTYPE book PUBLIC '-//OASIS//DTD DocBook XML V4.5//EN'\n"
"          'http://www.oasis-open.org/docbook/xml/4.5/docbookx.dtd'>\n"
"<!-- This is a %s file.  Created by docs/libgwy/fit-funcs.c. -->\n"
"<refentry id='%s'>\n"
"<refmeta>\n"
"<manvolnum>3</manvolnum>\n"
"<refmiscinfo>LIBGWY Library</refmiscinfo>\n"
"</refmeta>\n"
"<refnamediv>\n"
"<refname>Builtin fit funcs</refname>\n"
"<refpurpose>List of built-in fitting functions</refpurpose>\n"
"</refnamediv>\n";

static const gchar file_epilogue[] =
"</refentry>\n";

static const gchar group_prologue[] =
"<refsect1 id='%s.%s'>\n"
"<title>%s</title>\n"
"<para>Functions in group %s:</para>\n"
"<informaltable frame='none' rowsep='0' colsep='0' rules='none' cellspacing='12'>\n"
"<tgroup cols='3'>\n"
"<colspec align='left'/>\n"
"<colspec align='left'/>\n"
"<colspec align='left'/>\n"
"<thead>\n"
"<row>\n"
"<entry align='center'>Name</entry>\n"
"<entry align='center'>Formula</entry>\n"
"<entry align='center'>Parameters</entry>\n"
"</row>\n"
"</thead>\n"
"<tbody>\n";

static const gchar group_epilogue[] =
"</tbody>\n"
"</tgroup>\n"
"</informaltable>\n"
"</refsect1>\n";

static const gchar function_prologue[] =
"<row id='%s.%s'>\n"
"<entry>%s</entry>\n";

static const gchar function_epilogue[] =
"</row>\n";

static int
compare_functions(const void *a, const void *b)
{
    const GwyFitFunc *fa = *(const GwyFitFunc**)a,
                     *fb = *(const GwyFitFunc**)b;
    const gchar *ga = gwy_fit_func_get_group(fa),
                *gb = gwy_fit_func_get_group(fb);
    gint cmp = g_utf8_collate(ga, gb);
    if (cmp)
        return cmp;

    const gchar *na = gwy_fit_func_get_name(fa),
                *nb = gwy_fit_func_get_name(fb);
    return g_utf8_collate(na, nb);
}

static void
make_id(GString *str)
{
    guint j = 0;
    for (guint i = 0; i < str->len; i++) {
        if (g_ascii_isalnum(str->str[i]))
            str->str[j++] = str->str[i];
        else if (j && str->str[j-1] != '-')
            str->str[j++] = '-';
    }

    do {
        str->str[j] = '\0';
    } while (j-- && str->str[j] == '-');
}

static void
make_math(GString *str)
{
    g_string_prepend(str, "<inlineequation><mathphrase>");
    g_string_append(str, "</mathphrase></inlineequation>");
}

static void
replace_tag(GString *str, const gchar *from, const gchar *to)
{
    gchar *tf = g_strdup_printf("(</?)%s(>)", from);
    gchar *tt = g_strdup_printf("\\g<1>%s\\g<2>", to);
    GRegex *regex = g_regex_new(tf, 0, 0, NULL);
    g_assert(regex);
    gchar *repl = g_regex_replace(regex, str->str, str->len, 0, tt, 0, NULL);
    g_assert(repl);
    g_string_assign(str, repl);
    g_free(repl);
    g_regex_unref(regex);
    g_free(tt);
    g_free(tf);
}

static void
convert_pango_to_docbook(GString *str)
{
    replace_tag(str, "i", "emphasis");
    replace_tag(str, "sup", "superscript");
    replace_tag(str, "sub", "subscript");
}

int
main(void)
{
    gwy_type_init();

    printf(file_prologue, generated, id);

    const gchar **names = gwy_fit_func_list_builtins();
    guint nfuncs = g_strv_length((gchar**)names);
    GwyFitFunc **fitfuncs = g_new(GwyFitFunc*, nfuncs);
    for (guint i = 0; i < nfuncs; i++) {
        fitfuncs[i] = gwy_fit_func_new(names[i]);
        g_assert(GWY_IS_FIT_FUNC(fitfuncs[i]));
    }
    qsort(fitfuncs, nfuncs, sizeof(GwyFitFunc*), compare_functions);

    GString *str = g_string_new(NULL);
    const gchar *processing_group = "NONE";
    for (guint i = 0; i < nfuncs; i++) {
        GwyFitFunc *fitfunc = fitfuncs[i];
        const gchar *name = gwy_fit_func_get_name(fitfunc);
        const gchar *group = gwy_fit_func_get_group(fitfunc);
        if (!gwy_strequal(group, processing_group)) {
            if (!gwy_strequal(processing_group, "NONE"))
                printf(group_epilogue);
            g_string_assign(str, group);
            make_id(str);
            printf(group_prologue, id, str->str, group, group);
            processing_group = group;
        }
        g_string_assign(str, name);
        make_id(str);
        printf(function_prologue, id, str->str, name);
        g_string_assign(str, gwy_fit_func_formula(fitfunc));
        convert_pango_to_docbook(str);
        make_math(str);
        printf("<entry>%s</entry>\n", str->str);
        guint n = gwy_fit_func_n_params(fitfunc);
        printf("<entry>");
        for (guint j = 0; j < n; j++) {
            g_string_assign(str, gwy_fit_func_param_name(fitfunc, j));
            convert_pango_to_docbook(str);
            make_math(str);
            printf("%s %s", j ? "," : "", str->str);
        }
        printf("</entry>\n");
        printf(function_epilogue);
        g_object_unref(fitfunc);
    }
    if (!gwy_strequal(processing_group, "NONE"))
        printf(group_epilogue);

    printf(file_epilogue);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
