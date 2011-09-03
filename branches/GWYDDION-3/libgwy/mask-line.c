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

#include <glib/gi18n-lib.h>
#include "libgwy/object-utils.h"
#include "libgwy/serialize.h"
#include "libgwy/math.h"
#include "libgwy/mask-line.h"
#include "libgwy/line-internal.h"
#include "libgwy/object-internal.h"
#include "libgwy/mask-line-internal.h"

enum { N_ITEMS = 2 };

enum {
    DATA_CHANGED,
    N_SIGNALS
};

enum {
    PROP_0,
    PROP_RES,
    N_PROPS
};

static void     gwy_mask_line_finalize         (GObject *object);
static void     gwy_mask_line_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_mask_line_n_items          (GwySerializable *serializable);
static gsize    gwy_mask_line_itemize          (GwySerializable *serializable,
                                                 GwySerializableItems *items);
static void     gwy_mask_line_done             (GwySerializable *serializable);
static gboolean gwy_mask_line_construct        (GwySerializable *serializable,
                                                 GwySerializableItems *items,
                                                 GwyErrorList **error_list);
static GObject* gwy_mask_line_duplicate_impl   (GwySerializable *serializable);
static void     gwy_mask_line_assign_impl      (GwySerializable *destination,
                                                 GwySerializable *source);
static void     gwy_mask_line_set_property     (GObject *object,
                                                 guint prop_id,
                                                 const GValue *value,
                                                 GParamSpec *pspec);
static void     gwy_mask_line_get_property     (GObject *object,
                                                 guint prop_id,
                                                 GValue *value,
                                                 GParamSpec *pspec);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "res",  .ctype = GWY_SERIALIZABLE_INT32,       },
    /*1*/ { .name = "data", .ctype = GWY_SERIALIZABLE_INT32_ARRAY, },
};

static guint mask_line_signals[N_SIGNALS];
static GParamSpec *mask_line_pspecs[N_PROPS];

G_DEFINE_TYPE_EXTENDED
    (GwyMaskLine, gwy_mask_line, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_mask_line_serializable_init));

static void
gwy_mask_line_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_mask_line_n_items;
    iface->itemize   = gwy_mask_line_itemize;
    iface->done      = gwy_mask_line_done;
    iface->construct = gwy_mask_line_construct;
    iface->duplicate = gwy_mask_line_duplicate_impl;
    iface->assign    = gwy_mask_line_assign_impl;
}

static void
gwy_mask_line_class_init(GwyMaskLineClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(MaskLine));

    gobject_class->finalize = gwy_mask_line_finalize;
    gobject_class->get_property = gwy_mask_line_get_property;
    gobject_class->set_property = gwy_mask_line_set_property;

    mask_line_pspecs[PROP_RES]
        = g_param_spec_uint("res",
                            "Resolution",
                            "Pixel length of the line.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, mask_line_pspecs[i]);

    /**
     * GwyMaskLine::data-changed:
     * @gwymaskline: The #GwyMaskLine which received the signal.
     * @arg1: (allow-none):
     *        Segment in @gwymaskline that has changed.
     *        It may be %NULL, meaning the entire mask line.
     *
     * The ::data-changed signal is emitted when mask line data changes.
     * More precisely, #GwyMaskLine itself never emits this signal.  You can
     * emit it explicitly with gwy_mask_line_data_changed() to notify anything
     * that displays (or otherwise uses) the mask line.
     **/
    mask_line_signals[DATA_CHANGED]
        = g_signal_new_class_handler("data-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__BOXED,
                                     G_TYPE_NONE, 1,
                                     GWY_TYPE_LINE_PART);
}

static void
alloc_data(GwyMaskLine *line,
           gboolean clear)
{
    guint n = stride_for_width(line->res);
    if (clear)
        line->data = g_new0(guint32, n);
    else
        line->data = g_new(guint32, n);
    line->priv->stride = n;
    line->priv->allocated = TRUE;
}

static void
free_data(GwyMaskLine *line)
{
    if (line->priv->allocated)
        GWY_FREE(line->data);
    else
        line->data = NULL;
}

