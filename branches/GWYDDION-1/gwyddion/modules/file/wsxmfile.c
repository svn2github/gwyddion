/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klwsxmtek.
 *  E-mail: yeti@gwyddion.net, klwsxmtek@gwyddion.net.
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

#define MAGIC "WSxM file copyright Nanotec Electronica\r\n" \
              "SxM Image file\r\n"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

static gboolean      module_register       (const gchar *name);
static gint          wsxmfile_detect        (const gchar *filename,
                                            gboolean only_name);
static GwyContainer* wsxmfile_load          (const gchar *filename);
static GwyDataField* file_load_real        (const guchar *buffer,
                                            gsize size,
                                            GHashTable *meta);
static GwyDataField* read_data_field       (const guchar *buffer,
                                            gint xres,
                                            gint yres);
static gboolean      file_read_meta        (GHashTable *meta,
                                            gchar *buffer);
static void          load_metadata         (gchar *buffer,
                                            GHashTable *meta);
static void          store_metadata        (GHashTable *meta,
                                            GwyContainer *container);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "wsxmfile",
    N_("Imports Nanotec WSxM data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo wsxmfile_func_info = {
        "wsxmfile",
        N_("WSXM files (.tom)"),
        (GwyFileDetectFunc)&wsxmfile_detect,
        (GwyFileLoadFunc)&wsxmfile_load,
        NULL
    };

    gwy_file_func_register(name, &wsxmfile_func_info);

    return TRUE;
}

static gint
wsxmfile_detect(const gchar *filename, gboolean only_name)
{
    gint score = 0;
    FILE *fh;
    gchar magic[MAGIC_SIZE];

    if (only_name) {
        if (g_str_has_suffix(filename, ".tom"))
            score = 20;

        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;

    if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
        && !memcmp(magic, MAGIC, MAGIC_SIZE))
        score = 100;
    fclose(fh);

    return score;
}

