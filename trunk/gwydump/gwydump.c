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

/* Borrow types from GLIB to convert old GwyContainers */
#define G_TYPE_FUNDAMENTAL_SHIFT (2)
#define G_TYPE_MAKE_FUNDAMENTAL(x) (((x) << G_TYPE_FUNDAMENTAL_SHIFT))

#define G_TYPE_UCHAR G_TYPE_MAKE_FUNDAMENTAL (4)
#define G_TYPE_BOOLEAN G_TYPE_MAKE_FUNDAMENTAL (5)
#define G_TYPE_INT G_TYPE_MAKE_FUNDAMENTAL (6)
#define G_TYPE_INT64 G_TYPE_MAKE_FUNDAMENTAL (10)
#define G_TYPE_DOUBLE G_TYPE_MAKE_FUNDAMENTAL (15)
#define G_TYPE_STRING G_TYPE_MAKE_FUNDAMENTAL (16)
#define G_TYPE_OBJECT G_TYPE_MAKE_FUNDAMENTAL (20)

static void print_help      (void);
static void process_options (int *argc, char ***argv);
static void dump_object     (gchar **buffer, gsize *size);

/* Options */
static guint opt_indent = 4;
static gboolean opt_offset = FALSE;
static gboolean opt_object_size = FALSE;
static gboolean opt_type = FALSE;
static gboolean opt_value = FALSE;
static gboolean opt_raw = FALSE;

/* Global state */
static gint level;
static gchar *buffer0;
static gboolean v1file = FALSE;

int
main(int argc, char *argv[])
{
    gsize size;
    gchar *buffer;
    GError *err = NULL;

    process_options(&argc, &argv);
    if (argc < 2) {
        print_help();
        return 0;
    }

    if (!g_file_get_contents(argv[1], &buffer, &size, &err)) {
        g_printerr("Cannot open `%s': %s\n", argv[1], g_strerror(errno));
        return 1;
    }

    buffer0 = buffer;
    /* Detect file type, if not raw */
    if (!opt_raw) {
        if (size < 10
            || (strncmp(buffer, "GWYP", 4) && strncmp(buffer, "GWYO", 4))) {
            g_printerr("File `%s' is not a Gwyddion file\n", argv[1]);
            return 1;
        }
        if (!strncmp(buffer, "GWYO", 4))
            v1file = TRUE;

        if (opt_offset)
            g_print("%08x: Header %.4s\n", 0, buffer);
        else
            g_print("Header %.4s\n", buffer);
        buffer += 4;
        size -= 4;
    }
    level = -1;
    if (opt_offset)
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

    if (!opt_indent)
        return "";

    if (!str)
        str = g_string_new("");

    for (i = str->len/opt_indent; i < level; i++)
        g_string_append_len(str, spaces, opt_indent);
    g_string_truncate(str, opt_indent*level);

    return str->str;
}

static void G_GNUC_NORETURN
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
        fail(*buffer, "Truncated int32");
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
        g_print("%s%s", opt_type ? "boolean=" : "", value ? "TRUE" : "FALSE");
    else if (opt_type)
        g_print("boolean");
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
            g_print("%s%c", opt_type ? "char=" : "", value);
        else
            g_print("%s\\%03o", opt_type ? "char=" : "", value);
    }
    else if (opt_type)
        g_print("char");
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
        g_print("%s%d", opt_type ? "int32=" : "", value);
    else if (opt_type)
        g_print("int32");
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
        g_print("%s%" G_GINT64_FORMAT, opt_type ? "int64=" : "", value);
    else if (opt_type)
        g_print("int64");
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
        g_print("%s%g", opt_type ? "double=" : "", value.d);
    else if (opt_type)
        g_print("double");
    *buffer += sizeof(gdouble);
    *size -= sizeof(gdouble);
}

