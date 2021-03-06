/*
 *  $Id$
 *  Copyright (C) 2009,2012 David Nečas (Yeti).
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
#include "libgwy/math.h"
#include "libgwy/serializable-boxed.h"
#include "libgwy/serialize.h"
#include "libgwy/rgba.h"

enum { N_ITEMS = 4 };

static gsize    gwy_rgba_itemize  (gpointer boxed,
                                   GwySerializableItems *items);
static gpointer gwy_rgba_construct(GwySerializableItems *items,
                                   GwyErrorList **error_list);

static const GwyRGBA preset_colours[] = {
    { 0.000, 0.000, 0.000, 1.000 },
    { 1.000, 0.000, 0.000, 1.000 },
    { 0.000, 0.784, 0.000, 1.000 },
    { 0.000, 0.000, 1.000, 1.000 },
    { 1.000, 0.000, 1.000, 1.000 },
    { 0.325, 0.851, 1.000, 1.000 },
    { 1.000, 0.812, 0.122, 1.000 },
    { 0.988, 0.502, 0.000, 1.000 },
    { 0.620, 0.620, 0.620, 1.000 },
    { 0.620, 0.000, 0.000, 1.000 },
    { 0.000, 0.278, 0.000, 1.000 },
    { 0.000, 0.000, 0.412, 1.000 },
    { 0.424, 0.000, 0.424, 1.000 },
    { 0.000, 0.467, 0.467, 1.000 },
    { 0.843, 0.596, 0.000, 1.000 },
    { 0.529, 0.216, 0.000, 1.000 },
    { 1.000, 0.357, 0.255, 1.000 },
    { 0.000, 0.886, 0.525, 1.000 },
    { 0.024, 0.643, 1.000, 1.000 },
    { 1.000, 0.443, 0.725, 1.000 },
    { 0.604, 0.365, 0.094, 1.000 },
    { 0.588, 0.588, 0.000, 1.000 },
    { 0.369, 0.369, 0.369, 1.000 },
    { 0.682, 0.000, 1.000, 1.000 },
    { 0.129, 0.467, 0.000, 1.000 },
    { 0.082, 0.306, 0.435, 1.000 },
};

static const GwySerializableItem serialize_items[N_ITEMS] = {
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
            sizeof(GwyRGBA), N_ITEMS, gwy_rgba_itemize, gwy_rgba_construct,
            NULL, NULL,
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

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    GwySerializableItem *it = items->items + items->n;

    *it = serialize_items[0];
    it->value.v_double = rgba->r;
    it++, items->n++;

    *it = serialize_items[1];
    it->value.v_double = rgba->g;
    it++, items->n++;

    *it = serialize_items[2];
    it->value.v_double = rgba->b;
    it++, items->n++;

    *it = serialize_items[3];
    it->value.v_double = rgba->a;
    it++, items->n++;

    return N_ITEMS;
}

static gpointer
gwy_rgba_construct(GwySerializableItems *items,
                   GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyRGBA", error_list);

    GwyRGBA *rgba = g_slice_new(GwyRGBA);
    rgba->r = its[0].value.v_double;
    rgba->g = its[1].value.v_double;
    rgba->b = its[2].value.v_double;
    rgba->a = its[3].value.v_double;
    gwy_rgba_fix(rgba);
    return rgba;
}

/**
 * gwy_rgba_copy:
 * @rgba: An RGBA colour.
 *
 * Copies an RGBA colour.
 *
 * Returns: A copy of @rgba. The result must be freed using gwy_rgba_free(),
 *          not g_free().
 **/
GwyRGBA*
gwy_rgba_copy(const GwyRGBA *rgba)
{
    g_return_val_if_fail(rgba, NULL);
    return g_slice_copy(sizeof(GwyRGBA), rgba);
}

/**
 * gwy_rgba_free:
 * @rgba: An RGBA colour.
 *
 * Frees an RGBA colour created with gwy_rgba_copy().
 **/
void
gwy_rgba_free(GwyRGBA *rgba)
{
    g_slice_free1(sizeof(GwyRGBA), rgba);
}

