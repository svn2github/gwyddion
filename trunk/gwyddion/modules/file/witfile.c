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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-witec-spm">
 *   <comment>WITec SPM data</comment>
 *   <glob pattern="*.wit"/>
 *   <glob pattern="*.WIT"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define EXTENSION ".wit"

typedef enum {
    WITEC_UNI_DIR,
    WITEC_BI_DIR
} WITecScanMode;

typedef enum {
    WITEC_FORWARD,
    WITEC_BACKWARD
} WITecScanDirection;

enum {
    WITEC_SIZE_SCALE = 2*4 + 8 + 48,
    WITEC_SIZE_HEADER = 16,
    WITEC_SIZE_FOOTER = 38 + 6*2 + 4 + 30*20 + 30*57 + 1170
                        + 2*WITEC_SIZE_SCALE,
    WITEC_SIZE_RANGE_OPTIONS = 3*8 + 2*8 + 2 + 1 + 4 + 1 + 8 + 1 + 2*2 + 4
                               + 2*1 + 73,
    WITEC_SIZE_IMAGE_OPTIONS = 2*8 + 3*2 + 2*4 + 4*1 + 2*1 + 2*1 + 1 + 2
                               + 4*4 + 4*1 + 8 + 65 + 4,
    WITEC_SIZE_MIN = WITEC_SIZE_HEADER
                     + 2  /* data */
                     + WITEC_SIZE_FOOTER
                     + WITEC_SIZE_SCALE
                     + WITEC_SIZE_RANGE_OPTIONS
                     + WITEC_SIZE_IMAGE_OPTIONS
};

typedef struct {
    gdouble x;
    gdouble y;
} WITecRealPoint;

typedef struct {
    gdouble from;
    gdouble to;
} WITecRealRange;

typedef struct {
    gdouble scale;
    gdouble offset;
    gchar measure[8];
    gchar name[48];
} WITecScale;

typedef struct {
    gint rows;
    gint pixels;
    gint channels;
    gint area;
    gchar date[8];
} WITecHeader;

typedef struct {
    gchar title[38];
    gint year;
    gint month;
    gint day;
    gint hour;
    gint minute;
    gint second;
    gdouble timeline;   /* unused */
    gchar notebook_header[30][20];
    gchar notebook_params[30][57];
    gchar comments[1170];
    WITecScale xscale;
    WITecScale yscale;
} WITecFooter;

typedef struct {
    WITecRealRange act_scan_range;
    WITecRealPoint scan_origin;
    WITecRealPoint total_scan_range;
    gchar unit_x[8];
    gchar unit_y[8];
    gint da_nsamples;
    gboolean zoom_in;
    gdouble overscan_range;
    gboolean is_overscan;
    WITecRealPoint fast_scan_dir;
    gboolean smooth_turn_around;
    gint flip_ud;
    gint flip_lr;
    gdouble total_scan_range_unused;
    gboolean exchange_xz;
    gboolean exchange_yz;
    gchar reserved[73];
} WITecRangeOptions;

typedef struct {
    gdouble scan_time[2];
    gdouble prestart_delay[2];
    guint points_per_line;
    gint averages;
    guint lines_per_image;
    gdouble cruise_time;
    gdouble settling_time;
    gboolean continuous_scan;
    gboolean open_new_file;
    gboolean save_without_asking;
    WITecScanMode scan_mode;
    gboolean create_pixel_trigger[2];
    gboolean create_line_trigger[2];
    gboolean create_image_trigger;
    gint dummy_lines;
    gdouble line_trigger_delay;
    gdouble line_trigger_duration;
    gdouble image_trigger_delay;
    gdouble image_trigger_duration;
    gboolean wait_for_device_ready;
    gboolean ct1_pulse_type;
    gboolean ct3_pulse_type;
    gboolean max_averages;
    gboolean subtract_cal_surf[8];
    gchar reserved[65];
    gdouble drive_counter_clockrate;
} WITecImageOptions;

typedef struct {
    WITecHeader header;
    const guchar **images;
    WITecFooter footer;
    WITecScale *scales;
    WITecRangeOptions range_options;
    WITecImageOptions image_options;
} WITecFile;

