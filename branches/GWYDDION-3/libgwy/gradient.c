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
#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/strfuncs.h"
#include "libgwy/serialize.h"
#include "libgwy/gradient.h"
#include "libgwy/libgwy-aliases.h"

#define gwy_debug(fmt...)  /* FIXME */

#define EPS 1e-10
#define MAX_CVAL (0.99999999*(1 << (BITS_PER_SAMPLE)))

enum {
    N_ITEMS = 1,
    BITS_PER_SAMPLE = 8,
    GWY_GRADIENT_DEFAULT_SIZE = 1024
};

/* FIXME: The editting functionality duplicated GwyArray a bit but it emits
 * too much signals to our taste... */

struct _GwyGradientPrivate {
    GwyResource parent_instance;

    GArray *points;
    gboolean samples_valid : 1;
};

typedef struct _GwyGradientPrivate Gradient;

static void         gwy_gradient_finalize         (GObject *object);
static void         gwy_gradient_serializable_init(GwySerializableInterface *iface);
static gsize        gwy_gradient_n_items          (GwySerializable *serializable);
static gsize        gwy_gradient_itemize          (GwySerializable *serializable,
                                                   GwySerializableItems *items);
static gboolean     gwy_gradient_construct        (GwySerializable *serializable,
                                                   GwySerializableItems *items,
                                                   GwyErrorList **error_list);
static GObject*     gwy_gradient_duplicate_impl   (GwySerializable *serializable);
static void         gwy_gradient_assign_impl      (GwySerializable *destination,
                                                   GwySerializable *source);
static void         gwy_gradient_sanitize         (GwyGradient *gradient);
static void         gwy_gradient_setup_inventory  (GwyInventory *inventory);
static GwyResource* gwy_gradient_copy             (GwyResource *resource);
static gchar*       gwy_gradient_dump             (GwyResource *resource);
static gboolean     gwy_gradient_parse            (GwyResource *resource,
                                                   gchar *text,
                                                   GError **error);
static void         refine_interval               (GList *points,
                                                   gint n,
                                                   const GwyGradientPoint *samples,
                                                   gdouble threshold);
static void         gwy_gradient_changed          (GwyGradient *gradient);

static const GwyGradientPoint null_point = { 0, { 0, 0, 0, 0 } };

static const GwyGradientPoint default_gray[] = {
    { 0.0, { 0.0, 0.0, 0.0, 1.0 }, },
    { 1.0, { 1.0, 1.0, 1.0, 1.0 }, },
};

static const GwySerializableItem serialize_items[N_ITEMS] = {
    { .name = "data", .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
};

GwySerializableInterface *gwy_gradient_parent_serializable = NULL;

G_DEFINE_TYPE_EXTENDED
    (GwyGradient, gwy_gradient, GWY_TYPE_RESOURCE, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_gradient_serializable_init))

static void
gwy_gradient_serializable_init(GwySerializableInterface *iface)
{
    gwy_gradient_parent_serializable = g_type_interface_peek_parent(iface);
    iface->n_items   = gwy_gradient_n_items;
    iface->itemize   = gwy_gradient_itemize;
    iface->construct = gwy_gradient_construct;
    iface->duplicate = gwy_gradient_duplicate_impl;
    iface->assign    = gwy_gradient_assign_impl;
}

static void
gwy_gradient_class_init(GwyGradientClass *klass)
{
    GwyResourceClass *res_class = GWY_RESOURCE_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Gradient));

    gobject_class->finalize = gwy_gradient_finalize;

    res_class->setup_inventory = gwy_gradient_setup_inventory;
    res_class->copy = gwy_gradient_copy;
    res_class->dump = gwy_gradient_dump;
    res_class->parse = gwy_gradient_parse;

    gwy_resource_class_register(res_class, "gradients", NULL);
}

static void
gwy_gradient_init(GwyGradient *gradient)
{
    gradient->priv = G_TYPE_INSTANCE_GET_PRIVATE(gradient, GWY_TYPE_GRADIENT,
                                                 Gradient);
    Gradient *priv = gradient->priv;
    priv->points = g_array_sized_new(FALSE, FALSE, sizeof(GwyGradientPoint),
                                         G_N_ELEMENTS(default_gray));
    g_array_append_vals(priv->points, default_gray, G_N_ELEMENTS(default_gray));
}

