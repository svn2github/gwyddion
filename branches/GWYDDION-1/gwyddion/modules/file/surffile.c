/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  Date conversion code copied from Wine, see below.
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
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <app/gwyapp.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if (defined(HAVE_TIME_H) || defined(G_OS_WIN32))
#include <time.h>
#endif

#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#endif

#include "get.h"

/* Just guessing, format has no real magic header */
#define HEADER_SIZE 240

typedef enum {
    SURF_PC = 0,
    SURF_MACINTOSH = 257
} SurfFormatType;

typedef enum {
    SURF_PROFILE = 1,
    SURF_SURFACE = 2,
    SURF_BINARY = 3,
    SURF_SERIES_PROFILES = 4,
    SURF_SERIES_SURFACES = 5
} SurfObjectType;

typedef enum {
    SURF_ACQ_UNKNOWN = 0,
    SURF_ACQ_STYLUS = 1,
    SURF_ACQ_OPTICAL = 2,
    SURF_ACQ_THERMOCOUPLE = 3,
    SURF_ACQ_STYLUS_SKID = 5,
    SURF_ACQ_AFM = 6,
    SURF_ACQ_STM = 7,
    SURF_ACQ_VIDEO = 8,
    SURF_ACQ_INTERFEROMETER = 9,
    SURF_ACQ_LIGHT = 10,
} SurfAcqusitionType;
    

typedef enum {
    SURF_RANGE_NORMAL = 0,
    SURF_RANGE_HIGH = 1,
} SurfRangeType;

typedef enum {
    SURF_SP_NORMAL = 0,
    SURF_SP_SATURATIONS = 1,
} SurfSpecialPointsType;

typedef enum {
    SURF_INV_NONE = 0,
    SURF_INV_Z = 1,
    SURF_FLIP_Z = 2,
    SURF_FLOP_Z = 3,
} SurfInversionType;

typedef enum {
    SURF_LEVELING_NONE = 0,
    SURF_LEVELING_LSM = 1,
    SURF_LEVELING_MZ = 2,
} SurfLevelingType;



typedef struct {
    SurfFormatType format;
    guint nobjects;    
    guint version;
    gchar object_name[30];
    gchar operator_name[30];
    gint material_code;
    SurfAcqusitionType acquisiton;
    SurfRangeType range;
    SurfSpecialPointsType special_points;
    gboolean absolute;
    guint pointsize;
    gint zmin;
    gint zmax;
    gint xres; /*number of points per line*/
    gint yres; /*number of lines*/
    gint nofpoints;
    gdouble dx;
    gdouble dy;
    gdouble dz;
    gchar xaxis[15];
    gchar yaxis[15];
    gchar zaxis[15];
    gchar dx_unit[15];
    gchar dy_unit[15];
    gchar dz_unit[15];
    gchar xlength_unit[15];
    gchar ylength_unit[15];
    gchar zlength_unit[15];
    gint xunit_ratio;
    gint yunit_ratio;
    gint zunit_ratio;
    gint imprint;
    SurfInversionType inversion;
    SurfLevelingType leveling;
    gint seconds;
    gint minutes;
    gint hours;
    gint day;
    gint month;
    gint year;
    gint measurement_duration;
    gint comment_size;
    gint private_size;
    gchar client_zone[128];
    gdouble XOffset;
    gdouble YOffset;
    gdouble ZOffset;
    GwyDataField *dfield;
} SurfFile;

typedef struct {
    SurfFile *file;
    GwyContainer *data;
    GtkWidget *data_view;
} SurfControls;

static gboolean      module_register    (const gchar *name);
static gint          surffile_detect     (const gchar *filename,
                                         gboolean only_name);
static GwyContainer* surffile_load       (const gchar *filename);
static void          fill_data_fields   (SurfFile *surffile,
                                         const guchar *buffer);
static void          store_metadata     (SurfFile *surffile,
                                         GwyContainer *container);
