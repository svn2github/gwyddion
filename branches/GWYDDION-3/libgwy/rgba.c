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

#include <string.h>
#include <glib-object.h>
#include "libgwy/serializable-boxed.h"
#include "libgwy/serialize.h"
#include "libgwy/rgba.h"
#include "libgwy/libgwy-aliases.h"

enum { N_ITEMS = 4 };

static gsize    gwy_rgba_itemize  (gpointer boxed,
                                   GwySerializableItems *items);
static gpointer gwy_rgba_construct(GwySerializableItems *items,
                                   GwyErrorList **error_list);
static void     gwy_rgba_assign   (gpointer destination,
                                   gconstpointer source);

static const GwySerializableItem default_items[N_ITEMS] = {
    /*0*/ { .name = "r", .ctype = GWY_SERIALIZABLE_DOUBLE, },
    /*1*/ { .name = "g", .ctype = GWY_SERIALIZABLE_DOUBLE, },
    /*2*/ { .name = "b", .ctype = GWY_SERIALIZABLE_DOUBLE, },
    /*3*/ { .name = "a", .ctype = GWY_SERIALIZABLE_DOUBLE, },
};

GType
gwy_rgba_get_type(void)
{
    static GType rgba_type = 0;

    if (G_UNLIKELY(!rgba_type)) {
        rgba_type = g_boxed_type_register_static("GwyRGBA",
                                                 (GBoxedCopyFunc)gwy_rgba_copy,
                                                 (GBoxedFreeFunc)gwy_rgba_free);
        static const GwySerializableBoxedInfo boxed_info = {
            N_ITEMS, gwy_rgba_itemize, gwy_rgba_construct, gwy_rgba_assign,
        };
        gwy_serializable_boxed_register_static(rgba_type, &boxed_info);
    }

    return rgba_type;
}

static gsize
gwy_rgba_itemize(gpointer boxed,
                 GwySerializableItems *items)
{
    GwyRGBA *rgba = (GwyRGBA*)boxed;
    GwySerializableItem it;

    g_return_val_if_fail(items->len - items->n_items >= N_ITEMS, 0);

    it = default_items[0];
    it.value.v_double = rgba->r;
    items->items[items->n_items++] = it;

    it = default_items[1];
    it.value.v_double = rgba->g;
    items->items[items->n_items++] = it;

    it = default_items[2];
    it.value.v_double = rgba->b;
    items->items[items->n_items++] = it;

    it = default_items[3];
    it.value.v_double = rgba->a;
    items->items[items->n_items++] = it;

    return N_ITEMS;
}

static gpointer
gwy_rgba_construct(GwySerializableItems *items,
                   GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, default_items, sizeof(default_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, "GwyRGBA", error_list);

    GwyRGBA *rgba = g_slice_new(GwyRGBA);
    rgba->r = CLAMP(its[0].value.v_double, 0.0, 1.0);
    rgba->g = CLAMP(its[1].value.v_double, 0.0, 1.0);
    rgba->b = CLAMP(its[2].value.v_double, 0.0, 1.0);
    rgba->a = CLAMP(its[3].value.v_double, 0.0, 1.0);
    return rgba;
}

static void
gwy_rgba_assign(gpointer destination,
                gconstpointer source)
{
    memcpy(destination, source, sizeof(GwyRGBA));
}

/**
 * gwy_rgba_copy:
 * @rgba: A RGBA color.
 *
 * Makes a copy of a rgba structure. The result must be freed using
 * gwy_rgba_free().
 *
 * Returns: A copy of @rgba.
 **/
GwyRGBA*
gwy_rgba_copy(const GwyRGBA *rgba)
{
    g_return_val_if_fail(rgba, NULL);
    return g_slice_copy(sizeof(GwyRGBA), rgba);
}

/**
 * gwy_rgba_free:
 * @rgba: A RGBA color.
 *
 * Frees an rgba structure created with gwy_rgba_copy().
 **/
void
gwy_rgba_free(GwyRGBA *rgba)
{
    g_slice_free1(sizeof(GwyRGBA), rgba);
}

/**
 * gwy_rgba_interpolate:
 * @src1: Color at point @x = 0.0.
 * @src2: Color at point @x = 1.0.
 * @x: Point in interval 0..1 to take color from.
 * @rgba: A #GwyRGBA to store result to.
 *
 * Linearly interpolates two colors, including alpha blending.
 *
 * Correct blending of two not fully opaque colors is tricky.  Always use
 * this function, not simple independent interpolation of r, g, b, and a.
 **/
void
gwy_rgba_interpolate(const GwyRGBA *src1,
                     const GwyRGBA *src2,
                     gdouble x,
                     GwyRGBA *rgba)
{
    /* for alpha = 0.0 there's actually no limit, but average is psychologicaly
     * better than some random value */
    if (G_LIKELY(src1->a == src2->a)) {
        rgba->a = src1->a;
        rgba->r = x*src2->r + (1.0 - x)*src1->r;
        rgba->g = x*src2->g + (1.0 - x)*src1->g;
        rgba->b = x*src2->b + (1.0 - x)*src1->b;
        return;
    }

    if (src2->a == 0.0) {
        rgba->a = (1.0 - x)*src1->a;
        rgba->r = src1->r;
        rgba->g = src1->g;
        rgba->b = src1->b;
        return;
    }
    if (src1->a == 0.0) {
        rgba->a = x*src2->a;
        rgba->r = src2->r;
        rgba->g = src2->g;
        rgba->b = src2->b;
        return;
    }

    /* nothing helped, it's a general case
     * however, for meaningful values, rgba->a cannot be 0.0 */
    rgba->a = x*src2->a + (1.0 - x)*src1->a;
    rgba->r = (x*src2->a*src2->r + (1.0 - x)*src1->a*src1->r)/rgba->a;
    rgba->g = (x*src2->a*src2->g + (1.0 - x)*src1->a*src1->g)/rgba->a;
    rgba->b = (x*src2->a*src2->b + (1.0 - x)*src1->a*src1->b)/rgba->a;
}

#define __LIBGWY_RGBA_C__
#include "libgwy/libgwy-aliases.c"

/************************** Documentation ****************************/

/**
 * SECTION:gwyrgba
 * @title: GwyRGBA
 * @short_description: Bit depth independet RGBA colors
 *
 * #GwyRGBA is a bit depth independent representation of an RGB or RGBA color,
 * using floating point values from the [0,1] interval.
 *
 * #GwyRGBA is not an object, but a simple struct that can be allocated on
 * stack on created with g_new() or malloc().  It implements the serializable
 * boxed protocol so it can be put into #GwyContainers and serialized as a
 * data member of objects.
 **/

/**
 * GwyRGBA:
 * @r: The red component.
 * @g: The green component.
 * @b: The blue component.
 * @a: The alpha (opacity) value.
 *
 * RGBA color specification type.
 *
 * All values are from the range [0,1].  Alpha is not premultiplied.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
