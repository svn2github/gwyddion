/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Necas (Yeti).
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
#include "libgwy/curve-statistics.h"
#include "libgwy/line.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/line-statistics.h"
#include "libgwy/math-internal.h"
#include "libgwy/object-internal.h"
#include "libgwy/curve-internal.h"
#include "libgwy/line-internal.h"

enum { N_ITEMS = 5 };

enum {
    DATA_CHANGED,
    N_SIGNALS
};

enum {
    PROP_0,
    PROP_RES,
    PROP_REAL,
    PROP_OFFSET,
    PROP_UNIT_X,
    PROP_UNIT_Y,
    N_PROPS
};

static void     gwy_line_finalize         (GObject *object);
static void     gwy_line_dispose          (GObject *object);
static void     gwy_line_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_line_n_items          (GwySerializable *serializable);
static gsize    gwy_line_itemize          (GwySerializable *serializable,
                                            GwySerializableItems *items);
static gboolean gwy_line_construct        (GwySerializable *serializable,
                                            GwySerializableItems *items,
                                            GwyErrorList **error_list);
static GObject* gwy_line_duplicate_impl   (GwySerializable *serializable);
static void     gwy_line_assign_impl      (GwySerializable *destination,
                                            GwySerializable *source);
static void     gwy_line_set_property     (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void     gwy_line_get_property     (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "real",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*1*/ { .name = "off",    .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*2*/ { .name = "unit-x", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*3*/ { .name = "unit-y", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*4*/ { .name = "data",   .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
};

static guint line_signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED
    (GwyLine, gwy_line, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_line_serializable_init))

static void
gwy_line_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_line_n_items;
    iface->itemize   = gwy_line_itemize;
    iface->construct = gwy_line_construct;
    iface->duplicate = gwy_line_duplicate_impl;
    iface->assign    = gwy_line_assign_impl;
}

static void
gwy_line_class_init(GwyLineClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Line));

    gobject_class->dispose = gwy_line_dispose;
    gobject_class->finalize = gwy_line_finalize;
    gobject_class->get_property = gwy_line_get_property;
    gobject_class->set_property = gwy_line_set_property;

    g_object_class_install_property
        (gobject_class,
         PROP_RES,
         g_param_spec_uint("res",
                           "Resolution",
                           "Pixel length of the line.",
                           1, G_MAXUINT, 1,
                           G_PARAM_READABLE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_REAL,
         g_param_spec_double("real",
                             "Real size",
                             "Length of the line in physical units.",
                             G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                             G_PARAM_READWRITE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_OFFSET,
         g_param_spec_double("offset",
                             "Offset",
                             "Offset of the line start in physical units.",
                             -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                             G_PARAM_READWRITE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_UNIT_X,
         g_param_spec_object("unit-x",
                             "X unit",
                             "Physical units of lateral dimension of the "
                             "line.",
                             GWY_TYPE_UNIT,
                             G_PARAM_READABLE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_UNIT_Y,
         g_param_spec_object("unit-y",
                             "Y unit",
                             "Physical units of line values.",
                             GWY_TYPE_UNIT,
                             G_PARAM_READABLE | STATICP));

    /**
     * GwyLine::data-changed:
     * @gwyline: The #GwyLine which received the signal.
     *
     * The ::data-changed signal is emitted whenever line data changes.
     **/
    line_signals[DATA_CHANGED]
        = g_signal_new_class_handler("data-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_line_init(GwyLine *line)
{
    line->priv = G_TYPE_INSTANCE_GET_PRIVATE(line, GWY_TYPE_LINE, Line);
    line->res = 1;
    line->real = 1.0;
    line->data = g_slice_new(gdouble);
}

static void
alloc_data(GwyLine *line,
           gboolean clear)
{
    if (clear)
        line->data = g_new0(gdouble, line->res);
    else
        line->data = g_new(gdouble, line->res);
    line->priv->allocated = TRUE;
}

static void
free_data(GwyLine *line)
{
    if (line->priv->allocated)
        GWY_FREE(line->data);
    else
        GWY_SLICE_FREE(gdouble, line->data);
}

static void
gwy_line_finalize(GObject *object)
{
    GwyLine *line = GWY_LINE(object);
    free_data(line);
    G_OBJECT_CLASS(gwy_line_parent_class)->finalize(object);
}

static void
gwy_line_dispose(GObject *object)
{
    GwyLine *line = GWY_LINE(object);
    GWY_OBJECT_UNREF(line->priv->unit_x);
    GWY_OBJECT_UNREF(line->priv->unit_y);
    G_OBJECT_CLASS(gwy_line_parent_class)->dispose(object);
}

static gsize
gwy_line_n_items(GwySerializable *serializable)
{
    GwyLine *line = GWY_LINE(serializable);
    Line *priv = line->priv;
    gsize n = N_ITEMS;
    if (priv->unit_x)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_x));
    if (priv->unit_y)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_y));
    return n;
}

static gsize
gwy_line_itemize(GwySerializable *serializable,
                 GwySerializableItems *items)
{
    GwyLine *line = GWY_LINE(serializable);
    Line *priv = line->priv;
    GwySerializableItem it;
    guint n = 0;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    it = serialize_items[0];
    it.value.v_double = line->real;
    items->items[items->n++] = it;
    n++;

    if (line->off) {
        it = serialize_items[1];
        it.value.v_double = line->off;
        items->items[items->n++] = it;
        n++;
    }

    if (priv->unit_x) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[2];
        it.value.v_object = (GObject*)priv->unit_x;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_x), items);
        n++;
    }

    if (priv->unit_y) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[3];
        it.value.v_object = (GObject*)priv->unit_y;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_y), items);
        n++;
    }

    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[4];
    it.value.v_double_array = line->data;
    it.array_size = line->res;
    items->items[items->n++] = it;
    n++;

    return n;
}

