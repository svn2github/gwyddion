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

#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/serialize.h"
#include "libgwy/curve.h"
#include "libgwy/curve-statistics.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/line-internal.h"
#include "libgwy/object-internal.h"
#include "libgwy/curve-internal.h"

enum { N_ITEMS = 3 };

enum {
    DATA_CHANGED,
    N_SIGNALS
};

enum {
    PROP_0,
    PROP_N_POINTS,
    PROP_UNIT_X,
    PROP_UNIT_Y,
    N_PROPS
};

static void     gwy_curve_finalize         (GObject *object);
static void     gwy_curve_dispose          (GObject *object);
static void     gwy_curve_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_curve_n_items          (GwySerializable *serializable);
static gsize    gwy_curve_itemize          (GwySerializable *serializable,
                                            GwySerializableItems *items);
static gboolean gwy_curve_construct        (GwySerializable *serializable,
                                            GwySerializableItems *items,
                                            GwyErrorList **error_list);
static GObject* gwy_curve_duplicate_impl   (GwySerializable *serializable);
static void     gwy_curve_assign_impl      (GwySerializable *destination,
                                            GwySerializable *source);
static void     gwy_curve_set_property     (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void     gwy_curve_get_property     (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "unit-x", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*1*/ { .name = "unit-y", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*2*/ { .name = "data",   .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
};

static guint curve_signals[N_SIGNALS];
static GParamSpec *curve_pspecs[N_PROPS];

G_DEFINE_TYPE_EXTENDED
    (GwyCurve, gwy_curve, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_curve_serializable_init))

static void
gwy_curve_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_curve_n_items;
    iface->itemize   = gwy_curve_itemize;
    iface->construct = gwy_curve_construct;
    iface->duplicate = gwy_curve_duplicate_impl;
    iface->assign    = gwy_curve_assign_impl;
}

static void
gwy_curve_class_init(GwyCurveClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Curve));

    gobject_class->dispose = gwy_curve_dispose;
    gobject_class->finalize = gwy_curve_finalize;
    gobject_class->get_property = gwy_curve_get_property;
    gobject_class->set_property = gwy_curve_set_property;

    curve_pspecs[PROP_N_POINTS]
        = g_param_spec_uint("n-points",
                            "N points",
                            "Number of curve points.",
                            0, G_MAXUINT, 0,
                            G_PARAM_READABLE | STATICP);

    curve_pspecs[PROP_UNIT_X]
        = g_param_spec_object("unit-x",
                              "X unit",
                              "Physical units of the abscissa values.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | STATICP);

    curve_pspecs[PROP_UNIT_Y]
        = g_param_spec_object("unit-y",
                              "Y unit",
                              "Physical units of the ordinate values.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | STATICP);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, curve_pspecs[i]);

    /**
     * GwyCurve::data-changed:
     * @gwycurve: The #GwyCurve which received the signal.
     *
     * The ::data-changed signal is emitted whenever curve data changes.
     **/
    curve_signals[DATA_CHANGED]
        = g_signal_new_class_handler("data-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_curve_init(GwyCurve *curve)
{
    curve->priv = G_TYPE_INSTANCE_GET_PRIVATE(curve, GWY_TYPE_CURVE, Curve);
}

static void
alloc_data(GwyCurve *curve)
{
    GWY_FREE(curve->data);
    if (curve->n)
        curve->data = g_new(GwyXY, curve->n);
}

static void
free_data(GwyCurve *curve)
{
    GWY_FREE(curve->data);
}

static void
sort_data(GwyCurve *curve)
{
    // If the data is already sorted this should be a quick O(n) operation with
    // a reasonable qsort() implementation, e.g. the glibc one.
    if (curve->n > 1)
        qsort(curve->data, curve->n, sizeof(GwyXY), &gwy_double_compare);
}

static void
gwy_curve_finalize(GObject *object)
{
    GwyCurve *curve = GWY_CURVE(object);
    free_data(curve);
    G_OBJECT_CLASS(gwy_curve_parent_class)->finalize(object);
}

static void
gwy_curve_dispose(GObject *object)
{
    GwyCurve *curve = GWY_CURVE(object);
    GWY_OBJECT_UNREF(curve->priv->unit_x);
    GWY_OBJECT_UNREF(curve->priv->unit_y);
    G_OBJECT_CLASS(gwy_curve_parent_class)->dispose(object);
}

static gsize
gwy_curve_n_items(GwySerializable *serializable)
{
    GwyCurve *curve = GWY_CURVE(serializable);
    Curve *priv = curve->priv;
    gsize n = N_ITEMS;
    if (priv->unit_x)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_x));
    if (priv->unit_y)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_y));
    return n;
}

