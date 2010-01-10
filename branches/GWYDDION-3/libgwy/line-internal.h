/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

#ifndef __LIBGWY_LINE_INTERNAL_H__
#define __LIBGWY_LINE_INTERNAL_H__

#include "libgwy/line.h"

G_BEGIN_DECLS

// FIXME: Where these two belong?
#define _GWY_FFTW_PATIENCE FFTW_ESTIMATE

#define ASSIGN_UNITS(dest, src) \
    do { \
        if (src && dest) \
            gwy_unit_assign(dest, src); \
        else if (dest) \
            GWY_OBJECT_UNREF(dest); \
        else if (src) \
            dest = gwy_unit_duplicate(src); \
    } while (0)

struct _GwyLinePrivate {
    GwyUnit *unit_x;
    GwyUnit *unit_y;
    gboolean allocated;
};

typedef struct _GwyLinePrivate Line;

struct _GwyCurvePrivate {
    GwyUnit *unit_x;
    GwyUnit *unit_y;
};

typedef struct _GwyCurvePrivate Curve;

// FIXME: Where this one belong?
G_GNUC_INTERNAL
void _gwy_notify_properties(GObject *object,
                            const gchar **properties,
                            guint nproperties);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
