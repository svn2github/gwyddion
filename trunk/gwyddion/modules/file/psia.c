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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-USERGUIDE]
 * Park Systems
 * .tiff, .tif
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libprocess/spectra.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "gwytiff.h"

#define MAGIC      "II\x2a\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define Micrometre (1e-6)

enum {
    /* The value of PSIA_TIFFTAG_MagicNumber */
    PSIA_MAGIC_NUMBER = 0x0E031301u,
    /* Version values */
    PSIA_VERSION1 = 0x1000001u,
    PSIA_VERSION2 = 0x1000002u,
    /* Maximum number of different spectroscopy channels */
    PSIA_MAX_SPECTRO_CHANNEL = 8,
};

/* Custom TIFF tags */
enum {
    PSIA_TIFFTAG_MagicNumber        = 50432,
    PSIA_TIFFTAG_Version            = 50433,
    PSIA_TIFFTAG_Data               = 50434,
    PSIA_TIFFTAG_Header             = 50435,
    PSIA_TIFFTAG_Comments           = 50436,
    PSIA_TIFFTAG_LineProfileHeader  = 50437,
    PSIA_TIFFTAG_SpectroscopyHeader = 50438,
    PSIA_TIFFTAG_SpectroscopyData   = 50439,
    /* Nothing is known about these */
    PSIA_TIFFTAG_InternalData       = 50440,
    PSIA_TIFFTAG_Reserved           = 50441,
};

typedef enum {
    PSIA_2D_MAPPED    = 0,
    PSIA_LINE_PROFILE = 1,
    PSIA_SPECTROSCOPY = 2,
} PSIAImageType;

typedef enum {
    PSIA_SPEC_SPECTROSCOPY = 0,
    PSIA_SPEC_INDENTOR     = 1
} PSIASpectroType;

/* Version 2+ only */
typedef enum {
    PSIA_DATA_INT16 = 0,
    PSIA_DATA_INT32 = 1,
    PSIA_DATA_FLOAT = 2,
} PSIADataType;

typedef struct {
    PSIAImageType image_type;
    gchar *source_name;     /* [32] Topography, ... */
    gchar *image_mode;      /* [8] AFM, NCM, ... */
    gdouble lpf_strength;   /* Low-pass filter strength */
    gboolean auto_flatten;  /* Automatic flatten after scan */
    gboolean ac_track;    /* AC track, order of flattening + 1 (WTF?) */
    guint32 xres;
    guint32 yres;
    gdouble angle;          /* Of fast axis wrt positive x-axis */
    gboolean sine_scan;
    gdouble overscan_rate;  /* In % */
    gboolean forward;       /* Otherwise backward */
    gboolean scan_up;       /* Otherwise scan down */
    gboolean swap_xy;       /* Swap slow/fast, actually */
    gdouble xreal;          /* In micrometers */
    gdouble yreal;
    gdouble xoff;
    gdouble yoff;
    gdouble scan_rate;      /* In rows per second */
    gdouble set_point;      /* Error signal set point */
    gchar *set_point_unit;  /* [8] */
    gdouble tip_bias;       /* In volts */
    gdouble sample_bias;
    gdouble data_gain;
    gdouble z_scale;        /* Scale multiplier, they say it is always 1 */
    gdouble z_offset;
    gchar *z_unit;          /* [8] */
    gint data_min;          /* Statistics, we do not trust these anyway */
    gint data_max;
    gint data_avg;
    gboolean compression;
    gboolean logscale;
    gboolean square;
    /* Only in version 2+
     * NB: This must be interpreted as the new version can have different data
     * types. */
    gdouble z_servo_gain;
    gdouble z_scanner_range;
    gchar *xy_voltage_mode;  /* [8] */
    gchar *z_voltage_mode;   /* [8] */
    gchar *xy_servo_mode;    /* [8] */
    PSIADataType data_type;
    gint reserved1;
    gint reserved2;
    gdouble ncm_amplitude;
    gdouble ncm_frequency;
    gdouble head_rotation_angle;
    gchar *cantilever_name;  /* [16] */
} PSIAImageHeader;

