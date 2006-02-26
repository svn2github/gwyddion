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

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
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

enum { HEADER_SIZE = 640 };

static gboolean      module_register(void);
static gint          hitachi_detect (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* hitachi_load   (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static GwyDataField* read_data_field(const guchar *buffer,
                                     guint size,
                                     GError **error);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Hitachi AFM files."),
    "Yeti <yeti@gwyddion.net>",
    "0.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("hitachi-afm",
                           N_("Hitachi AFM files (.afm)"),
                           (GwyFileDetectFunc)&hitachi_detect,
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
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && fileinfo->file_size >= HEADER_SIZE + 2
        && !memcmp(fileinfo->buffer, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static GwyContainer*
hitachi_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        g_clear_error(&err);
        return NULL;
    }
    if (size < HEADER_SIZE + 2) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    dfield = read_data_field(buffer, size, error);
    gwy_file_abandon_contents(buffer, size, NULL);
    if (!dfield)
        return NULL;

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    return container;
}

static GwyDataField*
read_data_field(const guchar *buffer,
                guint size,
                GError **error)
{
    enum {
        XREAL_OFFSET  = 0x16c,
        YREAL_OFFSET  = 0x176,
        ZSCALE_OFFSET = 0x184,
        XRES_OFFSET   = 0x1dc,
        YRES_OFFSET   = 0x1e0,
    };
    gint xres, yres, n, i, j;
    gdouble xreal, yreal, q;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble *data, *row;
    const gint16 *pdata;
    const guchar *p;

    p = buffer + XRES_OFFSET;
    xres = get_DWORD(&p);
    p = buffer + YRES_OFFSET;
    yres = get_DWORD(&p);
    gwy_debug("xres: %d, yres: %d", xres, yres);

    n = xres*yres;
    if (size != 2*n + HEADER_SIZE) {
        err_SIZE_MISMATCH(error, 2*n + HEADER_SIZE, size);
        return NULL;
    }

    p = buffer + XREAL_OFFSET;
    xreal = get_DOUBLE(&p) * Nanometer;
    p = buffer + YREAL_OFFSET;
    yreal = get_DOUBLE(&p) * Nanometer;
    p = buffer + ZSCALE_OFFSET;
    q = get_DOUBLE(&p) * Nanometer;
    gwy_debug("xreal: %g, yreal: %g, zreal: %g",
              xreal/Nanometer, yreal/Nanometer, q/Nanometer);
    /* XXX: I don't know where the factor of 0.5 comes from.  But it makes
     * the imported data match the original software. */
    q /= 2.0;

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    pdata = (const gint16*)(buffer + HEADER_SIZE);
    for (i = 0; i < yres; i++) {
        row = data + (yres-1 - i)*xres;
        for (j = 0; j < xres; j++)
            row[j] = GUINT16_TO_LE(pdata[i*xres + j])/65536.0*q;
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

