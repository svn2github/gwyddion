/*
 *  $Id$
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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
#include "libgwy/math.h"
#include "libgwy/serialize.h"
#include "libgwy/coords-point.h"
#include "libgwy/object-internal.h"
#include "libgwy/array-internal.h"

enum {
    SHAPE_SIZE = 2,
    DIMENSIONS = 2,
    N_ITEMS = 0
};

typedef struct _GwyCoordsPointPrivate CoordsPoint;

typedef struct {
    GwyArray *array;
    GwyXY *xy;
    const GwyXY *transxy;
    gboolean flipx;
    gboolean flipy;
} TransformFuncData;

static void     gwy_coords_point_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_coords_point_n_items          (GwySerializable *serializable);
static gsize    gwy_coords_point_itemize          (GwySerializable *serializable,
                                                   GwySerializableItems *items);
static gboolean gwy_coords_point_construct        (GwySerializable *serializable,
                                                   GwySerializableItems *items,
                                                   GwyErrorList **error_list);
static void     gwy_coords_point_translate        (GwyCoords *coords,
                                                   const GwyIntSet *indices,
                                                   const gdouble *offsets);
static void     gwy_coords_point_flip             (GwyCoords *coords,
                                                   const GwyIntSet *indices,
                                                   guint axes);
static void     gwy_coords_point_scale            (GwyCoords *coords,
                                                   const GwyIntSet *indices,
                                                   const gdouble *factors);

static const guint unit_map[DIMENSIONS] = { 0, 1 };

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
}

static void
gwy_coords_point_class_init(GwyCoordsPointClass *klass)
{
    GwyCoordsClass *coords_class = GWY_COORDS_CLASS(klass);

    coords_class->shape_size = SHAPE_SIZE;
    coords_class->dimension = G_N_ELEMENTS(unit_map);
    coords_class->unit_map = unit_map;
    coords_class->translate = gwy_coords_point_translate;
    coords_class->flip = gwy_coords_point_flip;
    coords_class->scale = gwy_coords_point_scale;
}

static void
gwy_coords_point_init(GwyCoordsPoint *coordspoint)
{
    gwy_array_set_item_type(GWY_ARRAY(coordspoint), SHAPE_SIZE*sizeof(gdouble),
                            NULL);
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

static void
translate_func(gint value, gpointer user_data)
{
    TransformFuncData *data = (TransformFuncData*)user_data;
    GwyXY *xy = data->xy + value;
    const GwyXY *dxy = data->transxy;
    xy->x += dxy->x;
    xy->y += dxy->y;
    gwy_array_updated(data->array, value);
}

static void
gwy_coords_point_translate(GwyCoords *coords,
                           const GwyIntSet *indices,
                           const gdouble *offsets)
{
    GwyArray *array = GWY_ARRAY(coords);
    guint n = gwy_array_size(array);
    GwyXY *xy = (GwyXY*)gwy_array_get_data(array);

    if (!indices) {
        gdouble xd = offsets[0], yd = offsets[1];
        for (guint i = 0; i < n; i++) {
            xy[i].x += xd;
            xy[i].y += yd;
            gwy_array_updated(array, i);
        }
    }
    else {
        TransformFuncData data = {
            .array = array,
            .xy = xy,
            .transxy = (const GwyXY*)offsets,
        };
        gwy_int_set_foreach(indices, translate_func, &data);
    }
}

static void
flip_func(gint value, gpointer user_data)
{
    TransformFuncData *data = (TransformFuncData*)user_data;
    GwyXY *xy = data->xy + value;
    if (data->flipx)
        xy->x = -xy->x;
    if (data->flipy)
        xy->y = -xy->y;
    gwy_array_updated(data->array, value);
}

static void
gwy_coords_point_flip(GwyCoords *coords,
                      const GwyIntSet *indices,
                      guint axes)
{
    GwyArray *array = GWY_ARRAY(coords);
    guint n = gwy_array_size(array);
    GwyXY *xy = (GwyXY*)gwy_array_get_data(array);
    gboolean flipx = axes & (1 << 0), flipy = axes & (1 << 1);

    if (!indices) {
        for (guint i = 0; i < n; i++) {
            if (flipx)
                xy[i].x = -xy[i].x;
            if (flipy)
                xy[i].y = -xy[i].y;
            gwy_array_updated(array, i);
        }
    }
    else {
        TransformFuncData data = {
            .array = array,
            .xy = xy,
            .flipx = flipx,
            .flipy = flipy,
        };
        gwy_int_set_foreach(indices, flip_func, &data);
    }
}

static void
scale_func(gint value, gpointer user_data)
{
    TransformFuncData *data = (TransformFuncData*)user_data;
    GwyXY *xy = data->xy + value;
    const GwyXY *dxy = data->transxy;
    xy->x *= dxy->x;
    xy->y *= dxy->y;
    gwy_array_updated(data->array, value);
}


static void
gwy_coords_point_scale(GwyCoords *coords,
                       const GwyIntSet *indices,
                       const gdouble *factors)
{
    GwyArray *array = GWY_ARRAY(coords);
    guint n = gwy_array_size(array);
    GwyXY *xy = (GwyXY*)gwy_array_get_data(array);

    if (!indices) {
        gdouble xs = factors[0], ys = factors[1];
        for (guint i = 0; i < n; i++) {
            xy[i].x *= xs;
            xy[i].y *= ys;
            gwy_array_updated(array, i);
        }
    }
    else {
        TransformFuncData data = {
            .array = array,
            .xy = xy,
            .transxy = (const GwyXY*)factors,
        };
        gwy_int_set_foreach(indices, scale_func, &data);
    }
}

/**
 * gwy_coords_point_new:
 *
 * Creates a new point coordinates object.
 *
 * Returns: A new point coordinates object.
 **/
GwyCoords*
gwy_coords_point_new(void)
{
    return g_object_newv(GWY_TYPE_COORDS_POINT, 0, NULL);
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

/**
 * gwy_coords_point_duplicate:
 * @coordspoint: A group of coordinates of points in plane.
 *
 * Duplicates a group coordinates of points in plane.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_coords_point_assign:
 * @dest: Destination group of coordinates of points in plane.
 * @src: Source group of coordinates of points in plane.
 *
 * Copies the value of a group of coordinates of points in plane.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