static void
gwy_gradient_finalize(GObject *object)
{
    GwyGradient *gradient = GWY_GRADIENT(object);
    g_array_free(gradient->priv->points, TRUE);
    G_OBJECT_CLASS(gwy_gradient_parent_class)->finalize(object);
}

static gsize
gwy_gradient_n_items(GwySerializable *serializable)
{
    return N_ITEMS+1 + gwy_gradient_parent_serializable->n_items(serializable);
}

static gsize
gwy_gradient_itemize(GwySerializable *serializable,
                     GwySerializableItems *items)
{
    g_return_val_if_fail(items->len - items->n >= N_ITEMS+1, 0);

    GwyGradient *gradient = GWY_GRADIENT(serializable);
    GArray *points = gradient->priv->points;
    GwySerializableItem *it = items->items + items->n;

    // Our own data
    *it = serialize_items[0];
    it->value.v_double_array = (gdouble*)points->data;
    it->array_size = 5*points->len;
    it++, items->n++;

    // Chain to parent
    it->ctype = GWY_SERIALIZABLE_PARENT;
    it->name = g_type_name(GWY_TYPE_RESOURCE);
    it->array_size = 0;
    it->value.v_type = GWY_TYPE_RESOURCE;
    it++, items->n++;

    return N_ITEMS+1 + gwy_gradient_parent_serializable->itemize(serializable,
                                                                 items);
}

static gboolean
gwy_gradient_construct(GwySerializable *serializable,
                       GwySerializableItems *items,
                       GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gsize np = gwy_deserialize_filter_items(its, N_ITEMS, items, "GwyGradient",
                                            error_list);
    // Chain to parent
    if (np < items->n) {
        np++;
        GwySerializableItems parent_items = {
            items->len - np, items->n - np, items->items + np
        };
        if (!gwy_gradient_parent_serializable->construct(serializable,
                                                         &parent_items,
                                                         error_list))
            goto fail;
    }

    // Our own data
    GwyGradient *gradient = GWY_GRADIENT(serializable);
    GArray *points = gradient->priv->points;

    guint len = its[0].array_size;
    if (len && its[0].value.v_double_array) {
        if (len % 5 != 0) {
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_INVALID,
                               _("Gradient data length is %lu which is not "
                                 "a multiple of 5."),
                               (gulong)its[0].array_size);
            goto fail;
        }
        g_array_set_size(points, 0);
        g_array_append_vals(points, its[0].value.v_double_array, len/5);
        if (len == 5)
            g_array_append_vals(points, default_gray + 1, 1);
        gwy_gradient_sanitize(gradient);

        GWY_FREE(its[0].value.v_double_array);
        its[0].array_size = 0;
    }

    return TRUE;

fail:
    GWY_FREE(its[0].value.v_double_array);
    return FALSE;
}

static GObject*
gwy_gradient_duplicate_impl(GwySerializable *serializable)
{
    GwyGradient *gradient = GWY_GRADIENT(serializable);
    GArray *points = gradient->priv->points;

    GwyGradient *duplicate = g_object_newv(GWY_TYPE_GRADIENT, 0, NULL);
    GArray *dpoints = gradient->priv->points;

    gwy_gradient_parent_serializable->assign(GWY_SERIALIZABLE(duplicate),
                                             serializable);
    g_array_set_size(dpoints, 0);
    g_array_append_vals(dpoints, points->data, points->len);

    return G_OBJECT(duplicate);
}

static void
gwy_gradient_assign_impl(GwySerializable *destination,
                         GwySerializable *source)
{
    GwyGradient *gradient = GWY_GRADIENT(destination);
    GArray *points = gradient->priv->points;
    GwyGradient *src = GWY_GRADIENT(source);
    GArray *spoints = src->priv->points;
    gboolean emit_changed = FALSE;

    g_object_freeze_notify(G_OBJECT(gradient));
    gwy_gradient_parent_serializable->assign(destination, source);
    if (points->len != spoints->len
        || memcpy(points->data, spoints->data,
                  5*points->len*sizeof(gdouble)) != 0) {
        g_array_set_size(points, 0);
        g_array_append_vals(points, spoints->data, spoints->len);
        emit_changed = TRUE;
    }
    g_object_thaw_notify(G_OBJECT(gradient));
    if (emit_changed)
        gwy_resource_data_changed(GWY_RESOURCE(destination));
}

