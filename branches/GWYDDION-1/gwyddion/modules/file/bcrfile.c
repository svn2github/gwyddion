/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klbcrtek.
 *  E-mail: yeti@gwyddion.net, klbcrtek@gwyddion.net.
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
#define DEBUG 1
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "get.h"

#define HEADER_SIZE 2048

#define MAGIC1 "fileformat = bcrstm\n"
#define MAGIC_SIZE1 (sizeof(MAGIC1) - 1)
#define MAGIC2 "fileformat = bcrf\n"
#define MAGIC_SIZE2 (sizeof(MAGIC2) - 1)
#define MAGIC_SIZE (MAX(MAGIC_SIZE1, MAGIC_SIZE2))

/* values are bytes per pixel */
typedef enum {
    BCR_FILE_INT16 = 2,
    BCR_FILE_FLOAT = 4
} BCRFileType;

static gboolean      module_register       (const gchar *name);
static gint          bcrfile_detect        (const gchar *filename,
                                            gboolean only_name);
static GwyContainer* bcrfile_load          (const gchar *filename);
static GwyDataField* read_data_field       (const guchar *buffer,
                                            gint xres,
                                            gint yres,
                                            BCRFileType type,
                                            gboolean little_endian);
static GHashTable*   load_metadata         (gchar *buffer);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "bcrfile",
    N_("Imports Imagemet BCR data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klbcrtek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo bcrfile_func_info = {
        "bcrfile",
        N_("BCR files (.bcr, .bcrf)"),
        (GwyFileDetectFunc)&bcrfile_detect,
        (GwyFileLoadFunc)&bcrfile_load,
        NULL
    };

    gwy_file_func_register(name, &bcrfile_func_info);

    return TRUE;
}

static gint
bcrfile_detect(const gchar *filename, gboolean only_name)
{
    gint score = 0;
    FILE *fh;
    gchar magic[MAGIC_SIZE];

    if (only_name) {
        if (g_str_has_suffix(filename, ".bcr")
            || g_str_has_suffix(filename, ".bcrf"))
            score = 20;

        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;

    if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
        && (!memcmp(magic, MAGIC1, MAGIC_SIZE1)
            || !memcmp(magic, MAGIC2, MAGIC_SIZE2)))
        score = 100;
    fclose(fh);

    return score;
}

static GwyContainer*
bcrfile_load(const gchar *filename)
{
    BCRFileType type;
    GObject *object = NULL;
    guchar *buffer = NULL;
    guchar *s;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GHashTable *meta = NULL;
    gint xres, yres;
    gboolean intelmode = TRUE;
    gdouble q;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < HEADER_SIZE) {
        g_warning("File %s is not a BCR file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    if (!memcmp(buffer, MAGIC1, MAGIC_SIZE1))
        type = BCR_FILE_INT16;
    else if (!memcmp(buffer, MAGIC2, MAGIC_SIZE2))
        type = BCR_FILE_FLOAT;
    else {
        g_warning("Cannot understand file type header");
        goto end;
    }
    gwy_debug("File type: %u", type);

    s = g_memdup(buffer, HEADER_SIZE);
    s[HEADER_SIZE-1] = '\0';
    meta = load_metadata(s);
    g_free(s);

    if (!(s = g_hash_table_lookup(meta, "xpixels"))) {
        g_warning("No xpixels (x resolution) info");
        goto end;
    }
    xres = atol(s);

    if (!(s = g_hash_table_lookup(meta, "ypixels"))) {
        g_warning("No ypixels (y resolution) info");
        goto end;
    }
    yres = atol(s);

    if ((s = g_hash_table_lookup(meta, "intelmode")))
        intelmode = !!atol(s);

    if (size < HEADER_SIZE + xres*yres*type) {
        g_warning("Expected data size %u, but it's %u",
                  xres*yres*type, (guint)(size - HEADER_SIZE));
        goto end;
    }

    dfield = read_data_field(buffer + HEADER_SIZE, xres, yres,
                             type, intelmode);

    if ((s = g_hash_table_lookup(meta, "xlength"))
        && (q = g_ascii_strtod(s, NULL)) > 0)
        gwy_data_field_set_xreal(dfield, 1e-9*q);
    if ((s = g_hash_table_lookup(meta, "ylength"))
        && (q = g_ascii_strtod(s, NULL)) > 0)
        gwy_data_field_set_yreal(dfield, 1e-9*q);
    if (type == BCR_FILE_INT16
        && (s = g_hash_table_lookup(meta, "bit2nm"))
        && (q = g_ascii_strtod(s, NULL)) > 0)
        gwy_data_field_multiply(dfield, 1e-9*q);

    object = gwy_container_new();
    gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                     G_OBJECT(dfield));
    /*store_metadata(&bcrfile, GWY_CONTAINER(object));*/

end:
    gwy_file_abandon_contents(buffer, size, NULL);
    if (meta)
        g_hash_table_destroy(meta);

    return (GwyContainer*)object;
}

static GwyDataField*
read_data_field(const guchar *buffer,
                gint xres,
                gint yres,
                BCRFileType type,
                gboolean little_endian)
{
    GwyDataField *dfield;
    gdouble *data;
    guint i;

    dfield = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, 1e-6, 1e-6, FALSE));
    data = gwy_data_field_get_data(dfield);
    switch (type) {
        case BCR_FILE_INT16:
        if (little_endian) {
            for (i = 0; i < xres*yres; i++) {
                data[i] = (gint)(buffer)[0] + ((signed char)(buffer)[1]*256.0);
                buffer += 2;
            }
        }
        else {
            for (i = 0; i < xres*yres; i++) {
                data[i] = (gint)(buffer)[1] + ((signed char)(buffer)[0]*256.0);
                buffer += 2;
            }
        }
        break;

        case BCR_FILE_FLOAT:
        if (little_endian) {
            for (i = 0; i < xres*yres; i++)
                data[i] = get_FLOAT(&buffer);
        }
        else {
            for (i = 0; i < xres*yres; i++)
                data[i] = get_FLOAT_BE(&buffer);
        }
        gwy_data_field_multiply(dfield, 1e-9);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    return dfield;
}

static GHashTable*
load_metadata(gchar *buffer)
{
    gchar *line, *p;
    GHashTable *meta;
    gchar *key, *value;

    meta = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    while ((line = gwy_str_next_line(&buffer))) {
        if (line[0] == '%' || line[0] == '#')
            continue;

        p = strchr(line, '=');
        if (!p || p == line || !p[1])
            continue;

        key = g_strstrip(g_strndup(line, p-line));
        if (!key[0]) {
            g_free(key);
            continue;
        }
        value = g_strstrip(g_strdup(p+1));
        if (!value[0]) {
            g_free(key);
            g_free(value);
            continue;
        }

        g_hash_table_insert(meta, key, value);
    }

    return meta;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