static GwyContainer*
wsxmfile_load(const gchar *filename)
{
    GObject *object = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GHashTable *meta = NULL;
    guint header_size;
    gchar *p;
    gboolean ok;
    gint xres = 0, yres = 0;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (strncmp(buffer, MAGIC, MAGIC_SIZE)
        || sscanf(buffer + MAGIC_SIZE,
                  "Image header size: %u", &header_size) < 1) {
        g_warning("File %s is not a WSXM file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    if (size < header_size) {
        g_warning("File %s shorter than header size", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    meta = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    p = g_strndup(buffer, header_size);
    ok = file_read_meta(meta, p);
    g_free(p);

    if (!(p = g_hash_table_lookup(meta, "General Info::Number of columns"))
        || (xres = atol(p)) <= 0) {
        g_warning("Missing or invalid number of columns");
        ok = FALSE;
    }

    if (!(p = g_hash_table_lookup(meta, "General Info::Number of rows"))
        || (yres = atol(p)) <= 0) {
        g_warning("Missing or invalid number of rows");
        ok = FALSE;
    }

    if ((guint)size - header_size < 2*xres*yres) {
        g_warning("Expected data size %u, but it's %u",
                  2*xres*yres, (guint)size - header_size);
        ok = FALSE;
    }

    if (ok)
        dfield = read_data_field(buffer + header_size, xres, yres);
    gwy_file_abandon_contents(buffer, size, NULL);

    if (dfield) {
        object = gwy_container_new();
        gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                         G_OBJECT(dfield));
        /*store_metadata(meta, GWY_CONTAINER(object));*/
    }
    g_hash_table_destroy(meta);

    return (GwyContainer*)object;
}

static gboolean
file_read_meta(GHashTable *meta,
               gchar *buffer)
{
    /* If these parameters are missing, use defaults from
     * http://www.nanotec.es/fileform.htm */
    static struct {
        const gchar *key;
        const gchar *value;
    }
    const defaults[] = {
        { "Control::Signal Gain", "1" },
        { "Control::Z Gain", "1" },
        { "Head Settings::Preamp Gain", "1 V/nA" },
        { "Head Settings::X Calibration", "1 \305/V" },
        { "Head Settings::Y Calibration", "1 \305/V" },
        { "Head Settings::Z Calibration", "1 \305/V" },
    };
    gchar *p, *line, *key, *value, *section = NULL;
    guint len, i;

    while ((line = gwy_str_next_line(&buffer))) {
        line = g_strstrip(line);
        if (!(len = strlen(line)))
            continue;
        if (line[0] == '[' && line[len-1] == ']') {
            line[len-1] = '\0';
            section = line + 1;
            gwy_debug("Section <%s>", section);
            continue;
        }
        /* skip pre-header */
        if (!section)
            continue;

        p = strchr(line, ':');
        if (!p) {
            g_warning("Cannot parse line <%s>", line);
            continue;
        }

        *(p++) = '\0';
        key = g_strconcat(section, "::", line, NULL);
        value = g_strstrip(g_strdup(p));
        gwy_debug("<%s> = <%s>", key, value);
        g_hash_table_replace(meta, key, value);
    }
    if (strcmp(section, "Header end")) {
        g_warning("Missed end of file header");
        return FALSE;
    }

    for (i = 0; i < G_N_ELEMENTS(defaults); i++) {
        if (!g_hash_table_lookup(meta, defaults[i].key))
            g_hash_table_insert(meta,
                                g_strdup(defaults[i].key),
                                g_strdup(defaults[i].value));
    }

    return TRUE;
}

#if 0
static GwyDataField*
file_load_real(const guchar *buffer,
               gsize size,
               GHashTable *meta)
{
    GwyDataField *dfield;
    gboolean intelmode = TRUE;
    gdouble q;
    gint xres, yres;
    guchar *s;

    s = g_memdup(buffer, HEADER_SIZE);
    s[HEADER_SIZE-1] = '\0';
    load_metadata(s, meta);
    g_free(s);

    if (!(s = g_hash_table_lookup(meta, "fileformat"))) {
        g_warning("File is not a WSXM file");
        return NULL;
    }

    if (!strcmp(s, "wsxmstm"))
        type = WSXM_FILE_INT16;
    else if (!strcmp(s, "wsxmf"))
        type = WSXM_FILE_FLOAT;
    else {
        g_warning("Cannot understand file type header `%s'", s);
        return NULL;
    }
    gwy_debug("File type: %u", type);

    if (!(s = g_hash_table_lookup(meta, "xpixels"))) {
        g_warning("No xpixels (x resolution) info");
        return NULL;
    }
    xres = atol(s);

    if (!(s = g_hash_table_lookup(meta, "ypixels"))) {
        g_warning("No ypixels (y resolution) info");
        return NULL;
    }
    yres = atol(s);

    if ((s = g_hash_table_lookup(meta, "intelmode")))
        intelmode = !!atol(s);

    if (size < HEADER_SIZE + xres*yres*type) {
        g_warning("Expected data size %u, but it's %u",
                  xres*yres*type, (guint)(size - HEADER_SIZE));
        return NULL;
    }

    dfield = read_data_field(buffer + HEADER_SIZE, xres, yres,
                             type, intelmode);

    if ((s = g_hash_table_lookup(meta, "xlength"))
        && (q = g_ascii_strtod(s, NULL)) > 0)
        gwy_data_field_set_xreal(dfield, 1e-9*q);

    if ((s = g_hash_table_lookup(meta, "ylength"))
        && (q = g_ascii_strtod(s, NULL)) > 0)
        gwy_data_field_set_yreal(dfield, 1e-9*q);

    if (type == WSXM_FILE_INT16
        && (s = g_hash_table_lookup(meta, "bit2nm"))
        && (q = g_ascii_strtod(s, NULL)) > 0)
        gwy_data_field_multiply(dfield, 1e-9*q);

    return dfield;
}

static void
store_metadata(GHashTable *meta,
               GwyContainer *container)
{
    const struct {
        const gchar *id;
        const gchar *unit;
        const gchar *key;
    }
    metakeys[] = {
        { "scanspeed",   "nm/s",    "Scan speed"        },
        { "xoffset",     "nm",      "X offset"          },
        { "yoffset",     "nm",      "Y offset"          },
        { "bias",        "V",       "Bias voltage"      },
        { "current",     "nA",      "Tunneling current" },
        { "starttime",   NULL,      "Scan time"         },
        /* FIXME: I've seen other stuff, but don't know interpretation */
    };
    gchar *value;
    GString *key;
    guint i;

    key = g_string_new("");
    for (i = 0; i < G_N_ELEMENTS(metakeys); i++) {
        if (!(value = g_hash_table_lookup(meta, metakeys[i].id)))
            continue;

        g_string_printf(key, "/meta/%s", metakeys[i].key);
        if (metakeys[i].unit)
            gwy_container_set_string_by_name(container, key->str,
                                             g_strdup_printf("%s %s",
                                                             value,
                                                             metakeys[i].unit));
        else
            gwy_container_set_string_by_name(container, key->str,
                                             g_strdup(value));
    }
    g_string_free(key, TRUE);
}
#endif

static GwyDataField*
read_data_field(const guchar *buffer,
                gint xres,
                gint yres)
{
    const guint16 *p = (const guint16*)buffer;
    GwyDataField *dfield;
    gdouble *data;
    guint i;

    dfield = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, 1e-6, 1e-6, FALSE));
    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < xres*yres; i++)
        data[i] = GINT16_FROM_LE(p[i]);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

