/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-bcr-spm">
 *   <comment>BCR SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="fileformat = bcrstm\n"/>
 *     <match type="string" offset="0" value="f\0i\0l\0e\0f\0o\0r\0m\0a\0t\0 \0=\0 \0b\0c\0r\0s\0t\0m\0_\0u\0n\0i\0c\0o\0d\0e\0\n\0"/>
 *   </magic>
 *   <glob pattern="*.bcr"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-bcrf-spm">
 *   <comment>BCRF SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="fileformat = bcrf\n"/>
 *     <match type="string" offset="0" value="f\0i\0l\0e\0f\0o\0r\0m\0a\0t\0 \0=\0 \0b\0c\0r\0f\0_\0u\0n\0i\0c\0o\0d\0e\0\n\0"/>
 *   </magic>
 *   <glob pattern="*.bcrf"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # Image Metrology BCR 3423/1/0/184/4/91-BCR-DK(30).
 * 0 string fileformat\ =\ bcrstm\x0a SPM data exchange format BCR, 16bit int data, ASCII
 * 0 string fileformat\ =\ bcrf\x0a SPM data exchange format BCR, 32bit float data, ASCII
 * 0 lestring16 fileformat\ =\ bcrstm_unicode\x0a SPM data exchange format BCR, 16bit int data, Unicode
 * 0 lestring16 fileformat\ =\ bcrf_unicode\x0a SPM data exchange format BCR, 32bit float data, Unicode
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Image Metrology BCR, BCRF
 * .bcr .bcrf
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/gwyapp.h>

#include "err.h"

/* in characters, not bytes */
#define HEADER_SIZE 2048

/* 16bit integers */
#define MAGIC1 "fileformat = bcrstm\n"
#define MAGIC_SIZE1 (sizeof(MAGIC1) - 1)

/* floats */
#define MAGIC2 "fileformat = bcrf\n"
#define MAGIC_SIZE2 (sizeof(MAGIC2) - 1)

/* 16bit integers, text header is in Microsoft-style wide characters (utf16) */
#define MAGIC3 \
    "f\0i\0l\0e\0f\0o\0r\0m\0a\0t\0 \0=\0 " \
    "\0b\0c\0r\0s\0t\0m\0_\0u\0n\0i\0c\0o\0d\0e\0\n\0"
#define MAGIC_SIZE3 (sizeof(MAGIC3) - 1)

/* floats, text header is in Microsoft-style wide characters (utf16) */
#define MAGIC4 \
    "f\0i\0l\0e\0f\0o\0r\0m\0a\0t\0 \0=\0 " \
    "\0b\0c\0r\0f\0_\0u\0n\0i\0c\0o\0d\0e\0\n\0"
#define MAGIC_SIZE4 (sizeof(MAGIC4) - 1)

#define MAGIC5 "fileformat = bcrf\r\n"
#define MAGIC_SIZE5 (sizeof(MAGIC5) - 1)

#define MAGIC_SIZE \
    (MAX(MAX(MAX(MAGIC_SIZE1, MAGIC_SIZE2), MAX(MAGIC_SIZE3, MAGIC_SIZE4)), MAGIC_SIZE5))

#define HEADER_SIZE_MAGIC "h\000e\000a\000d\000e\000r\000s\000i\000z\000e\000"

/* values are bytes per pixel */
typedef enum {
    BCR_FILE_INT16 = 2,
    BCR_FILE_FLOAT = 4
} BCRFileType;

