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
#define DEBUG 1
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <app/gwyapp.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "get.h"

/* Just guessing, format has no real magic header */
#define HEADER_SIZE 240

typedef enum {
    SPM_MODE_SNOM = 0,
    SPM_MODE_AFM_NONCONTACT = 1,
    SPM_MODE_AFM_CONTACT = 2,
    SPM_MODE_STM = 3,
    SPM_MODE_PHASE_DETECT_AFM = 4,
    SPM_MODE_LAST
} SPMModeType;

typedef struct {
    guint version;
    SPMModeType spm_mode;
    gdouble scan_date;
    gdouble maxr_x;
    gdouble maxr_y;
    gdouble xreal;   /* computed */
    gdouble yreal;   /* computed */
    guint x_offset;
    guint y_offset;
    guint size_flag;
    guint res;   /* computed, 2^(4+size_flag) */
    gdouble acquire_delay;
    gdouble raster_delay;
    gdouble tip_dist;
    gdouble v_ref;
    gdouble vpmt1;
    gdouble vpmt2;
    gchar *remark;   /* 120 chars */
    guint x_piezo_factor;   /* nm/V */
    guint y_piezo_factor;
    guint z_piezo_factor;
    gdouble hv_gain;
    gdouble freq_osc_tip;
    gdouble rotate;
    gdouble slope_x;
    gdouble slope_y;
    guint topo_means;
    guint optical_means;
    guint error_means;
    guint channels;
    guint ndata;   /* computed, number of nonzero bits in channels */
    gdouble range_x;
    gdouble range_y;
    GwyDataField **data;
} APEFile;

static gboolean      module_register    (const gchar *name);
static gint          apefile_detect     (const gchar *filename,
                                         gboolean only_name);
static GwyContainer* apefile_load       (const gchar *filename);
static void          fill_data_fields   (APEFile *apefile,
                                         const guchar *buffer);
static void          store_metadata     (APEFile *apefile,
                                         GwyContainer *container);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "apefile",
    N_("Imports APE (Applied Physics and Engineering) data files."),
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
    static GwyFileFuncInfo apefile_func_info = {
        "apefile",
        N_("APE files (.dat)"),
        (GwyFileDetectFunc)&apefile_detect,
        (GwyFileLoadFunc)&apefile_load,
        NULL
    };

    gwy_file_func_register(name, &apefile_func_info);

    return TRUE;
}

