/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <app/gwyapp.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "err.h"

#define EXTENSION ".txt"

static gboolean      module_register     (void);
static gint          asciiexport_detect  (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static gboolean      asciiexport_export  (GwyContainer *data,
                                          const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Exports data as simple ASCII matrix."),
    "Yeti <yeti@gwyddion.net>",
    "0.4",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("asciiexport",
                           N_("ASCII data matrix (.txt)"),
                           (GwyFileDetectFunc)&asciiexport_detect,
                           NULL,
                           NULL,
                           (GwyFileSaveFunc)&asciiexport_export);

    return TRUE;
}

static gint
asciiexport_detect(const GwyFileDetectInfo *fileinfo,
                   G_GNUC_UNUSED gboolean only_name)
{
    return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;
}

static gboolean
asciiexport_export(G_GNUC_UNUSED GwyContainer *data,
                   const gchar *filename,
                   G_GNUC_UNUSED GwyRunType mode,
                   GError **error)
{
    GwyDataField *dfield;
    gint xres, yres, i;
    gdouble *d;
    FILE *fh;

    if (!(fh = g_fopen(filename, "w"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield, 0);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(dfield);
    for (i = 0; i < xres*yres; i++) {
        if (fprintf(fh, "%g%c", d[i],
                    (i + 1) % xres ? '\t' : '\n') < 2) {
            err_WRITE(error);
            g_unlink(filename);
            fclose(fh);
            return FALSE;
        }
    }
    fclose(fh);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