static GwyResource*
gwy_gradient_copy(GwyResource *resource)
{
    return GWY_RESOURCE(gwy_gradient_duplicate_impl(GWY_SERIALIZABLE(resource)));
}

/* This is an internal function and does NOT call gwy_gradient_changed(). */
static void
gwy_gradient_sanitize(GwyGradient *gradient)
{
    GArray *points = gradient->priv->points;
    GwyGradientPoint *pts = (GwyGradientPoint*)points->data;
    guint n = points->len;

    /* first make points ordered, in 0..1, starting with 0, ending with 1,
     * and fix colors */
    for (guint i = 0; i < n; i++) {
        GwyGradientPoint *pt = pts + i;
        gwy_rgba_fix(&pt->color);
        pt->x = CLAMP(pt->x, 0.0, 1.0);
        if (i && pt->x < pts[i - 1].x)
            pt->x = pts[i - 1].x;
    }
    GwyGradientPoint *pt = pts + n-1;
    if (pt->x != 1.0)
        pt->x = 1.0;

    /* then remove redundant pts */
    guint j = 0;
    for (guint i = 0; i < n; i++) {
        if (!i || pts[i].x > pts[i-1].x) {
            if (i != j)
                pts[j] = pts[i];
            j++;
        }
    }
    points->len = j;
}

static gchar*
gwy_gradient_dump(GwyResource *resource)
{
    GwyGradient *gradient = GWY_GRADIENT(resource);
    GString *dump = g_string_new(NULL);
    GArray *points = gradient->priv->points;
    const gdouble *pts = (const gdouble*)points->data;
    guint len = points->len;
    for (guint i = 0; i < len; i++) {
        gchar *row = gwy_resource_dump_data_line(pts + 5*i, 5);
        g_string_append(dump, row);
        g_string_append_c(dump, '\n');
        g_free(row);
    }
    return g_string_free(dump, FALSE);
}

static gboolean
gwy_gradient_parse(GwyResource *resource,
                   gchar *text,
                   G_GNUC_UNUSED GError **error)
{
    GwyGradient *gradient = GWY_GRADIENT(resource);
    GArray *points = gradient->priv->points;
    g_array_set_size(points, 0);
    for (gchar *line = gwy_str_next_line(&text);
         line;
         line = gwy_str_next_line(&text)) {
        GwyGradientPoint pt;
        switch (gwy_resource_parse_data_line(line, 5, (gdouble*)&pt)) {
            case GWY_RESOURCE_LINE_OK:
            g_array_append_vals(points, &pt, 1);
            break;

            case GWY_RESOURCE_LINE_EMPTY:
            case GWY_RESOURCE_LINE_BAD_NUMBER:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }
    gwy_gradient_sanitize(gradient);
    return TRUE;
}

/**
 * gwy_gradient_new:
 *
 * Creates a new color gradient.
 *
 * Returns: A new color gradient.
 **/
GwyGradient*
gwy_gradient_new(void)
{
    return g_object_newv(GWY_TYPE_GRADIENT, 0, NULL);
}

/**
 * gwy_gradient_get_color:
 * @gradient: A color gradient.
 * @x: Position in gradient, in interval [0,1].
 * @color: Color to fill with interpolated color at position @x.
 *
 * Computes the color at a given position of a color gradient.
 **/
void
gwy_gradient_get_color(GwyGradient *gradient,
                       gdouble x,
                       GwyRGBA *color)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(color);
    g_return_if_fail(x >= 0.0 && x <= 1.0);

    GArray *points = gradient->priv->points;
    GwyGradientPoint *pts = (GwyGradientPoint*)points->data;
    guint len = points->len;
    GwyGradientPoint *pt = pts;
    guint i;

    /* find the right subinterval */
    for (i = 0; i < len; i++) {
        pt = pts + i;
        if (pt->x == x) {
            *color = pt->color;
            return;
        }
        if (pt->x > x)
            break;
    }
    g_assert(i);
    GwyGradientPoint *pt2 = pts + i - 1;

    gwy_rgba_interpolate(&pt2->color, &pt->color, (x - pt2->x)/(pt->x - pt2->x),
                         color);
}