static void
dump_string(gchar **buffer,
            gsize *size)
{
    gchar *p, *q;

    if (!(p = memchr(*buffer, 0, *size)))
        fail(*buffer, "Runaway string");
    q = g_strescape(*buffer, NULL);
    if (opt_value)
        g_print("%s\"%s\"", opt_type ? "string=" : "", q);
    else if (opt_type)
        g_print("string");
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
    if (opt_value || opt_type) {
        if (opt_type)
            g_print("%s ", typename);
        g_print("array");
        if (opt_value)
            g_print(" of length %u", mysize);
    }
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
                fail(*buffer, "Runaway string");
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

static gchar
gtype_to_ctype(guint32 gtype)
{
    switch (gtype) {
        case G_TYPE_UCHAR: return 'c'; break;
        case G_TYPE_BOOLEAN: return 'b'; break;
        case G_TYPE_INT: return 'i'; break;
        case G_TYPE_INT64: return 'q'; break;
        case G_TYPE_DOUBLE: return 'd'; break;
        case G_TYPE_STRING: return 's'; break;
        case G_TYPE_OBJECT: return 'o'; break;
    }
    return 0;
}

static void
dump_hash(gchar *buffer,
          gsize size,
          gboolean oldfile)
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
    guint32 gtype;
    gchar ctype = 0;
    gchar *p, *q;

    while (size) {
        if (opt_offset)
            g_print("%08x: ", buffer - buffer0);

        /* Type for old files */
        if (oldfile) {
            gtype = get_size(&buffer, &size);
            if (!(ctype = gtype_to_ctype(gtype)))
                fail(buffer, "Unknown GLib type %u", gtype);
        }

        /* Name */
        if (!(p = memchr(buffer, 0, size)))
            fail(buffer, "Runaway component name");
        q = g_strescape(buffer, NULL);
        g_print("%s\"%s\"", indent(), q);
        g_free(q);
        size -= (p - buffer) + 1;
        buffer = p + 1;

        /* Type for new files */
        if (!oldfile) {
            if (!size)
                fail(buffer, "Truncated component type");
            ctype = *buffer;
            buffer += sizeof(gchar);
            size -= sizeof(gchar);
        }

        /* Data */
        handled = FALSE;
        for (i = 0; i < G_N_ELEMENTS(atomic); i++) {
            if (atomic[i].ctype == ctype) {
                if (opt_value)
                    g_print(", ");
                else if (ctype == 'o' || opt_type)
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
    gboolean container;
    gint32 mysize;
    gchar *p;

    /* Name */
    if (!(p = memchr(*buffer, 0, *size)))
        fail(*buffer, "Runaway object name");
    if (opt_value)
        g_print("%s%s", opt_type ? "object=" : "", *buffer);
    else if (opt_type)
        g_print("object");
    container = !strcmp(*buffer, "GwyContainer");
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
    dump_hash(*buffer - mysize, mysize, v1file && container);
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
    static gboolean opt_all = FALSE;
    static GOptionEntry entries[] = {
        { "all", 'a', 0, G_OPTION_ARG_NONE, &opt_all, NULL, NULL },
        { "value", 'v', 0, G_OPTION_ARG_NONE, &opt_value, NULL, NULL },
        { "offset", 'o', 0, G_OPTION_ARG_NONE, &opt_offset, NULL, NULL },
        { "type", 't', 0, G_OPTION_ARG_NONE, &opt_type, NULL, NULL },
        { "object-size", 's', 0, G_OPTION_ARG_NONE, &opt_object_size, NULL, NULL },
        { "indent", 'i', 0, G_OPTION_ARG_INT, &opt_indent, NULL, NULL },
        { "raw", 'r', 0, G_OPTION_ARG_NONE, &opt_raw, NULL, NULL },
        { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, NULL, NULL },
        { "help", 'h', 0, G_OPTION_ARG_NONE, &opt_help, NULL, NULL },
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
    if (opt_all)
        opt_value = opt_offset = opt_type = opt_object_size = TRUE;
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
" -o, --offset         Print offsets in file.\n"
" -t, --type           Print component types.\n"
" -v, --values         Print values of atomic objects.\n"
" -s, --object-size    Print serialized object sizes.\n"
" -a, --all            Print all above.\n"
" -i, --indent=N       Indent each nesting level by N spaces [0-32].\n"
" -r, --raw            File is raw serialized data, no header.\n\n"
" -h, --help           Print this help and terminate.\n"
" -V, --version        Print version info and terminate.\n\n"
        );
    g_print("Please report bugs in Gwyddion bugzilla "
            "http://trific.ath.cx/bugzilla/\n");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
