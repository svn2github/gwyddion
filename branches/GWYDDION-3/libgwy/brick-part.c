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
#include "libgwy/brick-part.h"
#include "libgwy/serialize.h"
#include "libgwy/serializable-boxed.h"

enum { N_ITEMS = 6 };

static gsize    gwy_brick_part_itemize  (gpointer boxed,
                                         GwySerializableItems *items);
static gpointer gwy_brick_part_construct(GwySerializableItems *items,
                                         GwyErrorList **error_list);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "col",    .ctype = GWY_SERIALIZABLE_INT32, },
    /*1*/ { .name = "row",    .ctype = GWY_SERIALIZABLE_INT32, },
    /*2*/ { .name = "level",  .ctype = GWY_SERIALIZABLE_INT32, },
    /*3*/ { .name = "width",  .ctype = GWY_SERIALIZABLE_INT32, },
    /*4*/ { .name = "height", .ctype = GWY_SERIALIZABLE_INT32, },
    /*5*/ { .name = "depth",  .ctype = GWY_SERIALIZABLE_INT32, },
};

GType
gwy_brick_part_get_type(void)
{
    static GType brick_part_type = 0;

    if (G_UNLIKELY(!brick_part_type)) {
        brick_part_type = g_boxed_type_register_static("GwyBrickPart",
                                                       (GBoxedCopyFunc)gwy_brick_part_copy,
                                                       (GBoxedFreeFunc)gwy_brick_part_free);
        static const GwySerializableBoxedInfo boxed_info = {
            sizeof(GwyBrickPart), N_ITEMS,
            gwy_brick_part_itemize, gwy_brick_part_construct,
            NULL, NULL,
        };
        gwy_serializable_boxed_register_static(brick_part_type, &boxed_info);
    }

    return brick_part_type;
}

static gsize
gwy_brick_part_itemize(gpointer boxed,
                       GwySerializableItems *items)
{
    GwyBrickPart *bpart = (GwyBrickPart*)boxed;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    GwySerializableItem *it = items->items + items->n;

    *it = serialize_items[0];
    it->value.v_uint32 = bpart->col;
    it++, items->n++;

    *it = serialize_items[1];
    it->value.v_uint32 = bpart->row;
    it++, items->n++;

    *it = serialize_items[3];
    it->value.v_uint32 = bpart->level;
    it++, items->n++;

    *it = serialize_items[3];
    it->value.v_uint32 = bpart->width;
    it++, items->n++;

    *it = serialize_items[4];
    it->value.v_uint32 = bpart->height;
    it++, items->n++;

    *it = serialize_items[5];
    it->value.v_uint32 = bpart->depth;
    it++, items->n++;

    return N_ITEMS;
}

static gpointer
gwy_brick_part_construct(GwySerializableItems *items,
                         GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyBrickPart", error_list);

    GwyBrickPart *bpart = g_slice_new(GwyBrickPart);
    bpart->col = its[0].value.v_uint32;
    bpart->row = its[1].value.v_uint32;
    bpart->level = its[2].value.v_uint32;
    bpart->width = its[3].value.v_uint32;
    bpart->height = its[4].value.v_uint32;
    bpart->depth = its[5].value.v_uint32;
    // Avoid integer overflows
    if (bpart->width > G_MAXUINT - bpart->col)
        bpart->width = 0;
    if (bpart->height > G_MAXUINT - bpart->row)
        bpart->height = 0;
    if (bpart->depth > G_MAXUINT - bpart->level)
        bpart->depth = 0;
    return bpart;
}

/**
 * gwy_brick_part_copy:
 * @bpart: Pixel-wise rectangle in plane.
 *
 * Copies a pixel-wise rectangle in plane.
 *
 * Returns: A copy of @bpart. The result must be freed using
 *          gwy_brick_part_free(), not g_free().
 **/
GwyBrickPart*
gwy_brick_part_copy(const GwyBrickPart *bpart)
{
    g_return_val_if_fail(bpart, NULL);
    return g_slice_copy(sizeof(GwyBrickPart), bpart);
}

/**
 * gwy_brick_part_free:
 * @bpart: Pixel-wise rectangle in plane.
 *
 * Frees pixel-wise a rectangle in plane created with gwy_brick_part_copy().
 **/
void
gwy_brick_part_free(GwyBrickPart *bpart)
{
    g_slice_free1(sizeof(GwyBrickPart), bpart);
}

/**
 * SECTION: brick-part
 * @title: GwyBrickPart
 * @short_description: Pixel-wise rectangle in plane
 *
 * #GwyBrickPart is usually used to specify a rectangular part of a
 * #GwyBrick.  It is registered as a serialisable boxed type.
 *
 * The usual convention for functions that take a #GwyBrickPart argument is
 * that %NULL means the entire brick.
 *
 * Most functions taking a #GwyBrickPart argument require the brick part to
 * be fully contained in the brick and have non-zero width and height.  Data
 * copying functions such ash gwy_brick_copy() are a notable exception.
 **/

/**
 * GwyBrickPart:
 * @col: Column index of the top upper-left corner of the part.
 * @row: Row index of the top upper-left corner of the part.
 * @level: Level index of the top upper-left corner of the part.
 * @width: Block width (number of columns).
 * @height: Block height (number of rows).
 * @depth: Block depth (number of levels).
 *
 * Representation of a pixel-wise block in space.
 **/

/**
 * GWY_TYPE_BRICK_PART:
 *
 * The #GType for a boxed type holding a #GwyBrickPart.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
