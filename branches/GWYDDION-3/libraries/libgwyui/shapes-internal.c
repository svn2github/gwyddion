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

#include "libgwyui/shapes-internal.h"

void
_gwy_shapes_constrain_horiz_vert(const GwyShapes *shapes, GwyXY *dxy)
{
    const cairo_matrix_t *matrix = &shapes->coords_to_view;
    gdouble x = dxy->x, y = dxy->y;
    cairo_matrix_transform_distance(matrix, &x, &y);
    if (fabs(x) <= fabs(y))
        dxy->x = 0.0;
    else
        dxy->y = 0.0;
}

void
_gwy_shapes_remove_null_box(GwyShapes *shapes, guint i)
{
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    g_assert(gwy_coords_shape_size(coords) == 4);
    gdouble xy[4];
    gwy_coords_get(coords, i, xy);
    gdouble lx = xy[2] - xy[0], ly = xy[3] - xy[1];
    cairo_matrix_transform_distance(&shapes->coords_to_view, &lx, &ly);
    cairo_matrix_transform_distance(&shapes->view_to_pixel, &lx, &ly);
    if (lx*lx + ly*ly >= NULL_DIST2)
        return;

    gwy_coords_delete(coords, i);
    gwy_shapes_update(shapes);
}

gboolean
_gwy_shapes_snap_to_pixel_centre(const GwyShapes *shapes,
                                 gdouble *x, gdouble *y)
{
    if (!gwy_shapes_get_snapping(shapes))
        return FALSE;

    cairo_matrix_transform_point(&shapes->coords_to_view, x, y);
    cairo_matrix_transform_point(&shapes->view_to_pixel, x, y);
    *x = gwy_round_to_half(*x);
    *y = gwy_round_to_half(*y);
    cairo_matrix_transform_point(&shapes->pixel_to_view, x, y);
    cairo_matrix_transform_point(&shapes->view_to_coords, x, y);
    return TRUE;
}

gboolean
_gwy_shapes_snap_to_pixel_corner(const GwyShapes *shapes,
                                 gdouble *x, gdouble *y)
{
    if (!gwy_shapes_get_snapping(shapes))
        return FALSE;

    cairo_matrix_transform_point(&shapes->coords_to_view, x, y);
    cairo_matrix_transform_point(&shapes->view_to_pixel, x, y);
    *x = gwy_round(*x);
    *y = gwy_round(*y);
    cairo_matrix_transform_point(&shapes->pixel_to_view, x, y);
    cairo_matrix_transform_point(&shapes->view_to_coords, x, y);
    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
