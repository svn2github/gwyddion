/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek, Markus Pristovsek
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *  prissi@gift.physik.tu-berlin.de.
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

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "err.h"
#include "get.h"

#define MAGIC "AFM/Ver. "
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".afm"

#define Nanometer 1e-9

enum {
    HEADER_SIZE   = 0x280,
    XREAL_OFFSET  = 0x16c,
    YREAL_OFFSET  = 0x176,
    ZSCALE_OFFSET = 0x184,
    RES_OFFSET    = 0x1dc,
};

enum {
    HEADER_SIZE_OLD  = 0x100,
    RES_OFFSET_OLD   = 0xc2,
    SCALE_OFFSET_OLD = 0x42,
    UNIT_OFFSET_OLD  = 0x62,
    SPEED_OFFSET_OLD = 0x82,
    NS_OFFSET_OLD    = 0xc8,
};

static gboolean      module_register    (void);
static gint          hitachi_detect     (const GwyFileDetectInfo *fileinfo,
                                         gboolean only_name);
static gint          hitachi_old_detect (const GwyFileDetectInfo *fileinfo,
                                         gboolean only_name);
static GwyContainer* hitachi_load       (const gchar *filename,
                                         GwyRunType mode,
                                         GError **error,
                                         const gchar *name);
static GwyDataField* read_data_field    (const guchar *buffer,
                                         guint size,
                                         GError **error);
static GwyDataField* read_data_field_old(const guchar *buffer,
                                         guint size,
                                         GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Hitachi AFM files."),
    "Yeti <yeti@gwyddion.net>",
    "0.4",
    "David Nečas (Yeti) & Petr Klapetek & Markus Pristovsek",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    /* Register two functions to keep the disctinction in the app although
     * the load function physically the same */
    gwy_file_func_register("hitachi-afm",
                           N_("Hitachi AFM files (.afm)"),
                           (GwyFileDetectFunc)&hitachi_detect,
                           (GwyFileLoadFunc)&hitachi_load,
                           NULL,
                           NULL);
    gwy_file_func_register("hitachi-afm-old",
                           N_("Hitachi AFM files, old (.afm)"),
                           (GwyFileDetectFunc)&hitachi_old_detect,
                           (GwyFileLoadFunc)&hitachi_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
hitachi_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && fileinfo->file_size >= HEADER_SIZE + 2
        && !memcmp(fileinfo->head, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static gint
hitachi_old_detect(const GwyFileDetectInfo *fileinfo,
                   gboolean only_name)
{
    guint xres, yres;
    const guchar *p;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len < HEADER_SIZE_OLD
        || fileinfo->file_size < HEADER_SIZE_OLD + 2
        /* This is actually header size (0x100), just weed out non-AFM files */
        || fileinfo->head[0] != 0 || fileinfo->head[1] != 1)
        return 0;

    p = fileinfo->head + RES_OFFSET_OLD;
    xres = get_WORD_LE(&p);
    yres = get_WORD_LE(&p);

    if (fileinfo->file_size == 2*xres*yres + HEADER_SIZE_OLD)
        return 100;

    return 0;
}

static gboolean
data_field_has_highly_nosquare_samples(GwyDataField *dfield)
{
    gint xres, yres;
    gdouble xreal, yreal, q;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);

    q = (xreal/xres)/(yreal/yres);

    /* The threshold is somewhat arbitrary.  Fortunately, most files encoutered
     * in practice have either q very close to 1, or 2 or more */
    return q > G_SQRT2 || q < 1.0/G_SQRT2;
}

static GwyContainer*
hitachi_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error,
             const gchar *name)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GwyDataField *(*do_load)(const guchar*, guint, GError**);
    guint header_size;

    if (gwy_strequal(name, "hitachi-afm")) {
        do_load = &read_data_field;
        header_size = HEADER_SIZE;
    }
    else if (gwy_strequal(name, "hitachi-afm-old")) {
        do_load = &read_data_field_old;
        header_size = HEADER_SIZE_OLD;
    }
    else {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_UNIMPLEMENTED,
                    _("Hitachi-AFM has not registered file type `%s'."), name);
        return NULL;
    }

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        g_clear_error(&err);
        return NULL;
    }
    if (size < header_size + 2) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    dfield = do_load(buffer, size, error);
    gwy_file_abandon_contents(buffer, size, NULL);
    if (!dfield)
        return NULL;

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    /* FIXME: this can be generally useful, move it to gwyddion */
    if (data_field_has_highly_nosquare_samples(dfield))
        gwy_container_set_boolean_by_name(container, "/0/data/realsquare",
                                          TRUE);

    return container;
}