static gboolean
gwy_line_construct(GwySerializable *serializable,
                   GwySerializableItems *items,
                   GwyErrorList **error_list)
{
    GwyLine *line = GWY_LINE(serializable);
    Line *priv = line->priv;

    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    its[0].value.v_double = line->real;
    its[1].value.v_double = line->off;
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyLine", error_list);

    if (!_gwy_check_object_component(its + 2, line, GWY_TYPE_UNIT, error_list))
        goto fail;
    if (!_gwy_check_object_component(its + 3, line, GWY_TYPE_UNIT, error_list))
        goto fail;

    if (G_UNLIKELY(!its[4].array_size)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Line contains no data."));
        goto fail;
    }

    line->res = its[4].array_size;
    // FIXME: Catch near-zero and near-infinity values.
    line->real = CLAMP(its[0].value.v_double, G_MINDOUBLE, G_MAXDOUBLE);
    line->off = CLAMP(its[1].value.v_double, -G_MAXDOUBLE, G_MAXDOUBLE);
    priv->unit_x = (GwyUnit*)its[2].value.v_object;
    priv->unit_y = (GwyUnit*)its[3].value.v_object;
    free_data(line);
    line->data = its[4].value.v_double_array;
    priv->allocated = TRUE;

    return TRUE;

fail:
    GWY_OBJECT_UNREF(its[2].value.v_object);
    GWY_OBJECT_UNREF(its[3].value.v_object);
    GWY_FREE(its[4].value.v_double_array);
    return FALSE;
}

static GObject*
gwy_line_duplicate_impl(GwySerializable *serializable)
{
    GwyLine *line = GWY_LINE(serializable);
    GwyLine *duplicate = gwy_line_new_alike(line, FALSE);
    gwy_assign(duplicate->data, line->data, line->res);
    return G_OBJECT(duplicate);
}

static void
copy_info(GwyLine *dest,
          const GwyLine *src)
{
    dest->res = src->res;
    dest->real = src->real;
    dest->off = src->off;
    Line *dpriv = dest->priv, *spriv = src->priv;
    ASSIGN_UNITS(dpriv->unit_x, spriv->unit_x);
    ASSIGN_UNITS(dpriv->unit_y, spriv->unit_y);
}