static gsize
gwy_curve_itemize(GwySerializable *serializable,
                  GwySerializableItems *items)
{
    GwyCurve *curve = GWY_CURVE(serializable);
    Curve *priv = curve->priv;
    GwySerializableItem it;
    guint n = 0;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    if (priv->unit_x) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[0];
        it.value.v_object = (GObject*)priv->unit_x;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_x), items);
        n++;
    }

    if (priv->unit_y) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[1];
        it.value.v_object = (GObject*)priv->unit_y;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_y), items);
        n++;
    }

    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[2];
    it.value.v_double_array = (gdouble*)curve->data;
    it.array_size = 2*curve->n;
    items->items[items->n++] = it;
    n++;

    return n;
}

static gboolean
gwy_curve_construct(GwySerializable *serializable,
                    GwySerializableItems *items,
                    GwyErrorList **error_list)
{
    GwyCurve *curve = GWY_CURVE(serializable);
    Curve *priv = curve->priv;

    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyCurve", error_list);

    if (!_gwy_check_object_component(its + 0, curve, GWY_TYPE_UNIT, error_list))
        goto fail;
    if (!_gwy_check_object_component(its + 1, curve, GWY_TYPE_UNIT, error_list))
        goto fail;

    guint len = its[2].array_size;
    if (len && its[2].value.v_double_array) {
        if (len % 2 != 0) {
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_INVALID,
                               _("Curve data length is %lu which is not "
                                 "a multiple of 2."),
                               (gulong)its[2].array_size);
            goto fail;
        }
        curve->n = its[2].array_size/2;
        curve->data = (GwyXY*)its[2].value.v_double_array;
        sort_data(curve);
    }
    priv->unit_x = (GwyUnit*)its[0].value.v_object;
    priv->unit_y = (GwyUnit*)its[1].value.v_object;

    return TRUE;

fail:
    GWY_OBJECT_UNREF(its[0].value.v_object);
    GWY_OBJECT_UNREF(its[1].value.v_object);
    GWY_FREE(its[2].value.v_double_array);
    return FALSE;
}

static void
copy_info(GwyCurve *dest,
          const GwyCurve *src)
{
    Curve *dpriv = dest->priv, *spriv = src->priv;
    ASSIGN_UNITS(dpriv->unit_x, spriv->unit_x);
    ASSIGN_UNITS(dpriv->unit_y, spriv->unit_y);
}

static GObject*
gwy_curve_duplicate_impl(GwySerializable *serializable)
{
    GwyCurve *curve = GWY_CURVE(serializable);
    GwyCurve *duplicate = gwy_curve_new_from_data(curve->data, curve->n);
    copy_info(duplicate, curve);
    return G_OBJECT(duplicate);
}

static void
gwy_curve_assign_impl(GwySerializable *destination,
                      GwySerializable *source)
{
    GwyCurve *dest = GWY_CURVE(destination);
    GwyCurve *src = GWY_CURVE(source);

    gboolean notify = FALSE;
    if (dest->n != src->n) {
        dest->n = src->n;
        alloc_data(dest);
        notify = TRUE;
    }
    gwy_assign(dest->data, src->data, src->n);
    copy_info(dest, src);
    if (notify)
        g_object_notify_by_pspec(G_OBJECT(dest), curve_pspecs[PROP_N_POINTS]);
}

