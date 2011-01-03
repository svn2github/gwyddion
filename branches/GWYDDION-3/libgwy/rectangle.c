/*
 *  $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  The quicksort algorithm was copied from GNU C library,
 *  Copyright (C) 1991, 1992, 1996, 1997, 1999 Free Software Foundation, Inc.
 *  See below.
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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/rectangle.h"
#include "libgwy/serialize.h"
#include "libgwy/serializable-boxed.h"

enum { N_ITEMS = 4 };

static gsize    gwy_rectangle_itemize  (gpointer boxed,
                                        GwySerializableItems *items);
static gpointer gwy_rectangle_construct(GwySerializableItems *items,
                                        GwyErrorList **error_list);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "col",    .ctype = GWY_SERIALIZABLE_INT32, },
    /*1*/ { .name = "row",    .ctype = GWY_SERIALIZABLE_INT32, },
    /*2*/ { .name = "width",  .ctype = GWY_SERIALIZABLE_INT32, },
    /*3*/ { .name = "height", .ctype = GWY_SERIALIZABLE_INT32, },
};

GType
gwy_rectangle_get_type(void)
{
    static GType rectangle_type = 0;

    if (G_UNLIKELY(!rectangle_type)) {
        rectangle_type = g_boxed_type_register_static("GwyRectangle",
                                                      (GBoxedCopyFunc)gwy_rectangle_copy,
                                                      (GBoxedFreeFunc)gwy_rectangle_free);
        static const GwySerializableBoxedInfo boxed_info = {
            sizeof(GwyRectangle), N_ITEMS,
            gwy_rectangle_itemize, gwy_rectangle_construct,
            NULL, NULL,
        };
        gwy_serializable_boxed_register_static(rectangle_type, &boxed_info);
    }

    return rectangle_type;
}

static gsize
gwy_rectangle_itemize(gpointer boxed,
                      GwySerializableItems *items)
{
    GwyRectangle *rectangle = (GwyRectangle*)boxed;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    GwySerializableItem *it = items->items + items->n;

    *it = serialize_items[0];
    it->value.v_uint32 = rectangle->col;
    it++, items->n++;

    *it = serialize_items[1];
    it->value.v_uint32 = rectangle->row;
    it++, items->n++;

    *it = serialize_items[2];
    it->value.v_uint32 = rectangle->width;
    it++, items->n++;

    *it = serialize_items[3];
    it->value.v_uint32 = rectangle->height;
    it++, items->n++;

    return N_ITEMS;
}

static gpointer
gwy_rectangle_construct(GwySerializableItems *items,
                        GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyRectangle", error_list);

    GwyRectangle *rectangle = g_slice_new(GwyRectangle);
    rectangle->col = its[0].value.v_uint32;
    rectangle->row = its[1].value.v_uint32;
    rectangle->width = its[2].value.v_uint32;
    rectangle->height = its[3].value.v_uint32;
    // Avoid integer overflows
    if (rectangle->width > G_MAXUINT - rectangle->col)
        rectangle->width = 0;
    if (rectangle->height > G_MAXUINT - rectangle->row)
        rectangle->height = 0;
    return rectangle;
}

/**
 * gwy_rectangle_copy:
 * @rectangle: Pixel-wise rectangle in plane.
 *
 * Copies a pixel-wise rectangle in plane.
 *
 * Returns: A copy of @rectangle. The result must be freed using
 *          gwy_rectangle_free(), not g_free().
 **/
GwyRectangle*
gwy_rectangle_copy(const GwyRectangle *rectangle)
{
    g_return_val_if_fail(rectangle, NULL);
    return g_slice_copy(sizeof(GwyRectangle), rectangle);
}

/**
 * gwy_rectangle_free:
 * @rectangle: Pixel-wise rectangle in plane.
 *
 * Frees pixel-wise a rectangle in plane created with gwy_rectangle_copy().
 **/
void
gwy_rectangle_free(GwyRectangle *rectangle)
{
    g_slice_free1(sizeof(GwyRectangle), rectangle);
}

/**
 * SECTION: rectangle
 * @title: GwyRectangle
 * @short_description: Pixel-wise rectangle in plane
 *
 * #GwyRectangle is usually used to specify a rectangular part of a
 * #GwyField.  It is registered as a serialisable boxed type.
 *
 * The usual convention for functions that take a #GwyRectangle argument is
 * that %NULL means the entire field.  The following two examples are thus
 * equivalent:
 * |[
 * // Fill the entire field using explicit full-field rectangle.
 * GwyRectangle rectangle = { 0, 0, field->xres, field->yres };
 * gwy_field_fill(field, &rectangle, NULL, GWY_MASK_IGNORE, 1.0);
 *
 * // Implicit full-field rectangle.
 * gwy_field_fill(field, NULL, NULL, GWY_MASK_IGNORE, 1.0);
 * ]|
 * Some commonly used functions have also a variant suffixed with
 * <literal>_full</literal> that do not take any rectangle or mask arguments
 * and always operate on the entire field.  Hence the following example is
 * also equivalent:
 * |[
 * // Use the plain fill function to fill the entire field.
 * gwy_field_fill_full(field, 1.0);
 * ]|
 * ISO C99 introduced compund type literals so #GwyRectangle<!-- -->s can
 * be constructed on the fly as follows:
 * |[
 * // Fill a part of the field using a GwyRectangle variable.
 * GwyRectangle rectangle = { col, row, width, height };
 * gwy_field_fill(field, &rectangle, NULL, GWY_MASK_IGNORE, 1.0);
 *
 * // Fill a part of the field using a GwyRectangle compound literal.
 * gwy_field_fill(field, &((GwyRectangle){ col, row, width, height }),
 *                NULL, GWY_MASK_IGNORE, 1.0);
 * ]|
 *
 * Most functions taking a #GwyRectangle argument require the rectangle to
 * be fully contained in the field and have non-zero width and height.  Data
 * copying functions such ash gwy_field_copy() or gwy_mask_field_copy() are
 * a notable exception.
 **/

/**
 * GwyRectangle:
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 *
 * Representation of a pixel-wise rectangle in plane.
 **/

/**
 * GWY_TYPE_RECTANGLE:
 *
 * The #GType for a boxed type holding a #GwyRectangle.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