static void
gwy_line_assign_impl(GwySerializable *destination,
                     GwySerializable *source)
{
    GwyLine *dest = GWY_LINE(destination);
    GwyLine *src = GWY_LINE(source);

    const gchar *notify[N_PROPS];
    guint nn = 0;
    if (dest->res != src->res)
        notify[nn++] = "res";
    if (dest->real != src->real)
        notify[nn++] = "real";
    if (dest->off != src->off)
        notify[nn++] = "offset";

    if (dest->res != src->res) {
        free_data(dest);
        dest->data = g_new(gdouble, src->res);
        dest->priv->allocated = TRUE;
    }
    gwy_assign(dest->data, src->data, src->res);
    copy_info(dest, src);
    _gwy_notify_properties(G_OBJECT(dest), notify, nn);
}

static void
gwy_line_set_property(GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
    GwyLine *line = GWY_LINE(object);

    switch (prop_id) {
        case PROP_REAL:
        line->real = g_value_get_double(value);
        break;

        case PROP_OFFSET:
        line->off = g_value_get_double(value);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_line_get_property(GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
    GwyLine *line = GWY_LINE(object);
    Line *priv = line->priv;

    switch (prop_id) {
        case PROP_RES:
        g_value_set_uint(value, line->res);
        break;

        case PROP_REAL:
        g_value_set_double(value, line->real);
        break;

        case PROP_OFFSET:
        g_value_set_double(value, line->off);
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
 * gwy_line_new:
 *
 * Creates a new one-dimensional data line.
 *
 * The line dimensions will be 1×1 and it will be zero-filled.  This
 * parameterless constructor exists mainly for language bindings,
 * gwy_line_new_sized() and gwy_line_new_alike() are usually more useful.
 *
 * Returns: A new one-dimensional data line.
 **/
GwyLine*
gwy_line_new(void)
{
    return g_object_newv(GWY_TYPE_LINE, 0, NULL);
}

/**
 * gwy_line_new_sized:
 * @res: Line resolution (width).
 * @clear: %TRUE to fill the new line data with zeroes, %FALSE to leave it
 *         uninitialised.
 *
 * Creates a new one-dimensional data line of specified dimension.
 *
 * Returns: A new one-dimensional data line.
 **/
GwyLine*
gwy_line_new_sized(guint res,
                   gboolean clear)
{
    g_return_val_if_fail(res, NULL);

    GwyLine *line = g_object_newv(GWY_TYPE_LINE, 0, NULL);
    free_data(line);
    line->res = res;
    alloc_data(line, clear);
    return line;
}

/**
 * gwy_line_new_alike:
 * @model: A one-dimensional data line to use as the template.
 * @clear: %TRUE to fill the new line data with zeroes, %FALSE to leave it
 *         uninitialised.
 *
 * Creates a new one-dimensional data line similar to another line.
 *
 * All properties of the newly created line will be identical to @model,
 * except the data that will be either zeroes or uninitialised.  Use
 * gwy_line_duplicate() to completely duplicate a line including data.
 *
 * Returns: A new one-dimensional data line.
 **/
GwyLine*
gwy_line_new_alike(const GwyLine *model,
                   gboolean clear)
{
    g_return_val_if_fail(GWY_IS_LINE(model), NULL);
    GwyLine *line = gwy_line_new_sized(model->res, clear);
    copy_info(line, model);
    return line;
}

/**
 * gwy_line_new_part:
 * @line: A one-dimensional data line.
 * @pos: Position of the line part start.
 * @len: Part length (number of items).
 * @keep_offset: %TRUE to set the offset of the new line
 *                using @pos and @line offsets.  %FALSE to set the offset
 *                of the new line to zero.
 *
 * Creates a new one-dimensional line as a part of another line.
 *
 * The block of length @len starting at @pos must be entirely contained in
 * @line.  The dimension must be non-zero.
 *
 * Data are physically copied, i.e. changing the new line data does not change
 * @line's data and vice versa.  Physical dimensions of the new line are
 * calculated to correspond to the extracted part.
 *
 * Returns: A new one-dimensional data line.
 **/
GwyLine*
gwy_line_new_part(const GwyLine *line,
                  guint pos,
                  guint len,
                  gboolean keep_offset)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);
    g_return_val_if_fail(len, NULL);
    g_return_val_if_fail(pos + len <= line->res, NULL);

    GwyLine *part;

    if (len == line->res) {
        part = gwy_line_duplicate(line);
        if (!keep_offset)
            part->off = 0.0;
        return part;
    }

    part = gwy_line_new_sized(len, FALSE);
    gwy_line_copy(line, pos, len, part, 0);
    part->real = line->real*len/line->res;
    ASSIGN_UNITS(part->priv->unit_x, line->priv->unit_x);
    ASSIGN_UNITS(part->priv->unit_y, line->priv->unit_y);
    if (keep_offset)
        part->off = line->off + line->real*pos/line->res;
    return part;
}

/**
 * gwy_line_new_resampled:
 * @line: A one-dimensional data line.
 * @res: Desired resolution.
 * @interpolation: Interpolation method to use.
 *
 * Creates a new one-dimensional data line by resampling another line.
 *
 * Returns: A new one-dimensional data line.
 **/
GwyLine*
gwy_line_new_resampled(const GwyLine *line,
                       guint res,
                       GwyInterpolationType interpolation)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);
    g_return_val_if_fail(res, NULL);
    if (res == line->res)
        return gwy_line_duplicate(line);

    GwyLine *dest;
    dest = gwy_line_new_sized(res, FALSE);
    copy_info(dest, line);
    dest->res = res;  // Undo
    gwy_interpolation_resample_block_1d(line->res, line->data,
                                        dest->res, dest->data,
                                        interpolation, TRUE);

    return dest;
}