static guint         select_which_data  (SurfFile *surffile);
static void          selection_changed  (GtkWidget *button,
                                         SurfControls *controls);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "surffile",
    N_("Imports Surf (Applied Physics and Engineering) data files."),
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
    static GwyFileFuncInfo surffile_func_info = {
        "surffile",
        N_("Surf files (.sur)"),
        (GwyFileDetectFunc)&surffile_detect,
        (GwyFileLoadFunc)&surffile_load,
        NULL
    };

    gwy_file_func_register(name, &surffile_func_info);

    return TRUE;
}

static gint
surffile_detect(const gchar *filename, gboolean only_name)
{
    gint score = 0;
    FILE *fh;
    gchar header[12];
    const guchar *p;
    guint version, mode, vbtype;

    if (only_name) {
        if (g_str_has_suffix(filename, ".sur"))
            score = 10;

        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;
    if (!(fread(header, 1, 12, fh) == 12)) {
        fclose(fh);
        return 0;
    }
    fclose(fh);

    if (!strncmp(header, "DIGITAL SURF", 11))
            score = 100;

    return score;
}

static GwyContainer*
surffile_load(const gchar *filename)
{
    SurfFile surffile;
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
    surffile.version = *(p++);
    if (size < 1294) {
        g_warning("File %s is not a Surf file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    surffile.spm_mode = *(p++);
    p += 2;   /* Skip VisualBasic VARIANT type type */
    surffile.scan_date = get_DOUBLE(&p);
    surffile.maxr_x = get_FLOAT(&p);
    surffile.maxr_y = get_FLOAT(&p);
    surffile.x_offset = get_DWORD(&p);
    surffile.y_offset = get_DWORD(&p);
    surffile.size_flag = get_WORD(&p);
    surffile.res = 16 << surffile.size_flag;
    surffile.acquire_delay = get_FLOAT(&p);
    surffile.raster_delay = get_FLOAT(&p);
    surffile.tip_dist = get_FLOAT(&p);
    surffile.v_ref = get_FLOAT(&p);
    if (surffile.version == 1) {
        surffile.vpmt1 = get_WORD(&p);
        surffile.vpmt2 = get_WORD(&p);
    }
    else {
        surffile.vpmt1 = get_FLOAT(&p);
        surffile.vpmt2 = get_FLOAT(&p);
    }
    surffile.remark = g_strndup(p, 120);
    p += 120;
    surffile.x_piezo_factor = get_DWORD(&p);
    surffile.y_piezo_factor = get_DWORD(&p);
    surffile.z_piezo_factor = get_DWORD(&p);
    surffile.hv_gain = get_FLOAT(&p);
    surffile.freq_osc_tip = get_DOUBLE(&p);
    surffile.rotate = get_FLOAT(&p);
    surffile.slope_x = get_FLOAT(&p);
    surffile.slope_y = get_FLOAT(&p);
    surffile.topo_means = get_WORD(&p);
    surffile.optical_means = get_WORD(&p);
    surffile.error_means = get_WORD(&p);
    surffile.channels = get_DWORD(&p);
    surffile.ndata = 0;
    for (b = surffile.channels; b; b = b >> 1)
        surffile.ndata += (b & 1);
    surffile.range_x = get_FLOAT(&p);
    surffile.range_y = get_FLOAT(&p);
    surffile.xreal = surffile.maxr_x * surffile.x_piezo_factor * surffile.range_x
                    * surffile.hv_gain/65535.0 * 1e-9;
    surffile.yreal = surffile.maxr_y * surffile.y_piezo_factor * surffile.range_y
                    * surffile.hv_gain/65535.0 * 1e-9;
    /* reserved */
    p += 46;

    gwy_debug("version = %u, spm_mode = %u", surffile.version, surffile.spm_mode);
    gwy_debug("scan_date = %f", surffile.scan_date);
    gwy_debug("maxr_x = %g, maxr_y = %g", surffile.maxr_x, surffile.maxr_y);
    gwy_debug("x_offset = %u, y_offset = %u",
              surffile.x_offset, surffile.y_offset);
    gwy_debug("size_flag = %u", surffile.size_flag);
    gwy_debug("acquire_delay = %g, raster_delay = %g, tip_dist = %g",
              surffile.acquire_delay, surffile.raster_delay, surffile.tip_dist);
    gwy_debug("v_ref = %g, vpmt1 = %g, vpmt2 = %g",
              surffile.v_ref, surffile.vpmt1, surffile.vpmt2);
    gwy_debug("x_piezo_factor = %u, y_piezo_factor = %u, z_piezo_factor = %u",
              surffile.x_piezo_factor, surffile.y_piezo_factor,
              surffile.z_piezo_factor);
    gwy_debug("hv_gain = %g, freq_osc_tip = %g, rotate = %g",
              surffile.hv_gain, surffile.freq_osc_tip, surffile.rotate);
    gwy_debug("slope_x = %g, slope_y = %g",
              surffile.slope_x, surffile.slope_y);
    gwy_debug("topo_means = %u, optical_means = %u, error_means = %u",
              surffile.topo_means, surffile.optical_means, surffile.error_means);
    gwy_debug("channel bitmask = %03x, ndata = %u",
              surffile.channels, surffile.ndata);
    gwy_debug("range_x = %g, range_y = %g",
              surffile.range_x, surffile.range_y);

    n = (surffile.res + 1)*(surffile.res + 1)*sizeof(float);
    if (size - (p - buffer) != n*surffile.ndata) {
        g_warning("Expected data size %u, but it's %u.",
                  n*surffile.ndata, (guint)(size - (p - buffer)));
        surffile.ndata = MIN(surffile.ndata, (size - (p - buffer))/n);
        if (!surffile.ndata) {
            g_warning("No data");
            gwy_file_abandon_contents(buffer, size, NULL);

            return NULL;
        }
    }
    fill_data_fields(&surffile, p);
    gwy_file_abandon_contents(buffer, size, NULL);

    if (surffile.dfield) {
        object = gwy_container_new();
        gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                         G_OBJECT(surffile.dfield));
        store_metadata(&surffile, GWY_CONTAINER(object));
    }
    for (b = 0; b < surffile.ndata; b++)
        g_object_unref(surffile.data[b]);

    g_free(surffile.remark);

    return (GwyContainer*)object;
}

static void
fill_data_fields(SurfFile *surffile,
                 const guchar *buffer)
{
    gdouble *data;
    guint n, i, j;

    surffile->dfield = GWY_DATA_FIELD(gwy_data_field_new(surffile->res,
                                                   surffile->res,
                                                   surffile->xreal,
                                                   surffile->yreal,
                                                   FALSE));
    data = gwy_data_field_get_data(surffile->dfield);
    buffer += (surffile->res + 1)*sizeof(float);
    for (i = 0; i < surffile->res; i++) {
        buffer += sizeof(float);
        for (j = 0; j < surffile->res; j++) {
            *(data++) = get_WORD(&buffer);
        }
    }
    gwy_data_field_multiply(surffile->dfield, 1e-9);
    
}

#define HASH_STORE(key, fmt, field) \
    gwy_container_set_string_by_name(container, "/meta/" key, \
                                     g_strdup_printf(fmt, surffile->field))

static void
store_metadata(SurfFile *surffile,
               GwyContainer *container)
{
    gchar *p;

    HASH_STORE("Version", "%u", version);
    HASH_STORE("Tip oscilation frequency", "%g Hz", freq_osc_tip);
    HASH_STORE("Acquire delay", "%.6f s", acquire_delay);
    HASH_STORE("Raster delay", "%.6f s", raster_delay);
    HASH_STORE("Tip distance", "%g nm", tip_dist);

    if (surffile->remark && *surffile->remark
        && (p = g_convert(surffile->remark, strlen(surffile->remark),
                          "UTF-8", "ISO-8859-1", NULL, NULL, NULL)))
        gwy_container_set_string_by_name(container, "/meta/Comment", p);
    gwy_container_set_string_by_name
        (container, "/meta/SPM mode",
         g_strdup(gwy_enum_to_string(surffile->spm_mode, spm_modes,
                                     G_N_ELEMENTS(spm_modes))));
    gwy_container_set_string_by_name(container, "/meta/Date",
                                     format_vt_date(surffile->scan_date));
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