typedef struct {
    gboolean low_pass_filter;
    gdouble lp_strength;
    gint avg_mode;
    /* There is much more stuff, but it seems GUI-related */
} PSIALineProfileHeader;

typedef struct {
    gchar *source_name;   /* [32] Topography, ... */
    gchar *w_unit;        /* [8] */
    gdouble data_gain;
    gboolean x_axis_source;
    gboolean y_axis_source;
} PSIASpectroscopyChannel;

typedef struct {
    PSIASpectroscopyChannel channel[PSIA_MAX_SPECTRO_CHANNEL];
    gint spect_sources;      /* Number of spectro sources (channels?) */
    gint average;
    gint res;                /* Number of data values in a spectrum */
    gint npoints;            /* Number spectroscopy points */
    gint driving_source_index;
    gdouble forward_period;  /* In seconds */
    gdouble backward_period; /* In seconds */
    gdouble forward_speed;   /* Driving source unit/second */
    gdouble backward_speed;  /* Driving source unit/second */
    gboolean volume_image;
    gdouble offset[PSIA_MAX_SPECTRO_CHANNEL];
    gboolean log_scale[PSIA_MAX_SPECTRO_CHANNEL];
    gboolean square[PSIA_MAX_SPECTRO_CHANNEL];
    /* Version 2+ only */
    gint spec_point_per_x;   /* If volume_image && !spec_point_per_x, then
                                points are arranged in a square matrix */
    gboolean has_reference_image;
    gdouble xreal;     /* Grid dimensions */
    gdouble yreal;
    gdouble xoff;
    gdouble yoff;
    gdouble force_constant;      /* F-D, in Newton/meter */
    gdouble sensitivity;         /* F-D, in Volt/micrometer */
    gdouble force_limit;         /* F-D, in Volts */
    gdouble time_interval;       /* F-D, in seconds? */
    gdouble max_voltage;         /* I-V, in Volts */
    gdouble min_voltage;         /* I-V, in Volts */
    gdouble start_voltage;       /* I-V, in Volts */
    gdouble end_voltage;         /* I-V, in Volts */
    gdouble delayed_start_time;  /* I-V, in seconds? */
    gboolean z_servo;            /* I-V */
    gdouble data_gain;
    gchar *w_unit;
    gboolean use_extended_header;
    PSIASpectroType spec_type;
    gdouble reset_level;           /* Photo-current information */
    gdouble reset_duration;
    gdouble operation_level;
    gdouble operation_duration;
    gdouble time_before_reset;
    gdouble time_after_reset;
    gdouble time_before_light_on;
    gdouble time_light_duration;
    gint reserved[30];
} PSIASpectroscopyHeader;