/**
 * gwy_line_set_size:
 * @line: A one-dimensional data line.
 * @res: Desired resolution.
 * @clear: %TRUE to fill the new line data with zeroes, %FALSE to leave it
 *         uninitialised.
 *
 * Resizes a one-dimensional data line.
 *
 * If the new data size differs from the old data size this method is only
 * marginally more efficient than destroying the old line and creating a new
 * one.
 *
 * In no case the original data are preserved, not even if @res is equal to the
 * current line resolution.  Use gwy_line_new_part() to extract a part of a
 * line into a new line.  Only the dimensions are changed; all other properies,
 * such as physical dimensions, offsets and units, are kept.
 **/
void
gwy_line_set_size(GwyLine *line,
                  guint res,
                  gboolean clear)
{
    g_return_if_fail(GWY_IS_LINE(line));
    g_return_if_fail(res);

    if (line->res != res) {
        free_data(line);
        line->res = res;
        alloc_data(line, clear);
        g_object_notify(G_OBJECT(line), "res");
    }
    else if (clear)
        gwy_line_clear_full(line);
}

/**
 * gwy_line_new_from_curve:
 * @curve: A curve.  It must have at least one point.
 * @res: Required line resolution.  Pass 0 to chose a resolution automatically.
 *
 * Creates a one-dimensional data line from a curve.
 *
 * If the curve has at least two points with different abscissa values, they
 * are equidistant and the requested number of points matches the @curve's
 * number of points, then one-to-one data point mapping can be used and the
 * conversion will be information-preserving.  Otherwise linear interpolation
 * is used.
 *
 * Returns: A new one-dimensional data line.
 **/
