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
#define HEADER_SIZE 12

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
    guint x_offset;
    guint y_offset;
    guint size_flag;  /* dimension = 2^(4+size_flag) */
    gdouble acquire_delay;
    gdouble raster_delay;
    gdouble tip_dist;
    gdouble v_ref;
    gdouble vpmt1;
    gdouble vpmt2;
    gchar *remark;  /* 120 chars */
    guint x_piezo_factor;  /* nm/V */
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
    gdouble range_x;
    gdouble range_y;
    gdouble **data;
} APEFile;

static gboolean      module_register    (const gchar *name);
static gint          apefile_detect      (const gchar *filename,
                                         gboolean only_name);
static GwyContainer* apefile_load        (const gchar *filename);
static GwyDataField* read_data_field    (const guchar *buffer,
                                         guint size,
                                         guchar version);


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
    if (!fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE) {
        fclose(fh);
        return 0;
    }
    p = (guchar*)header;
    version = *(p++);
    mode = *(p++);
    /* FIXME */
    if (version >= 1 && version <= 2 && mode < SPM_MODE_LAST)
        score = 50;

    return score;
}

static GwyContainer*
apefile_load(const gchar *filename)
{
    APEFile apefile;
    GObject *object = NULL;
    guchar *buffer = NULL, *p;
    guint size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;

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
    apefile.acquire_delay = get_FLOAT(&p);
    apefile.rasert_delay = get_FLOAT(&p);
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
    apefile.range_x = get_FLOAT(&p);
    apefile.range_y = get_FLOAT(&p);
    /* reserved */
    p += 44;

    gwy_file_abandon_contents(buffer, size, NULL);
    if (!dfield)
        return NULL;

    object = gwy_container_new();
    gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                     G_OBJECT(dfield));

    return (GwyContainer*)object;
}

static GwyDataField*
read_data_field(const guchar *buffer, guint size, guchar version)
{
    enum { MIN_REMAINDER = 2620 };
    /* information offsets in different versions, in r5+ relative to data
     * start, in order: data offset, pixel dimensions, physical dimensions,
     * value multiplier, multipliers (units) */
    const guint offsets34[] = { 0x0104, 0x0196, 0x01a2, 0x01b2, 0x01be };
    const guint offsets56[] = { 0x0104, 0x025c, 0x0268, 0x0288, 0x029c };
    /* there probably more constants, the left and right 1e-6 also serve as
     * defaults after CLAMP() */
    const gdouble zfactors[] = { 1e-6, 1e-9, 1e-10, 1e-6 };
    const gdouble lfactors[] = { 1e-6, 1e-10, 1e-9, 1e-6 };
    gint xres, yres, doffset, lf, zf, i;
    gdouble xreal, yreal, q, z0;
    GwyDataField *dfield;
    gdouble *data;
    const guint *offset;
    const guchar *p, *r, *last;
    /* get floats in single precision from r4 but double from r5+ */
    gdouble (*getflt)(const guchar**);

    if (version == '5' || version == '6') {
        /* There are more data in r5,
         * try to find something that looks like #R5. */
        last = r = buffer;
        while ((p = memchr(r, '#', size - (r - buffer) - MIN_REMAINDER))) {
            if (p[1] == 'R' && p[2] == version && p[3] == '.') {
                gwy_debug("pos: %d", p - buffer);
                last = p;
                r = p + MIN_REMAINDER-1;
            }
            else
                r = p + 1;
        }
        offset = &offsets56[0];
        buffer = last;
        getflt = &get_DOUBLE;
    }
    else {
        offset = &offsets34[0];
        getflt = &get_FLOAT;
    }

    p = buffer + *(offset++);
    doffset = get_DWORD(&p);    /* this appears to be the same number as in
                                   the ASCII miniheader -- so get it here
                                   since it's easier */
    gwy_debug("data offset = %u", doffset);
    p = buffer + *(offset++);
    xres = get_DWORD(&p);
    yres = get_DWORD(&p);
    p = buffer + *(offset++);
    xreal = -getflt(&p);
    xreal += getflt(&p);
    yreal = -getflt(&p);
    yreal += getflt(&p);
    p = buffer + *(offset++);
    q = getflt(&p);
    z0 = getflt(&p);
    gwy_debug("xreal.raw = %g, yreal.raw = %g, q.raw = %g, z0.raw = %g",
              xreal, yreal, q, z0);
    p = buffer + *(offset++);
    zf = get_WORD(&p);
    lf = get_WORD(&p);
    gwy_debug("lf = %d, zf = %d", lf, zf);
    lf = CLAMP(lf, 0, G_N_ELEMENTS(lfactors)-1);
    xreal *= lfactors[lf];
    yreal *= lfactors[lf];
    zf = CLAMP(zf, 0, G_N_ELEMENTS(zfactors)-1);
    q *= zfactors[zf];
    z0 *= zfactors[zf];
    gwy_debug("xres = %d, yres = %d, xreal = %g, yreal = %g, q = %g, z0 = %g",
              xres, yres, xreal, yreal, q, z0);

    p = buffer + doffset;
    if (size - (p - buffer) < 2*xres*yres) {
        g_warning("Truncated data?");
        return NULL;
    }

    dfield = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, xreal, yreal,
                                               FALSE));
    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < xres*yres; i++)
        data[i] = (p[2*i] + 256.0*p[2*i + 1])*q + z0;

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

