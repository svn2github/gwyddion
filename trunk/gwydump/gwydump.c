/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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
#include <string.h>
#include <errno.h>
#include <glib.h>

#define IND 2

static void print_help(void);
static void process_preinit_options(int *argc,
                                    char ***argv);
static gboolean dump_object(gchar **buffer,
                            gsize *size);

static gint level;
static gchar *buffer0;

int
main(int argc, char *argv[])
{
    gsize size;
    gchar *buffer;
    GError *err = NULL;
    gboolean ok;

    /* Check for --help and --version before rash file loading and GUI
     * initializatioon */
    process_preinit_options(&argc, &argv);
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
    ok = dump_object(&buffer, &size);

    return !ok;
}

static gchar*
indent(void)
{
    static GString *str = NULL;
    gint i;

    if (!str)
        str = g_string_new("");

    for (i = str->len/IND; i < level; i++)
        g_string_append(str, "  ");
    g_string_truncate(str, IND*level);

    return str->str;
}

static inline void
print_object_name(const gchar *name)
{
    g_print("object: %s\n", name);
}

static inline void
print_object_size(guint32 size)
{
    /*g_print("size: %u\n", size);*/
}

static inline void
print_component_name(const gchar *name)
{
    g_print("%s%s, ", indent(), name);
}

static inline void
print_component_type(gchar c)
{
    /*g_print("%sType: %c\n", indent(), c);*/
}

static inline gboolean
get_size(gchar **buffer,
         gsize *size,
         guint32 *value)
{
    if (*size < sizeof(gint32)) {
        g_printerr("\nTruncated size\n");
        return FALSE;
    }
    memcpy(value, *buffer, sizeof(gint32));
    *value = GUINT32_FROM_LE(*value);
    *buffer += sizeof(gint32);
    *size -= sizeof(gint32);

    return TRUE;
};

static inline gboolean
dump_boolean(gchar **buffer,
             gsize *size)
{
    gboolean value;

    if (!*size) {
        g_printerr("\nTruncated boolean\n");
        return FALSE;
    }
    value = !!**buffer;
    g_print("boolean: %s\n", value ? "TRUE" : "FALSE");
    *buffer += sizeof(gchar);
    *size -= sizeof(gchar);

    return TRUE;
}

static inline gboolean
dump_char(gchar **buffer,
          gsize *size)
{
    gchar value;

    if (!*size) {
        g_printerr("\nTruncated char\n");
        return FALSE;
    }
    value = **buffer;
    g_print("char: %02x\n", value);
    *buffer += sizeof(gchar);
    *size -= sizeof(gchar);

    return TRUE;
}

static inline gboolean
dump_int32(gchar **buffer,
           gsize *size)
{
    gint32 value;

    if (*size < sizeof(gint32)) {
        g_printerr("\nTruncated int32\n");
        return FALSE;
    }
    memcpy(&value, *buffer, sizeof(gint32));
    value = GINT32_FROM_LE(value);
    g_print("int32: %d\n", value);
    *buffer += sizeof(gint32);
    *size -= sizeof(gint32);

    return TRUE;
}

static inline gboolean
dump_double(gchar **buffer,
            gsize *size)
{
    union {
        gdouble d;
        guint64 i;
    } value;

    if (*size < sizeof(gdouble)) {
        g_printerr("\nTruncated double\n");
        return FALSE;
    }
    memcpy(&value, *buffer, sizeof(gdouble));
    value.i = GUINT64_FROM_LE(value.i);
    g_print("double: %g\n", value.d);
    *buffer += sizeof(gdouble);
    *size -= sizeof(gdouble);

    return TRUE;
}

static inline gboolean
dump_string(gchar **buffer,
            gsize *size)
{
    gchar *p;

    if (!(p = memchr(*buffer, 0, *size))) {
        g_printerr("\nTruncated string\n");
        return FALSE;
    }
    g_print("string: %s\n", *buffer);
    *size -= (p - *buffer) + 1;
    *buffer = p + 1;

    return TRUE;
}