static gboolean      module_register         (void);
static gint          psia_detect             (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer* psia_load               (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static GwyContainer* psia_load_tiff          (GwyTIFF *tiff,
                                              GError **error);
static void          psia_free_image_header  (PSIAImageHeader *header);
static void          psia_free_spectro_header(PSIASpectroscopyHeader *header);
static void          psia_read_data_field    (GwyDataField *dfield,
                                              const guchar *p,
                                              PSIADataType data_type,
                                              gdouble q,
                                              gdouble z_scale,
                                              gdouble z0);
static guint         psia_read_image_header  (const guchar *p,
                                              gsize size,
                                              guint version,
                                              PSIAImageHeader *header,
                                              GError **error);
static gboolean      psia_read_spectro_header(const guchar *p,
                                              gsize size,
                                              guint version,
                                              PSIASpectroscopyHeader *header,
                                              GError **error);
static void          psia_read_spectra       (GwyContainer *container,
                                              GwyTIFF *tiff,
                                              PSIADataType data_type,
                                              guint version);
static gchar*        psia_wchar_to_utf8      (const guchar **src,
                                              guint len);
static GwyContainer* psia_get_metadata       (PSIAImageHeader *header,
                                              guint version);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports Park Systems data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("psia",
                           N_("Park Systems data files (.tiff, .tif)"),
                           (GwyFileDetectFunc)&psia_detect,
                           (GwyFileLoadFunc)&psia_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
psia_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    GwyTIFF *tiff;
    gint score = 0;
    guint magic, version;

    if (only_name)
        return score;

    /* Weed out non-TIFFs */
    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    if ((tiff = gwy_tiff_load(fileinfo->name, NULL))
        && gwy_tiff_get_uint0(tiff, PSIA_TIFFTAG_MagicNumber, &magic)
        && magic == PSIA_MAGIC_NUMBER
        && gwy_tiff_get_uint0(tiff, PSIA_TIFFTAG_Version, &version)
        && (version == PSIA_VERSION1 || version == PSIA_VERSION2))
        score = 100;

    if (tiff)
        gwy_tiff_free(tiff);

    return score;
}

static GwyContainer*
psia_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyTIFF *tiff;
    GwyContainer *container = NULL;

    tiff = gwy_tiff_load(filename, error);
    if (!tiff)
        return NULL;

    container = psia_load_tiff(tiff, error);
    gwy_tiff_free(tiff);

    return container;
}

static GwyContainer*
psia_load_tiff(GwyTIFF *tiff, GError **error)
{
    const GwyTIFFEntry *entry;
    PSIAImageHeader header;
    GwyContainer *container = NULL;
    GwyContainer *meta = NULL;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    guint magic, version, bps, i;
    const guchar *p, *data;
    gchar *comment = NULL;
    gint count, data_len, power10;
    gdouble q, z0;

    if (!gwy_tiff_get_uint0(tiff, PSIA_TIFFTAG_MagicNumber, &magic)
        || magic != PSIA_MAGIC_NUMBER
        || !gwy_tiff_get_uint0(tiff, PSIA_TIFFTAG_Version, &version)
        || !(version == PSIA_VERSION1 || version == PSIA_VERSION2)) {
        err_FILE_TYPE(error, "Park Systems");
        return NULL;
    }
    gwy_debug("version: %x", version);

    /* Data */
    entry = gwy_tiff_find_tag(tiff, 0, PSIA_TIFFTAG_Data);
    if (!entry) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Data tag is missing."));
        return NULL;
    }
    p = entry->value;
    count = tiff->get_guint32(&p);
    data = tiff->data + count;
    data_len = entry->count;
    gwy_debug("data_len: %d", data_len);
    if (data_len + count > tiff->size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File is truncated."));
        return NULL;
    }

    /* Header */
    entry = gwy_tiff_find_tag(tiff, 0, PSIA_TIFFTAG_Header);
    if (!entry) {
        err_FILE_TYPE(error, "Park Systems");
        return NULL;
    }
    p = entry->value;
    i = tiff->get_guint32(&p);
    p = tiff->data + i;
    count = entry->count;
    gwy_debug("[Header] count: %d", count);

    /* Parse header */
    if (!(bps = psia_read_image_header(p, count, version, &header, error))) {
        psia_free_image_header(&header);
        return NULL;
    }

    if (err_SIZE_MISMATCH(error, bps*header.xres*header.yres, data_len, TRUE)) {
        psia_free_image_header(&header);
        return NULL;
    }

    gwy_tiff_get_string0(tiff, PSIA_TIFFTAG_Comments, &comment);

    dfield = gwy_data_field_new(header.xres, header.yres,
                                header.xreal, header.yreal,
                                FALSE);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    if (header.z_unit)
        siunit = gwy_si_unit_new_parse(header.z_unit, &power10);
    else {
        g_warning("Z units are missing");
        siunit = gwy_si_unit_new_parse("um", &power10);
    }
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    if (header.z_scale == 0.0)
        header.z_scale = 1.0;
    z0 = header.z_offset;
    q = pow10(power10)*header.data_gain;
    psia_read_data_field(dfield, data, header.data_type, q, header.z_scale, z0);

    gwy_data_field_invert(dfield, header.scan_up, !header.forward, FALSE);
    if (header.swap_xy) {
        gwy_data_field_rotate(dfield, 0.5*G_PI, GWY_INTERPOLATION_ROUND);
        gwy_data_field_invert(dfield, FALSE, TRUE, FALSE);
    }

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    if (header.source_name && *header.source_name)
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(header.source_name));

    meta = psia_get_metadata(&header, version);
    if (comment && *comment) {
        /* FIXME: Charset conversion. But from what? */
        gwy_container_set_string_by_name(meta, "Comment", comment);
        comment = NULL;
    }
    g_free(comment);
    gwy_container_set_string_by_name(meta, "Version",
                                     g_strdup_printf("%08x", version));

    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    psia_free_image_header(&header);

    /* psia_read_spectra(container, tiff, header.data_type, version); */

    return container;
}