static void
gwy_mask_line_init(GwyMaskLine *line)
{
    line->priv = G_TYPE_INSTANCE_GET_PRIVATE(line,
                                             GWY_TYPE_MASK_LINE,
                                             MaskLine);
    line->res = 1;
    line->priv->stride = stride_for_width(line->res);
    line->data = &line->priv->storage;
}

static void
gwy_mask_line_finalize(GObject *object)
{
    GwyMaskLine *line = GWY_MASK_LINE(object);
    free_data(line);
    G_OBJECT_CLASS(gwy_mask_line_parent_class)->finalize(object);
}

static gsize
gwy_mask_line_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return N_ITEMS;
}

static gsize
gwy_mask_line_itemize(GwySerializable *serializable,
                       GwySerializableItems *items)
{
    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    GwyMaskLine *line = GWY_MASK_LINE(serializable);
    GwySerializableItem *it = items->items + items->n;
    gsize n = line->priv->stride;

    *it = serialize_items[0];
    it->value.v_uint32 = line->res;
    it++, items->n++;

    *it = serialize_items[1];
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        it->value.v_uint32_array = line->data;
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        MaskLine *priv = line->priv;
        priv->serialized_swapped = g_new(guint32, n);
        swap_bits_uint32_array(priv->serialized_swapped, n);
        it->value.v_uint32_array = priv->serialized_swapped;
    }
    it->array_size = n;
    it++, items->n++;

    return N_ITEMS;
}

static void
gwy_mask_line_done(GwySerializable *serializable)
{
    GwyMaskLine *line = GWY_MASK_LINE(serializable);
    GWY_FREE(line->priv->serialized_swapped);
}

static gboolean
gwy_mask_line_construct(GwySerializable *serializable,
                         GwySerializableItems *items,
                         GwyErrorList **error_list)
{
    GwyMaskLine *line = GWY_MASK_LINE(serializable);

    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyMaskLine", error_list);

    if (G_UNLIKELY(!its[0].value.v_uint32)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Mask line dimension %u is invalid."),
                           its[0].value.v_uint32);
        goto fail;
    }

    gsize n = stride_for_width(its[0].value.v_uint32);
    if (G_UNLIKELY(n != its[1].array_size)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Mask line dimension %u does not match "
                             "data size %lu."),
                           its[0].value.v_uint32, (gulong)its[1].array_size);
        goto fail;
    }

    free_data(line);
    line->res = its[0].value.v_uint32;
    line->priv->stride = stride_for_width(line->res);
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        line->data = its[1].value.v_uint32_array;
        its[1].value.v_uint32_array = NULL;
        its[1].array_size = 0;
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        line->data = g_memdup(its[1].value.v_uint32_array, n*sizeof(guint32));
        swap_bits_uint32_array(line->data, n);
    }
    line->priv->allocated = TRUE;

    return TRUE;

fail:
    GWY_FREE(its[1].value.v_uint32_array);
    return FALSE;
}

static GObject*
gwy_mask_line_duplicate_impl(GwySerializable *serializable)
{
    GwyMaskLine *line = GWY_MASK_LINE(serializable);
    GwyMaskLine *duplicate = gwy_mask_line_new_sized(line->res, FALSE);
    gwy_assign(duplicate->data, line->data, line->priv->stride);
    return G_OBJECT(duplicate);
}

static void
gwy_mask_line_assign_impl(GwySerializable *destination,
                          GwySerializable *source)
{
    GwyMaskLine *dest = GWY_MASK_LINE(destination);
    GwyMaskLine *src = GWY_MASK_LINE(source);

    GParamSpec *notify[N_PROPS];
    guint nn = 0;
    if (dest->res != src->res)
        notify[nn++] = mask_line_pspecs[PROP_RES];

    gsize n = src->priv->stride;
    if (dest->priv->stride != n) {
        free_data(dest);
        dest->data = g_new(guint32, n);
        dest->priv->stride = n;
        dest->priv->allocated = TRUE;
    }
    gwy_assign(dest->data, src->data, n);
    dest->res = src->res;
    _gwy_notify_properties_by_pspec(G_OBJECT(dest), notify, nn);
}