GwyLine*
gwy_line_new_from_curve(const GwyCurve *curve,
                        guint res)
{
    g_return_val_if_fail(GWY_IS_CURVE(curve), NULL);
    g_return_val_if_fail(curve->n, NULL);

    guint last = curve->n - 1;
    const GwyXY *cdata = curve->data;
    gdouble length = cdata[last].x - cdata[0].x;
    GwyLine *line;

    if (length) {
        if (!res) {
            gdouble mdx = gwy_curve_median_dx(curve);
            res = mdx ? gwy_round(length/mdx) + 1 : curve->n;
            res = MIN(res, 4*curve->n);
        }
        line = gwy_line_new_sized(res, FALSE);
        gdouble dx = length/(res - 1);
        gdouble *ldata = line->data;
        line->off = cdata[0].x - dx/2.0;
        line->real = res*dx;

        *(ldata++) = cdata[0].y;
        guint j = 0;
        for (guint i = 1; i < res-1; i++) {
            gdouble x = i*dx + cdata[0].x;
            while (j+1 < res && cdata[j+1].x < x)
                j++;
            gdouble r = (x - cdata[j].x)/(cdata[j+1].x - cdata[j].x);
            *(ldata++) = r*cdata[j+1].y + (1.0 - r)*cdata[j].y;
        }
        *ldata = cdata[last].y;
    }
    else {
        // Special case, usually just one point but possibly a degenerate curve.
        res = MAX(res, 1);
        line = gwy_line_new_sized(res, FALSE);
        gwy_line_fill_full(line, gwy_curve_mean(curve));

        gdouble x = curve->data[0].x;
        if (x)
            line->real = 2*x;
        else {
            line->real = 2.0;
            line->off = -1.0;
        }
    }

    ASSIGN_UNITS(line->priv->unit_x, curve->priv->unit_x);
    ASSIGN_UNITS(line->priv->unit_y, curve->priv->unit_y);

    return line;
}

/**
 * gwy_line_data_changed:
 * @line: A one-dimensional data line.
 *
 * Emits signal GwyLine::data-changed on a line.
 **/
void
gwy_line_data_changed(GwyLine *line)
{
    g_return_if_fail(GWY_IS_LINE(line));
    g_signal_emit(line, line_signals[DATA_CHANGED], 0);
}

/**
 * gwy_line_copy_full:
 * @src: Source one-dimensional data line.
 * @dest: Destination one-dimensional data line.
 *
 * Copies the entire data from one line to another.
 *
 * The two lines must be of identical dimensions.
 *
 * Only the data are copied.  To make a line completely identical to another,
 * including units, offsets and change of dimensions, you can use
 * gwy_line_assign().
 **/
void
gwy_line_copy_full(const GwyLine *src,
                   GwyLine *dest)
{
    g_return_if_fail(GWY_IS_LINE(src));
    g_return_if_fail(GWY_IS_LINE(dest));
    g_return_if_fail(dest->res == src->res);
    gwy_assign(dest->data, src->data, src->res);
}

/**
 * gwy_line_copy:
 * @src: Source one-dimensional data data line.
 * @pos: Position of the line part start.
 * @len: Part length (number of items).
 * @dest: Destination one-dimensional data line.
 * @destpos: Destination position in @dest.
 *
 * Copies data from one line to another.
 *
 * The copied block starts at @pos in @src and its lenght is @len.  It is
 * copied to @dest starting from @destpos.
 *
 * There are no limitations on the indices or dimensions.  Only the part of the
 * block that is corrsponds to data inside @src and @dest is copied.  This can
 * also mean nothing is copied at all.
 *
 * If @src is equal to @dest the areas may <emphasis>not</emphasis> overlap.
 **/
void
gwy_line_copy(const GwyLine *src,
              guint pos,
              guint len,
              GwyLine *dest,
              guint destpos)
{
    g_return_if_fail(GWY_IS_LINE(src));
    g_return_if_fail(GWY_IS_LINE(dest));

    if (pos >= src->res || destpos >= dest->res)
        return;

    len = MIN(len, src->res - pos);
    len = MIN(len, dest->res - destpos);
    if (!len)
        return;

    if (src == dest && OVERLAPPING(pos, len, destpos, len)) {
        g_warning("Source and destination blocks overlap.  "
                  "Data corruption is imminent.");
    }

    gwy_assign(dest->data + destpos, src->data + pos, len);
}

/**
 * gwy_line_set_real:
 * @line: A one-dimensional data line.
 * @real: Length in physical units.
 *
 * Sets the physical lenght of a one-dimensional data line.
 **/
void
gwy_line_set_real(GwyLine *line,
                  gdouble real)
{
    g_return_if_fail(GWY_IS_LINE(line));
    g_return_if_fail(real > 0.0);
    if (real != line->real) {
        line->real = real;
        g_object_notify(G_OBJECT(line), "real");
    }
}

/**
 * gwy_line_set_offset:
 * @line: A one-dimensional data line.
 * @offset: Offset of the line start from 0 in physical units.
 *
 * Sets the offset of a one-dimensional data line.
 **/