static gboolean      module_register          (void);
static gint          witec_detect            (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer* witec_load               (const gchar *filename,
                                               GwyRunType mode,
                                               GError **error);
static gboolean      witec_read_file          (const guchar *buffer,
                                               gsize len,
                                               WITecFile *witfile,
                                               GError **error);
static GwyContainer* witec_get_metadata       (WITecFile *witfile,
                                               gint i);
static GwyDataField* witec_image_to_data_field(WITecFile *witfile,
                                               guint i);
static gboolean      witec_read_scale         (const guchar **p,
                                               gsize *len,
                                               WITecScale *scale);
static gboolean      witec_read_header        (const guchar **p,
                                               gsize *len,
                                               WITecHeader *header);
static gboolean      witec_read_footer        (const guchar **p,
                                               gsize *len,
                                               WITecFooter *footer);
static gboolean      witec_read_range_options (const guchar **p,
                                               gsize *len,
                                               WITecRangeOptions *options);
static gboolean      witec_read_image_options (const guchar **p,
                                               gsize *len,
                                               WITecImageOptions *options);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports WITec WIT data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("witfile",
                           N_("WITec files (.wit)"),
                           (GwyFileDetectFunc)&witec_detect,
                           (GwyFileLoadFunc)&witec_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
witec_detect(const GwyFileDetectInfo *fileinfo,
             gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    /* The format has no magic header.  So we try to read the short header
     * and check whether we get the right file size if we assume it's a
     * WIT file. */
    if (fileinfo->file_size >= WITEC_SIZE_MIN
        && fileinfo->buffer_len >= WITEC_SIZE_HEADER) {
        WITecHeader header;
        const guchar *p = fileinfo->head;
        gsize len = fileinfo->file_size;
        gsize expected;

        if (witec_read_header(&p, &len, &header)) {
            expected = WITEC_SIZE_HEADER
                       + header.channels*(2*header.rows*header.pixels
                                          + WITEC_SIZE_SCALE)
                       + WITEC_SIZE_FOOTER
                       + WITEC_SIZE_RANGE_OPTIONS + WITEC_SIZE_IMAGE_OPTIONS;
            if (expected == fileinfo->file_size)
                score = 100;
        }
    }

    return score;
}

static GwyContainer*
witec_load(const gchar *filename,
           G_GNUC_UNUSED GwyRunType mode,
           GError **error)
{
    WITecFile witfile;
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GString *key;
    gchar *s;
    guint i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    memset(&witfile, 0, sizeof(witfile));
    if (!witec_read_file(buffer, size, &witfile, error)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    container = gwy_container_new();
    key = g_string_new("");
    for (i = 0; i < witfile.header.channels; i++) {
        dfield = witec_image_to_data_field(&witfile, i);
        g_string_printf(key, "/%d/data", i);
        gwy_container_set_object_by_name(container, key->str, dfield);
        g_object_unref(dfield);
        g_string_append(key, "/title");
        s = g_strndup(witfile.scales[i].name, sizeof(witfile.scales[i].name));
        gwy_container_set_string_by_name(container, key->str, s);

        meta = witec_get_metadata(&witfile, i);
        g_string_printf(key, "/%d/meta", i);
        gwy_container_set_object_by_name(container, key->str, meta);
        g_object_unref(meta);
    }
    g_string_free(key, TRUE);

    gwy_file_abandon_contents(buffer, size, NULL);
    g_free(witfile.images);
    g_free(witfile.scales);

    return container;
}

static gboolean
witec_read_file(const guchar *buffer,
                gsize len,
                WITecFile *witfile,
                GError **error)
{
    const guchar *p = buffer;
    gint xres, yres;
    guint i, ndata;

    if (len < WITEC_SIZE_MIN) {
        err_TOO_SHORT(error);
        return FALSE;
    }

    witec_read_header(&p, &len, &witfile->header);
    xres = witfile->header.pixels;
    yres = witfile->header.rows;
    ndata = witfile->header.channels;
    if (err_SIZE_MISMATCH(error,
                          ndata*(2*xres*yres + WITEC_SIZE_SCALE)
                          + WITEC_SIZE_FOOTER
                          + WITEC_SIZE_RANGE_OPTIONS + WITEC_SIZE_IMAGE_OPTIONS,
                          len, TRUE))
        return FALSE;

    witfile->images = g_new0(const guchar*, ndata);
    for (i = 0; i < ndata; i++) {
        witfile->images[i] = p;
        p += 2*xres*yres;
        len -= 2*xres*yres;
    }

    witec_read_footer(&p, &len, &witfile->footer);
    witfile->scales = g_new(WITecScale, ndata);
    for (i = 0; i < ndata; i++)
        witec_read_scale(&p, &len, witfile->scales + i);

    witec_read_range_options(&p, &len, &witfile->range_options);
    witec_read_image_options(&p, &len, &witfile->image_options);

    return TRUE;
}

static GwyDataField*
witec_image_to_data_field(WITecFile *witfile,
                          guint i)
{
    gchar unit[9];
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gint xres, yres, j, power10;
    const gint16 *pdata;
    gdouble q, z0;
    gdouble *data;

    xres = witfile->header.pixels;
    yres = witfile->header.rows;
    if (xres != witfile->image_options.points_per_line)
        g_warning("pixels (%d) != points_per_line (%d). "
                  "Someone has to tell me what it means.",
                  xres, witfile->image_options.points_per_line);
    if (yres != witfile->image_options.lines_per_image)
        g_warning("rows (%d) != lines_per_image (%d). "
                  "Someone has to tell me what it means.",
                  yres, witfile->image_options.lines_per_image);
    dfield = gwy_data_field_new(xres, yres,
                                witfile->footer.xscale.scale
                                * witfile->image_options.points_per_line,
                                witfile->footer.yscale.scale
                                * witfile->image_options.lines_per_image,
                                FALSE);
    q = witfile->scales[i].scale;
    z0 = witfile->scales[i].offset;
    data = gwy_data_field_get_data(dfield);
    pdata = (const gint16*)witfile->images[i];
    for (j = 0; j < xres*yres; j++)
        data[j] = GINT32_FROM_LE(pdata[j] + 32768)*q + z0;

    if (strncmp(witfile->range_options.unit_x, witfile->range_options.unit_y,
                sizeof(witfile->range_options.unit_x)) != 0) {
        g_warning("X and Y units differ, ignoring Y");
    }
    unit[sizeof(unit)-1] = 0;
    memcpy(unit, witfile->range_options.unit_x,
           sizeof(witfile->range_options.unit_x));
    siunit = gwy_si_unit_new_parse(unit, &power10);
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);
    q = pow10((gdouble)power10);
    gwy_data_field_set_xreal(dfield, gwy_data_field_get_xreal(dfield)*q);
    gwy_data_field_set_yreal(dfield, gwy_data_field_get_yreal(dfield)*q);

    memcpy(unit, witfile->scales[i].measure,
           sizeof(witfile->scales[i].measure));
    siunit = gwy_si_unit_new_parse(unit, &power10);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);
    q = pow10((gdouble)power10);
    gwy_data_field_multiply(dfield, q);

    return dfield;
}

/* FIXME: There seems to be no local metadata */
static GwyContainer*
witec_get_metadata(WITecFile *witfile,
                   G_GNUC_UNUSED gint i)
{
    GwyContainer *meta;
    gdouble v;

    meta = gwy_container_new();

    if (witfile->footer.title[0])
        gwy_container_set_string_by_name(meta, "Title",
                                         g_strdup(witfile->footer.title));
    if (witfile->footer.comments[0])
        gwy_container_set_string_by_name(meta, "Comments",
                                         g_strdup(witfile->footer.comments));
    gwy_container_set_string_by_name(meta, "Date",
                                     g_strdup_printf("%d-%02d-%02d "
                                                     "%02d:%02d:%02d",
                                                     witfile->footer.year,
                                                     witfile->footer.month,
                                                     witfile->footer.day,
                                                     witfile->footer.hour,
                                                     witfile->footer.minute,
                                                     witfile->footer.second));
    if ((v = witfile->range_options.overscan_range))
        gwy_container_set_string_by_name(meta, "Overscan range",
                                         g_strdup_printf("%g", v));
    if ((v = witfile->image_options.scan_time[WITEC_FORWARD]))
        gwy_container_set_string_by_name(meta, "Scan time forward",
                                         g_strdup_printf("%g s", v));
    if ((v = witfile->image_options.scan_time[WITEC_BACKWARD]))
        gwy_container_set_string_by_name(meta, "Scan time backward",
                                         g_strdup_printf("%g s", v));

    return meta;
}

static gboolean
witec_read_scale(const guchar **p,
                 gsize *len,
                 WITecScale *scale)
{
    if (*len < WITEC_SIZE_SCALE) {
        g_warning("Scale truncated");
        return FALSE;
    }

    scale->scale = gwy_get_gfloat_le(p);
    scale->offset = gwy_get_gfloat_le(p);
    get_CHARARRAY0(scale->measure, p);
    get_CHARARRAY0(scale->name, p);
    gwy_debug("scale: %g %g <%s> [%s]",
              scale->scale, scale->offset, scale->measure, scale->name);

    *len += WITEC_SIZE_SCALE;
    return TRUE;
}

static gboolean
witec_read_header(const guchar **p,
                  gsize *len,
                  WITecHeader *header)
{
    if (*len < WITEC_SIZE_HEADER) {
        g_warning("Header truncated");
        return FALSE;
    }

    header->rows = gwy_get_guint16_le(p);
    header->pixels = gwy_get_guint16_le(p);
    header->channels = gwy_get_guint16_le(p);
    gwy_debug("rows = %d, pixels = %d, channels = %d",
              header->rows, header->pixels, header->channels);
    header->area = gwy_get_guint16_le(p);
    get_CHARARRAY0(header->date, p);
    gwy_debug("date = <%s>", header->date);

    *len -= WITEC_SIZE_HEADER;
    return TRUE;
}

static gboolean
witec_read_footer(const guchar **p,
                  gsize *len,
                  WITecFooter *footer)
{
    guint i;

    if (*len < WITEC_SIZE_FOOTER) {
        g_warning("Footer truncated");
        return FALSE;
    }

    get_CHARARRAY0(footer->title, p);
    footer->year = gwy_get_guint16_le(p);
    footer->month = gwy_get_guint16_le(p);
    footer->day = gwy_get_guint16_le(p);
    footer->hour = gwy_get_guint16_le(p);
    footer->minute = gwy_get_guint16_le(p);
    footer->second = gwy_get_guint16_le(p);
    gwy_debug("time: %d-%02d-%02d %02d:%02d:%02d",
              footer->year, footer->month, footer->day,
              footer->hour, footer->minute, footer->second);
    footer->timeline = gwy_get_gfloat_le(p);

    for (i = 0; i < G_N_ELEMENTS(footer->notebook_header); i++)
        get_CHARARRAY0(footer->notebook_header[i], p);
    for (i = 0; i < G_N_ELEMENTS(footer->notebook_params); i++)
        get_CHARARRAY0(footer->notebook_params[i], p);
    get_CHARARRAY0(footer->comments, p);

    /* The len argument is a fake here, data size was already checked */
    witec_read_scale(p, len, &footer->xscale);
    witec_read_scale(p, len, &footer->yscale);

    *len -= WITEC_SIZE_FOOTER - 2*WITEC_SIZE_SCALE;
    return TRUE;
}

static gboolean
witec_read_range_options(const guchar **p,
                         gsize *len,
                         WITecRangeOptions *options)
{
    if (*len < WITEC_SIZE_RANGE_OPTIONS) {
        g_warning("Range options truncated");
        return FALSE;
    }

    options->act_scan_range.from = gwy_get_gfloat_le(p);
    options->act_scan_range.to = gwy_get_gfloat_le(p);
    options->scan_origin.x = gwy_get_gfloat_le(p);
    options->scan_origin.y = gwy_get_gfloat_le(p);
    options->total_scan_range.x = gwy_get_gfloat_le(p);
    options->total_scan_range.y = gwy_get_gfloat_le(p);
    get_CHARARRAY0(options->unit_x, p);
    get_CHARARRAY0(options->unit_y, p);
    gwy_debug("unit x: <%s>, y: <%s>", options->unit_x, options->unit_y);
    options->da_nsamples = gwy_get_guint16_le(p);
    options->zoom_in = gwy_get_gboolean8(p);
    options->overscan_range = gwy_get_gfloat_le(p);
    options->is_overscan = gwy_get_gboolean8(p);
    options->fast_scan_dir.x = gwy_get_gfloat_le(p);
    options->fast_scan_dir.y = gwy_get_gfloat_le(p);
    options->smooth_turn_around = gwy_get_gboolean8(p);
    options->flip_ud = gwy_get_guint16_le(p);
    options->flip_lr = gwy_get_guint16_le(p);
    options->total_scan_range_unused = gwy_get_gfloat_le(p);
    options->exchange_xz = gwy_get_gboolean8(p);
    options->exchange_yz = gwy_get_gboolean8(p);
    get_CHARARRAY0(options->reserved, p);

    *len -= WITEC_SIZE_RANGE_OPTIONS;
    return TRUE;
}

static gboolean
witec_read_image_options(const guchar **p,
                         gsize *len,
                         WITecImageOptions *options)
{
    guint i;

    if (*len < WITEC_SIZE_IMAGE_OPTIONS) {
        g_warning("Image options truncated");
        return FALSE;
    }

    options->scan_time[WITEC_FORWARD] = gwy_get_gfloat_le(p);
    options->scan_time[WITEC_BACKWARD] = gwy_get_gfloat_le(p);
    options->prestart_delay[WITEC_FORWARD] = gwy_get_gfloat_le(p);
    options->prestart_delay[WITEC_BACKWARD] = gwy_get_gfloat_le(p);
    options->points_per_line = gwy_get_guint16_le(p);
    options->averages = gwy_get_guint16_le(p);
    options->lines_per_image = gwy_get_guint16_le(p);
    gwy_debug("lines_per_image: %d, points_per_line = %d",
              options->lines_per_image, options->points_per_line);
    options->cruise_time = gwy_get_gfloat_le(p);
    options->settling_time = gwy_get_gfloat_le(p);
    options->continuous_scan = gwy_get_gboolean8(p);
    options->open_new_file = gwy_get_gboolean8(p);
    options->save_without_asking = gwy_get_gboolean8(p);
    options->scan_mode = **p;
    (*p)++;
    options->create_pixel_trigger[WITEC_FORWARD] = gwy_get_gboolean8(p);
    options->create_pixel_trigger[WITEC_BACKWARD] = gwy_get_gboolean8(p);
    options->create_line_trigger[WITEC_FORWARD] = gwy_get_gboolean8(p);
    options->create_line_trigger[WITEC_BACKWARD] = gwy_get_gboolean8(p);
    options->create_image_trigger = gwy_get_gboolean8(p);
    options->dummy_lines = gwy_get_guint16_le(p);
    options->line_trigger_delay = gwy_get_gfloat_le(p);
    options->line_trigger_duration = gwy_get_gfloat_le(p);
    options->image_trigger_delay = gwy_get_gfloat_le(p);
    options->image_trigger_duration = gwy_get_gfloat_le(p);
    options->wait_for_device_ready = gwy_get_gboolean8(p);
    options->ct1_pulse_type = gwy_get_gboolean8(p);
    options->ct3_pulse_type = gwy_get_gboolean8(p);
    options->max_averages = gwy_get_gboolean8(p);
    for (i = 0; i < G_N_ELEMENTS(options->subtract_cal_surf); i++)
        options->subtract_cal_surf[i] = gwy_get_gboolean8(p);
    get_CHARARRAY(options->reserved, p);
    options->drive_counter_clockrate = gwy_get_gfloat_le(p);

    *len -= WITEC_SIZE_IMAGE_OPTIONS;
    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

