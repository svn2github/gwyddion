/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/*
 * XXX: This module implements its own TIFF loader because Intematix files
 * do not contain tags in ascending tag number order which chokes libTIFF.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-intematix-spm">
 *   <comment>Intematix SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="II\x2a\x00">
 *       <match type="string" offset="8:160" value="Intematix"/>
 *     </match>
 *   </magic>
 *   <glob pattern="*.sdf"/>
 *   <glob pattern="*.SDF"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Intematix SDF
 * .sdf
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "gwytiff.h"

#define MAGIC      "II\x2a\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

/* The value of ISDF_TIFFTAG_FILEID */
#define ISDF_MAGIC_NUMBER 0x00534446

/* Custom TIFF tags */
enum {
    ISDF_TIFFTAG_FILEID = 65000,
    ISDF_TIFFTAG_FILETYPE,
    ISDF_TIFFTAG_DATATYPE,
    ISDF_TIFFTAG_FILEINFO,
    ISDF_TIFFTAG_USERINFO,
    ISDF_TIFFTAG_DATARANGE,
    ISDF_TIFFTAG_SPMDATA,
    ISDF_TIFFTAG_DATACNVT,
    ISDF_TIFFTAG_DATAUNIT,
    ISDF_TIFFTAG_IMGXDIM,
    ISDF_TIFFTAG_IMGYDIM,
    ISDF_TIFFTAG_IMGZDIM,
    ISDF_TIFFTAG_XDIMUNIT,
    ISDF_TIFFTAG_YDIMUNIT,
    ISDF_TIFFTAG_ZDIMUNIT,
    ISDF_TIFFTAG_SPMDATAPOS,
    ISDF_TIFFTAG_IMAGEDEPTH,
    ISDF_TIFFTAG_SAMPLEINFO,
    ISDF_TIFFTAG_SCANRATE,
    ISDF_TIFFTAG_BIASVOLTS,
    ISDF_TIFFTAG_ZSERVO,
    ISDF_TIFFTAG_ZSERVOREF,
    ISDF_TIFFTAG_ZSERVOBW,
    ISDF_TIFFTAG_ZSERVOSP,
    ISDF_TIFFTAG_ZSERVOPID
};

typedef struct {
    guint file_type;
    guint data_type;
    guint xres;
    guint yres;
    guint zres;
    GwyTIFFDataType raw_data_type;
    guint raw_data_len;
    const guchar *raw_data;
    gdouble data_cnvt;
    gdouble xreal;
    gdouble yreal;
    gchar *xunit;
    gchar *yunit;
    gchar *dataunit;
} ISDFImage;

