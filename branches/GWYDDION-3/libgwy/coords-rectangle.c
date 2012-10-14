/*
 *  $Id$
 *  Copyright (C) 2012 David Nečas (Yeti).
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

#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/serialize.h"
#include "libgwy/coords-rectangle.h"
#include "libgwy/object-internal.h"

enum {
    DIMENSIONS = 2,
    SHAPE_SIZE = 4,
    N_ITEMS = 0
};

typedef struct _GwyCoordsRectanglePrivate CoordsRectangle;

static void     gwy_coords_rectangle_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_coords_rectangle_n_items          (GwySerializable *serializable);
static gsize    gwy_coords_rectangle_itemize          (GwySerializable *serializable,
                                                       GwySerializableItems *items);
static gboolean gwy_coords_rectangle_construct        (GwySerializable *serializable,
                                                       GwySerializableItems *items,
                                                       GwyErrorList **error_list);

static const guint dimension_map[SHAPE_SIZE] = { 0, 1, 0, 1 };

static GwySerializableInterface *parent_serializable = NULL;

G_DEFINE_TYPE_EXTENDED
    (GwyCoordsRectangle, gwy_coords_rectangle, GWY_TYPE_COORDS, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_coords_rectangle_serializable_init));

static void
gwy_coords_rectangle_serializable_init(GwySerializableInterface *iface)
{
    parent_serializable = g_type_interface_peek_parent(iface);
    iface->n_items   = gwy_coords_rectangle_n_items;
    iface->itemize   = gwy_coords_rectangle_itemize;
    iface->construct = gwy_coords_rectangle_construct;
}

static void
gwy_coords_rectangle_class_init(GwyCoordsRectangleClass *klass)
{
    GwyCoordsClass *coords_class = GWY_COORDS_CLASS(klass);

    coords_class->shape_size = SHAPE_SIZE;
    coords_class->dimension = DIMENSIONS;
    coords_class->dimension_map = dimension_map;
    gwy_coords_class_set_generic_transforms(coords_class,
                                            GWY_COORDS_TRANSFORM_TRANSLATE
                                            | GWY_COORDS_TRANSFORM_FLIP
                                            | GWY_COORDS_TRANSFORM_TRANSPOSE
                                            | GWY_COORDS_TRANSFORM_SCALE);
}

static void
gwy_coords_rectangle_init(GwyCoordsRectangle *coordsrectangle)
{
    gwy_array_set_item_type(GWY_ARRAY(coordsrectangle),
                            SHAPE_SIZE*sizeof(gdouble),
                            NULL);
}

static gsize
gwy_coords_rectangle_n_items(GwySerializable *serializable)
{
    return N_ITEMS+1 + parent_serializable->n_items(serializable);
}

static gsize
gwy_coords_rectangle_itemize(GwySerializable *serializable,
                         GwySerializableItems *items)
{
    g_return_val_if_fail(items->len - items->n >= N_ITEMS+1, 0);
    return _gwy_itemize_chain_to_parent(serializable, GWY_TYPE_COORDS,
                                        parent_serializable, items, N_ITEMS);
}

static gboolean
gwy_coords_rectangle_construct(GwySerializable *serializable,
                           GwySerializableItems *items,
                           GwyErrorList **error_list)
{
    GwySerializableItems parent_items;
    if (gwy_deserialize_filter_items(NULL, N_ITEMS, items, &parent_items,
                                     "GwyCoordsRectangle", error_list)) {
        return parent_serializable->construct(serializable, &parent_items,
                                              error_list);
    }
    return TRUE;
}

/**
 * gwy_coords_rectangle_new:
 *
 * Creates a new rectangle coordinates object.
 *
 * Returns: A new rectangle coordinates object.
 **/
GwyCoords*
gwy_coords_rectangle_new(void)
{
    return g_object_newv(GWY_TYPE_COORDS_RECTANGLE, 0, NULL);
}

/**
 * SECTION: coords-rectangle
 * @title: GwyCoordsRectangle
 * @short_description: Coordinates of rectangles in plane.
 **/

/**
 * GwyCoordsRectangle:
 *
 * Object representing a group of coordinates of rectangles in plane.
 *
 * The #GwyCoordsRectangle struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyCoordsRectangleClass:
 *
 * Class of groups of coordinates of rectangles in plane.
 *
 * #GwyCoordsRectangleClass does not contain any public members.
 **/

/**
 * gwy_coords_rectangle_duplicate:
 * @coordsrectangle: A group of coordinates of rectangles in plane.
 *
 * Duplicates a group coordinates of rectangles in plane.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_coords_rectangle_assign:
 * @dest: Destination group of coordinates of rectangles in plane.
 * @src: Source group of coordinates of rectangles in plane.
 *
 * Copies the value of a group of coordinates of rectangles in plane.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