static void
psia_free_image_header(PSIAImageHeader *header)
{
    g_free(header->source_name);
    g_free(header->image_mode);
    g_free(header->set_point_unit);
    g_free(header->z_unit);
}

static void
psia_free_spectro_header(PSIASpectroscopyHeader *header)
{
    guint i;

    for (i = 0; i < PSIA_MAX_SPECTRO_CHANNEL; i++) {
        g_free(header->channel[i].source_name);
        g_free(header->channel[i].w_unit);
    }
    g_free(header->w_unit);
}

static void
psia_read_data_field(GwyDataField *dfield,
                     const guchar *p,
                     PSIADataType data_type,
                     gdouble q,
                     gdouble z_scale,
                     gdouble z0)
{
    GwyRawDataType rawdatatype;
    gint xres, yres;

    if (data_type == PSIA_DATA_INT16)
        rawdatatype = GWY_RAW_DATA_SINT16;
    else if (data_type == PSIA_DATA_INT32)
        rawdatatype = GWY_RAW_DATA_SINT32;
    else if (data_type == PSIA_DATA_FLOAT)
        rawdatatype = GWY_RAW_DATA_FLOAT;
    else
        g_return_if_reached();

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gwy_convert_raw_data(p, xres*yres, 1, rawdatatype,
                         GWY_BYTE_ORDER_LITTLE_ENDIAN,
                         gwy_data_field_get_data(dfield),
                         q*z_scale, q*z0);
}