static gboolean      module_register     (void);
static gint          bcrfile_detect      (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static GwyContainer* bcrfile_load        (const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static void          find_header_size    (const guchar *buffer,
                                          gsize *header_size);
static GwyDataField* file_load_real      (const guchar *buffer,
                                          gsize size,
                                          gsize header_size,
                                          GHashTable *meta,
                                          GwyDataField **voidmask,
                                          GError **error);
static GwyDataField* read_data_field     (const guchar *buffer,
                                          gint xres,
                                          gint yres,
                                          BCRFileType type,
                                          gboolean little_endian,
                                          GwyDataField **voidmask);
static GwyContainer* bcrfile_get_metadata(GHashTable *bcrmeta);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Image Metrology BCR data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.14",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("bcrfile",
                           N_("BCR files (.bcr, .bcrf)"),
                           (GwyFileDetectFunc)&bcrfile_detect,
                           (GwyFileLoadFunc)&bcrfile_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
bcrfile_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return (g_str_has_suffix(fileinfo->name_lowercase, ".bcr")
                || g_str_has_suffix(fileinfo->name_lowercase, ".bcrf"))
                ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && (memcmp(fileinfo->head, MAGIC1, MAGIC_SIZE1) == 0
            || memcmp(fileinfo->head, MAGIC2, MAGIC_SIZE2) == 0
            || memcmp(fileinfo->head, MAGIC3, MAGIC_SIZE3) == 0
            || memcmp(fileinfo->head, MAGIC4, MAGIC_SIZE4) == 0
            || memcmp(fileinfo->head, MAGIC5, MAGIC_SIZE5) == 0))
        score = 100;

    return score;
}

static GwyContainer*
bcrfile_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    gchar *header;
    gsize header_size, size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL, *voidmask = NULL;
    GwyTextHeaderParser parser;
    GHashTable *bcrmeta = NULL;
    gboolean utf16 = FALSE;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size > 2*HEADER_SIZE
        && (memcmp(buffer, MAGIC3, MAGIC_SIZE3) == 0
            || memcmp(buffer, MAGIC4, MAGIC_SIZE4) == 0))
        utf16 = TRUE;
    else if (size < HEADER_SIZE) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    gwy_debug("utf16: %d", utf16);

    if (utf16) {
        gunichar2 *s;

        header_size = 2*HEADER_SIZE;
        find_header_size(buffer, &header_size);
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
        s = (gunichar2*)g_memdup(buffer, header_size);
#endif
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
        s = g_new(gunichar2, HEADER_SIZE);
        swab(buffer, s, header_size);
#endif
        header = g_utf16_to_utf8(s, HEADER_SIZE, 0, 0, &err);
        g_free(s);
        if (!header) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("File header is not convertible from UTF-16: %s"),
                        err->message);
            gwy_file_abandon_contents(buffer, size, NULL);

            return NULL;
        }

    }
    else {
        header_size = HEADER_SIZE;
        header = g_memdup(buffer, header_size);
        header[header_size-1] = '\0';
    }

    gwy_clear(&parser, 1);
    parser.comment_prefix = "#\n%";
    parser.key_value_separator = "=";
    bcrmeta = gwy_text_header_parse(header, &parser, NULL, NULL);

    dfield = file_load_real(buffer + header_size, size, header_size, bcrmeta,
                            &voidmask, error);
    gwy_file_abandon_contents(buffer, size, NULL);
    if (dfield) {
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);

        /*********************************************************************/
        /*gwy_app_set_data_field_title(container, 0, g_strdup(g_strrstr(filename, "/") + 1));*/
        /***********************************************************************/
        gwy_app_channel_title_fall_back(container, 0);
        if (voidmask) {
            gwy_container_set_object_by_name(container, "/0/mask", voidmask);
            g_object_unref(voidmask);
        }

        meta = bcrfile_get_metadata(bcrmeta);
        if (meta)
            gwy_container_set_object_by_name(container, "/0/meta", meta);
        g_object_unref(meta);
    }
    g_hash_table_destroy(bcrmeta);
    g_free(header);

    return container;
}

/* Newer files contain header size embedded in the header.  Change header_size
 * if we find it. */