// FIXME: Not yet.
#if 0
/**
 * gwy_gradient_get_samples:
 * @gradient: A color gradient to get samples of.
 * @nsamples: A location to store the number of samples (or %NULL).
 *
 * Returns color gradient sampled to integers in #GdkPixbuf-like scheme.
 *
 * The returned samples are owned by @gradient and must not be modified or
 * freed.  They are automatically updated when the gradient changes, although
 * their number never changes.  The returned pointer is valid only as long
 * as the gradient used, indicated by gwy_resource_use().
 *
 * Returns: Sampled @gradient as a sequence of #GdkPixbuf-like RRGGBBAA
 *          quadruplets.
 **/
const guchar*
gwy_gradient_get_samples(GwyGradient *gradient,
                         gint *nsamples)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), NULL);
    if (!GWY_RESOURCE(gradient)->use_count) {
        g_warning("You have to call gwy_resource_use() first. "
                  "I'll try to be nice and do that for this once.");
        gwy_resource_use(GWY_RESOURCE(gradient));
    }
    if (nsamples)
        *nsamples = GWY_GRADIENT_DEFAULT_SIZE;

    return gradient->pixels;
}

/**
 * gwy_gradient_sample:
 * @gradient: A color gradient to sample.
 * @nsamples: Required number of samples.
 * @samples: Pointer to array to be filled.
 *
 * Samples a gradient to an array #GdkPixbuf-like samples.
 *
 * If @samples is not %NULL, it's resized to 4*@nsamples bytes, otherwise a
 * new buffer is allocated.
 *
 * If you don't have a reason for specific sample size (and are not going
 * to modify the samples or otherwise dislike the automatic resampling on
 * gradient definition change), use gwy_gradient_get_samples() instead.
 * This function does not need the gradient to be in use, though.
 *
 * Returns: Sampled @gradient as a sequence of #GdkPixbuf-like RRGGBBAA
 *          quadruplets.
 **/
guchar*
gwy_gradient_sample(GwyGradient *gradient,
                    gint nsamples,
                    guchar *samples)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), NULL);
    g_return_val_if_fail(nsamples > 1, NULL);

    samples = g_renew(guchar, samples, 4*nsamples);
    gwy_gradient_sample_real(gradient, nsamples, samples);

    return samples;
}

static void
gwy_gradient_sample_real(GwyGradient *gradient,
                         gint nsamples,
                         guchar *samples)
{
    GwyGradientPoint *pt, *pt2 = NULL;
    gint i, j, k;
    gdouble q, x;
    GwyRGBA color;

    q = 1.0/(nsamples - 1.0);
    pt = &g_array_index(gradient->points, GwyGradientPoint, 0);
    for (i = j = k = 0; i < nsamples; i++) {
        x = MIN(i*q, 1.0);
        while (G_UNLIKELY(x > pt->x)) {
            j++;
            pt2 = pt;
            pt = &g_array_index(gradient->points, GwyGradientPoint, j);
        }
        if (G_UNLIKELY(x == pt->x))
            color = pt->color;
        else
            gwy_rgba_interpolate(&pt2->color, &pt->color,
                                 (x - pt2->x)/(pt->x - pt2->x),
                                 &color);

        samples[k++] = (guchar)(gint32)(MAX_CVAL*color.r);
        samples[k++] = (guchar)(gint32)(MAX_CVAL*color.g);
        samples[k++] = (guchar)(gint32)(MAX_CVAL*color.b);
        samples[k++] = (guchar)(gint32)(MAX_CVAL*color.a);
    }
}

/**
 * gwy_gradient_sample_to_pixbuf:
 * @gradient: A color gradient to sample.
 * @pixbuf: A pixbuf to sample gradient to (in horizontal direction).
 *
 * Samples a color gradient to a provided pixbuf.
 *
 * Unlike gwy_gradient_sample() which simply takes samples at equidistant
 * points this method uses supersampling and thus it gives a bit better
 * looking gradient presentation.
 **/