void
gwy_line_set_offset(GwyLine *line,
                    gdouble offset)
{
    g_return_if_fail(GWY_IS_LINE(line));
    if (offset != line->off) {
        line->off = offset;
        g_object_notify(G_OBJECT(line), "offset");
    }
}

/**
 * gwy_line_get_unit_x:
 * @line: A one-dimensional data line.
 *
 * Obtains the lateral units of a one-dimensional data line.
 *
 * Returns: (transfer none):
 *          The lateral units of @line.
 **/
GwyUnit*
gwy_line_get_unit_x(GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);
    Line *priv = line->priv;
    if (!priv->unit_x)
        priv->unit_x = gwy_unit_new();
    return priv->unit_x;
}

/**
 * gwy_line_get_unit_y:
 * @line: A one-dimensional data line.
 *
 * Obtains the value units of a one-dimensional data line.
 *
 * Returns: (transfer none):
 *          The value units of @line.
 **/
GwyUnit*
gwy_line_get_unit_y(GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);
    Line *priv = line->priv;
    if (!priv->unit_y)
        priv->unit_y = gwy_unit_new();
    return priv->unit_y;
}

/**
 * gwy_line_format_x:
 * @line: A one-dimensional data line.
 * @style: Output format style.
 *
 * Finds a suitable format for displaying coordinates in a data line.
 *
 * The created format has a sufficient precision to represent coordinates
 * of neighbour pixels as different values.
 *
 * Returns: A newly created value format.
 **/
GwyValueFormat*
gwy_line_format_x(GwyLine *line,
                 GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);
    gdouble max = MAX(line->real, fabs(line->real + line->off));
    gdouble unit = gwy_line_dx(line);
    return gwy_unit_format_with_resolution(gwy_line_get_unit_x(line),
                                           style, max, unit);
}

/**
 * gwy_line_format_y:
 * @line: A one-dimensional data line.
 * @style: Output format style.
 *
 * Finds a suitable format for displaying values in a data line.
 *
 * Returns: A newly created value format.
 **/
GwyValueFormat*
gwy_line_format_y(GwyLine *line,
                  GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);
    gdouble min, max;
    gwy_line_min_max(line, &min, &max);
    if (max == min) {
        max = ABS(max);
        min = 0.0;
    }
    return gwy_unit_format_with_digits(gwy_line_get_unit_y(line),
                                       style, max - min, 3);
}

/**
 * SECTION: line
 * @title: GwyLine
 * @short_description: One-dimensional data in regular grid
 *
 * #GwyLine represents one dimensional data in a regular grid.
 *
 * Data are stored in a flat array #GwyLine-struct.data of #gdouble values.
 **/

/**
 * GwyLine:
 * @res: Resolution, i.e. length in pixels.
 * @real: Length in physical units.
 * @off: Offset of the line start from 0 in physical units.
 * @data: Line data.  See the introductory section for details.
 *
 * Object representing one-dimensional data in a regular grid.
 *
 * The #GwyLine struct contains some public lines that can be directly
 * accessed for reading.  To set them, you must use the methods such as
 * gwy_line_set_real().
 **/

/**
 * GwyLineClass:
 *
 * Class of one-dimensional data lines.
 **/

/**
 * gwy_line_duplicate:
 * @line: A one-dimensional data line.
 *
 * Duplicates a one-dimensional data line.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_line_assign:
 * @dest: Destination one-dimensional data line.
 * @src: Source one-dimensional data line.
 *
 * Copies the value of a one-dimensional data line.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_line_index:
 * @line: A one-dimensional data line.
 * @pos: Position in @line.
 *
 * Accesses a one-dimensional data line pixel.
 *
 * This macro may evaluate its arguments several times.
 * This macro expands to a left-hand side expression.
 *
 * No argument validation is performed.  If you process the data in a loop,
 * you are encouraged to access #GwyLine-struct.data directly.
 **/

/**
 * gwy_line_dx:
 * @line: A one-dimensional data line.
 *
 * Calculates the pixel size in physical units.
 *
 * This macro may evaluate its arguments several times.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