static GwyDataField*
read_data_field(const guchar *buffer,
                guint size,
                GError **error)
{
    gint xres, yres, n, i, j;
    gdouble xreal, yreal, q;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble *data, *row;
    const gint16 *pdata;
    const guchar *p;

    p = buffer + RES_OFFSET;
    xres = get_DWORD_LE(&p);
    yres = get_DWORD_LE(&p);
    gwy_debug("xres: %d, yres: %d", xres, yres);

    n = xres*yres;
    if (size != 2*n + HEADER_SIZE) {
        err_SIZE_MISMATCH(error, 2*n + HEADER_SIZE, size);
        return NULL;
    }

    p = buffer + XREAL_OFFSET;
    xreal = get_DOUBLE_LE(&p) * Nanometer;
    p = buffer + YREAL_OFFSET;
    yreal = get_DOUBLE_LE(&p) * Nanometer;
    p = buffer + ZSCALE_OFFSET;
    q = get_DOUBLE_LE(&p) * Nanometer;
    gwy_debug("xreal: %g, yreal: %g, zreal: %g",
              xreal/Nanometer, yreal/Nanometer, q/Nanometer);
    /* XXX: I don't know where the factor of 0.5 comes from.  But it makes
     * the imported data match the original software. */
    q /= 2.0;
    q /= 65536.0;

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    pdata = (const gint16*)(buffer + HEADER_SIZE);
    for (i = 0; i < yres; i++) {
        row = data + (yres-1 - i)*xres;
        for (j = 0; j < xres; j++)
            row[j] = GUINT16_TO_LE(pdata[i*xres + j])*q;
    }

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    return dfield;
}

static GwyDataField*
read_data_field_old(const guchar *buffer,
                    guint size,
                    GError **error)
{
    gint xres, yres, n, i, j, vx, vy, vz;
    gdouble xscale, yscale, zscale, xunit, yunit, zunit, xreal, yreal, q;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble *data, *row;
    const gint16 *pdata;
    const guchar *p;

    p = buffer + RES_OFFSET_OLD;
    xres = get_WORD_LE(&p);
    yres = get_WORD_LE(&p);
    gwy_debug("xres: %d, yres: %d", xres, yres);

    n = xres*yres;
    if (size != 2*n + HEADER_SIZE_OLD) {
        err_SIZE_MISMATCH(error, 2*n + HEADER_SIZE_OLD, size);
        return NULL;
    }

    p = buffer + SCALE_OFFSET_OLD;
    xscale = get_DOUBLE_LE(&p);
    yscale = get_DOUBLE_LE(&p);
    zscale = get_DOUBLE_LE(&p);
    p = buffer + UNIT_OFFSET_OLD;
    xunit = get_DOUBLE_LE(&p);
    yunit = get_DOUBLE_LE(&p);
    zunit = get_DOUBLE_LE(&p);
    p = buffer + SPEED_OFFSET_OLD;
    vx = get_DWORD_LE(&p);
    vy = get_DWORD_LE(&p);
    vz = get_DWORD_LE(&p);

    xreal = xscale * vx;
    yreal = yscale * vy;
    q = zscale;
    gwy_debug("xreal: %g, yreal: %g, zscale: %g", xreal, yreal, q);

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    pdata = (const gint16*)(buffer + HEADER_SIZE_OLD);
    for (i = 0; i < yres; i++) {
        row = data + i*xres;
        for (j = 0; j < xres; j++)
            row[j] = GUINT16_TO_LE(pdata[i*xres + j])*q;
    }

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

