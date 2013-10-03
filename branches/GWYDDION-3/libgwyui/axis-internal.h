/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*< private_header >*/

#ifndef __LIBGWY_AXIS_INTERNAL_H__
#define __LIBGWY_AXIS_INTERNAL_H__

#include "libgwyui/axis.h"

G_BEGIN_DECLS

// Not sure where these two belong.  We don't want all kinds of axes to support
// logscale.  But we do want the tick calculation within GwyAxis for those that
// do.
G_GNUC_INTERNAL
gboolean _gwy_axis_set_logscale(GwyAxis *axis,
                                gboolean setting);

G_GNUC_INTERNAL
gboolean _gwy_axis_get_logscale(GwyAxis *axis) G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
