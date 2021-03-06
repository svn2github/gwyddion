/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek, Hans-Peter Doerr.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net,
 *          doerr@cip.physik.uni-freiburg.de.
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

/* See:
 * http://www.physik.uni-freiburg.de/~doerr/readimg
 * http://www.weizmann.ac.il/Chemical_Research_Support/surflab/peter/headers/burl.html
 */

#include <string.h>
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <libgwyddion/gwywin32unistd.h>

#if (defined(HAVE_SYS_STAT_H) || defined(_WIN32))
#include <sys/stat.h>
/* And now we are in a deep s... */
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "get.h"

#define EXTENSION ".img"

#define HEADER_SIZE_MIN 42

#define HEADER_SIZE_V21 8
#define FOOTER_SIZE_V21 40
#define TOTAL_SIZE_V21 (HEADER_SIZE_V21 + FOOTER_SIZE_V21)

#define Angstrom (1e-10)

enum {
    BURLEIGH_CURRENT = 0,
    BURLEIGH_TOPOGRAPHY = 1
};

typedef struct {
    gdouble version;
    guint version_int;
    guint32 header_size;  /* Not in v2.1 */
    guint32 xres;
    guint32 yres;

    /* In v2.1, this in in the footer */
    guint32 xrangemax;
    guint32 yrangemax;
    guint32 zrangemax;
    guint32 xrange;
    guint32 yrange;
    guint32 zrange;
    guint32 data_type;
    guint32 scan_speed;
    gdouble z_gain;

    /* Not in v2.1 */
    guint32 afm_head_id;
    gdouble zoom_factor;

    /* Not in the older version described at weizmann.ac.il */
    guint32 zoom_level;
    guint32 bias_volts;
    guint32 tunneling_current;
} IMGFile;

static gboolean      module_register  (const gchar *name);
static gint          burleigh_detect  (const gchar *filename,
                                       gboolean only_name);
static GwyContainer* burleigh_load    (const gchar *filename);
static const gint16* burleigh_load_v21(IMGFile *imgfile,
                                       const guchar *buffer,
                                       gsize size);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "burleigh",
    N_("Imports Burleigh IMG data files version 2.1."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek & Hans-Peter Doerr",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo burleigh_func_info = {
        "burleigh",
        N_("Burleigh 2.1 files (.img)"),
        (GwyFileDetectFunc)&burleigh_detect,
        (GwyFileLoadFunc)&burleigh_load,
        NULL,
    };

    gwy_file_func_register(name, &burleigh_func_info);

    return TRUE;
}

static gint
burleigh_detect(const gchar *filename,
                gboolean only_name)
{
    FILE *fh;
    struct stat st;
    guchar buffer[4 + 2 + 2];
    const guchar *p;
    guint version_int, xres, yres;
    gdouble version;
    gint score = 0;

    if (only_name) {
        gchar *filename_lc;

        filename_lc = g_ascii_strdown(filename, -1);
        score = g_str_has_suffix(filename_lc, EXTENSION) ? 10 : 0;
        g_free(filename_lc);

        return score;
    }

    if (stat(filename, &st))
        return 0;

    if (!(fh = fopen(filename, "rb")))
        return 0;

    if (fread(buffer, sizeof(buffer), 1, fh) == 1)
        score = 100;
    fclose(fh);

    if (!score)
        return 0;

    p = buffer;
    version = get_FLOAT(&p);
    version_int = ROUND(10*version);
    gwy_debug("Version: %g", version);

    if (version_int == 21) {
        if (st.st_size < TOTAL_SIZE_V21 + 2)
            return 0;

        xres = get_WORD(&p);
        yres = get_WORD(&p);
        if (st.st_size == TOTAL_SIZE_V21 + 2*xres*yres)
            return 100;
        return 0;
    }

    return 0;
}

static GwyContainer*
burleigh_load(const gchar *filename)
{
    GwySIUnit *unit;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    IMGFile imgfile;
    GwyDataField *dfield;
    gdouble *data;
    const gint16 *d;
    gdouble scale;
    guint i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot get file contents");
        g_clear_error(&err);
        return NULL;
    }
    if (size < HEADER_SIZE_MIN + 2) {
        g_warning("File is too short");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    memset(&imgfile, 0, sizeof(imgfile));
    p = buffer;
    imgfile.version = get_FLOAT(&p);
    imgfile.version_int = ROUND(10*imgfile.version);
    if (imgfile.version_int == 21) {
        d = burleigh_load_v21(&imgfile, buffer, size);
        if (!d) {
            gwy_file_abandon_contents(buffer, size, NULL);
            return NULL;
        }
    }
    else {
        g_warning("File format version %.f is not supported", imgfile.version);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    dfield = GWY_DATA_FIELD(gwy_data_field_new(imgfile.xres, imgfile.yres,
                                               Angstrom*imgfile.xrange,
                                               Angstrom*imgfile.yrange,
                                               FALSE));
    data = gwy_data_field_get_data(dfield);
    scale = Angstrom * imgfile.z_gain * imgfile.zrange;
    for (i = 0; i < imgfile.xres*imgfile.yres; i++)
        data[i] = scale * GINT16_FROM_LE(d[i]);

    gwy_file_abandon_contents(buffer, size, NULL);

    unit = GWY_SI_UNIT(gwy_si_unit_new("m"));
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    container = GWY_CONTAINER(gwy_container_new());
    switch (imgfile.data_type) {
        case BURLEIGH_CURRENT:
        unit = GWY_SI_UNIT(gwy_si_unit_new("A"));
        gwy_container_set_string_by_name(container, "/filename/title",
                                         g_strdup("Current"));
        break;

        case BURLEIGH_TOPOGRAPHY:
        unit = GWY_SI_UNIT(gwy_si_unit_new("m"));
        gwy_container_set_string_by_name(container, "/filename/title",
                                         g_strdup("Topography"));
        break;

        default:
        unit = GWY_SI_UNIT(gwy_si_unit_new("m"));
        break;
    }
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    gwy_container_set_object_by_name(container, "/0/data", (GObject*)dfield);
    g_object_unref(dfield);

    return container;
}

static const gint16*
burleigh_load_v21(IMGFile *imgfile,
                  const guchar *buffer,
                  gsize size)
{
    const guchar *p = buffer + 4; /* size of version */
    guint32 n;

    /* Header */
    imgfile->xres = get_WORD(&p);
    imgfile->yres = get_WORD(&p);
    n = imgfile->xres * imgfile->yres;
    if (size != 2*n + TOTAL_SIZE_V21) {
        g_warning("File size mismatch");
        return NULL;
    }
    /* Skip to footer */
    p += 2*n;
    imgfile->xrangemax = get_DWORD(&p);
    imgfile->yrangemax = get_DWORD(&p);
    imgfile->zrangemax = get_DWORD(&p);
    imgfile->xrange = get_DWORD(&p);
    imgfile->yrange = get_DWORD(&p);
    imgfile->zrange = get_DWORD(&p);
    imgfile->scan_speed = get_WORD(&p);
    imgfile->zoom_level = get_WORD(&p);
    imgfile->data_type = get_WORD(&p);
    imgfile->z_gain = get_WORD(&p);
    imgfile->bias_volts = get_FLOAT(&p);
    imgfile->tunneling_current = get_FLOAT(&p);

    return (const gint16*)(buffer + HEADER_SIZE_V21);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