void
gwy_gradient_sample_to_pixbuf(GwyGradient *gradient,
                              GdkPixbuf *pixbuf)
{
    /* Supersample to capture abrupt changes and peaks more faithfully.
     * Note an even number would lead to biased integer averaging. */
    enum { SUPERSAMPLE = 3 };
    gint width, height, rowstride, i, j;
    gboolean has_alpha, must_free_data;
    guchar *data, *pdata;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(GDK_IS_PIXBUF(pixbuf));

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
    pdata = gdk_pixbuf_get_pixels(pixbuf);

    /* Usually the pixbuf is large enough to be used as a scratch space,
     * there is no need to allocate extra memory then. */
    if ((must_free_data = (SUPERSAMPLE*width*4 > rowstride*height)))
        data = g_new(guchar, SUPERSAMPLE*width*4);
    else
        data = pdata;

    gwy_gradient_sample_real(gradient, SUPERSAMPLE*width, data);

    /* Scale down to original size */
    for (i = 0; i < width; i++) {
        guchar *row = data + 4*SUPERSAMPLE*i;
        guint r, g, b, a;

        r = g = b = a = SUPERSAMPLE/2;
        for (j = 0; j < SUPERSAMPLE; j++) {
            r += *(row++);
            g += *(row++);
            b += *(row++);
            a += *(row++);
        }
        *(pdata++) = r/SUPERSAMPLE;
        *(pdata++) = g/SUPERSAMPLE;
        *(pdata++) = b/SUPERSAMPLE;
        if (has_alpha)
            *(pdata++) = a/SUPERSAMPLE;
    }

    /* Duplicate rows */
    pdata = gdk_pixbuf_get_pixels(pixbuf);
    for (i = 1; i < height; i++)
        memcpy(pdata + i*rowstride, pdata, rowstride);

    if (must_free_data)
        g_free(data);
}
#endif

/**
 * gwy_gradient_n_points:
 * @gradient: A color gradient.
 *
 * Returns the number of points in a color gradient.
 *
 * Returns: The number of points in @gradient.
 **/
guint
gwy_gradient_n_points(GwyGradient *gradient)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), 0);
    return gradient->priv->points->len;
}

/**
 * gwy_gradient_get:
 * @gradient: A color gradient.
 * @n: Color point index in @gradient.
 *
 * Returns the point at given index of a color gradient.
 *
 * Returns: Color point at @n.
 **/
GwyGradientPoint
gwy_gradient_get(GwyGradient *gradient,
                 guint n)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), null_point);
    g_return_val_if_fail(n < gradient->priv->points->len, null_point);
    return g_array_index(gradient->priv->points, GwyGradientPoint, n);
}

/**
 * fix_position:
 * @points: Array of color points (gradient definition).
 * @i: Index a point should be inserted.
 * @pos: Position the point should be inserted to.
 *
 * Fixes the position of a color point between neighbours and to range 0..1.
 *
 * Returns: Fixed position.
 **/
static inline gdouble
fix_position(GArray *points,
             guint i,
             gdouble x)
{
    if (i == 0)
        return 0.0;
    if (i+1 == points->len)
        return 1.0;

    gdouble xprec = g_array_index(points, GwyGradientPoint, i-1).x;
    gdouble xsucc = g_array_index(points, GwyGradientPoint, i+1).x;
    if (x <= xprec)
        x = xprec + EPS;
    if (x >= xsucc)
        x = xsucc - EPS;
    // Very, very closely spaced points.
    if (x <= xprec)
        x = 0.5*(xprec + xsucc);

    return x;
}

/**
 * gwy_gradient_set:
 * @gradient: A color gradient.
 * @n: Color point index in @gradient.
 * @point: Color point to replace current point at @n with.
 *
 * Sets a single color point in a color gradient.
 *
 * It is an error to try to place a point beyond its neighbours, or to move the
 * first or last point from 0 or 1, respectively.
 **/
void
gwy_gradient_set(GwyGradient *gradient,
                 guint n,
                 const GwyGradientPoint *point)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gwy_resource_is_modifiable(GWY_RESOURCE(gradient)));
    g_return_if_fail(point);
    GArray *points = gradient->priv->points;
    g_return_if_fail(n < points->len);

    GwyGradientPoint pt = *point;
    gwy_rgba_fix(&pt.color);
    pt.x = fix_position(points, n, pt.x);
    GwyGradientPoint *gradpt = &g_array_index(points, GwyGradientPoint, n);
    if (memcmp(&pt, gradpt, sizeof(GwyGradientPoint)) != 0) {
        *gradpt = pt;
        gwy_gradient_changed(gradient);
    }
}

