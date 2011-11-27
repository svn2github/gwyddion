#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "libgwy/libgwy.h"

#define id "libgwy-builtin-fit-func"
#define generated "GENERATED"

static const gchar file_prologue[] =
"<?xml version='1.0' encoding='utf-8'?>\n"
"<!DOCTYPE book PUBLIC '-//OASIS//DTD DocBook XML V4.1.2//EN'\n"
"          'http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd'>\n"
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

static const gchar function_prologue[] =
"<refsect1 id='%s.%s'>\n"
"<title>%s</title>\n";

static const gchar function_epilogue[] =
"</refsect1>\n";

static int
compare_names(const void *a, const void *b)
{
    const gchar *stra = *(const gchar**)a;
    const gchar *strb = *(const gchar**)b;
    return g_utf8_collate(stra, strb);
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
    qsort(names, g_strv_length((gchar**)names), sizeof(gchar*), compare_names);

    GString *str = g_string_new(NULL);
    for (guint i = 0; names[i]; i++) {
        const gchar *name = names[i];
        GwyFitFunc *fitfunc = gwy_fit_func_new(name);
        g_string_assign(str, name);
        make_id(str);
        printf(function_prologue, id, str->str, name);
        printf("<para>Group: %s</para>\n", gwy_fit_func_get_group(fitfunc));
        g_string_assign(str, gwy_fit_func_formula(fitfunc));
        convert_pango_to_docbook(str);
        make_math(str);
        printf("<para>Formula: %s</para>", str->str);
        guint n = gwy_fit_func_n_params(fitfunc);
        printf("<para>Parameters:");
        for (guint j = 0; j < n; j++) {
            g_string_assign(str, gwy_fit_func_param_name(fitfunc, j));
            convert_pango_to_docbook(str);
            make_math(str);
            printf("%s %s", j ? "," : "", str->str);
        }
        printf("</para>\n");
        printf(function_epilogue);
        g_object_unref(fitfunc);
    }
    g_free(names);

    printf(file_epilogue);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