static void
find_header_size(const guchar *buffer,
                 gsize *header_size)
{
    gsize size = 0;
    const guchar *s;
    guint16 c;

    s = gwy_memmem(buffer, *header_size,
                   HEADER_SIZE_MAGIC, sizeof(HEADER_SIZE_MAGIC)-1);
    if (!s)
        return;

    s += sizeof(HEADER_SIZE_MAGIC)-1;
    while (s - buffer < *header_size && (c = gwy_get_guint16_le(&s) == ' '))
        ;
    s -= sizeof(guint16);
    while (s - buffer < *header_size && (c = gwy_get_guint16_le(&s) == '='))
        ;
    s -= sizeof(guint16);
    while (s - buffer < *header_size && (c = gwy_get_guint16_le(&s) == ' '))
        ;
    s -= sizeof(guint16);
    while (s - buffer < *header_size
           && (c = gwy_get_guint16_le(&s))
           && g_ascii_isdigit(c)) {
        size *= 10;
        size += c - '0';
    }
    if (size > 0) {
        gwy_debug("header size found in the file: %lu", (gulong)size);
        *header_size = 2*size;
    }
}

static GwyDataField*
file_load_real(const guchar *buffer,
               gsize size,
               gsize header_size,
               GHashTable *meta,
               GwyDataField **voidmask,
               GError **error)
{
    GwyDataField *dfield;
    GwySIUnit *siunit1 = NULL, *siunit2 = NULL;
    gboolean intelmode = TRUE, voidpixels = FALSE;
    BCRFileType type;
    gdouble q, qq;
    gint xres, yres, power10;
    guchar *s;

    if (!(s = g_hash_table_lookup(meta, "fileformat"))) {
        err_FILE_TYPE(error, "BCR/BCFR");
        return NULL;
    }

    if (gwy_strequal(s, "bcrstm") || gwy_strequal(s, "bcrstm_unicode"))
        type = BCR_FILE_INT16;
    else if (gwy_strequal(s, "bcrf") || gwy_strequal(s, "bcrf_unicode"))
        type = BCR_FILE_FLOAT;
    else {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unknown file type header: `%s'."), s);
        return NULL;
    }
    gwy_debug("File type: %u", type);

    if (!(s = g_hash_table_lookup(meta, "xpixels"))) {
        err_MISSING_FIELD(error, "xpixels");
        return NULL;
    }
    xres = atol(s);

    if (!(s = g_hash_table_lookup(meta, "ypixels"))) {
        err_MISSING_FIELD(error, "ypixels");
        return NULL;
    }
    yres = atol(s);

    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        return NULL;

    if ((s = g_hash_table_lookup(meta, "intelmode")))
        intelmode = !!atol(s);

    if (err_SIZE_MISMATCH(error, xres*yres*type, (guint)(size - header_size),
                          FALSE))
        return NULL;

    /* Do not trust `voidpixels' and always read data assuming the rules for
     * bad pixels. */
    dfield = read_data_field(buffer, xres, yres, type, intelmode, voidmask);

    if ((s = g_hash_table_lookup(meta, "xlength"))
        && (q = g_ascii_strtod(s, NULL)) > 0) {
        if (!(s = g_hash_table_lookup(meta, "xunit")))
            s = "nm";

        siunit1 = gwy_si_unit_new_parse(s, &power10);
        q *= pow10(power10);
        gwy_data_field_set_si_unit_xy(dfield, siunit1);
        gwy_data_field_set_xreal(dfield, q);
    }

    if ((s = g_hash_table_lookup(meta, "ylength"))
        && (q = g_ascii_strtod(s, NULL)) > 0) {
        if (!(s = g_hash_table_lookup(meta, "yunit")))
            s = "nm";

        siunit2 = gwy_si_unit_new_parse(s, &power10);
        q *= pow10(power10);
        if (siunit1 && !gwy_si_unit_equal(siunit1, siunit2))
            g_warning("Incompatible x and y units");
        g_object_unref(siunit2);
        gwy_data_field_set_yreal(dfield, q);
    }
    gwy_object_unref(siunit1);

    if (!(s = g_hash_table_lookup(meta, "zunit")))
        s = "nm";
    siunit1 = gwy_si_unit_new_parse(s, &power10);
    gwy_data_field_set_si_unit_z(dfield, siunit1);
    g_object_unref(siunit1);
    q = pow10(power10);

    if (type == BCR_FILE_INT16) {
        if ((s = g_hash_table_lookup(meta, "bit2nm"))
            && (qq = g_ascii_strtod(s, NULL)) > 0)
            gwy_data_field_multiply(dfield, q*qq);
    }
    else if (type == BCR_FILE_FLOAT)
        gwy_data_field_multiply(dfield, q);

    if ((s = g_hash_table_lookup(meta, "zmin"))
        && (qq = g_ascii_strtod(s, NULL)) > 0)
        gwy_data_field_add(dfield, q*qq - gwy_data_field_get_min(dfield));

    if (voidpixels) {
        gwy_data_field_set_xreal(*voidmask, gwy_data_field_get_xreal(dfield));
        gwy_data_field_set_yreal(*voidmask, gwy_data_field_get_yreal(dfield));
        siunit1 = gwy_si_unit_duplicate(gwy_data_field_get_si_unit_xy(dfield));
        gwy_data_field_set_si_unit_xy(*voidmask, siunit1);
        g_object_unref(siunit1);
        siunit1 = gwy_si_unit_new(NULL);
        gwy_data_field_set_si_unit_z(*voidmask, siunit1);
        g_object_unref(siunit1);
    }

    return dfield;
}

static GwyContainer*
bcrfile_get_metadata(GHashTable *bcrmeta)
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
    GwyContainer *meta;
    gchar *value;
    guint i;

    meta = gwy_container_new();

    for (i = 0; i < G_N_ELEMENTS(metakeys); i++) {
        if (!(value = g_hash_table_lookup(bcrmeta, metakeys[i].id)))
            continue;

        if (metakeys[i].unit)
            gwy_container_set_string_by_name(meta, metakeys[i].key,
                                             g_strdup_printf("%s %s",
                                                             value,
                                                             metakeys[i].unit));
        else
            gwy_container_set_string_by_name(meta, metakeys[i].key,
                                             g_strdup(value));
    }

    if (!gwy_container_get_n_items(meta)) {
        g_object_unref(meta);
        meta = NULL;
    }

    return meta;
}

