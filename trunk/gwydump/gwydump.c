/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti)
 *  E-mail: yeti@gwyddion.net
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <glib.h>

static void print_help      (void);
static void process_options (int *argc, char ***argv);
static void dump_object     (gchar **buffer, gsize *size);

/* Options */
static guint opt_indent = 4;
static gboolean opt_address = FALSE;
static gboolean opt_object_size = FALSE;
static gboolean opt_value = FALSE;

/* Global state */
static gint level;
static gchar *buffer0;

int
main(int argc, char *argv[])
{
    gsize size;
    gchar *buffer;
    GError *err = NULL;

    /* Check for --help and --version before rash file loading and GUI
     * initializatioon */
    process_options(&argc, &argv);
    if (argc < 2) {
        print_help();
        return 0;
    }

    if (!g_file_get_contents(argv[1], &buffer, &size, &err)) {
        g_printerr("Cannot open `%s': %s\n", argv[1], g_strerror(errno));
        return 1;
    }

    if (size < 10 || strncmp(buffer, "GWYP", 4)) {
        if (!strncmp(buffer, "GWYO", 4))
            g_printerr("Cannot dump old-style file `%s'\n", argv[1]);
        else
            g_printerr("File `%s' is not a Gwyddion file\n", argv[1]);
        return 1;
    }

    buffer0 = buffer;
    level = -1;
    buffer += 4;
    size -= 4;
    if (opt_address)
        g_print("%08x: ", buffer - buffer0);
    dump_object(&buffer, &size);
    g_free(buffer0);

    return 0;
}

static gchar*
indent(void)
{
    static GString *str = NULL;
    static gchar spaces[] = "                                ";
    gint i;

    if (!str)
        str = g_string_new("");

    for (i = str->len/opt_indent; i < level; i++)
        g_string_append_len(str, spaces, opt_indent);
    g_string_truncate(str, opt_indent*level);

    return str->str;
}

static void
fail(gchar *buffer,
     const gchar *format,
     ...)
{
    va_list ap;

    fprintf(stderr, "\nERROR at position %08x: ", buffer - buffer0);
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    g_free(buffer0);
    exit(EXIT_FAILURE);
}

static guint32
get_size(gchar **buffer,
         gsize *size)
{
    guint32 value;

    if (*size < sizeof(gint32))
        fail(*buffer, "Truncated size");
    memcpy(&value, *buffer, sizeof(gint32));
    value = GUINT32_FROM_LE(value);
    *buffer += sizeof(gint32);
    *size -= sizeof(gint32);

    return value;
};

static void
dump_boolean(gchar **buffer,
             gsize *size)
{
    gboolean value;

    if (!*size)
        fail(*buffer, "Truncated boolean");
    value = !!**buffer;
    if (opt_value)
        g_print("boolean: %s", value ? "TRUE" : "FALSE");
    *buffer += sizeof(gchar);
    *size -= sizeof(gchar);
}

static void
dump_char(gchar **buffer,
          gsize *size)
{
    gchar value;

    if (!*size)
        fail(*buffer, "Truncated char\n");
    value = **buffer;
    if (opt_value) {
        if (g_ascii_isprint(value))
            g_print("char: %c", value);
        else
            g_print("char: \\%03o", value);
    }
    *buffer += sizeof(gchar);
    *size -= sizeof(gchar);
}

static void
dump_int32(gchar **buffer,
           gsize *size)
{
    gint32 value;

    if (*size < sizeof(gint32))
        fail(*buffer, "Truncated int32");
    memcpy(&value, *buffer, sizeof(gint32));
    value = GINT32_FROM_LE(value);
    if (opt_value)
        g_print("int32: %d", value);
    *buffer += sizeof(gint32);
    *size -= sizeof(gint32);
}

static void
dump_int64(gchar **buffer,
           gsize *size)
{
    gint64 value;

    if (*size < sizeof(gint64))
        fail(*buffer, "Truncated int64");
    memcpy(&value, *buffer, sizeof(gint64));
    value = GINT64_FROM_LE(value);
    if (opt_value)
        g_print("int64: %" G_GINT64_FORMAT, value);
    *buffer += sizeof(gint64);
    *size -= sizeof(gint64);
}

static void
dump_double(gchar **buffer,
            gsize *size)
{
    union { gdouble d; guint64 i; } value;

    if (*size < sizeof(gdouble))
        fail(*buffer, "Truncated double");
    memcpy(&value, *buffer, sizeof(gdouble));
    value.i = GUINT64_FROM_LE(value.i);
    if (opt_value)
        g_print("double: %g", value.d);
    *buffer += sizeof(gdouble);
    *size -= sizeof(gdouble);
}

static void
dump_string(gchar **buffer,
            gsize *size)
{
    gchar *p, *q;

    if (!(p = memchr(*buffer, 0, *size)))
        fail(*buffer, "Truncated string");
    q = g_strescape(*buffer, NULL);
    if (opt_value)
        g_print("string: \"%s\"", q);
    g_free(q);
    *size -= (p - *buffer) + 1;
    *buffer = p + 1;
}

static void
dump_array(gchar **buffer,
           gsize *size,
           gsize membersize,
           const gchar *typename)
{
    guint32 mysize;
    gchar *p;

    mysize = get_size(buffer, size);
    if (opt_value)
        g_print("%s array of length %u\n", typename, mysize);
    else
        g_print("\n");
    if (!strcmp(typename, "object")) {
        while (mysize) {
            dump_object(buffer, size);
            mysize--;
        }
    }
    else if (!strcmp(typename, "string")) {
        while (mysize) {
            if (!(p = memchr(*buffer, 0, *size)))
                fail(*buffer, "Truncated string");
            *size -= (p - *buffer) + 1;
            *buffer = p + 1;
            mysize--;
        }
    }
    else if (*size < mysize*membersize)
        fail(*buffer, "Truncated %s array", typename);
    else {
        *buffer += mysize*membersize;
        *size -= mysize*membersize;
    }
}

