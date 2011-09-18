/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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

#include <glib.h>
#include "libgwy/macros.h"
#include "libgwy/serialize.h"
#include "libgwy/coords-point.h"
#include "libgwy/object-internal.h"
#include "libgwy/array-internal.h"

enum { N_ITEMS = 0 };

typedef struct _GwyCoordsPointPrivate CoordsPoint;

static void     gwy_coords_point_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_coords_point_n_items          (GwySerializable *serializable);
static gsize    gwy_coords_point_itemize          (GwySerializable *serializable,
                                             GwySerializableItems *items);
static gboolean gwy_coords_point_construct        (GwySerializable *serializable,
                                             GwySerializableItems *items,
                                             GwyErrorList **error_list);

static const guint unit_map[] = { 0, 1 };

static GwySerializableInterface *parent_serializable = NULL;

G_DEFINE_TYPE_EXTENDED
    (GwyCoordsPoint, gwy_coords_point, GWY_TYPE_COORDS, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_coords_point_serializable_init));

static void
gwy_coords_point_serializable_init(GwySerializableInterface *iface)
{
    parent_serializable = g_type_interface_peek_parent(iface);
    iface->n_items   = gwy_coords_point_n_items;
    iface->itemize   = gwy_coords_point_itemize;
    iface->construct = gwy_coords_point_construct;
    g_printerr("iface->assign: %p\n", iface->assign);
}

static void
gwy_coords_point_class_init(GwyCoordsPointClass *klass)
{
    GwyCoordsClass *coords_class = GWY_COORDS_CLASS(klass);

    coords_class->shape_size = 2;
    coords_class->dimension = G_N_ELEMENTS(unit_map);
    coords_class->unit_map = unit_map;
}

static void
gwy_coords_point_init(G_GNUC_UNUSED GwyCoordsPoint *coords_point)
{
}

static gsize
gwy_coords_point_n_items(GwySerializable *serializable)
{
    return N_ITEMS+1 + parent_serializable->n_items(serializable);
}

static gsize
gwy_coords_point_itemize(GwySerializable *serializable,
                         GwySerializableItems *items)
{
    g_return_val_if_fail(items->len - items->n >= N_ITEMS+1, 0);
    return _gwy_itemize_chain_to_parent(serializable, GWY_TYPE_COORDS,
                                        parent_serializable, items, N_ITEMS);
}

static gboolean
gwy_coords_point_construct(GwySerializable *serializable,
                           GwySerializableItems *items,
                           GwyErrorList **error_list)
{
    GwySerializableItems parent_items;
    if (gwy_deserialize_filter_items(NULL, N_ITEMS, items, &parent_items,
                                     "GwyCoordsPoint", error_list)) {
        return parent_serializable->construct(serializable, &parent_items,
                                              error_list);
    }
    return TRUE;
}

/**
 * SECTION: coords-point
 * @title: GwyCoordsPoint
 * @short_description: Coordinates of points in plane.
 **/

/**
 * GwyCoordsPoint:
 *
 * Object representing a group of coordinates of points in plane.
 *
 * The #GwyCoordsPoint struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyCoordsPointClass:
 *
 * Class of groups of coordinates of points in plane.
 *
 * #GwyCoordsPointClass does not contain any public members.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
