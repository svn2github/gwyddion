/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-olympus-lex-3000">
 *   <comment>Olympus LEX 3000</comment>
 *   <magic priority="10">
 *     <match type="string" offset="0" value="II\x2a\x00"/>
 *   </magic>
 *   <glob pattern="*.ols"/>
 *   <glob pattern="*.OLS"/>
 * </mime-type>
 **/

#include "config.h"
#include <stdlib.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>
#include "err.h"
#include "gwytiff.h"

#define MAGIC      "II\x2a\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define MAGIC_COMMENT "System Name =         OLS"

static gboolean      module_register (void);
static gint          ols_detect      (const GwyFileDetectInfo *fileinfo,
                                      gboolean only_name);
static GwyContainer* ols_load        (const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static GwyContainer* ols_load_tiff   (const GwyTIFF *tiff,
                                      GError **error);
static GHashTable*   ols_parse_header(gchar *p,
                                      GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports OLS data files."),
    "Jan Hořák <xhorak@gmail.com>, Yeti <yeti@gwyddion.net>",
    "0.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2008",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("ols",
                           N_("Olympus LEXT OLS3000 (.ols)"),
                           (GwyFileDetectFunc)&ols_detect,
                           (GwyFileLoadFunc)&ols_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
ols_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    GwyTIFF *tiff;
    gint score = 0;
    gchar *comment = NULL;

    if (only_name)
        return score;

    /* Weed out non-TIFFs */
    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    /* Use GwyTIFF for detection to avoid problems with fragile libtiff. */
    if ((tiff = gwy_tiff_load(fileinfo->name, NULL))
        && gwy_tiff_get_string0(tiff, GWY_TIFFTAG_IMAGE_DESCRIPTION, &comment)
        && strstr(comment, MAGIC_COMMENT))
        score = 100;

    g_free(comment);
    if (tiff)
        gwy_tiff_free(tiff);

    return score;
}

static GwyContainer*
ols_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyTIFF *tiff;
    GwyContainer *container = NULL;

    tiff = gwy_tiff_load(filename, error);
    if (!tiff)
        return NULL;

    container = ols_load_tiff(tiff, error);
    gwy_tiff_free(tiff);

    return container;
}

static GwyContainer*
ols_load_tiff(const GwyTIFF *tiff, GError **error)
{
    GwyContainer *container = NULL;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    GwyTIFFImageReader *reader = NULL;
    GHashTable *hash;
    gint i, power10;
    gchar *comment = NULL;
    const gchar *s1;
    GError *err = NULL;
    guint dir_num = 0;
    gdouble *data;
    double z_axis = 1.0, xy_axis, factor;
    GString *key;

    /* Comment with parameters is common for all data fields */
    if (!gwy_tiff_get_string0(tiff, GWY_TIFFTAG_IMAGE_DESCRIPTION, &comment)
        || !strstr(comment, MAGIC_COMMENT)) {
        g_free(comment);
        err_FILE_TYPE(error, "OLS");
        return NULL;
    }

    /* Read the comment header. */
    if (!(hash = ols_parse_header(comment, error))) {
        g_free(comment);
        return NULL;
    }

    key = g_string_new(NULL);
    for (dir_num = 0; dir_num < gwy_tiff_get_n_dirs(tiff); dir_num++) {
        reader = gwy_tiff_image_reader_free(reader);
        /* Request a reader, this ensures dimensions and stuff are defined. */
        reader = gwy_tiff_get_image_reader(tiff, dir_num, &err);
        if (!reader) {
            g_warning("Ignoring directory %u: %s.", dir_num, err->message);
            g_clear_error(&err);
            continue;
        }
        g_string_printf(key, "Data %u Info::XY Convert Value", dir_num+1);
        if (!(s1 = g_hash_table_lookup(hash, key->str))) {
            g_warning("Cannot find 'XY Convert Value' for data %u.", dir_num+1);
            continue;
        }
        xy_axis = g_ascii_strtod(s1, NULL);
        if (!((xy_axis = fabs(xy_axis)) > 0)) {
            g_warning("Real size step is 0.0, fixing to 1.0");
            xy_axis = 1.0;
        }
        g_string_printf(key, "Data %u Info::Z Convert Value", dir_num+1);
        if (!(s1 = g_hash_table_lookup(hash, key->str))) {
            g_warning("Cannot find 'Z Convert Value' for data %u.", dir_num+1);
            continue;
        }
        z_axis = g_ascii_strtod(s1, NULL);

        siunit = gwy_si_unit_new_parse("nm", &power10);
        dfield = gwy_data_field_new(reader->width, reader->height,
                                    reader->width * xy_axis * pow10(power10),
                                    reader->height * xy_axis * pow10(power10),
                                    FALSE);
        // units
        gwy_data_field_set_si_unit_xy(dfield, siunit);
        g_object_unref(siunit);

        siunit = gwy_si_unit_new_parse("nm", &power10);
        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);

        factor = z_axis * pow10(power10);
        data = gwy_data_field_get_data(dfield);

        for (i = 0; i < reader->height; i++)
            gwy_tiff_read_image_row(tiff, reader, i, factor, 0.0,
                                    data + (reader->height-1 - i)*reader->width);

        /* add read datafield to container */
        if (!container)
            container = gwy_container_new();

        gwy_container_set_object(container,
                                 gwy_app_get_data_key_for_id(dir_num),
                                 dfield);

        // free resources
        g_object_unref(dfield);
    }

    g_hash_table_destroy(hash);
    g_string_free(key, TRUE);
    g_free(comment);
    if (reader) {
        gwy_tiff_image_reader_free(reader);
        reader = NULL;
    }

    return container;
}

static GHashTable*
ols_parse_header(gchar *p, GError **error)
{
    GHashTable *hash;
    const gchar *sect = "";
    gchar *s, *line, *key;
    guint len;

    hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    while ((s = line = gwy_str_next_line(&p))) {
        g_strstrip(line);
        if (!*line)
            continue;

        /* Sections */
        len = strlen(line);
        if (line[0] == '[' && line[len-1] == ']') {
            line[len-1] = '\0';
            line++;
            g_strstrip(line);
            /* Start */
            if (!*sect) {
                sect = line;
                gwy_debug("section %s", sect);
                continue;
            }
            /* End */
            if (!g_str_has_suffix(line, " End")) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Section ‛%s’ ended with ‛%s’."), sect, line);
                g_hash_table_destroy(hash);
                return NULL;
            }
            len = strlen(line);
            line[len-4] = '\0';
            if (!gwy_strequal(line, sect)) {
                line[len-4] = ' ';
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Section ‛%s’ ended with ‛%s’."), sect, line);
                g_hash_table_destroy(hash);
                return NULL;
            }
            gwy_debug("end section %s", sect);
            sect = "";
        }
        /* Values */
        else if ((s = strchr(line, '='))) {
            if (!*sect) {
                g_warning("Field %s outside section", line);
                continue;
            }
            *s = '\0';
            s++;
            g_strstrip(line);
            g_strstrip(s);
            key = g_strconcat(sect, "::", line, NULL);
            g_hash_table_insert(hash, key, s);
            gwy_debug("<%s> = <%s>", key, s);
        }
        /* WTF? */
        else {
            g_warning("Stray line %s", line);
        }
    }

    return hash;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
