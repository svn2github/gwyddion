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

#define id "libgwy-builtin-grain-value"
#define generated "GENERATED"

static const gchar file_prologue[] =
"<?xml version='1.0' encoding='utf-8'?>\n"
"<!DOCTYPE book PUBLIC '-//OASIS//DTD DocBook XML V4.5//EN'\n"
"          'http://www.oasis-open.org/docbook/xml/4.5/docbookx.dtd'>\n"
"<!-- This is a %s file.  Created by docs/libgwy/grain-values.c. -->\n"
"<refentry id='%s'>\n"
"<refmeta>\n"
"<manvolnum>3</manvolnum>\n"
"<refmiscinfo>LIBGWY Library</refmiscinfo>\n"
"</refmeta>\n"
"<refnamediv>\n"
"<refname>Builtin grain values</refname>\n"
"<refpurpose>List of built-in grain values</refpurpose>\n"
"</refnamediv>\n";

static const gchar file_epilogue[] =
"</refentry>\n";

static const gchar group_prologue[] =
"<refsect1 id='%s.%s'>\n"
"<title>%s</title>\n"
"<para>Values in group %s:</para>\n"
"<informaltable frame='none' rowsep='0' colsep='0' rules='none' cellspacing='12'>\n"
"<tgroup cols='4'>\n"
"<colspec align='left'/>\n"
"<colspec align='left'/>\n"
"<colspec align='left'/>\n"
"<colspec align='left'/>\n"
"<thead>\n"
"<row>\n"
"<entry align='center'>Name</entry>\n"
"<entry align='center'>Symbol</entry>\n"
"<entry align='center'>Identifier</entry>\n"
"<entry align='center'>Notes</entry>\n"
"</row>\n"
"</thead>\n"
"<tbody>\n";

static const gchar group_epilogue[] =
"</tbody>\n"
"</tgroup>\n"
"</informaltable>\n"
"</refsect1>\n";

static const gchar value_prologue[] =
"<row id='%s.%s'>\n"
"<entry>%s</entry>\n";

static const gchar value_epilogue[] =
"</row>\n";

static int
compare(const void *a, const void *b)
{
    GObject *obja = *(GObject**)a, *objb = *(GObject**)b;
    gchar *stra, *strb;
    gint cmp;

    g_object_get(obja, "group", &stra, NULL);
    g_object_get(objb, "group", &strb, NULL);
    cmp = g_utf8_collate(stra, strb);
    g_free(stra);
    g_free(strb);
    if (cmp)
        return cmp;

    g_object_get(obja, "name", &stra, NULL);
    g_object_get(objb, "name", &strb, NULL);
    cmp = g_utf8_collate(stra, strb);
    g_free(stra);
    g_free(strb);
    return cmp;
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

static void
append_separated(GString *str, const gchar *what)
{
    if (str->len)
        g_string_append(str, ", ");
    g_string_append(str, what);
}

int
main(void)
{
    gwy_type_init();

    printf(file_prologue, generated, id);

    const gchar **names = gwy_grain_value_list_builtins();
    guint nvalues = g_strv_length((gchar**)names);
    GwyGrainValue **grainvalues = g_new(GwyGrainValue*, nvalues);
    for (guint i = 0; i < nvalues; i++) {
        grainvalues[i] = gwy_grain_value_new(names[i]);
        g_assert(GWY_IS_GRAIN_VALUE(grainvalues[i]));
    }
    qsort(grainvalues, nvalues, sizeof(GwyGrainValue*), compare);

    GString *str = g_string_new(NULL);
    const gchar *processing_group = "NONE";
    for (guint i = 0; i < nvalues; i++) {
        GwyGrainValue *grainvalue = grainvalues[i];
        const gchar *name = gwy_grain_value_get_name(grainvalue);
        const gchar *group = gwy_grain_value_get_group(grainvalue);
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
        printf(value_prologue, id, str->str, name);
        g_string_assign(str, gwy_grain_value_get_symbol(grainvalue));
        convert_pango_to_docbook(str);
        make_math(str);
        printf("<entry>%s</entry>\n", str->str);
        printf("<entry><code>%s</code></entry>",
               gwy_grain_value_get_ident(grainvalue));
        g_string_assign(str, "");
        if (gwy_grain_value_needs_same_units(grainvalue))
            append_separated(str, "needs same lateral and value units");
        if (gwy_grain_value_is_angle(grainvalue))
            append_separated(str, "represents angle in radians");
        printf("<entry>%s</entry>\n", str->str);
        printf(value_epilogue);
        g_object_unref(grainvalue);
    }
    if (!gwy_strequal(processing_group, "NONE"))
        printf(group_epilogue);

    printf(file_epilogue);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
