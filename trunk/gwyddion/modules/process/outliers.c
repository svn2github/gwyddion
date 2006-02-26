/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/correct.h>
#include <app/gwyapp.h>

#define OUTLIERS_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean module_register(const gchar *name);
static void     outliers       (GwyContainer *data,
                                GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates mask of outliers."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    gwy_process_func_register("outliers",
                              (GwyProcessFunc)&outliers,
                              N_("/_Correct Data/Mask of _Outliers"),
                              NULL,
                              OUTLIERS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Mark data farther than 3σ from mean value"));

    return TRUE;
}

static void
outliers(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *maskfield;
    GQuark dquark, mquark;
    gdouble thresh = 3.0;

    g_return_if_fail(run & OUTLIERS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     GWY_APP_MASK_FIELD, &maskfield,
                                     0);
    g_return_if_fail(dfield && dquark);
    gwy_app_undo_qcheckpoint(data, dquark, mquark, 0);
    if (!maskfield) {
        maskfield = gwy_data_field_new_alike(dfield, FALSE);
        gwy_container_set_object(data, mquark, maskfield);
        g_object_unref(maskfield);
    }
    gwy_data_field_mask_outliers(dfield, maskfield, thresh);
    gwy_data_field_data_changed(maskfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
