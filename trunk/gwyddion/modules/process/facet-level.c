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
#include <libprocess/level.h>
#include <libgwydgets/gwystock.h>
#include <app/gwyapp.h>

#define LEVEL_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean    module_register            (const gchar *name);
static void        facet_level                (GwyContainer *data,
                                               GwyRunType run);
static void        facet_level_coeffs         (GwyDataField *dfield,
                                               gdouble *bx, gdouble *by);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Automatic facet-orientation based levelling. "
       "Levels data to make facets point up."),
    "Yeti <yeti@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    gwy_process_func_register("facet-level",
                              (GwyProcessFunc)&facet_level,
                              N_("/_Level/_Facet Level"),
                              GWY_STOCK_FACET_LEVEL,
                              LEVEL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Level data to make facets point upward"));

    return TRUE;
}

static void
facet_level(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *old;
    GQuark quark;
    gdouble c, bx, by, b2;
    gdouble p, progress, maxb2 = 666, eps = 1e-8;
    gint i;
    gboolean cancelled = FALSE;

    g_return_if_fail(run & LEVEL_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     0);
    g_return_if_fail(dfield && quark);
    old = dfield;
    dfield = gwy_data_field_duplicate(dfield);

    /* converge
     * FIXME: this can take a long time */
    i = 0;
    progress = 0.0;
    gwy_app_wait_start(GTK_WIDGET(gwy_app_data_window_get_for_data(data)),
                       _("Facet-leveling"));
    while (i < 100) {
        facet_level_coeffs(dfield, &bx, &by);
        b2 = bx*bx + by*by;
        if (!i)
            maxb2 = MAX(b2, eps);
        c = -0.5*(bx*gwy_data_field_get_xres(dfield)
                  + by*gwy_data_field_get_yres(dfield));
        gwy_data_field_plane_level(dfield, c, bx, by);
        if (b2 < eps)
            break;
        i++;
        p = log(b2/maxb2)/log(eps/maxb2);
        gwy_debug("progress = %f, p = %f, ip = %f", progress, p, i/100.0);
        /* never decrease progress, that would look silly */
        progress = MAX(progress, p);
        progress = MAX(progress, i/100.0);
        if (!gwy_app_wait_set_fraction(progress)) {
            cancelled = TRUE;
            break;
        }
    };
    gwy_app_wait_finish();
    if (!cancelled) {
        gwy_app_undo_qcheckpointv(data, 1, &quark);
        gwy_data_field_copy(dfield, old, FALSE);
        gwy_data_field_data_changed(old);
    }
    g_object_unref(dfield);
}

static void
facet_level_coeffs(GwyDataField *dfield, gdouble *bx, gdouble *by)
{
    gdouble *data, *row, *newrow;
    gdouble vx, vy, q, sumvx, sumvy, sumvz, xr, yr;
    gint xres, yres, i, j;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    if (xres < 2 || yres < 2) {
        g_warning("Cannot facet-level datafield of size smaller than 2");
        *bx = *by = 0;
        return;
    }
    xr = gwy_data_field_get_xreal(dfield)/xres;
    yr = gwy_data_field_get_yreal(dfield)/yres;

    data = gwy_data_field_get_data(dfield);
    sumvx = sumvy = sumvz = 0.0;
    newrow = data;
    for (i = 1; i < yres; i++) {
        row = newrow;
        newrow += xres;

        for (j = 1; j < xres; j++) {
            vx = 0.5*(newrow[j] + row[j] - newrow[j-1] - row[j-1])/xr;
            vy = 0.5*(newrow[j-1] + newrow[j] - row[j-1] - row[j])/yr;
            /* XXX: braindamaged heuristics; I thought q alone (i.e., normal
             * normalization) whould give nice facet leveling, but alas! the
             * higher norm values has to be suppressed much more -- it seems */
            q = exp(20.0*(vx*vx + vy*vy));
            sumvx += vx/q;
            sumvy += vy/q;
            sumvz -= 1.0/q;
        }
    }
    q = sumvz/-1.0;
    *bx = sumvx/q*xr;
    *by = sumvy/q*yr;
    gwy_debug("(%g, %g, %g) %g (%g, %g)", sumvx, sumvy, sumvz, q, *bx, *by);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