/**
 * gwy_gradient_set_color:
 * @gradient: A color gradient.
 * @n: Color point index in @gradient.
 * @color: Color to set the point to.
 *
 * Sets the color of a color gradient point without moving it.
 **/
void
gwy_gradient_set_color(GwyGradient *gradient,
                       guint n,
                       const GwyRGBA *color)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gwy_resource_is_modifiable(GWY_RESOURCE(gradient)));
    g_return_if_fail(color);
    GArray *points = gradient->priv->points;
    g_return_if_fail(n < points->len);

    GwyRGBA rgba = *color;
    gwy_rgba_fix(&rgba);
    GwyGradientPoint *gradpt = &g_array_index(points, GwyGradientPoint, n);
    if (memcmp(&rgba, &gradpt->color, sizeof(GwyRGBA)) != 0) {
        gradpt->color = rgba;
        gwy_gradient_changed(gradient);
    }
}

/**
 * gwy_gradient_insert:
 * @gradient: A color gradient.
 * @n: Color point index in @gradient.
 * @point: Color point to insert at @n.
 *
 * Inserts a point to a color gradient.
 *
 * It is an error to try to insert a point beyond its neighbours, or at the
 * first and last position that are always occupied by the point at 0.0 and
 * 1.0, respectively.
 **/
void
gwy_gradient_insert(GwyGradient *gradient,
                    guint n,
                    const GwyGradientPoint *point)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gwy_resource_is_modifiable(GWY_RESOURCE(gradient)));
    g_return_if_fail(point);
    GArray *points = gradient->priv->points;
    g_return_if_fail(n > 0 && n < points->len);

    GwyGradientPoint pt = *point;
    gwy_rgba_fix(&pt.color);
    g_array_insert_val(points, n, pt);
    g_array_index(points, GwyGradientPoint, n).x
        = fix_position(points, n, pt.x);

    gwy_gradient_changed(gradient);
}

/**
 * gwy_gradient_insert_sorted:
 * @gradient: A color gradient.
 * @point: Color point to insert.
 *
 * Inserts a point into a color gradient based on its x position.
 *
 * It is an error to try to insert a point at the position 0.0 or 1.0.
 *
 * Returns: The index @point was inserted at.
 **/
guint
gwy_gradient_insert_sorted(GwyGradient *gradient,
                           const GwyGradientPoint *point)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), G_MAXUINT);
    g_return_val_if_fail(gwy_resource_is_modifiable(GWY_RESOURCE(gradient)),
                         G_MAXUINT);
    g_return_val_if_fail(point, G_MAXUINT);
    g_return_val_if_fail(point->x > 0.0 && point->x < 1.0, G_MAXUINT);

    GwyGradientPoint pt = *point;
    gwy_rgba_fix(&pt.color);

    /* find the right subinterval */
    GArray *points = gradient->priv->points;
    GwyGradientPoint *pts = (GwyGradientPoint*)points->data;
    guint len = points->len;
    guint i;
    for (i = 0; i < len; i++) {
        if (pts[i].x >= pt.x)
            break;
    }
    g_assert(i < len);

    g_array_insert_val(points, i, pt);
    g_array_index(points, GwyGradientPoint, i).x
        = fix_position(points, i, pt.x);

    gwy_gradient_changed(gradient);

    return i;
}

/**
 * gwy_gradient_delete:
 * @gradient: A color gradient.
 * @n: Color point index in @gradient.
 *
 * Deletes a point at given index in a color gradient.
 *
 * It is an error to try to delete the first and last point.
 **/
void
gwy_gradient_delete(GwyGradient *gradient,
                    guint n)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gwy_resource_is_modifiable(GWY_RESOURCE(gradient)));
    GArray *points = gradient->priv->points;
    g_return_if_fail(n > 0 && n+1 < points->len);

    g_array_remove_index(points, n);
    gwy_gradient_changed(gradient);
}

/**
 * gwy_gradient_get_data:
 * @gradient: A color gradient.
 * @npoints: A location to store the number of color points (or %NULL).
 *
 * Returns the complete set of color points of a gradient.
 *
 * Returns: Complete set @gradient's color points.  The returned array is
 *          owned by @gradient and must not be modified or freed.
 **/
