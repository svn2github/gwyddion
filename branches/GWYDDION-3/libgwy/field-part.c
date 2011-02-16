/*
 *  $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/field-part.h"
#include "libgwy/serialize.h"
#include "libgwy/serializable-boxed.h"

enum { N_ITEMS = 4 };

static gsize    gwy_field_part_itemize  (gpointer boxed,
                                         GwySerializableItems *items);
static gpointer gwy_field_part_construct(GwySerializableItems *items,
                                         GwyErrorList **error_list);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "col",    .ctype = GWY_SERIALIZABLE_INT32, },
    /*1*/ { .name = "row",    .ctype = GWY_SERIALIZABLE_INT32, },
    /*2*/ { .name = "width",  .ctype = GWY_SERIALIZABLE_INT32, },
    /*3*/ { .name = "height", .ctype = GWY_SERIALIZABLE_INT32, },
};

GType
gwy_field_part_get_type(void)
{
    static GType field_part_type = 0;

    if (G_UNLIKELY(!field_part_type)) {
        field_part_type = g_boxed_type_register_static("GwyFieldPart",
                                                       (GBoxedCopyFunc)gwy_field_part_copy,
                                                       (GBoxedFreeFunc)gwy_field_part_free);
        static const GwySerializableBoxedInfo boxed_info = {
            sizeof(GwyFieldPart), N_ITEMS,
            gwy_field_part_itemize, gwy_field_part_construct,
            NULL, NULL,
        };
        gwy_serializable_boxed_register_static(field_part_type, &boxed_info);
    }

    return field_part_type;
}

static gsize
gwy_field_part_itemize(gpointer boxed,
                       GwySerializableItems *items)
{
    GwyFieldPart *fpart = (GwyFieldPart*)boxed;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    GwySerializableItem *it = items->items + items->n;

    *it = serialize_items[0];
    it->value.v_uint32 = fpart->col;
    it++, items->n++;

    *it = serialize_items[1];
    it->value.v_uint32 = fpart->row;
    it++, items->n++;

    *it = serialize_items[2];
    it->value.v_uint32 = fpart->width;
    it++, items->n++;

    *it = serialize_items[3];
    it->value.v_uint32 = fpart->height;
    it++, items->n++;

    return N_ITEMS;
}

static gpointer
gwy_field_part_construct(GwySerializableItems *items,
                         GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyFieldPart", error_list);

    GwyFieldPart *fpart = g_slice_new(GwyFieldPart);
    fpart->col = its[0].value.v_uint32;
    fpart->row = its[1].value.v_uint32;
    fpart->width = its[2].value.v_uint32;
    fpart->height = its[3].value.v_uint32;
    // Avoid integer overflows
    if (fpart->width > G_MAXUINT - fpart->col)
        fpart->width = 0;
    if (fpart->height > G_MAXUINT - fpart->row)
        fpart->height = 0;
    return fpart;
}

/**
 * gwy_field_part_copy:
 * @fpart: Pixel-wise rectangle in plane.
 *
 * Copies a pixel-wise rectangle in plane.
 *
 * Returns: A copy of @fpart. The result must be freed using
 *          gwy_field_part_free(), not g_free().
 **/
GwyFieldPart*
gwy_field_part_copy(const GwyFieldPart *fpart)
{
    g_return_val_if_fail(fpart, NULL);
    return g_slice_copy(sizeof(GwyFieldPart), fpart);
}

/**
 * gwy_field_part_free:
 * @fpart: Pixel-wise rectangle in plane.
 *
 * Frees pixel-wise a rectangle in plane created with gwy_field_part_copy().
 **/
void
gwy_field_part_free(GwyFieldPart *fpart)
{
    g_slice_free1(sizeof(GwyFieldPart), fpart);
}

/**
 * SECTION: field-part
 * @title: GwyFieldPart
 * @short_description: Pixel-wise rectangle in plane
 *
 * #GwyFieldPart is usually used to specify a rectangular part of a
 * #GwyField or #GwyMaskField.  It is registered as a serialisable boxed type.
 *
 * The usual convention for functions that take a #GwyFieldPart argument is
 * that %NULL means the entire field.  The following two examples are thus
 * equivalent:
 * |[
 * // Fill the entire field using explicit full-field rectangle.
 * GwyFieldPart fpart = { 0, 0, field->xres, field->yres };
 * gwy_field_fill(field, &fpart, NULL, GWY_MASK_IGNORE, 1.0);
 *
 * // Implicit full-field rectangle.
 * gwy_field_fill(field, NULL, NULL, GWY_MASK_IGNORE, 1.0);
 * ]|
 * Some commonly used functions have also a variant suffixed with
 * <literal>_full</literal> that do not take any field part or mask arguments
 * and always operate on the entire field.  Hence the following example is
 * also equivalent:
 * |[
 * // Use the plain fill function to fill the entire field.
 * gwy_field_fill_full(field, 1.0);
 * ]|
 * ISO C99 introduced compund type literals so #GwyFieldPart<!-- -->s can
 * be constructed on the fly as follows:
 * |[
 * // Fill a part of the field using a GwyFieldPart variable.
 * GwyFieldPart fpart = { col, row, width, height };
 * gwy_field_fill(field, &fpart, NULL, GWY_MASK_IGNORE, 1.0);
 *
 * // Fill a part of the field using a GwyFieldPart compound literal.
 * gwy_field_fill(field, &((GwyFieldPart){ col, row, width, height }),
 *                NULL, GWY_MASK_IGNORE, 1.0);
 * ]|
 *
 * Most functions taking a #GwyFieldPart argument require the field part to
 * be fully contained in the field and have non-zero width and height.  Data
 * copying functions such ash gwy_field_copy() or gwy_mask_field_copy() are
 * a notable exception.
 **/

/**
 * GwyFieldPart:
 * @col: Column index of the upper-left corner of the part.
 * @row: Row index of the upper-left corner of the part.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 *
 * Representation of a pixel-wise rectangle in plane.
 **/

/**
 * GWY_TYPE_FIELD_PART:
 *
 * The #GType for a boxed type holding a #GwyFieldPart.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