static void
dump_hash(gchar *buffer,
          gsize size)
{
    static struct {
        gchar ctype;
        void (*func)(gchar**, gsize*);
    } atomic[] = {
        { 'o', dump_object }, { 'b', dump_boolean }, { 'c', dump_char },
        { 'i', dump_int32 }, { 'q', dump_int64 }, { 'd', dump_double },
        { 's', dump_string },
    };
    static struct {
        gchar ctype;
        const gchar *name;
        const gsize size;
    } array[] = {
        { 'B', "boolean", sizeof(gchar) }, { 'C', "char", sizeof(gchar) },
        { 'I', "int32", sizeof(gint32) }, { 'Q', "int64", sizeof(gint64) },
        { 'D', "double", sizeof(gdouble) },
        { 'S', "string", 0 }, { 'O', "object", 0 },
    };
    gboolean handled;
    guint i;
    gchar ctype;
    gchar *p;

    while (size) {
        /* Name */
        if (!(p = memchr(buffer, 0, size)))
            fail(buffer, "Runaway component name");
        if (opt_address)
            g_print("%08x: ", buffer - buffer0);
        g_print("%s%s", indent(), buffer);
        size -= (p - buffer) + 1;
        buffer = p + 1;

        /* Type */
        if (!size)
            fail(buffer, "Runaway component type");
        ctype = *buffer;
        buffer += sizeof(gchar);
        size -= sizeof(gchar);

        /* Data */
        handled = FALSE;
        for (i = 0; i < G_N_ELEMENTS(atomic); i++) {
            if (atomic[i].ctype == ctype) {
                if (opt_value)
                    g_print(", ");
                else if (ctype == 'o')
                    g_print(" ");
                atomic[i].func(&buffer, &size);
                if (ctype != 'o')
                    g_print("\n");
                handled = TRUE;
                break;
            }
        }
        for (i = 0; i < G_N_ELEMENTS(array); i++) {
            if (array[i].ctype == ctype) {
                if (opt_value)
                    g_print(", ");
                dump_array(&buffer, &size, array[i].size, array[i].name);
                handled = TRUE;
                break;
            }
        }
        if (!handled)
            fail(buffer, "Unknown type `%c'", ctype);
    }
}

static void
dump_object_real(gchar **buffer,
                 gsize *size)
{
    gint32 mysize;
    gchar *p;

    /* Name */
    if (!(p = memchr(*buffer, 0, *size)))
        fail(*buffer, "Runaway object name");
    if (opt_value)
        g_print("object: %s", *buffer);
    else
        g_print("%s", *buffer);
    *size -= (p - *buffer) + 1;
    *buffer = p + 1;

    /* Size */
    mysize = get_size(buffer, size);
    if (mysize > *size)
        fail(*buffer, "Truncated object data");
    if (opt_object_size)
        g_print(", size: %" G_GSIZE_FORMAT "\n", *size);
    else
        g_print("\n");

    /* Hash */
    *size -= mysize;
    *buffer += mysize;
    dump_hash(*buffer - mysize, mysize);
}

static void
dump_object(gchar **buffer,
            gsize *size)
{
    level++;
    dump_object_real(buffer, size);
    level--;
}

static void
process_options(int *argc,
                char ***argv)
{
    static gboolean opt_version = FALSE;
    static gboolean opt_help = FALSE;
    static GOptionEntry entries[] = {
        {
            "value", 'v', 0,
            G_OPTION_ARG_NONE, &opt_value, NULL, NULL
        },
        {
            "address", 'a', 0,
            G_OPTION_ARG_NONE, &opt_address, NULL, NULL
        },
        {
            "object-size", 's', 0,
            G_OPTION_ARG_NONE, &opt_object_size, NULL, NULL
        },
        {
            "indent", 'i', 0,
            G_OPTION_ARG_INT, &opt_indent, NULL, NULL
        },
        {
            "version", 'V', 0,
            G_OPTION_ARG_NONE, &opt_version, NULL, NULL
        },
        {
            "help", 'h', 0,
            G_OPTION_ARG_NONE, &opt_help, NULL, NULL
        },
        { NULL }
    };
    GOptionContext *context;
    GError *err = NULL;

    context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_set_help_enabled(context, FALSE);
    if (!g_option_context_parse(context, argc, argv, &err)) {
        g_print("Cannot parse options: %s\n", err->message);
        g_clear_error(&err);
        exit(EXIT_FAILURE);
    }
    g_option_context_free(context);

    if (opt_version)
        g_print("%s %s\n", PACKAGE, VERSION);

    if (opt_help)
        print_help();

    if (opt_help || opt_version)
        exit(EXIT_SUCCESS);

    opt_indent = CLAMP(opt_indent, 0, 32);
}

static void
print_help(void)
{
    g_print(
"Usage: gwyiew [OPTIONS...] FILENAME\n"
"Dump Gwyddion .gwy file in a text format.\n\n"
        );
    g_print(
"Options:\n"
" -h, --help                 Print this help and terminate.\n"
" -V, --version              Print version info and terminate.\n"
" -a, --address              Print addresses in file.\n"
" -v, --values               Print values of atomic objects.\n"
" -s, --object-size          Print object sizes.\n"
" -i, --indent=N             Indent each nesting level by N spaces.\n\n"
        );
    g_print("Please report bugs in Gwyddion bugzilla "
            "http://trific.ath.cx/bugzilla/\n");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