const GwyGradientPoint*
gwy_gradient_get_data(GwyGradient *gradient,
                      guint *npoints)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), NULL);
    GArray *points = gradient->priv->points;

    if (npoints)
        *npoints = points->len;
    return (const GwyGradientPoint*)points->data;
}

/**
 * gwy_gradient_set_data:
 * @gradient: A color gradient.
 * @npoints: The length of @points, it must be at least 2.
 * @points: Color points to set as new gradient definition.
 *
 * Sets the complete color gradient definition to a given set of points.
 *
 * The point positions must be ordered, and first point should start at 0.0,
 * last end at 1.0.  There should be no redundant points.
 **/
void
gwy_gradient_set_data(GwyGradient *gradient,
                      guint npoints,
                      const GwyGradientPoint *srcpoints)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gwy_resource_is_modifiable(GWY_RESOURCE(gradient)));
    g_return_if_fail(npoints >= 2);
    g_return_if_fail(srcpoints);

    GArray *points = gradient->priv->points;
    if (npoints == points->len
        && memcmp(srcpoints, points->data,
                  npoints*sizeof(GwyGradientPoint)) == 0)
        return;

    g_array_set_size(points, 0);
    g_array_append_vals(points, srcpoints, npoints);
    gwy_gradient_sanitize(gradient);
    gwy_gradient_changed(gradient);
}

/**
 * gwy_gradient_set_from_samples:
 * @gradient: A color gradient.
 * @nsamples: Number of samples, it must be at least one.
 * @samples: Sampled color gradient in #GdkPixbuf-like RRGGBBAA form.
 * @threshold: Maximum allowed difference (for color components in range 0..1).
 *             When negative, default value 1/80 suitable for most purposes
 *             is used.
 *
 * Reconstructs a color gradient definition from sampled colors.
 *
 * The result is usually approximate.
 **/
void
gwy_gradient_set_from_samples(GwyGradient *gradient,
                              guint nsamples,
                              const guchar *samples,
                              gdouble threshold)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gwy_resource_is_modifiable(GWY_RESOURCE(gradient)));
    g_return_if_fail(samples);
    g_return_if_fail(nsamples > 0);

    if (threshold < 0)
        threshold = 1.0/80.0;

    /* Preprocess guchar data to doubles */
    GwyGradientPoint *pts = g_new(GwyGradientPoint, MAX(nsamples, 2));
    for (guint k = 0, i = 0; i < nsamples; i++) {
        pts[i].x = i/(nsamples - 1.0);
        pts[i].color.r = samples[k++]/255.0;
        pts[i].color.g = samples[k++]/255.0;
        pts[i].color.b = samples[k++]/255.0;
        pts[i].color.a = samples[k++]/255.0;
    }

    /* Handle special silly case */
    if (nsamples == 1) {
        pts[0].x = 0.0;
        pts[1].x = 1.0;
        pts[1].color = pts[0].color;
        nsamples = 2;
    }

    /* Start with first and last point and recurse */
    GList *list = NULL;
    list = g_list_append(list, pts + nsamples-1);
    list = g_list_prepend(list, pts);
    refine_interval(list, nsamples, pts, threshold);

    /* Set the new points */
    GArray *points = gradient->priv->points;
    g_array_set_size(points, 0);
    for (GList *l = list; l; l = g_list_next(l))
        g_array_append_vals(points, l->data, 1);
    g_list_free(list);
    g_free(pts);
    gwy_gradient_changed(gradient);
}

