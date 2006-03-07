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
#include <stdio.h>

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
    gint nobjects;
    gint version;
    SurfObjectType type;
    gchar object_name[30];
    gchar operator_name[30];
    gint material_code;
    SurfAcqusitionType acquisition;
    SurfRangeType range;
    SurfSpecialPointsType special_points;
    gboolean absolute;
    gint pointsize;
    gint zmin;
    gint zmax;
    gint xres; /*number of points per line*/
    gint yres; /*number of lines*/
    gint nofpoints;
    gdouble dx;
    gdouble dy;
    gdouble dz;
    gchar *xaxis;
    gchar *yaxis;
    gchar *zaxis;
    gchar *dx_unit;
    gchar *dy_unit;
    gchar *dz_unit;
    gchar *xlength_unit;
    gchar *ylength_unit;
    gchar *zlength_unit;
    gdouble xunit_ratio;
    gdouble yunit_ratio;
    gdouble zunit_ratio;
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


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "surffile",
    N_("Imports Surf (Applied Physics and Engineering) data files."),
    "Petr Klapetek <klapetek@gwyddion.net>",
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
    gchar signature[12];

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    p = buffer;

    get_CHARARRAY(signature, &p);
    if (strncmp(signature, "DIGITAL SURF", 12) != 0) {
        g_warning("File %s is not a Surf file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    surffile.format = get_WORD(&p);
    surffile.nobjects = get_WORD(&p);
    surffile.version = get_WORD(&p);
    surffile.type = get_WORD(&p);
    get_CHARARRAY(surffile.object_name, &p);
    get_CHARARRAY(surffile.operator_name, &p);
    surffile.material_code = get_WORD(&p);
    surffile.acquisition = get_WORD(&p);
    surffile.range = get_WORD(&p);
    surffile.special_points = get_WORD(&p);
    surffile.absolute = get_WORD(&p);
    /*reserved*/
    p += 8;
    surffile.pointsize = get_WORD(&p);
    surffile.zmin = get_DWORD(&p);
    surffile.zmax = get_DWORD(&p);
    surffile.xres = get_DWORD(&p);
    surffile.yres = get_DWORD(&p);
    surffile.nofpoints = get_DWORD(&p);

    surffile.dx = get_FLOAT(&p);
    surffile.dy = get_FLOAT(&p);
    surffile.dz = get_FLOAT(&p);
    surffile.xaxis = g_strndup(p, 16);
    p += 16;
    surffile.yaxis = g_strndup(p, 16);
    p += 16;
    surffile.zaxis = g_strndup(p, 16);
    p += 16;
    surffile.dx_unit = g_strndup(p, 16);
    p += 16;
    surffile.dy_unit = g_strndup(p, 16);
    p += 16;
    surffile.dz_unit = g_strndup(p, 16);
    p += 16;
    surffile.xlength_unit = g_strndup(p, 16);
    p += 16;
    surffile.ylength_unit = g_strndup(p, 16);
    p += 16;
    surffile.zlength_unit = g_strndup(p, 16);
    p += 16;

    surffile.xunit_ratio = get_FLOAT(&p);
    surffile.yunit_ratio = get_FLOAT(&p);
    surffile.zunit_ratio = get_FLOAT(&p);
    surffile.imprint = get_WORD(&p);
    surffile.inversion = get_WORD(&p);
    surffile.leveling = get_WORD(&p);
    surffile.seconds = get_WORD(&p);
    surffile.minutes = get_WORD(&p);
    surffile.hours = get_WORD(&p);
    surffile.day = get_WORD(&p);
    surffile.month = get_WORD(&p);
    surffile.year = get_WORD(&p);
    surffile.measurement_duration = get_WORD(&p);
    surffile.comment_size = get_WORD(&p);
    surffile.private_size = get_WORD(&p);

    get_CHARARRAY(surffile.client_zone, &p);

    surffile.XOffset = get_FLOAT(&p);
    surffile.YOffset = get_FLOAT(&p);
    surffile.ZOffset = get_FLOAT(&p);

    /*reserved*/
    p += 33;


    gwy_debug("fileformat: %d,  n_of_objects: %d, version: %d, object_type: %d\n",
              surffile.format, surffile.nobjects, surffile.version, surffile.type);
    gwy_debug("object name: %s\noperator name: %s\n", surffile.object_name, surffile.operator_name);

    gwy_debug("material code: %d, acquisition type: %d\n", surffile.material_code, surffile.acquisition);
    gwy_debug("range type: %d, special points: %d, absolute: %d\n", surffile.range,
           surffile.special_points, (gint)surffile.absolute);
    gwy_debug("data point size: %d\n", surffile.pointsize);
    gwy_debug("zmin: %d, zmax: %d\n", surffile.zmin, surffile.zmax);
    gwy_debug("xres: %d, yres: %d (xres*yres = %d)\n", surffile.xres, surffile.yres, (surffile.xres*surffile.yres));
    gwy_debug("total number of points: %d\n", surffile.nofpoints);
    gwy_debug("dx: %g, dy: %g, dz: %g\n", surffile.dx, surffile.dy, surffile.dz);
    gwy_debug("X axis name: %s\n", surffile.xaxis);
    gwy_debug("Y axis name: %s\n", surffile.yaxis);
    gwy_debug("Z axis name: %s\n", surffile.zaxis);
    gwy_debug("dx unit: %s\n", surffile.dx_unit);
    gwy_debug("dy unit: %s\n", surffile.dy_unit);
    gwy_debug("dz unit: %s\n", surffile.dz_unit);
    gwy_debug("X axis unit: %s\n", surffile.xlength_unit);
    gwy_debug("Y axis unit: %s\n", surffile.ylength_unit);
    gwy_debug("Z axis unit: %s\n", surffile.zlength_unit);
    gwy_debug("xunit_ratio: %g, yunit_ratio: %g, zunit_ratio: %g\n", surffile.xunit_ratio, surffile.yunit_ratio, surffile.zunit_ratio);
    gwy_debug("imprint: %d, inversion: %d, leveling: %d\n", surffile.imprint, surffile.inversion, surffile.leveling);

    fill_data_fields(&surffile, p);
    gwy_file_abandon_contents(buffer, size, NULL);

    if (surffile.dfield) {
        object = gwy_container_new();
        gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                         G_OBJECT(surffile.dfield));
        store_metadata(&surffile, GWY_CONTAINER(object));
    }
    return (GwyContainer*)object;

    return NULL;
}

static void
fill_data_fields(SurfFile *surffile,
                 const guchar *buffer)
{
    gdouble *data;
    guint n, i, j;

    surffile->dfield = GWY_DATA_FIELD(gwy_data_field_new(surffile->xres,
                                                   surffile->yres,
                                                   surffile->xres,
                                                   surffile->yres,
                                                   TRUE));

    data = gwy_data_field_get_data(surffile->dfield);
    buffer += (surffile->xres + 1)*surffile->pointsize;
    for (i = 0; i < surffile->xres; i++) {
        buffer += sizeof(surffile->pointsize);
        for (j = 0; j < surffile->yres; j++) {
            if (surffile->pointsize == 16)
                *(data++) = get_WORD(&buffer);
            else
                *(data++) = get_DWORD(&buffer);
        }
    }
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
    HASH_STORE("Operator name", "%s", operator_name);
    HASH_STORE("Object name", "%s", object_name);
    HASH_STORE("Measurement duration", "%d s", measurement_duration);
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