#define fix_component(rgba,x,ok) \
    if (G_UNLIKELY(rgba->x < 0.0 || !isnormal(rgba->x))) { \
        ok = FALSE; \
        rgba->x = 0.0; \
    } \
    else if (G_UNLIKELY(rgba->x > 1.0)) { \
        ok = FALSE; \
        rgba->x = 1.0; \
    } \
    else \
        (void)0

/**
 * gwy_rgba_fix:
 * @rgba: An RGBA colour.
 *
 * Corrects components of a colour to lie in the range [0,1].
 *
 * Returns: %TRUE if @rgba was all right, %FALSE if some components had to be
 *          corrected.
 **/
gboolean
gwy_rgba_fix(GwyRGBA *rgba)
{
    gboolean ok = TRUE;
    fix_component(rgba, r, ok);
    fix_component(rgba, g, ok);
    fix_component(rgba, b, ok);
    fix_component(rgba, a, ok);
    return ok;
}

/**
 * gwy_rgba_interpolate:
 * @src1: Colour at point @x = 0.0.
 * @src2: Colour at point @x = 1.0.
 * @x: Point in interval 0..1 to take colour from.
 * @rgba: An RGBA colour to store result to.
 *
 * Linearly interpolates two colours, including alpha blending.
 *
 * Correct blending of two not fully opaque colours is tricky.  Always use
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

/**
 * gwy_rgba_preset_color:
 * @rgba: (out):
 *        An RGBA colour to set to preset color.
 * @i: Index of preset color.
 *
 * Sets an RGBA colour to a preset colour.
 *
 * The index can be arbitrarily large, however, colours start to repeat after
 * gwy_rgba_n_preset_colors().
 *
 * All presets are fully opaque and the zeroth colour is always black.  They
 * are intended for drawing of curves or symbols on a white background.
 **/
void
gwy_rgba_preset_color(GwyRGBA *rgba,
                      guint i)
{
    g_return_if_fail(rgba);
    i = i % G_N_ELEMENTS(preset_colours);
    *rgba = preset_colours[i];
}

/**
 * gwy_rgba_get_preset_color:
 * @i: Index of preset color.
 *
 * Obtains a preset RGBA colour.
 *
 * The index can be arbitrarily large, however, colours start to repeat after
 * gwy_rgba_n_preset_colors().
 *
 * Returns: (transfer none):
 *          Preset RGBA colour corresponding to the index @i.
 **/
const GwyRGBA*
gwy_rgba_get_preset_color(guint i)
{
    return preset_colours + (i % G_N_ELEMENTS(preset_colours));
}

/**
 * gwy_rgba_n_preset_colors:
 *
 * Obtains the number of preset colours.
 *
 * The colours returned by gwy_rgba_preset_color() and
 * gwy_rgba_get_preset_color() start to repeat after this number of colours.
 **/
guint
gwy_rgba_n_preset_colors(void)
{
    return G_N_ELEMENTS(preset_colours);
}

/************************** Documentation ****************************/

/**
 * SECTION: rgba
 * @title: GwyRGBA
 * @short_description: Bit depth independet RGBA colours
 *
 * #GwyRGBA is a bit depth independent representation of an RGB or RGBA colour,
 * using floating point values from the [0,1] interval.  It can be directly
 * typecast to and from #GdkRGBA and hence it is also compatible to cairo's
 * notion of colour.
 *
 * #GwyRGBA is not an object, but a simple struct that can be allocated on
 * stack on created with g_new() or malloc().  It implements the serialisable
 * boxed protocol so it can be put into #GwyContainers and serialised as a
 * data member of objects.
 **/

/**
 * GwyRGBA:
 * @r: The red component.
 * @g: The green component.
 * @b: The blue component.
 * @a: The alpha (opacity) value.
 *
 * RGBA colour specification type.
 *
 * All values are from the range [0,1].  Alpha is not premultiplied.
 **/

/**
 * GWY_TYPE_RGBA:
 *
 * The #GType for a boxed type holding an RGBA colour.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