static void
gwy_curve_set_property(GObject *object,
                       guint prop_id,
                       G_GNUC_UNUSED const GValue *value,
                       GParamSpec *pspec)
{
    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_curve_get_property(GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
    GwyCurve *curve = GWY_CURVE(object);
    Curve *priv = curve->priv;

    switch (prop_id) {
        case PROP_N_POINTS:
        g_value_set_uint(value, curve->n);
        break;

        case PROP_UNIT_X:
        // Instantiate the units to be consistent with the direct interface
        // that never admits the units are NULL.
        if (!priv->unit_x)
            priv->unit_x = gwy_unit_new();
        g_value_set_object(value, priv->unit_x);
        break;

        case PROP_UNIT_Y:
        // Instantiate the units to be consistent with the direct interface
        // that never admits the units are NULL.
        if (!priv->unit_y)
            priv->unit_y = gwy_unit_new();
        g_value_set_object(value, priv->unit_y);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_curve_new:
 *
 * Creates a new empty curve.
 *
 * The curve will not contain any points. This parameterless constructor exists
 * mainly for language bindings, gwy_curve_new_from_data() is usually more
 * useful.
 *
 * Returns: A new curve.
 **/
GwyCurve*
gwy_curve_new(void)
{
    return g_object_newv(GWY_TYPE_CURVE, 0, NULL);
}

/**
 * gwy_curve_new_sized:
 * @n: Number of points.
 *
 * Creates a new curve with preallocated size.
 *
 * The curve will contain the speficied number of points with uninitialised
 * values.  Remember to run gwy_curve_sort() to sort the points if you fill
 * the data with unsorted points.
 *
 * Returns: A new curve.
 **/
GwyCurve*
gwy_curve_new_sized(guint n)
{
    GwyCurve *curve = g_object_newv(GWY_TYPE_CURVE, 0, NULL);
    curve->n = n;
    alloc_data(curve);
    return curve;
}

/**
 * gwy_curve_new_from_data:
 * @points: Array of @n points with the curve data.
 * @n: Number of points.
 *
 * Creates a new curve, filling it with provided points.
 *
 * The points will be sorted by abscissa values.
 *
 * Returns: A new curve.
 **/
GwyCurve*
gwy_curve_new_from_data(const GwyXY *points,
                        guint n)
{
    g_return_val_if_fail(!n || points, NULL);

    GwyCurve *curve = g_object_newv(GWY_TYPE_CURVE, 0, NULL);
    curve->n = n;
    alloc_data(curve);
    gwy_assign(curve->data, points, n);
    sort_data(curve);
    return curve;
}

/**
 * gwy_curve_new_alike:
 * @model: A curve to use as the template.
 *
 * Creates a new empty curve similar to another curve.
 *
 * The units of the new curve will be identical to those of @model but the new
 * curve will not contain any points. Use gwy_curve_duplicate() to completely
 * duplicate a curve including data.
 *
 * Returns: A new empty data curve.
 **/
GwyCurve*
gwy_curve_new_alike(const GwyCurve *model)
{
    g_return_val_if_fail(GWY_IS_CURVE(model), NULL);
    GwyCurve *curve = g_object_newv(GWY_TYPE_CURVE, 0, NULL);
    copy_info(curve, model);
    return curve;
}

/**
 * gwy_curve_new_part:
 * @curve: A curve.
 * @from: Left end of the interval.
 * @to: Righ end of the interval.
 *
 * Creates a new curve as a part of another curve.
 *
 * The new curve consits of data not smaller than @from and not larger than
 * @to.  It may be empty.
 *
 * Data are physically copied, i.e. changing the new curve data does not change
 * @curve's data and vice versa.
 *
 * Returns: A new curve.
 **/
GwyCurve*
gwy_curve_new_part(const GwyCurve *curve,
                   gdouble from,
                   gdouble to)
{
    g_return_val_if_fail(GWY_IS_CURVE(curve), NULL);

    GwyCurve *part = gwy_curve_new_alike(curve);

    if (!curve->n
        || from > to
        || from > curve->data[curve->n-1].x
        || to < curve->data[0].x)
        return part;

    guint first = 0, last = curve->n - 1;
    while (curve->data[first].x < from)
        first++;
    while (curve->data[last].x > to)
        last++;

    part->n = last+1 - first;
    alloc_data(part);
    gwy_assign(part->data, curve->data + first, part->n);
    return part;
}

static void
copy_line_to_curve(const GwyLine *line,
                   GwyCurve *curve)
{
    gdouble q = gwy_line_dx(line);
    gdouble off = 0.5*q + line->off;

    for (guint i = 0; i < line->res; i++) {
        curve->data[i].x = q*i + off;
        curve->data[i].y = line->data[i];
    }
    ASSIGN_UNITS(curve->priv->unit_x, line->priv->unit_x);
    ASSIGN_UNITS(curve->priv->unit_y, line->priv->unit_y);
}

/**
 * gwy_curve_new_from_line:
 * @line: A one-dimensional data line.
 *
 * Creates a new curve from a one-dimensional data line.
 *
 * The number of points in the new curve will be equal to the number of points
 * in the line.  Each ordinate value will be equal to the corresponding @line
 * value; abscissa values will be created in regular grid according to @line's
 * physical size and offset.
 *
 * Abscissa and ordinate units will correspond to @line's units.
 *
 * Returns: A new curve.
 **/
GwyCurve*
gwy_curve_new_from_line(const GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);

    GwyCurve *curve = g_object_newv(GWY_TYPE_CURVE, 0, NULL);
    curve->n = line->res;
    alloc_data(curve);
    copy_line_to_curve(line, curve);

    return curve;
}

/**
 * gwy_curve_data_changed:
 * @curve: A curve.
 *
 * Emits signal GwyCurve::data-changed on a curve.
 **/
void
gwy_curve_data_changed(GwyCurve *curve)
{
    g_return_if_fail(GWY_IS_CURVE(curve));
    g_signal_emit(curve, curve_signals[DATA_CHANGED], 0);
}

/**
 * gwy_curve_copy:
 * @src: Source curve.
 * @dest: Destination curve.
 *
 * Copies the data of a curve to another curve of the same dimensions.
 *
 * Only the data points are copied.  To make a curve completely identical to
 * another, including units and change of dimensions, you can use
 * gwy_curve_assign().
 **/
void
gwy_curve_copy(const GwyCurve *src,
               GwyCurve *dest)
{
    g_return_if_fail(GWY_IS_CURVE(src));
    g_return_if_fail(GWY_IS_CURVE(dest));
    g_return_if_fail(dest->n == src->n);
    gwy_assign(dest->data, src->data, src->n);
}

/**
 * gwy_curve_set_from_line:
 * @curve: A curve.
 * @line: A one-dimensional data line.
 *
 * Sets the data and units of a curve from a line.
 *
 * See gwy_curve_new_from_line() for details.
 **/
void
gwy_curve_set_from_line(GwyCurve *curve,
                        const GwyLine *line)
{
    g_return_if_fail(GWY_IS_CURVE(curve));
    g_return_if_fail(GWY_IS_LINE(line));

    gboolean notify = FALSE;
    if (curve->n != line->res) {
        free_data(curve);
        curve->n = line->res;
        alloc_data(curve);
        notify = TRUE;
    }
    copy_line_to_curve(line, curve);
    if (notify)
        g_object_notify_by_pspec(G_OBJECT(curve), curve_pspecs[PROP_N_POINTS]);
}

static gdouble
interpolate_linear(const GwyCurve *curve,
                   gdouble x,
                   guint *hint)
{
    const GwyXY *cdata = curve->data;
    guint n = curve->n;

    // Extrapolate the boundary points to the exterior.  This also catches n=1.
    if (x <= cdata[0].x) {
        *hint = 0;
        return cdata[0].y;
    }
    if (x >= cdata[n-1].x) {
        *hint = n-1;
        return cdata[n-1].y;
    }

    // Now @x must be within the curve range.  So locate it.
    guint j = *hint;
    while (j > 0 && x < cdata[j].x)
        j--;
    while (j < n-1 && x > cdata[j+1].x)
        j++;
    *hint = j;

    // Avoid division by zero in case of coinciding abscissas.
    gdouble r = x - cdata[j].x;
    if (r)
        r /= cdata[j+1].x - cdata[j].x;

    return r*cdata[j+1].y + (1.0 - r)*cdata[j].y;
}

GwyLine*
regularise(const GwyCurve *curve,
           gdouble from,
           gdouble to,
           guint res)
{
    guint last = curve->n - 1;
    const GwyXY *cdata = curve->data;
    gdouble length = cdata[last].x - cdata[0].x;
    GwyLine *line;

    if (!res) {
        if (length) {
            gdouble mdx = gwy_curve_median_dx(curve);
            res = mdx ? gwy_round((to - from)/mdx) + 1
                      : gwy_round((to - from)/length*curve->n) + 1;
            res = MIN(res, 4*curve->n);
        }
        else
            res = 1;
    }
    line = gwy_line_new_sized(res, FALSE);
    gdouble dx;
    if (res == 1) {
        if (!(dx = to - from))
            dx = from ? from : 1.0;
    }
    else
        dx = length/(res - 1);
    line->off = from - dx/2.0;
    line->real = res*dx;

    gdouble *ldata = line->data;
    guint j = 0;
    if (res == 1)
        *ldata = interpolate_linear(curve, 0.5*(from + to), &j);
    else {
        for (guint i = 0; i < res; i++)
            *(ldata++) = interpolate_linear(curve, i*dx + from, &j);
    }

    ASSIGN_UNITS(line->priv->unit_x, curve->priv->unit_x);
    ASSIGN_UNITS(line->priv->unit_y, curve->priv->unit_y);

    return line;
}

/**
 * gwy_curve_regularize_full:
 * @curve: A curve.  It must have at least one point.
 * @res: Required line resolution.  Pass 0 to chose a resolution automatically.
 *
 * Creates a one-dimensional data line from an entire curve.
 *
 * If the curve has at least two points with different abscissa values, they
 * are equidistant and the requested number of points matches the @curve's
 * number of points, then one-to-one data point mapping can be used and the
 * conversion will be information-preserving.  In other words,If the curve was
 * created from a #GwyLine this function performs a perfect reversal (provided
 * the same @res or @res=0 is passed), possibly up to some rounding errors.
 * Otherwise linear interpolation is used.
 *
 * Returns: (allow-none):
 *          A new one-dimensional data line or %NULL if the curve contains no
 *          points.
 **/
GwyLine*
gwy_curve_regularize_full(const GwyCurve *curve,
                          guint res)
{
    g_return_val_if_fail(GWY_IS_CURVE(curve), NULL);

    if (!curve->n)
        return NULL;

    return regularise(curve, curve->data[0].x, curve->data[curve->n-1].x, res);
}

/**
 * gwy_curve_regularize:
 * @curve: A curve.  It must have at least one point.
 * @from: Left end of the interval.
 * @to: Right end of the interval.
 * @res: Required line resolution.  Pass 0 to chose a resolution automatically.
 *
 * Creates a one-dimensional data line from a curve.
 *
 * Values are lineary interpolated between curve points.  Values outside the
 * curve abscissa are replaced with the boundary values.
 *
 * Returns: (allow-none):
 *          A new one-dimensional data line or %NULL if the curve contains no
 *          points.
 **/
GwyLine*
gwy_curve_regularize(const GwyCurve *curve,
                     gdouble from,
                     gdouble to,
                     guint res)
{
    g_return_val_if_fail(GWY_IS_CURVE(curve), NULL);
    g_return_val_if_fail(to >= from, NULL);

    if (!curve->n)
        return NULL;

    return regularise(curve, from, to, res);
}

/**
 * gwy_curve_sort:
 * @curve: A curve.
 *
 * Ensures that points in a curve are sorted by abscissa values.
 *
 * All #GwyCurve's operations preserve the sorting.  So this method is useful
 * mainly if you modify the points manually and cannot ensure the ordering
 * by abscissa values.
 **/
void
gwy_curve_sort(GwyCurve *curve)
{
    g_return_if_fail(GWY_IS_CURVE(curve));
    sort_data(curve);
}

/**
 * gwy_curve_get_unit_x:
 * @curve: A curve.
 *
 * Obtains the abscissa units of a curve.
 *
 * Returns: (transfer none):
 *          The abscissa units of @curve.
 **/
GwyUnit*
gwy_curve_get_unit_x(GwyCurve *curve)
{
    g_return_val_if_fail(GWY_IS_CURVE(curve), NULL);
    Curve *priv = curve->priv;
    if (!priv->unit_x)
        priv->unit_x = gwy_unit_new();
    return priv->unit_x;
}

/**
 * gwy_curve_get_unit_y:
 * @curve: A curve.
 *
 * Obtains the ordinate units of a curve.
 *
 * Returns: (transfer none):
 *          The ordinate units of @curve.
 **/
GwyUnit*
gwy_curve_get_unit_y(GwyCurve *curve)
{
    g_return_val_if_fail(GWY_IS_CURVE(curve), NULL);
    Curve *priv = curve->priv;
    if (!priv->unit_y)
        priv->unit_y = gwy_unit_new();
    return priv->unit_y;
}

/**
 * gwy_curve_format_x:
 * @curve: A curve.
 * @style: Value format style.
 *
 * Finds a suitable format for displaying abscissa values of a curve.
 *
 * The created format usually has a sufficient precision to represent abscissa
 * values of neighbour points as different values.  However, if the intervals
 * differ by several orders of magnitude this is not really guaranteed.
 *
 * Returns: A newly created value format.
 **/
GwyValueFormat*
gwy_curve_format_x(GwyCurve *curve,
                   GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_CURVE(curve), NULL);
    if (!curve->n)
        return gwy_unit_format_with_resolution(gwy_curve_get_unit_x(curve),
                                               style, 1.0, 0.1);
    if (curve->n == 1) {
        gdouble m = curve->data[0].x ? fabs(curve->data[0].x) : 1.0;
        return gwy_unit_format_with_resolution(gwy_curve_get_unit_x(curve),
                                               style, m, m/10.0);
    }
    gdouble max = MAX(fabs(curve->data[0].x), fabs(curve->data[curve->n-1].x));
    gdouble unit = 0.0;
    const GwyXY *p = curve->data;
    // Give more weight to the smaller differences but do not let the precision
    // grow faster than quadratically with the number of curve points.
    for (guint i = curve->n; i; i--, p++)
        unit += sqrt(p[1].x - p[0].x);
    unit /= curve->n - 1;
    return gwy_unit_format_with_resolution(gwy_curve_get_unit_x(curve),
                                           style, max, unit*unit);
}

/**
 * gwy_curve_format_y:
 * @curve: A curve.
 * @style: Value format style.
 *
 * Finds a suitable format for displaying values in a data curve.
 *
 * Returns: A newly created value format.
 **/
GwyValueFormat*
gwy_curve_format_y(GwyCurve *curve,
                   GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_CURVE(curve), NULL);
    gdouble min, max;
    if (curve->n) {
        gwy_curve_min_max(curve, &min, &max);
        if (max == min) {
            max = ABS(max);
            min = 0.0;
        }
    }
    else {
        min = 0.0;
        max = 1.0;
    }
    return gwy_unit_format_with_digits(gwy_curve_get_unit_y(curve),
                                       style, max - min, 3);
}


