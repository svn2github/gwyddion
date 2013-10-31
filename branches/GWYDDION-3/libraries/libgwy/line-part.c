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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/line-part.h"
#include "libgwy/serialize.h"
#include "libgwy/serializable-boxed.h"

enum { N_ITEMS = 2 };

static gsize    gwy_line_part_itemize  (gpointer boxed,
                                        GwySerializableItems *items);
static gpointer gwy_line_part_construct(GwySerializableItems *items,
                                        GwyErrorList **error_list);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "pos",    .ctype = GWY_SERIALIZABLE_INT32, },
    /*1*/ { .name = "len",    .ctype = GWY_SERIALIZABLE_INT32, },
};

GType
gwy_line_part_get_type(void)
{
    static GType line_part_type = 0;

    if (G_UNLIKELY(!line_part_type)) {
        line_part_type = g_boxed_type_register_static("GwyLinePart",
                                                      (GBoxedCopyFunc)gwy_line_part_copy,
                                                      (GBoxedFreeFunc)gwy_line_part_free);
        static const GwySerializableBoxedInfo boxed_info = {
            sizeof(GwyLinePart), N_ITEMS,
            gwy_line_part_itemize, gwy_line_part_construct,
            NULL, NULL,
        };
        gwy_serializable_boxed_register_static(line_part_type, &boxed_info);
    }

    return line_part_type;
}

static gsize
gwy_line_part_itemize(gpointer boxed,
                      GwySerializableItems *items)
{
    GwyLinePart *lpart = (GwyLinePart*)boxed;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    GwySerializableItem *it = items->items + items->n;

    *it = serialize_items[0];
    it->value.v_uint32 = lpart->pos;
    it++, items->n++;

    *it = serialize_items[1];
    it->value.v_uint32 = lpart->len;
    it++, items->n++;

    return N_ITEMS;
}

static gpointer
gwy_line_part_construct(GwySerializableItems *items,
                        GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyLinePart", error_list);

    GwyLinePart *lpart = g_slice_new(GwyLinePart);
    lpart->pos = its[0].value.v_uint32;
    lpart->len = its[1].value.v_uint32;
    // Avoid integer overflows
    if (lpart->len > G_MAXUINT - lpart->pos)
        lpart->len = 0;
    return lpart;
}

/**
 * gwy_line_part_copy:
 * @lpart: Pixel-wise rectangle in plane.
 *
 * Copies a pixel-wise rectangle in plane.
 *
 * Returns: A copy of @lpart. The result must be freed using
 *          gwy_line_part_free(), not g_free().
 **/
GwyLinePart*
gwy_line_part_copy(const GwyLinePart *lpart)
{
    g_return_val_if_fail(lpart, NULL);
    return g_slice_copy(sizeof(GwyLinePart), lpart);
}

/**
 * gwy_line_part_free:
 * @lpart: Pixel-wise rectangle in plane.
 *
 * Frees pixel-wise a rectangle in plane created with gwy_line_part_copy().
 **/
void
gwy_line_part_free(GwyLinePart *lpart)
{
    g_slice_free1(sizeof(GwyLinePart), lpart);
}

/**
 * SECTION: line-part
 * @title: GwyLinePart
 * @short_description: Pixel-wise rectangle in plane
 *
 * #GwyLinePart is usually used to specify a segment of a #GwyLine or
 * #GwyMaskLine.  It is registered as a serialisable boxed type.
 *
 * The usual convention for functions that take a #GwyLinePart argument is
 * that %NULL means the entire line.
 *
 * Most functions taking a #GwyLinePart argument require the line part to be
 * fully contained in the line and have non-zero length.  Data copying
 * functions such ash gwy_line_copy() or gwy_mask_line_copy() are a notable
 * exception.
 **/

/**
 * GwyLinePart:
 * @pos: Index of start of the segment.
 * @len: Segment length (number of items).
 *
 * Representation of a pixel-wise segment in one dimension.
 **/

/**
 * GWY_TYPE_LINE_PART:
 *
 * The #GType for a boxed type holding a #GwyLinePart.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