static void
gwy_mask_line_set_property(GObject *object,
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
gwy_mask_line_get_property(GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
    GwyMaskLine *line = GWY_MASK_LINE(object);

    switch (prop_id) {
        case PROP_RES:
        g_value_set_uint(value, line->res);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_mask_line_check_part:
 * @line: A one-dimensional mask line.
 * @lpart: (allow-none):
 *         Segment in @line, possibly %NULL.
 * @pos: Location to store the actual position of the part start.
 * @len: Location to store the actual length (number of items)
 *       of the part.
 *
 * Validates the position and length a mask line part.
 *
 * If @lpart is %NULL entire @line is to be used.  Otherwise @lpart must be
 * contained in @line.
 *
 * If the position and length are valid @pos and @len are set to the actual
 * part in @line.  If the function returns %FALSE their values are undefined.
 *
 * This function is typically used in functions that operate on a part of a
 * mask line.  Example (note gwy_mask_line_new_part() creates a new mask line
 * from a segment of another mask line):
 * |[
 * GwyMaskLine*
 * extract_mask_line_part(const GwyLine *line,
 *                        const GwyLinePart *lpart)
 * {
 *     guint pos, len;
 *     if (!gwy_mask_line_check_part(line, lpart, &pos, &len))
 *         return NULL;
 *
 *     // Extract the part of @line consisting of @len items from @pos...
 * }
 * ]|
 *
 * Returns: %TRUE if the position and length are valid and the caller
 *          should proceed.  %FALSE if the caller should not proceed, either
 *          because @line is not a #GwyMaskLine instance or the position or
 *          length is invalid (a critical error is emitted in these cases)
 *          or the actual part is zero-sized.
 **/
gboolean
gwy_mask_line_check_part(const GwyMaskLine *line,
                         const GwyLinePart *lpart,
                         guint *pos, guint *len)
{
    g_return_val_if_fail(GWY_IS_MASK_LINE(line), FALSE);
    if (lpart) {
        if (!lpart->len)
            return FALSE;
        // The two separate conditions are to catch integer overflows.
        g_return_val_if_fail(lpart->pos < line->res, FALSE);
        g_return_val_if_fail(lpart->len <= line->res - lpart->pos,
                             FALSE);
        *pos = lpart->pos;
        *len = lpart->len;
    }
    else {
        *pos = 0;
        *len = line->res;
    }

    return TRUE;
}

/**
 * gwy_mask_line_limit_parts:
 * @src: A source one-dimensional mask line.
 * @srcpart: (allow-none):
 *           Segment in @src, possibly %NULL.
 * @dest: A destination one-dimensional mask line.
 * @destpos: Column index for the upper-left corner of the part in @dest.
 * @pos: Location to store the actual position of the sourcr part start.
 * @len: Location to store the actual length (number of items)
 *       of the source part.
 *
 * Limits the length of a mask line part for copying.
 *
 * The segment is limited to make it contained both in @src and @dest and @pos
 * and @len are set to the actual position and length in @src.  If the function
 * returns %FALSE their values are undefined.
 *
 * If @src and @dest are the same line the source and destination parts should
 * not overlap.
 *
 * This function is typically used in copy-like functions that transfer a part
 * of a mask line into another mask line.
 * Example (note gwy_mask_line_copy() copies mask line parts):
 * |[
 * void
 * copy_mask_line(const GwyMaskLine *src,
 *                const GwyMaskLinePart *srcpart,
 *                GwyMaskLine *dest,
 *                guint destpos)
 * {
 *     guint pos, len;
 *     if (!gwy_mask_line_limit_parts(src, srcpart, dest, destpos,
 *                                    &pos, &len))
 *         return;
 *
 *     // Copy segment of length @len at @pos in @src to an equally-sized
 *     // segment at @destpos in @dest...
 * }
 * ]|
 *
 * Returns: %TRUE if the caller should proceed.  %FALSE if the caller should
 *          not proceed, either because @line or @target is not a #GwyMaskLine
 *          instance (a critical error is emitted in these cases) or the actual
 *          part is zero-sized.
 **/
gboolean
gwy_mask_line_limit_parts(const GwyMaskLine *src,
                          const GwyLinePart *srcpart,
                          const GwyMaskLine *dest,
                          guint destpos,
                          guint *pos, guint *len)
{
    g_return_val_if_fail(GWY_IS_MASK_LINE(src), FALSE);
    g_return_val_if_fail(GWY_IS_MASK_LINE(dest), FALSE);

    if (srcpart) {
        *pos = srcpart->pos;
        *len = srcpart->len;
        if (*pos >= src->res)
            return FALSE;
        *len = MIN(*len, src->res - *pos);
    }
    else {
        *pos = 0;
        *len = src->res;
    }

    if (destpos >= dest->res)
        return FALSE;

    *len = MIN(*len, dest->res - destpos);

    if (src == dest) {
        if (gwy_overlapping(*pos, *len, destpos, *len)) {
            g_warning("Source and destination blocks overlap.  "
                      "Data corruption is imminent.");
        }
    }

    return *len;
}

/**
 * gwy_mask_line_new:
 *
 * Creates a new one-dimensional mask line.
 *
 * The mask line dimension will be 1 and it will be zero-filled.  This
 * parameterless constructor exists mainly for language bindings,
 * gwy_mask_line_new_sized() is usually more useful.
 *
 * Returns: (transfer full):
 *          A new one-dimensional mask line.
 **/
GwyMaskLine*
gwy_mask_line_new(void)
{
    return g_object_newv(GWY_TYPE_MASK_LINE, 0, NULL);
}

/**
 * gwy_mask_line_new_sized:
 * @res: Line resolution (width).
 * @clear: %TRUE to fill the new mask line with zeroes, %FALSE to leave it
 *         uninitialised.
 *
 * Creates a new one-dimensional mask line of specified dimension.
 *
 * Returns: (transfer full):
 *          A new one-dimensional mask line.
 **/
GwyMaskLine*
gwy_mask_line_new_sized(guint res,
                        gboolean clear)
{
    g_return_val_if_fail(res, NULL);

    GwyMaskLine *line = g_object_newv(GWY_TYPE_MASK_LINE, 0, NULL);
    free_data(line);
    line->res = res;
    alloc_data(line, clear);
    return line;
}

/**
 * gwy_mask_line_new_part:
 * @line: A one-dimensional mask line.
 * @lpart: (allow-none):
 *         Segment in @line to extract to the new line.  Passing %NULL
 *         creates an identical copy of @line, similarly to
 *         gwy_mask_line_duplicate().
 *
 * Creates a new one-dimensional mask line as a part of another mask line.
 *
 * The part specified by @lpart must be entirely contained in @line.  The
 * dimension must be non-zero.
 *
 * Data are physically copied, i.e. changing the new mask line data does not
 * change @line's data and vice versa.
 *
 * Returns: (transfer full):
 *          A new one-dimensional mask line.
 **/
GwyMaskLine*
gwy_mask_line_new_part(const GwyMaskLine *line,
                       const GwyLinePart *lpart)
{
    guint pos, len;
    if (!gwy_mask_line_check_part(line, lpart, &pos, &len))
        return NULL;

    if (len == line->res)
        return gwy_mask_line_duplicate(line);

    GwyMaskLine *part = gwy_mask_line_new_sized(len, FALSE);
    gwy_mask_line_copy(line, lpart, part, 0);
    return part;
}

/**
 * _gwy_mask_prepare_scaling:
 * @pos: Initial position in the mask.  Can be non-integer.
 * @step: The size of one target pixel in the source (the inverse of zoom).
 * @nsteps: The number of pixels wanted for the target.
 * @required_bits: How many input bits it will consume.  This can be used for
 *                 verifying that the source is large enough.  All bits that
 *                 are used with nonzero weight are counted as consumed.
 *
 * Prepares auxiliary data for bit mask scaling.
 *
 * Returns: Array of scaling segment descriptors.
 **/
GwyMaskScalingSegment*
_gwy_mask_prepare_scaling(gdouble pos, gdouble step, guint nsteps,
                          guint *required_bits)
{
    GwyMaskScalingSegment *segments = g_new(GwyMaskScalingSegment, nsteps),
                          *seg = segments;
    guint end = floor(pos), first = end;
    gdouble x = pos - end;

    for (guint i = nsteps; i; i--, seg++) {
        guint begin = end;
        pos += step;
        end = floor(pos);
        if (end == begin) {
            seg->w0 = 1.0;
            x = pos - end;
            seg->w1 = 0.0;
            seg->move = 0;
        }
        else {
            seg->w0 = (1.0 - x)/step;
            x = pos - end;
            seg->w1 = x/step;
            seg->move = end - begin;
        }
    }

    // Try to avoid reading a slightly after the last bit.
    seg--;
    if (seg->move && seg->w1 < 1e-6) {
        seg->move--;
        seg->w1 = 1.0;
    }

    GWY_MAYBE_SET(required_bits, end+1 - first);

    return segments;
}

/**
 * gwy_mask_line_new_resampled:
 * @line: A one-dimensional mask line.
 * @res: Desired resolution.
 *
 * Creates a new one-dimensional mask line by resampling another mask line.
 *
 * Returns: (transfer full):
 *          A new one-dimensional mask line.
 **/
GwyMaskLine*
gwy_mask_line_new_resampled(const GwyMaskLine *line,
                            guint res)
{
    g_return_val_if_fail(GWY_IS_MASK_LINE(line), NULL);
    g_return_val_if_fail(res, NULL);
    if (res == line->res)
        return gwy_mask_line_duplicate(line);

    GwyMaskLine *dest;
    dest = gwy_mask_line_new_sized(res, FALSE);

    guint req_bits;
    gdouble step = line->res/(gdouble)res;
    GwyMaskScalingSegment *segments = _gwy_mask_prepare_scaling(0.0, step, res,
                                                                &req_bits);
    g_assert(req_bits == line->res);

    GwyMaskScalingSegment *seg = segments;
    GwyMaskIter srciter, destiter;
    gwy_mask_line_iter_init(line, srciter, 0);
    gwy_mask_line_iter_init(dest, destiter, 0);
    if (step > 1.0) {
        // seg->move is always nonzero.
        for (guint i = res; i; i--, seg++) {
            gdouble s = seg->w0 * !!gwy_mask_iter_get(srciter);
            guint c = 0;
            for (guint k = seg->move-1; k; k--) {
                gwy_mask_iter_next(srciter);
                c += !!gwy_mask_iter_get(srciter);
            }
            gwy_mask_iter_next(srciter);
            s += c/step + seg->w1 * !!gwy_mask_iter_get(srciter);
            gwy_mask_iter_set(destiter, s >= 0.5);
            gwy_mask_iter_next(destiter);
        }
    }
    else {
        // seg->move is at most 1.
        for (guint i = res; i; i--, seg++) {
            gdouble s = seg->w0 * !!gwy_mask_iter_get(srciter);
            if (seg->move) {
                gwy_mask_iter_next(srciter);
                s += seg->w1 * !!gwy_mask_iter_get(srciter);
            }
            gwy_mask_iter_set(destiter, s >= 0.5);
            gwy_mask_iter_next(destiter);
        }
    }

    g_free(segments);

    return dest;
}

/**
 * gwy_mask_line_set_size:
 * @line: A one-dimensional mask line.
 * @res: Desired resolution.
 * @clear: %TRUE to clear the new line, %FALSE to leave it uninitialised.
 *
 * Resizes a one-dimensional mask line.
 *
 * If the new data size differs from the old data size this method is only
 * marginally more efficient than destroying the old line and creating a new
 * one.
 *
 * In no case the original data are preserved, not even if @res is equal to the
 * current line dimension.  Use gwy_mask_line_new_part() to extract a part of
 * a line into a new line.
 **/
void
gwy_mask_line_set_size(GwyMaskLine *line,
                       guint res,
                       gboolean clear)
{
    g_return_if_fail(GWY_IS_MASK_LINE(line));
    g_return_if_fail(res);

    GParamSpec *notify[N_PROPS];
    guint nn = 0;
    guint stride = stride_for_width(res);
    if (line->res != res)
        notify[nn++] = mask_line_pspecs[PROP_RES];

    if (line->priv->stride != stride) {
        free_data(line);
        line->res = res;
        alloc_data(line, clear);
    }
    else {
        line->res = res;
        line->priv->stride = stride;
        gwy_clear(line->data, stride);
    }
    gwy_mask_line_invalidate(line);
    _gwy_notify_properties_by_pspec(G_OBJECT(line), notify, nn);
}

/**
 * gwy_mask_line_data_changed:
 * @line: A one-dimensional mask line.
 * @lpart: (allow-none):
 *         Segment in @line that has changed.  Passing %NULL means the entire
 *         mask line.
 *
 * Emits signal GwyMaskLine::data-changed on a mask line.
 **/
void
gwy_mask_line_data_changed(GwyMaskLine *line,
                           GwyLinePart *lpart)
{
    g_return_if_fail(GWY_IS_MASK_LINE(line));
    g_signal_emit(line, mask_line_signals[DATA_CHANGED], 0, lpart);
}

/**
 * gwy_mask_line_invalidate:
 * @line: A one-dimensional mask line.
 *
 * Invalidates mask line grain data.
 *
 * User code should seldom need this method since all #GwyMaskLine methods
 * correctly invalidate cached data when they change the mask.
 **/
void
gwy_mask_line_invalidate(GwyMaskLine *line)
{
    g_return_if_fail(GWY_IS_MASK_LINE(line));
}

/**
 * gwy_mask_line_count:
 * @line: A one-dimensional mask line.
 * @mask: A one-dimensional mask line determining to which bits of @line
 *        consider.  If it is %NULL entire @line is evaluated (as if
 *        all bits in @mask were set).
 * @value: %TRUE to count ones, %FALSE to count zeroes.
 *
 * Counts set or unset bits in a mask line.
 *
 * Returns: The number of bits equal to @value.
 **/
guint
gwy_mask_line_count(const GwyMaskLine *line,
                    const GwyMaskLine *mask,
                    gboolean value)
{
    g_return_val_if_fail(GWY_IS_MASK_LINE(line), 0);
    if (!mask)
        return gwy_mask_line_part_count(line, NULL, value);

    g_return_val_if_fail(GWY_IS_MASK_LINE(mask), 0);
    g_return_val_if_fail(line->res == mask->res, 0);

    const guint end = line->res & 0x1f;
    const guint32 m0 = MAKE_MASK(0, end);
    guint count = 0;

    const guint32 *m = mask->data;
    const guint32 *p = line->data;
    if (value) {
        for (guint j = line->res >> 5; j; j--, p++, m++)
            count += count_set_bits(*p & *m);
        if (end)
            count += count_set_bits(*p & *m & m0);
    }
    else {
        for (guint j = line->res >> 5; j; j--, p++, m++)
            count += count_set_bits(~*p & *m);
        if (end)
            count += count_set_bits(~*p & *m & m0);
    }

    return count;
}

/**
 * gwy_mask_line_part_count:
 * @line: A one-dimensional mask line.
 * @lpart: (allow-none):
 *         Segment in @line to process.  Pass %NULL to process entire @line.
 * @value: %TRUE to count ones, %FALSE to count zeroes.
 *
 * Counts set or unset bits in a mask line.
 *
 * Returns: The number of bits equal to @value.
 **/
guint
gwy_mask_line_part_count(const GwyMaskLine *line,
                         const GwyLinePart *lpart,
                         gboolean value)
{
    guint pos, len;
    if (!gwy_mask_line_check_part(line, lpart, &pos, &len))
        return 0;

    guint32 *base = line->data + (pos >> 5);
    const guint off = pos & 0x1f;
    const guint end = (pos + len) & 0x1f;
    guint count;
    if (len <= 0x20 - off) {
        const guint32 m = MAKE_MASK(off, len);
        count = count_row_single(base, m, value);
    }
    else {
        const guint32 m0 = MAKE_MASK(off, 0x20 - off);
        const guint32 m1 = MAKE_MASK(0, end);
        count = count_row(base, len, off, end, m0, m1, value);
    }
    return count;
}

/**
 * gwy_mask_line_get:
 * @line: A one-dimensional mask line.
 * @pos: Index in @line.
 *
 * Obtains one bit value from a one-dimensional mask line.
 *
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
 *
 * For language bindings, this macro is also provided as a (much slower)
 * function.
 *
 * Returns: Nonzero value (not necessarily 1) if the bit is set, zero if it's
 *          unset.
 **/
#undef gwy_mask_line_get
gboolean
gwy_mask_line_get(const GwyMaskLine *line,
                  guint pos)
{
    g_return_val_if_fail(GWY_IS_MASK_LINE(line), 0);
    g_return_val_if_fail(pos < line->res, 0);
    return _gwy_mask_line_get(line, pos);
}

/**
 * gwy_mask_line_set:
 * @line: A one-dimensional mask line.
 * @pos: Index in @line.
 * @value: Nonzero value to set the bit, zero to clear it.
 *
 * Sets one bit value in a one-dimensional mask line.
 *
 * This is a low-level macro and it does not invalidate the mask line.
 *
 * This macro may evaluate its arguments several times except for @value
 * which is evaluated only once.
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
 *
 * For language bindings, this macro is also provided as a (much slower)
 * function.
 **/
#undef gwy_mask_line_set
void
gwy_mask_line_set(const GwyMaskLine *line,
                  guint pos,
                  gboolean value)
{
    g_return_if_fail(GWY_IS_MASK_LINE(line));
    g_return_if_fail(pos < line->res);
    _gwy_mask_line_set(line, pos, value);
}

/**
 * SECTION: mask-line
 * @title: GwyMaskLine
 * @short_description: One-dimensional bit mask
 *
 * #GwyMaskLine represents one-dimensional bit mask that can be used to specify
 * subsets either of one-dimensional data such as #GwyLine or list-like data
 * such as #GwySurface whatever the actual dimension of the space is.
 *
 * The mask is stored in a flat array @data in #GwyMaskLine-struct as #guint32
 * values.
 *
 * The bit order is native, i.e. on little-endian machines the lowest bit is
 * first, whereas on big-endian machines the highest bit is first.  This
 * convention matches the %CAIRO_FORMAT_A1 format and makes the masks directly
 * usable for Cairo drawing operations.
 *
 * Individual bits in the mask can be get and set with macros
 * gwy_mask_line_get() and gwy_mask_line_set().  These macros are relatively
 * slow and they are suitable for casual use or when the amount of work
 * otherwise done per pixel makes their cost negligible.  For the common
 * sequential access, the mask iterator #GwyMaskIter macros offers a good
 * compromise between complexity and efficiency.  In a really
 * performance-sensitive code, you also might wish to access the bits in
 * @data in #GwyMaskLine directly.
 **/

/**
 * GwyMaskLine:
 * @res: Resolution, i.e. length in pixels.
 * @data: Mask data.  See the introductory section for details.
 *
 * Object representing one-dimensional bit mask.
 *
 * The #GwyMaskLine struct contains some public fields that can be
 * directly accessed for reading.  To set them, you must use the mask line
 * methods.
 **/

/**
 * GwyMaskLineClass:
 *
 * Class of one-dimensional bit masks.
 **/

/**
 * gwy_mask_line_duplicate:
 * @line: A one-dimensional mask line.
 *
 * Duplicates a one-dimensional mask line.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_mask_line_assign:
 * @dest: Destination one-dimensional mask line.
 * @src: Source one-dimensional mask line.
 *
 * Copies the value of a one-dimensional mask line.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_mask_line_iter_init:
 * @line: A one-dimensional mask line.
 * @iter: Mask iterator to initialise.  It must be an identifier.
 * @pos: Index in @line.
 *
 * Initialises a mask iterator to point to given pixel in a mask line.
 *
 * The iterator can be subsequently used to move back and forward within the
 * the line.
 *
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
