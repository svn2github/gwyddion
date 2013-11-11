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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <libgwyddion/gwymath.h>
#include <libprocess/cwt.h>

gdouble
gwy_cwt_wfunc_2d(gdouble scale,
                 gdouble mval,
                 gint xres,
                 Gwy2DCWTWaveletType wtype)
{
    gdouble dat2x, cur, scale2, cur2;

    dat2x = 4.0/(gdouble)xres;
    cur = mval*dat2x;
    cur2 = cur*cur;
    scale2 = scale*scale;

    switch (wtype) {
        case GWY_2DCWT_GAUSS:
        /* return exp(-(scale2*cur2)/2)*2*G_PI*scale2*2*G_PI*scale;
         * changed only for reasonable normalization*/
        return exp(-(scale2*cur2)/2);
        break;

        case GWY_2DCWT_HAT:
        return (scale2*cur2)*exp(-(scale2*cur2)/2)*2*G_PI*scale2;
        break;

        default:
        g_return_val_if_reached(1.0);
        break;
    }
    return 1.0;
}

/************************** Documentation ****************************/

/**
 * SECTION:cwt
 * @title: cwt
 * @short_description: Continuous Wavelet Transform
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