static GwyDataField*
read_data_field(const guchar *buffer,
                gint xres,
                gint yres,
                BCRFileType type,
                gboolean little_endian,
                GwyDataField **voidmask)
{
    const guint16 *p = (const guint16*)buffer;
    GwyDataField *dfield;
    gdouble *data, *voids;
    guint i;

    dfield = gwy_data_field_new(xres, yres, 1e-6, 1e-6, FALSE);
    *voidmask = gwy_data_field_new(xres, yres, 1e-6, 1e-6, FALSE);
    gwy_data_field_fill(*voidmask, 1.0);

    data = gwy_data_field_get_data(dfield);
    voids = gwy_data_field_get_data(*voidmask);
    switch (type) {
        case BCR_FILE_INT16:
        if (little_endian) {
            for (i = 0; i < xres*yres; i++) {
                gdouble v = GINT16_FROM_LE(p[i]);
                if (v == 32767.0)
                    voids[i] = 0.0;
                else
                    data[i] = v;
            }
        }
        else {
            for (i = 0; i < xres*yres; i++) {
                gdouble v = GINT16_FROM_BE(p[i]);
                if (v == 32767.0)
                    voids[i] = 0.0;
                else
                    data[i] = v;
            }
        }
        break;

        case BCR_FILE_FLOAT:
        if (little_endian) {
            for (i = 0; i < xres*yres; i++) {
                gdouble v = gwy_get_gfloat_le(&buffer);
                if (v > 1.7e38)
                    voids[i] = 0.0;
                else
                    data[i] = v;
            }
        }
        else {
            for (i = 0; i < xres*yres; i++) {
                gdouble v = gwy_get_gfloat_be(&buffer);
                if (v > 1.7e38)
                    voids[i] = 0.0;
                else
                    data[i] = v;
            }
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }

    if (!gwy_app_channel_remove_bad_data(dfield, *voidmask))
        gwy_object_unref(*voidmask);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
