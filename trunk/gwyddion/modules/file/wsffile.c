/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-wsf-spm">
 *   <comment>WSF SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0:160" value="\nPixels in X:">
 *         <match type="string" offset="0:240" value="\nLines in Y:"/>
 *     </match>
 *   </magic>
 *   <glob pattern="*.wsf"/>
 *   <glob pattern="*.WSF"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # WSF
 * # Actually, I'm not sure what the acronym stands for.  But we recognise it.
 * 0 search/160 \x0aPixels\ in\ X:
 * >&0 search/160 \x0aLines\ in\ Y: WSF AFM text data
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * WSF ASCII data
 * .wsf
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define Mili (1e-3)
#define Micro (1e-6)
#define Nano (1e-9)

#define MAGIC1 "\nPixels in X:"
#define MAGIC1_SIZE (sizeof(MAGIC1)-1)
#define MAGIC2 "\nLines in Y:"
#define MAGIC2_SIZE (sizeof(MAGIC2)-1)
#define EXTENSION ".wsf"

static gboolean      module_register(void);
static gint          wsf_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* wsf_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static GwyContainer* wsf_get_meta   (GHashTable *hash);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports WSF ASCII files."),
    "Yeti <yeti@gwyddion.net>",
    "0.4",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("wsffile",
                           N_("WSF ASCII files (.wsf)"),
                           (GwyFileDetectFunc)&wsf_detect,
                           (GwyFileLoadFunc)&wsf_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
wsf_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (strstr(fileinfo->head, MAGIC1) && strstr(fileinfo->head, MAGIC2))
        return 100;

    return 0;
}

static GwyContainer*
wsf_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL, *meta;
    GwyDataField *dfield = NULL;
    gchar *line, *p, *header_end, *value, *title,
          *header = NULL, *buffer = NULL;
    GwyTextHeaderParser parser;
    GHashTable *hash = NULL;
    gsize size;
    GError *err = NULL;
    gdouble xreal, yreal, q;
    gint i, xres, yres;
    const gchar *zunit;
    gdouble *data;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    p = buffer;
    // The first line apparently contains the file name.  Ignore it.
    if (!(line = gwy_str_next_line(&p))) {
        err_TOO_SHORT(error);
        goto fail;
    }
    // Then there are some empty lines.
    while (g_ascii_isspace(*p))
        p++;
    // Then there's the header, followed by empty lines again.
    if (!(header_end = strstr(p, "\r\n\r\n"))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header does not end with an empty line."));
        goto fail;
    }
    header = g_strndup(p, header_end - p);

    gwy_clear(&parser, 1);
    parser.key_value_separator = ":";
    hash = gwy_text_header_parse(header, &parser, NULL, NULL);

    if (!require_keys(hash, error,
                      "Pixels in X", "Lines in Y", "X Range", "Y Range",
                      "Display Type",
                      NULL))
        goto fail;

    xres = atoi(g_hash_table_lookup(hash, "Pixels in X"));
    yres = atoi(g_hash_table_lookup(hash, "Lines in Y"));
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        goto fail;

    xreal = g_ascii_strtod(g_hash_table_lookup(hash, "X Range"), NULL);
    yreal = g_ascii_strtod(g_hash_table_lookup(hash, "Y Range"), NULL);
    /* Use negated positive conditions to catch NaNs */
    if (!((xreal = fabs(xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }
    if (!((yreal = fabs(yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }
    xreal *= Micro;
    yreal *= Micro;

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
    value = title = g_hash_table_lookup(hash, "Display Type");
    if (gwy_stramong(value, "Z_DRIVE", NULL)) {
        zunit = "m";
        q = Nano;
    }
    else if (gwy_stramong(value, "Z_SENSE", "Z_ERR", "L-R", "T-B", "T+B",
                          "Z_PHASE", "Z_AMPL", "Z_DRIVE", "Aux ADC 1",
                          "Aux ADC 2", NULL)) {
        zunit = "V";
        q = Mili;
    }
    else {
        g_warning("Unknown type %s, cannot determine units.", value);
        zunit = NULL;
        q = 1.0;
    }
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), zunit);

    data = gwy_data_field_get_data(dfield);
    value = p = header_end;
    for (i = 0; i < xres*yres; i++) {
        data[i] = q*g_ascii_strtod(value, &p);
        if (p == value && (!*p || g_ascii_isspace(*p))) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("End of file reached when reading sample #%d of %d"),
                        i, xres*yres);
            goto fail;
        }
        if (p == value) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Malformed data encountered when reading sample "
                          "#%d of %d"),
                        i, xres*yres);
            goto fail;
        }
        value = p;
    }

    container = gwy_container_new();
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup(title));

    if ((meta = wsf_get_meta(hash))) {
        gwy_container_set_object_by_name(container, "/0/meta", meta);
        g_object_unref(meta);
    }

    gwy_file_channel_import_log_add(container, 0, NULL, filename);

fail:
    gwy_object_unref(dfield);
    g_free(header);
    g_free(buffer);
    g_hash_table_destroy(hash);

    return container;
}

static void
add_meta(gpointer hkey, gpointer hvalue, gpointer user_data)
{
    gchar *key = NULL, *value = NULL;

    if (!strlen((const gchar*)hvalue))
        return;

    if ((key = g_convert((const gchar*)hkey, -1,
                         "UTF-8", "ISO-8859-1",
                         NULL, NULL, NULL))
        && (value = g_convert((const gchar*)hvalue, -1,
                              "UTF-8", "ISO-8859-1",
                              NULL, NULL, NULL))) {
        /* Move units to the value */
        gchar *s;
        if ((s = strchr(key, '('))) {
            gchar *p = g_strdup(s+1);
            guint len;

            *s = '\0';
            g_strchomp(key);
            len = strlen(p);
            if (len && p[len-1] == ')') {
                p[len-1] = '\0';
                len--;
            }

            if (len) {
                gchar *q = g_strconcat(value, " ", p, NULL);
                g_free(value);
                value = q;
            }
        }

        gwy_container_set_string_by_name(GWY_CONTAINER(user_data),
                                         key, value);
    }
    g_free(key);
    /* value is either NULL or eaten by the container */
}

static GwyContainer*
wsf_get_meta(GHashTable *hash)
{
    GwyContainer *meta = gwy_container_new();
    g_hash_table_foreach(hash, add_meta, meta);
    if (gwy_container_get_n_items(meta))
        return meta;

    g_object_unref(meta);
    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