static void
refine_interval(GList *points,
                gint n,
                const GwyGradientPoint *samples,
                gdouble threshold)
{
    GList *item;
    const GwyRGBA *first, *last;
    GwyRGBA color;
    gint i, mi;
    gdouble max, s, d;

    if (n <= 2)
        return;

    first = &samples[0].color;
    last = &samples[n-1].color;
    gwy_debug("Working on %d samples from {%f: %f, %f, %f, %f} "
              "to {%f: %f, %f, %f, %f}",
              n,
              samples[0].x, first->r, first->g, first->b, first->a,
              samples[n-1].x, last->r, last->g, last->b, last->a);

    max = 0.0;
    mi = 0;
    for (i = 1; i < n-1; i++) {
        gwy_rgba_interpolate(first, last, i/(n - 1.0), &color);
        /* Maximum distance is the crucial metric */
        s = ABS(color.r - samples[i].color.r);
        d = ABS(color.g - samples[i].color.g);
        s = MAX(s, d);
        d = ABS(color.b - samples[i].color.b);
        s = MAX(s, d);
        d = ABS(color.a - samples[i].color.a);
        s = MAX(s, d);

        if (s > max) {
            max = s;
            mi = i;
        }
    }
    gwy_debug("Max. difference %f located at %d, {%f: %f, %f, %f, %f}",
              max, mi,
              samples[mi].x,
              samples[mi].color.r,
              samples[mi].color.g,
              samples[mi].color.b,
              samples[mi].color.a);

    if (max < threshold) {
        gwy_debug("Max. difference small enough, stopping recursion");
        return;
    }
    gwy_debug("Inserting new point at %f", samples[mi].x);

    /* Use g_list_alloc() manually, GList functions care too much about list
     * head which is something we don't want here, because we always work
     * in the middle of some list. */
    item = g_list_alloc();
    item->data = (gpointer)(samples + mi);
    item->prev = points;
    item->next = points->next;
    item->prev->next = item;
    item->next->prev = item;

    /* Recurse. */
    refine_interval(points, mi + 1, samples, threshold);
    refine_interval(item, n - mi, samples + mi, threshold);
}

static void
gwy_gradient_changed(GwyGradient *gradient)
{
    gradient->priv->samples_valid = FALSE;
    gwy_resource_data_changed(GWY_RESOURCE(gradient));
}

static void
gwy_gradient_setup_inventory(GwyInventory *inventory)
{
    gwy_inventory_set_default_name(inventory, GWY_GRADIENT_DEFAULT);
    GwyGradient *gradient = g_object_new(GWY_TYPE_GRADIENT,
                                         "is-modifiable", FALSE,
                                         "name", GWY_GRADIENT_DEFAULT,
                                         NULL);
    gwy_inventory_insert(inventory, gradient);
    g_object_unref(gradient);
}

#define __LIBGWY_GRADIENT_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: gradient
 * @title: GwyGradient
 * @short_description: A map from numbers to RGBA colors
 *
 * Gradient is a map from interval [0,1] to RGB(A) color space.  It is used
 * for false color visualization of data.
 *
 * Each gradient is defined by an ordered set of color points, the first of
 * them is always at 0.0, the last at 1.0.   Thus each gradient must consist of
 * at least two points.  Between these points, the color is interpolated.
 * Color points of modifiable gradients (see #GwyResource) can be edited with
 * functions such as gwy_gradient_insert(), gwy_gradient_set_color(), or
 * gwy_gradient_set_data().
 *
 * Gradient objects can be obtained from gwy_gradients_get(). New
 * gradients can be created with gwy_inventory_new_copy() on the #GwyInventory
 * returned by gwy_gradients().
 **/

/**
 * GwyGradient:
 *
 * Object represnting a color gradient.
 *
 * #GwyGradient struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyGradientClass:
 *
 * Class of color gradients.
 *
 * #GwyGradientClass does not contain any public members.
 **/

/**
 * GwyGradientPoint:
 * @x: Color point position (in interval [0,1]).
 * @color: The color at position @x.
 *
 * Type of gradient color point.
 **/

/**
 * GWY_GRADIENT_DEFAULT:
 *
 * The name of the default gray color gradient.
 *
 * It is guaranteed to always exist.
 *
 * Note this is not the same as user's default gradient which corresponds to
 * the default item in gwy_gradients() inventory and it changes over time.
 **/

/**
 * gwy_gradient_duplicate:
 * @gradient: A color gradient.
 *
 * Duplicates a color gradient.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_gradient_assign:
 * @dest: Destination color gradient.
 * @src: Source color gradient.
 *
 * Copies the value of a color gradient.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_gradients:
 *
 * Gets inventory with all the gradients.
 *
 * Returns: Gradient inventory.
 **/

/**
 * gwy_gradients_get:
 * @name: Gradient name.  May be %NULL to get the default gradient.
 *
 * Convenience function to get a gradient from gwy_gradients() by name.
 *
 * Returns: Gradient identified by @name or the default gradient if @name does
 *          not exist.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