static inline gboolean
dump_double_array(gchar **buffer,
                  gsize *size)
{
    guint32 mysize;

    if (!get_size(buffer, size, &mysize))
        return FALSE;

    if (*size < mysize*sizeof(gdouble)) {
        g_printerr("\nTruncated double array\n");
        return FALSE;
    }
    g_print("double array of size %u\n", mysize);
    *buffer += mysize*sizeof(gdouble);
    *size -= mysize*sizeof(gdouble);

    return TRUE;
}

static gboolean
dump_hash(gchar *buffer,
          gsize size)
{
    gboolean ok = TRUE;
    gchar ctype;
    gchar *p;

    while (size) {
        /* Name */
        if (!(p = memchr(buffer, 0, size))) {
            g_printerr("\nRunaway component name\n");
            return FALSE;
        }
        g_print("%08x: ", buffer - buffer0);
        print_component_name(buffer);
        size -= (p - buffer) + 1;
        buffer = p + 1;

        /* Type */
        if (!size) {
            g_printerr("\nTruncated component type\n");
            return FALSE;
        }
        ctype = *buffer;
        print_component_type(ctype);
        buffer += sizeof(gchar);
        size -= sizeof(gchar);

        /* Data */
        switch (ctype) {
            case 'o': ok = dump_object(&buffer, &size); break;
            case 'b': ok = dump_boolean(&buffer, &size); break;
            case 'c': ok = dump_char(&buffer, &size); break;
            case 'i': ok = dump_int32(&buffer, &size); break;
            case 'd': ok = dump_double(&buffer, &size); break;
            case 's': ok = dump_string(&buffer, &size); break;
            case 'D': ok = dump_double_array(&buffer, &size); break;

            default:
            g_printerr("\nUnknown component type `%c'\n", ctype);
            ok = FALSE;
            break;
        }
        if (!ok)
            return FALSE;
    }

    return TRUE;
}

static gboolean
dump_object_real(gchar **buffer,
                 gsize *size)
{
    gint32 mysize;
    gchar *p;

    /* Name */
    if (!(p = memchr(*buffer, 0, *size))) {
        g_printerr("\nRunaway object name\n");
        return FALSE;
    }
    print_object_name(*buffer);
    *size -= (p - *buffer) + 1;
    *buffer = p + 1;

    /* Size */
    if (!get_size(buffer, size, &mysize))
        return FALSE;

    if (mysize > *size) {
        g_printerr("\nTruncated object data\n");
        return FALSE;
    }
    print_object_size(mysize);

    /* Hash */
    *size -= mysize;
    *buffer += mysize;
    return dump_hash(*buffer - mysize, mysize);
}

static gboolean
dump_object(gchar **buffer,
            gsize *size)
{
    gboolean ok;

    level++;
    ok = dump_object_real(buffer, size);
    level--;

    return ok;
}

/* Check for --help and --version and eventually print help or version */
static void
process_preinit_options(int *argc,
                        char ***argv)
{
    if (*argc == 1)
        return;

    if (!strcmp((*argv)[1], "--help") || !strcmp((*argv)[1], "-h")) {
        print_help();
        exit(0);
    }

    if (!strcmp((*argv)[1], "--version") || !strcmp((*argv)[1], "-v")) {
        g_print("%s %s\n", PACKAGE, VERSION);
        exit(0);
    }
}

/* Print help */
static void
print_help(void)
{
    g_print(
"Usage: gwyiew FILENAME\n"
"Dumps Gwyddion .gwy file in a text format.\n\n"
        );
    g_print(
"Options:\n"
" -h, --help                 Print this help and terminate.\n"
" -v, --version              Print version info and terminate.\n\n"
        );
    g_print("Please report bugs in Gwyddion bugzilla "
            "http://trific.ath.cx/bugzilla/\n");
}

/******************** GwySerializable stuff *************************/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

