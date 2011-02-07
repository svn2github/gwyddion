/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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

#ifndef __LIBGWY_SURFACE_INTERNAL_H__
#define __LIBGWY_SURFACE_INTERNAL_H__

G_BEGIN_DECLS

/* Cache operations */
#define GWY_SURFACE_CVAL(arg, bit)  ((arg)->cache[GWY_SURFACE_CACHE_##bit])
#define GWY_SURFACE_CBIT(bit)       (1 << GWY_SURFACE_CACHE_##bit)
#define GWY_SURFACE_CTEST(arg, bit) ((arg)->cached & GWY_SURFACE_CBIT(bit))

typedef enum {
    GWY_SURFACE_CACHE_MIN = 0,
    GWY_SURFACE_CACHE_MAX,
    GWY_SURFACE_CACHE_AVG,  // Not implemented yet
    GWY_SURFACE_CACHE_RMS,  // Not implemented yet
    GWY_SURFACE_CACHE_MSQ,  // Not implemented yet
    GWY_SURFACE_CACHE_MED,
    GWY_SURFACE_CACHE_ARF,  // Not implemented yet
    GWY_SURFACE_CACHE_ART,  // Not implemented yet
    GWY_SURFACE_CACHE_ARE,  // Not implemented yet
    GWY_SURFACE_CACHE_SIZE
} GwySurfaceCached;

struct _GwySurfacePrivate {
    GwyUnit *unit_xy;
    GwyUnit *unit_z;
    gdouble xmin;
    gdouble xmax;
    gdouble ymin;
    gdouble ymax;
    gboolean cached_range;
    guint32 cached;
    gdouble cache[GWY_SURFACE_CACHE_SIZE];
    // TODO: Here some triangulation stuff will go once we implement it.
};

typedef struct _GwySurfacePrivate Surface;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