static gboolean      module_register       (void);
static gint          isdf_detect           (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* isdf_load             (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static gboolean      isdf_image_fill_info  (ISDFImage *image,
                                            const GwyTIFF *tiff,
                                            GError **error);
static GwyContainer* isdf_get_metadata     (const GwyTIFF *tiff,
                                            const ISDFImage *image);
static void          isdf_image_free       (ISDFImage *image);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports Intematix SDF data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("intematix",
                           N_("Intematix SDF data files (.sdf)"),
                           (GwyFileDetectFunc)&isdf_detect,
                           (GwyFileLoadFunc)&isdf_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
isdf_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    GwyTIFF *tiff;
    gint score = 0;
    guint magic;

    if (only_name)
        return score;

    /* Weed out non-TIFFs */
    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    if ((tiff = gwy_tiff_load(fileinfo->name, NULL))
         && gwy_tiff_get_sint0(tiff, ISDF_TIFFTAG_FILEID, &magic)
         && magic == ISDF_MAGIC_NUMBER)
        score = 100;

    if (tiff)
        gwy_tiff_free(tiff);

    return score;
}

static GwyContainer*
isdf_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *meta, *container = NULL;
    GwyDataField *dfield;
    GwySIUnit *siunitx, *siunity, *siunitz;
    GwyTIFF *tiff;
    ISDFImage image;
    guint magic, t, i, j;
    gint power10x, power10y, power10z;
    const guchar *p;
    gdouble *data;
    gdouble q;

    if (!(tiff = gwy_tiff_load(filename, error)))
        return NULL;

    gwy_clear(&image, 1);
    if (!gwy_tiff_get_sint0(tiff, ISDF_TIFFTAG_FILEID, &magic)
        || magic != ISDF_MAGIC_NUMBER) {
        err_FILE_TYPE(error, "Intematix SDF");
        goto fail;
    }

    if (!isdf_image_fill_info(&image, tiff, error))
        goto fail;

    if (image.yres < 2) {
        err_NO_DATA(error);
        goto fail;
    }

    if (err_DIMENSION(error, image.xres) || err_DIMENSION(error, image.yres))
        goto fail;

    t = gwy_tiff_data_type_size(image.raw_data_type);
    if (err_SIZE_MISMATCH(error, t*image.xres*image.yres, t*image.raw_data_len,
                          TRUE))
        goto fail;

    siunitx = gwy_si_unit_new_parse(image.xunit, &power10x);
    siunity = gwy_si_unit_new_parse(image.yunit, &power10y);
    siunitz = gwy_si_unit_new_parse(image.dataunit, &power10z);
    if (!gwy_si_unit_equal(siunitx, siunity))
        g_warning("Different x and y units are not representable, ignoring y.");

    /* Use negated positive conditions to catch NaNs */
    if (!((image.xreal = fabs(image.xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        image.xreal = 1.0;
    }
    if (!((image.yreal = fabs(image.yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        image.yreal = 1.0;
    }
    dfield = gwy_data_field_new(image.xres, image.yres,
                                image.xreal*pow10(power10x),
                                image.yreal*pow10(power10y),
                                FALSE);
    gwy_data_field_set_si_unit_xy(dfield, siunitx);
    gwy_data_field_set_si_unit_z(dfield, siunitz);
    g_object_unref(siunitx);
    g_object_unref(siunity);
    g_object_unref(siunitz);
    q = pow10(power10z)/image.data_cnvt;

    data = gwy_data_field_get_data(dfield);
    p = image.raw_data;
    switch (image.raw_data_type) {
        case GWY_TIFF_SLONG:
        for (i = 0; i < image.yres; i++) {
            for (j = 0; j < image.xres; j++)
                data[i*image.xres + j] = q*tiff->get_gint32(&p);
        }
        break;

        case GWY_TIFF_DOUBLE:
        for (i = 0; i < image.yres; i++) {
            for (j = 0; j < image.xres; j++)
                data[i*image.xres + j] = q*tiff->get_gdouble(&p);
        }
        break;

        default:
        g_critical("Should not be reached.");
        break;
    }

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    meta = isdf_get_metadata(tiff, &image);
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    if (gwy_container_gis_string_by_name(meta, "Data type", &p))
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(p));
    gwy_app_channel_title_fall_back(container, 0);

fail:
    isdf_image_free(&image);
    gwy_tiff_free(tiff);

    return container;
}

static gboolean
isdf_image_fill_info(ISDFImage *image,
                     const GwyTIFF *tiff,
                     GError **error)
{
    const GwyTIFFEntry *entry;
    const guchar *p;

    /* Required parameters */
    if (!(gwy_tiff_get_uint0(tiff, GWY_TIFFTAG_IMAGE_WIDTH, &image->xres)
          && gwy_tiff_get_uint0(tiff, GWY_TIFFTAG_IMAGE_LENGTH, &image->yres)
          && gwy_tiff_get_uint0(tiff, ISDF_TIFFTAG_IMAGEDEPTH, &image->zres)
          && gwy_tiff_get_uint0(tiff, ISDF_TIFFTAG_FILETYPE, &image->file_type)
          && gwy_tiff_get_uint0(tiff, ISDF_TIFFTAG_DATATYPE, &image->data_type)
          && gwy_tiff_get_float0(tiff, ISDF_TIFFTAG_DATACNVT, &image->data_cnvt)
          && gwy_tiff_get_string0(tiff, ISDF_TIFFTAG_XDIMUNIT, &image->xunit)
          && gwy_tiff_get_string0(tiff, ISDF_TIFFTAG_YDIMUNIT, &image->yunit)
          && gwy_tiff_get_string0(tiff, ISDF_TIFFTAG_DATAUNIT, &image->dataunit)
          && gwy_tiff_get_float0(tiff, ISDF_TIFFTAG_IMGXDIM, &image->xreal)
          && gwy_tiff_get_float0(tiff, ISDF_TIFFTAG_IMGYDIM, &image->yreal))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Parameter tag set is incomplete."));
        return FALSE;
    }

    if (image->zres != 1) {
        err_UNSUPPORTED(error, _("image depth"));
        return FALSE;
    }

    if (!(entry = gwy_tiff_find_tag(tiff, 0, ISDF_TIFFTAG_SPMDATA))) {
        err_NO_DATA(error);
        return FALSE;
    }
    p = entry->value;
    image->raw_data = tiff->data + tiff->get_guint32(&p);
    image->raw_data_type = entry->type;
    image->raw_data_len = entry->count;
    if (image->raw_data_type != GWY_TIFF_SLONG
        && image->raw_data_type != GWY_TIFF_DOUBLE) {
        err_DATA_TYPE(error, image->raw_data_type);
        return FALSE;
    }

    return TRUE;
}

static void
meta_add_string(GwyContainer *meta,
                const GwyTIFF *tiff,
                guint tag,
                const gchar *name)
{
    gchar *t;

    if (!gwy_tiff_get_string0(tiff, tag, &t))
        return;

    g_strstrip(t);
    if (*t)
        gwy_container_set_string_by_name(meta, name, t);
    else
        g_free(t);
}

static GwyContainer*
isdf_get_metadata(const GwyTIFF *tiff,
                  const ISDFImage *image)
{
    GwyContainer *meta;
    const gchar *s;
    gdouble v;

    meta = gwy_container_new();

    if ((s = gwy_enuml_to_string(image->file_type,
                                 "STM", 1, "AFM", 2, "EMP", 3,
                                 NULL)))
        gwy_container_set_string_by_name(meta, "File type", g_strdup(s));

    s = gwy_enuml_to_string
            (image->data_type,
             "STM Topography forward", 0x0001,
             "STM Topography backward", 0x0002,
             "STM Tunneling current forward", 0x0003,
             "STM Tunneling current backward", 0x0004,
             "STM A/D channel signal forward", 0x0005,
             "STM A/D channel signal backward", 0x0006,
             "STM 1D I-V spectroscopy", 0x0010,
             "STM 1D dI/dV spectroscopy", 0x0011,
             "STM 2D CITS spectroscopy", 0x0012,
             "STM 2D dI/dV spectroscopy", 0x0013,
             "STM 1D tunneling current approaching spectroscopy", 0x0020,
             "STM 1D A/D channel signal approaching spectroscopy", 0x0021,
             "STM 2D tunneling current approaching spectroscopy", 0x0022,
             "STM 2D A/D channel signal approaching spectroscopy", 0x0023,
             "AFM Topography forward", 0x0101,
             "AFM Topography backward", 0x0102,
             "AFM Error forward", 0x0103,
             "AFM Error backward", 0x0104,
             "AFM A/D channel signal forward", 0x0105,
             "AFM A/D channel signal backward", 0x0106,
             "EMP Topography forward", 0x0201,
             "EMP Topography backward", 0x0202,
             "EMP Resonnant frequency forward", 0x0203,
             "EMP Resonnant frequency backward", 0x0204,
             "EMP Quality factor backward", 0x0205,
             "EMP Quality factor forward", 0x0206,
             "EMP A/D channel signal forward", 0x0207,
             "EMP A/D channel signal backward", 0x0208,
             "EMP 1D frequency sweeping I-signal spectroscopy", 0x0210,
             "EMP 1D frequency sweeping Q-signal spectroscopy", 0x0211,
             "EMP 2D frequency sweeping I-signal spectroscopy", 0x0212,
             "EMP 2D frequency sweeping I-signal spectroscopy", 0x0213,
             "EMP 1D approaching resonant frequency spectroscopy", 0x0220,
             "EMP 1D approaching quality factor spectroscopy", 0x0221,
             "EMP 1D approaching A/D channel signal spectroscopy", 0x0222,
             "EMP 2D approaching resonant frequency spectroscopy", 0x0223,
             "EMP 2D approaching quality factor spectroscopy", 0x0224,
             "EMP 2D approaching A/D channel signal spectroscopy", 0x0225,
             NULL);
    if (s)
        gwy_container_set_string_by_name(meta, "Data type", g_strdup(s));

    meta_add_string(meta, tiff, GWY_TIFFTAG_IMAGE_DESCRIPTION, "Description");
    meta_add_string(meta, tiff, GWY_TIFFTAG_SOFTWARE, "Software");
    meta_add_string(meta, tiff, GWY_TIFFTAG_DATE_TIME, "Date");
    meta_add_string(meta, tiff, ISDF_TIFFTAG_FILEINFO, "File information");
    meta_add_string(meta, tiff, ISDF_TIFFTAG_USERINFO, "User information");
    meta_add_string(meta, tiff, ISDF_TIFFTAG_SAMPLEINFO, "Sample information");

    if (gwy_tiff_get_float0(tiff, ISDF_TIFFTAG_SCANRATE, &v))
        gwy_container_set_string_by_name(meta, "Scan rate",
                                         g_strdup_printf("%g line/s", v));
    if (gwy_tiff_get_float0(tiff, ISDF_TIFFTAG_BIASVOLTS, &v))
        gwy_container_set_string_by_name(meta, "Bias",
                                         g_strdup_printf("%g V", v));

    return meta;
}

static void
isdf_image_free(ISDFImage *image)
{
    g_free(image->xunit);
    g_free(image->yunit);
    g_free(image->dataunit);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