static gint
apefile_detect(const gchar *filename, gboolean only_name)
{
    gint score = 0;
    FILE *fh;
    gchar header[HEADER_SIZE];
    guchar *p;
    guint version, mode;

    if (only_name) {
        if (g_str_has_suffix(filename, ".dat"))
            score = 10;

        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;
    if (!fread(header, 1, HEADER_SIZE, fh) == HEADER_SIZE) {
        fclose(fh);
        return 0;
    }
    p = (guchar*)header;
    version = *(p++);
    mode = *(p++);
    /* FIXME */
    if (version >= 1 && version <= 2 && mode < SPM_MODE_LAST) {
        score = 50;
        /* This works for new file format only */
        if (!strncmp(header + 234, "APERES", 6))
            score = 100;
    }

    return score;
}

static GwyContainer*
apefile_load(const gchar *filename)
{
    APEFile apefile;
    GObject *object = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    guint b, n;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    p = buffer;
    apefile.version = *(p++);
    if (size < 1294) {
        g_warning("File %s is not a APE file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    apefile.spm_mode = *(p++);
    apefile.scan_date = 0.0; p += 10; /* FIXME */
    apefile.maxr_x = get_FLOAT(&p);
    apefile.maxr_y = get_FLOAT(&p);
    apefile.x_offset = get_DWORD(&p);
    apefile.y_offset = get_DWORD(&p);
    apefile.size_flag = get_WORD(&p);
    apefile.res = 16 << apefile.size_flag;
    apefile.acquire_delay = get_FLOAT(&p);
    apefile.raster_delay = get_FLOAT(&p);
    apefile.tip_dist = get_FLOAT(&p);
    apefile.v_ref = get_FLOAT(&p);
    if (apefile.version == 1) {
        apefile.vpmt1 = get_WORD(&p);
        apefile.vpmt2 = get_WORD(&p);
    }
    else {
        apefile.vpmt1 = get_FLOAT(&p);
        apefile.vpmt2 = get_FLOAT(&p);
    }
    apefile.remark = g_strndup(p, 120);
    p += 120;
    apefile.x_piezo_factor = get_DWORD(&p);
    apefile.y_piezo_factor = get_DWORD(&p);
    apefile.z_piezo_factor = get_DWORD(&p);
    apefile.hv_gain = get_FLOAT(&p);
    apefile.freq_osc_tip = get_DOUBLE(&p);
    apefile.rotate = get_FLOAT(&p);
    apefile.slope_x = get_FLOAT(&p);
    apefile.slope_y = get_FLOAT(&p);
    apefile.topo_means = get_WORD(&p);
    apefile.optical_means = get_WORD(&p);
    apefile.error_means = get_WORD(&p);
    apefile.channels = get_DWORD(&p);
    apefile.ndata = 0;
    for (b = apefile.channels; b; b = b >> 1)
        apefile.ndata += (b & 1);
    apefile.range_x = get_FLOAT(&p);
    apefile.range_y = get_FLOAT(&p);
    apefile.xreal = apefile.maxr_x * apefile.x_piezo_factor * apefile.range_x
                    * apefile.hv_gain/65535.0 * 1e-9;
    apefile.yreal = apefile.maxr_y * apefile.y_piezo_factor * apefile.range_y
                    * apefile.hv_gain/65535.0 * 1e-9;
    /* reserved */
    p += 46;

    gwy_debug("version = %u, spm_mode = %u", apefile.version, apefile.spm_mode);
    gwy_debug("maxr_x = %g, maxr_y = %g", apefile.maxr_x, apefile.maxr_y);
    gwy_debug("x_offset = %u, y_offset = %u",
              apefile.x_offset, apefile.y_offset);
    gwy_debug("size_flag = %u", apefile.size_flag);
    gwy_debug("acquire_delay = %g, raster_delay = %g, tip_dist = %g",
              apefile.acquire_delay, apefile.raster_delay, apefile.tip_dist);
    gwy_debug("v_ref = %g, vpmt1 = %g, vpmt2 = %g",
              apefile.v_ref, apefile.vpmt1, apefile.vpmt2);
    gwy_debug("x_piezo_factor = %u, y_piezo_factor = %u, z_piezo_factor = %u",
              apefile.x_piezo_factor, apefile.y_piezo_factor,
              apefile.z_piezo_factor);
    gwy_debug("hv_gain = %g, freq_osc_tip = %g, rotate = %g",
              apefile.hv_gain, apefile.freq_osc_tip, apefile.rotate);
    gwy_debug("slope_x = %g, slope_y = %g",
              apefile.slope_x, apefile.slope_y);
    gwy_debug("topo_means = %u, optical_means = %u, error_means = %u",
              apefile.topo_means, apefile.optical_means, apefile.error_means);
    gwy_debug("channel bitmask = %03x, ndata = %u",
              apefile.channels, apefile.ndata);
    gwy_debug("range_x = %g, range_y = %g",
              apefile.range_x, apefile.range_y);

    n = (apefile.res + 1)*(apefile.res + 1)*sizeof(float);
    if (size - (p - buffer) != n*apefile.ndata) {
        g_warning("Expected data size %u, but it's %u.",
                  n*apefile.ndata, size - (p - buffer));
        apefile.ndata = MIN(apefile.ndata, (size - (p - buffer))/n);
        if (!apefile.ndata) {
            g_warning("No data");
            gwy_file_abandon_contents(buffer, size, NULL);

            return NULL;
        }
    }
    fill_data_fields(&apefile, p);
    /* XXX */
    dfield = apefile.data[0];

    gwy_file_abandon_contents(buffer, size, NULL);

    object = gwy_container_new();
    gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                     G_OBJECT(dfield));
    store_metadata(&apefile, GWY_CONTAINER(object));

    return (GwyContainer*)object;
}

static void
fill_data_fields(APEFile *apefile,
                 const guchar *buffer)
{
    GwyDataField *dfield;
    gdouble *data;
    guint n, i, j;

    apefile->data = g_new0(GwyDataField*, apefile->ndata);
    for (n = 0; n < apefile->ndata; n++) {
        dfield = GWY_DATA_FIELD(gwy_data_field_new(apefile->res,
                                                   apefile->res,
                                                   apefile->xreal,
                                                   apefile->yreal,
                                                   FALSE));
        data = gwy_data_field_get_data(dfield);
        buffer += (apefile->res + 1)*sizeof(float);
        for (i = 0; i < apefile->res; i++) {
            buffer += sizeof(float);
            for (j = 0; j < apefile->res; j++) {
                *(data++) = get_FLOAT(&buffer);
            }
        }
        apefile->data[n] = dfield;
        gwy_data_field_multiply(dfield, apefile->z_piezo_factor * 1e-9);
    }
}

#define HASH_STORE(key, fmt, field) \
    gwy_container_set_string_by_name(container, "/meta/" key, \
                                     g_strdup_printf(fmt, apefile->field));

static void
store_metadata(APEFile *apefile,
               GwyContainer *container)
{
    HASH_STORE("Version", "%u", version);
    HASH_STORE("Tip oscilation frequency", "%g Hz", freq_osc_tip);
    HASH_STORE("Acquire delay", "%.6f s", acquire_delay);
    HASH_STORE("Raster delay", "%.6f s", raster_delay);
    HASH_STORE("Tip distance", "%g nm", tip_dist);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