static guint
psia_read_image_header(const guchar *p,
                       gsize size,
                       guint version,
                       PSIAImageHeader *header,
                       GError **error)
{
    guint bps = 0;

    gwy_clear(header, 1);

    if ((version == PSIA_VERSION1 && size < 356)
        || (version == PSIA_VERSION2 && size < 580)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Header is too short (only %lu bytes)."),
                    (gulong)size);
        return 0;
    }

    /* Parse header */
    gwy_clear(header, 1);
    header->image_type = gwy_get_guint32_le(&p);
    gwy_debug("image_type: %d", header->image_type);
    if (header->image_type != PSIA_2D_MAPPED
        && header->image_type != PSIA_SPECTROSCOPY) {
        err_NO_DATA(error);
        return 0;
    }
    header->source_name = psia_wchar_to_utf8(&p, 32);
    header->image_mode = psia_wchar_to_utf8(&p, 8);
    gwy_debug("source_name: <%s>, image_mode: <%s>",
              header->source_name, header->image_mode);
    header->lpf_strength = gwy_get_gdouble_le(&p);
    header->auto_flatten = gwy_get_guint32_le(&p);
    header->ac_track = gwy_get_guint32_le(&p);
    header->xres = gwy_get_guint32_le(&p);
    header->yres = gwy_get_guint32_le(&p);
    gwy_debug("xres: %d, yres: %d", header->xres, header->yres);
    if (err_DIMENSION(error, header->xres)
        || err_DIMENSION(error, header->yres))
        return 0;

    header->angle = gwy_get_gdouble_le(&p);
    header->sine_scan = gwy_get_guint32_le(&p);
    header->overscan_rate = gwy_get_gdouble_le(&p);
    header->forward = gwy_get_guint32_le(&p);
    header->scan_up = gwy_get_guint32_le(&p);
    header->swap_xy = gwy_get_guint32_le(&p);
    gwy_debug("forward: %d, upward: %d, swapxy: %d", header->forward, header->scan_up, header->swap_xy);
    header->xreal = gwy_get_gdouble_le(&p);
    header->yreal = gwy_get_gdouble_le(&p);
    gwy_debug("xreal: %g, yreal: %g", header->xreal, header->yreal);
    /* Use negated positive conditions to catch NaNs */
    if (!((header->xreal = fabs(header->xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        header->xreal = 1.0;
    }
    if (!((header->yreal = fabs(header->yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        header->yreal = 1.0;
    }
    header->xreal *= Micrometre;
    header->yreal *= Micrometre;

    header->xoff = gwy_get_gdouble_le(&p) * Micrometre;
    header->yoff = gwy_get_gdouble_le(&p) * Micrometre;
    gwy_debug("xoff: %g, yoff: %g", header->xoff, header->yoff);
    header->scan_rate = gwy_get_gdouble_le(&p);
    header->set_point = gwy_get_gdouble_le(&p);
    header->set_point_unit = psia_wchar_to_utf8(&p, 8);
    if (!header->set_point_unit)
        header->set_point_unit = g_strdup("V");
    header->tip_bias = gwy_get_gdouble_le(&p);
    header->sample_bias = gwy_get_gdouble_le(&p);
    header->data_gain = gwy_get_gdouble_le(&p);
    header->z_scale = gwy_get_gdouble_le(&p);
    header->z_offset = gwy_get_gdouble_le(&p);
    gwy_debug("data_gain: %g, z_scale: %g", header->data_gain, header->z_scale);
    header->z_unit = psia_wchar_to_utf8(&p, 8);
    gwy_debug("z_unit: <%s>", header->z_unit);
    header->data_min = gwy_get_gint32_le(&p);
    header->data_max = gwy_get_gint32_le(&p);
    header->data_avg = gwy_get_gint32_le(&p);
    header->compression = gwy_get_guint32_le(&p);
    header->logscale = gwy_get_guint32_le(&p);
    header->square = gwy_get_guint32_le(&p);

    if (version == PSIA_VERSION2) {
        header->z_servo_gain = gwy_get_gdouble_le(&p);
        header->z_scanner_range = gwy_get_gdouble_le(&p);
        header->xy_voltage_mode = psia_wchar_to_utf8(&p, 8);
        header->z_voltage_mode = psia_wchar_to_utf8(&p, 8);
        header->xy_servo_mode = psia_wchar_to_utf8(&p, 8);
        header->data_type = gwy_get_guint32_le(&p);
        header->reserved1 = gwy_get_guint32_le(&p);
        header->reserved2 = gwy_get_guint32_le(&p);
        header->ncm_amplitude = gwy_get_gdouble_le(&p);
        header->ncm_frequency = gwy_get_gdouble_le(&p);
        header->cantilever_name = psia_wchar_to_utf8(&p, 16);
    }
    else
        header->data_type = PSIA_DATA_INT16;

    gwy_debug("data_type: %d", header->data_type);
    if (header->data_type == PSIA_DATA_INT16)
        bps = 2;
    else if (header->data_type == PSIA_DATA_INT32
             || header->data_type == PSIA_DATA_FLOAT)
        bps = 4;
    else
        err_DATA_TYPE(error, header->data_type);

    return bps;
}

static gboolean
psia_read_spectro_header(const guchar *p,
                         gsize size,
                         guint version,
                         PSIASpectroscopyHeader *header,
                         GError **error)
{
    guint i;

    gwy_clear(header, 1);

    gwy_debug("size: %lu", (gulong)size);
    if ((version == PSIA_VERSION1 && size < 936)
        || (version == PSIA_VERSION2 && size < 1118)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Spectroscopy header is too short (only %lu bytes)."),
                    (gulong)size);
        return FALSE;
    }

    for (i = 0; i < PSIA_MAX_SPECTRO_CHANNEL; i++) {
        PSIASpectroscopyChannel *channel = header->channel + i;

        channel->source_name = psia_wchar_to_utf8(&p, 32);
        channel->w_unit = psia_wchar_to_utf8(&p, 8);
        gwy_debug("spectro channel[%u] source_name: %s, w_unit: %s",
                  i, channel->source_name, channel->w_unit);
        channel->data_gain = gwy_get_gdouble_le(&p);
        channel->x_axis_source = gwy_get_guint32_le(&p);
        channel->y_axis_source = gwy_get_guint32_le(&p);
    }

    header->spect_sources = gwy_get_gint32_le(&p);
    gwy_debug("spect_sources: %d", header->spect_sources);
    header->average = gwy_get_gint32_le(&p);
    header->res = gwy_get_gint32_le(&p);
    header->npoints = gwy_get_gint32_le(&p);
    gwy_debug("res: %d, npoints: %d", header->res, header->npoints);
    header->driving_source_index  = gwy_get_gint32_le(&p);
    header->forward_period = gwy_get_gfloat_le(&p);
    header->backward_period = gwy_get_gfloat_le(&p);
    header->forward_speed = gwy_get_gfloat_le(&p);
    header->backward_speed = gwy_get_gfloat_le(&p);
    header->volume_image = gwy_get_guint32_le(&p);
    gwy_debug("volume_image: %d", header->volume_image);
    for (i = 0; i < PSIA_MAX_SPECTRO_CHANNEL; i++)
        header->offset[i] = gwy_get_gdouble_le(&p);
    for (i = 0; i < PSIA_MAX_SPECTRO_CHANNEL; i++)
        header->log_scale[i] = gwy_get_guint32_le(&p);
    for (i = 0; i < PSIA_MAX_SPECTRO_CHANNEL; i++)
        header->square[i] = gwy_get_guint32_le(&p);

    if (version == PSIA_VERSION1)
        return TRUE;

    header->spec_point_per_x = gwy_get_gint32_le(&p);
    header->has_reference_image = gwy_get_guint32_le(&p);
    header->xreal = gwy_get_gdouble_le(&p);
    header->yreal = gwy_get_gdouble_le(&p);
    header->xoff = gwy_get_gdouble_le(&p);
    header->yoff = gwy_get_gdouble_le(&p);
    header->force_constant = gwy_get_gdouble_le(&p);
    header->sensitivity = gwy_get_gdouble_le(&p);
    header->force_limit = gwy_get_gfloat_le(&p);
    header->time_interval = gwy_get_gfloat_le(&p);
    header->max_voltage = gwy_get_gfloat_le(&p);
    header->min_voltage = gwy_get_gfloat_le(&p);
    header->start_voltage = gwy_get_gfloat_le(&p);
    header->end_voltage = gwy_get_gfloat_le(&p);
    header->delayed_start_time = gwy_get_gfloat_le(&p);
    header->z_servo = gwy_get_guint32_le(&p);
    header->data_gain = gwy_get_gdouble_le(&p);
    header->w_unit = psia_wchar_to_utf8(&p, 8);
    header->use_extended_header = gwy_get_guint32_le(&p);
    header->spec_type = gwy_get_gint32_le(&p);
    header->reset_level = gwy_get_gfloat_le(&p);
    header->reset_duration = gwy_get_gfloat_le(&p);
    header->operation_level = gwy_get_gfloat_le(&p);
    header->operation_duration = gwy_get_gfloat_le(&p);
    header->time_before_reset = gwy_get_gfloat_le(&p);
    header->time_after_reset = gwy_get_gfloat_le(&p);
    header->time_before_light_on = gwy_get_gfloat_le(&p);
    header->time_light_duration = gwy_get_gfloat_le(&p);
    memcpy(header->reserved, p, 30*sizeof(gint));
    p += 30;

    return TRUE;
}

static void
psia_read_spectra(GwyContainer *container,
                  GwyTIFF *tiff,
                  PSIADataType data_type,
                  guint version)
{
    PSIASpectroscopyHeader specheader;
    const guchar *p;
    const GwyTIFFEntry *entry;
    guint res, bps, npoints, nsources, data_len, driving_source_index, ipt, i;
    GwySpectra **spectra = NULL;
    GwyDataLine **lines = NULL;
    /* The API does not know non-fatal failures so just dump spectra loading
     * errors to stderr. */
    GError *error = NULL;
    GwyRawDataType rawdatatype;

    if (data_type == PSIA_DATA_INT16)
        rawdatatype = GWY_RAW_DATA_SINT16;
    else if (data_type == PSIA_DATA_INT32)
        rawdatatype = GWY_RAW_DATA_SINT32;
    else if (data_type == PSIA_DATA_FLOAT)
        rawdatatype = GWY_RAW_DATA_FLOAT;
    else
        g_return_if_reached();
    bps = gwy_raw_data_size(rawdatatype);

    if (!(entry = gwy_tiff_find_tag(tiff, 0, PSIA_TIFFTAG_SpectroscopyHeader)))
        return;

    gwy_debug("Found SpectroscopyHeader");
    p = entry->value;
    if (!psia_read_spectro_header(tiff->data + tiff->get_guint32(&p),
                                  entry->count, version, &specheader, &error)) {
        g_warning("%s", error->message);
        g_clear_error(&error);
        return;
    }

    if (!(entry = gwy_tiff_find_tag(tiff, 0, PSIA_TIFFTAG_SpectroscopyData)))
        goto fail;
    gwy_debug("Found SpectroscopyData");
    p = entry->value;
    i = tiff->get_guint32(&p);
    p = tiff->data + i;
    data_len = entry->count;
    gwy_debug("data_len = %u", data_len);

    res = specheader.res;
    npoints = specheader.npoints;
    nsources = specheader.spect_sources;
    if (nsources < 2 || !npoints || !res)
        goto fail;

    if (err_SIZE_MISMATCH(&error, bps*res*npoints*nsources, data_len, TRUE))
        goto fail;

    if (nsources > PSIA_MAX_SPECTRO_CHANNEL) {
        g_set_error(&error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    "Number of spectrum sources %u is larger than %u.",
                    nsources, PSIA_MAX_SPECTRO_CHANNEL);
        goto fail;
    }

    driving_source_index = specheader.driving_source_index;
    gwy_debug("driving_source_index = %u", driving_source_index);
    if (driving_source_index > nsources) {
        g_set_error(&error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    "Driving source index is larger "
                    "than the number of sources.");
        goto fail;
    }

    gwy_debug("use_extended_header: %d", specheader.use_extended_header);
    gwy_debug("%g %g :: %g %g", specheader.xreal, specheader.yreal, specheader.xoff, specheader.yoff);

    // XXX: This is wrong; each spectrum, as read here, actually should be
    // two curves, forward and back.
    spectra = g_new0(GwySpectra*, nsources);
    lines = g_new0(GwyDataLine*, nsources);
    for (i = 0; i < nsources; i++) {
        const PSIASpectroscopyChannel *channel = specheader.channel + i;

        if (i == driving_source_index)
            continue;

        spectra[i] = gwy_spectra_new();
        gwy_si_unit_set_from_string(gwy_spectra_get_si_unit_xy(spectra[i]),
                                    "m");
        gwy_spectra_set_title(spectra[i], channel->source_name);
    }

    for (ipt = 0; ipt < npoints; ipt++) {
        gdouble off, real;
        for (i = 0; i < nsources; i++) {
            lines[i] = gwy_data_line_new(res, 1.0, FALSE);
            /* FIXME: What are the scaling factors? */
            gwy_convert_raw_data(p + bps*res*nsources*ipt + i*res*bps,
                                 res, 1, rawdatatype,
                                 GWY_BYTE_ORDER_LITTLE_ENDIAN,
                                 gwy_data_line_get_data(lines[i]), 1.0, 0.0);
        }
        off = lines[driving_source_index]->data[0];
        real = lines[driving_source_index]->data[res-1] - off;
        for (i = 0; i < nsources; i++) {
            //const PSIASpectroscopyChannel *channel = specheader.channel + i;
            // TODO: Set units
            if (i != driving_source_index) {
                gwy_data_line_set_real(lines[i], real);
                gwy_data_line_set_offset(lines[i], off);
                gwy_spectra_add_spectrum(spectra[i], lines[i], 0.0, 0.0);
            }
            g_object_unref(lines[i]);
        }
    }

    for (i = 0; i < nsources; i++) {
        gchar *strkey;

        if (!spectra[i])
            continue;

        strkey = g_strdup_printf("/sps/%d", i);
        gwy_container_set_object_by_name(container, strkey, spectra[i]);
        g_free(strkey);
        g_object_unref(spectra[i]);
    }
    g_free(spectra);
    g_free(lines);

fail:
    if (error) {
        g_warning("%s", error->message);
        g_clear_error(&error);
    }
    psia_free_spectro_header(&specheader);
}

static gchar*
psia_wchar_to_utf8(const guchar **src,
                   guint len)
{
    gchar *s;
    gunichar2 *wstr;
    guint i;

    wstr = g_memdup(*src, 2*len);
    for (i = 0; i < len; i++)
        wstr[i] = GUINT16_FROM_LE(wstr[i]);
    s = g_utf16_to_utf8(wstr, len, NULL, NULL, NULL);
    g_free(wstr);
    *src += 2*len;

    return s;
}

static GwyContainer*
psia_get_metadata(PSIAImageHeader *header,
                  guint version)
{
    GwyContainer *meta;

    meta = gwy_container_new();

    if (header->source_name && *header->source_name) {
        gwy_container_set_string_by_name(meta, "Source name",
                                         header->source_name);
        header->source_name = NULL;
    }
    if (header->image_mode && *header->image_mode) {
        gwy_container_set_string_by_name(meta, "Image mode",
                                         header->image_mode);
        header->image_mode = NULL;
    }

    gwy_container_set_string_by_name(meta, "Version",
                                     g_strdup_printf("%u.%u.%u",
                                                     version >> 24,
                                                     (version >> 12) & 0xfff,
                                                     version & 0xfff));
    gwy_container_set_string_by_name(meta, "Overscan",
                                     g_strdup_printf("%g %%",
                                                     100*header->overscan_rate));
    gwy_container_set_string_by_name(meta, "Fast direction",
                                     g_strdup(header->swap_xy ? "Y" : "X"));
    gwy_container_set_string_by_name(meta, "Angle",
                                     g_strdup_printf("%g°", header->angle));
    gwy_container_set_string_by_name(meta, "Scanning direction",
                                     g_strdup(header->scan_up
                                              ? "Bottom to top"
                                              : "Top to bottom"));
    gwy_container_set_string_by_name(meta, "Line direction",
                                     g_strdup(header->forward
                                              ? "Forward"
                                              : "Backward"));
    gwy_container_set_string_by_name(meta, "Sine scan",
                                     g_strdup(header->sine_scan
                                              ? "Yes"
                                              : "No"));
    gwy_container_set_string_by_name(meta, "Scan rate",
                                     g_strdup_printf("%g s<sup>-1</sup>",
                                                     header->scan_rate));
    gwy_container_set_string_by_name(meta, "Set point",
                                     g_strdup_printf("%g %s",
                                                     header->set_point,
                                                     header->set_point_unit));
    gwy_container_set_string_by_name(meta, "Tip bias",
                                     g_strdup_printf("%g V", header->tip_bias));
    gwy_container_set_string_by_name(meta, "Sample bias",
                                     g_strdup_printf("%g V",
                                                     header->sample_bias));

    if (version == PSIA_VERSION1)
        return meta;

    if (header->xy_voltage_mode && *header->xy_voltage_mode) {
        gwy_container_set_string_by_name(meta, "XY voltage mode",
                                         header->xy_voltage_mode);
        header->xy_voltage_mode = NULL;
    }

    if (header->z_voltage_mode && *header->z_voltage_mode) {
        gwy_container_set_string_by_name(meta, "Z voltage mode",
                                         header->z_voltage_mode);
        header->z_voltage_mode = NULL;
    }

    if (header->xy_servo_mode && *header->xy_servo_mode) {
        gwy_container_set_string_by_name(meta, "XY servo mode",
                                         header->xy_servo_mode);
        header->xy_servo_mode = NULL;
    }

    if (header->cantilever_name && *header->cantilever_name) {
        gwy_container_set_string_by_name(meta, "Cantilever",
                                         header->cantilever_name);
        header->cantilever_name = NULL;
    }

    gwy_container_set_string_by_name(meta, "Z scanner range",
                                     g_strdup_printf("%g",
                                                     header->z_scanner_range));
    gwy_container_set_string_by_name(meta, "Z servo gain",
                                     g_strdup_printf("%g",
                                                     header->z_servo_gain));
    gwy_container_set_string_by_name(meta, "Head tilt angle",
                                     g_strdup_printf("%g°", header->head_rotation_angle));

    return meta;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