/**
 * SECTION: curve
 * @title: GwyCurve
 * @short_description: General one-dimensional data
 *
 * #GwyCurve represents general, i.e. possibly unevenly spaced, one-dimensional
 * data.
 *
 * Curve points are stored in a flat array #GwyCurve-struct.data of #GwyXY
 * values.  The points must be always ordered by the abscissa values, most
 * methods will not work properly if this is not satisfied.
 *
 * Unlike #GwyLine, a curve can also be empty, i.e. contain zero points.
 **/

/**
 * GwyCurve:
 * @n: Number of points.
 * @data: Curve data.  See the introductory section for details.
 *
 * Object representing curve data.
 *
 * The #GwyCurve struct contains some public fields that can be directly
 * accessed for reading.  To set them, you must use the #GwyCurve methods.
 **/

/**
 * GwyCurveClass:
 *
 * Class of curves.
 **/

/**
 * gwy_curve_duplicate:
 * @curve: A curve.
 *
 * Duplicates a curve.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_curve_assign:
 * @dest: Destination curve.
 * @src: Source curve.
 *
 * Copies the value of a curve.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_curve_index:
 * @curve: A curve.
 * @pos: Position in @curve.
 *
 * Accesses a curve point.
 *
 * This macro may evaluate its arguments several times.
 * This macro expands to a left-hand side expression.
 *
 * No argument validation is performed.  If you process the data in a loop,
 * you are encouraged to access #GwyCurve-struct.data directly.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
