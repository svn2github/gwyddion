/*
 *  $Id$
 *  Copyright (C) 2009-2012 David Nečas (Yeti).
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
#include "libgwy/mask-field.h"
#include "libgwy/object-internal.h"
#include "libgwy/mask-field-internal.h"
#include "libgwy/field-internal.h"

enum { N_ITEMS = 4 };

enum {
    SGNL_DATA_CHANGED,
    N_SIGNALS
};

enum {
    PROP_0,
    PROP_XRES,
    PROP_YRES,
    PROP_STRIDE,
    PROP_NAME,
    N_PROPS
};

static void     gwy_mask_field_finalize         (GObject *object);
static void     gwy_mask_field_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_mask_field_n_items          (GwySerializable *serializable);
static gsize    gwy_mask_field_itemize          (GwySerializable *serializable,
                                                 GwySerializableItems *items);
static void     gwy_mask_field_done             (GwySerializable *serializable);
static gboolean gwy_mask_field_construct        (GwySerializable *serializable,
                                                 GwySerializableItems *items,
                                                 GwyErrorList **error_list);
static GObject* gwy_mask_field_duplicate_impl   (GwySerializable *serializable);
static void     gwy_mask_field_assign_impl      (GwySerializable *destination,
                                                 GwySerializable *source);
static void     gwy_mask_field_set_property     (GObject *object,
                                                 guint prop_id,
                                                 const GValue *value,
                                                 GParamSpec *pspec);
static void     gwy_mask_field_get_property     (GObject *object,
                                                 guint prop_id,
                                                 GValue *value,
                                                 GParamSpec *pspec);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "xres", .ctype = GWY_SERIALIZABLE_INT32,       .value.v_uint32 = 1 },
    /*1*/ { .name = "yres", .ctype = GWY_SERIALIZABLE_INT32,       .value.v_uint32 = 1 },
    /*2*/ { .name = "name", .ctype = GWY_SERIALIZABLE_STRING,      },
    /*3*/ { .name = "data", .ctype = GWY_SERIALIZABLE_INT32_ARRAY, },
};

static guint signals[N_SIGNALS];
static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE_EXTENDED
    (GwyMaskField, gwy_mask_field, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_mask_field_serializable_init));

static void
gwy_mask_field_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_mask_field_n_items;
    iface->itemize   = gwy_mask_field_itemize;
    iface->done      = gwy_mask_field_done;
    iface->construct = gwy_mask_field_construct;
    iface->duplicate = gwy_mask_field_duplicate_impl;
    iface->assign    = gwy_mask_field_assign_impl;
}

static void
gwy_mask_field_class_init(GwyMaskFieldClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(MaskField));

    gobject_class->finalize = gwy_mask_field_finalize;
    gobject_class->get_property = gwy_mask_field_get_property;
    gobject_class->set_property = gwy_mask_field_set_property;

    properties[PROP_XRES]
        = g_param_spec_uint("x-res",
                            "X resolution",
                            "Pixel width of the mask field.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_YRES]
        = g_param_spec_uint("y-res",
                            "Y resolution",
                            "Pixel height of the mask field.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_STRIDE]
        = g_param_spec_uint("stride",
                            "Stride",
                            "Row stride of the mask field in items, i.e. "
                            "guint32s.",
                            1, G_MAXUINT, 2,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_NAME]
        = g_param_spec_string("name",
                              "Name",
                              "Name of the field.",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    /**
     * GwyMaskField::data-changed:
     * @gwymaskfield: The #GwyMaskField which received the signal.
     * @arg1: (allow-none):
     *        Area in @gwymaskfield that has changed.
     *        It may be %NULL, meaning the entire field.
     *
     * The ::data-changed signal is emitted when mask field data changes.  More
     * precisely, #GwyMaskField itself never emits this signal.  You can emit
     * it explicitly with gwy_mask_field_data_changed() to notify anything that
     * displays (or otherwise uses) the mask field.
     **/
    signals[SGNL_DATA_CHANGED]
        = g_signal_new_class_handler("data-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__BOXED,
                                     G_TYPE_NONE, 1,
                                     GWY_TYPE_FIELD_PART);
}

static void
alloc_data(GwyMaskField *field,
           gboolean clear)
{
    field->stride = stride_for_width(field->xres);
    if (clear)
        field->data = g_new0(guint32, field->stride * field->yres);
    else
        field->data = g_new(guint32, field->stride * field->yres);
    field->priv->allocated = TRUE;
}

static void
free_data(GwyMaskField *field)
{
    if (field->priv->allocated)
        GWY_FREE(field->data);
    else
        field->data = NULL;
}

static void
free_caches(GwyMaskField *field)
{
    MaskField *priv = field->priv;
    GWY_FREE(priv->grains);
    GWY_FREE(priv->distances);
    GWY_FREE(priv->grain_sizes);
    GWY_FREE(priv->grain_bounding_boxes);
    GWY_FREE(priv->grain_positions);
}

static void
gwy_mask_field_init(GwyMaskField *field)
{
    field->priv = G_TYPE_INSTANCE_GET_PRIVATE(field,
                                              GWY_TYPE_MASK_FIELD,
                                              MaskField);
    field->xres = field->yres = 1;
    field->stride = stride_for_width(field->xres);
    field->data = &field->priv->storage;
}

static void
gwy_mask_field_finalize(GObject *object)
{
    GwyMaskField *field = GWY_MASK_FIELD(object);
    GWY_FREE(field->priv->name);
    free_data(field);
    free_caches(field);
    G_OBJECT_CLASS(gwy_mask_field_parent_class)->finalize(object);
}

static gsize
gwy_mask_field_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return N_ITEMS;
}

static gsize
gwy_mask_field_itemize(GwySerializable *serializable,
                       GwySerializableItems *items)
{
    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    GwyMaskField *field = GWY_MASK_FIELD(serializable);
    MaskField *priv = field->priv;
    GwySerializableItem it;
    gsize n32 = field->stride * field->yres;
    guint n = 0;

    it = serialize_items[0];
    it.value.v_uint32 = field->xres;
    items->items[items->n++] = it;
    n++;

    it = serialize_items[1];
    it.value.v_uint32 = field->yres;
    items->items[items->n++] = it;
    n++;

    _gwy_serialize_string(priv->name, serialize_items + 2, items, &n);

    it = serialize_items[3];
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        it.value.v_uint32_array = field->data;
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        priv->serialized_swapped = g_memdup(field->data, n32*sizeof(guint32));
        swap_bits_uint32_array(priv->serialized_swapped, n32);
        it.value.v_uint32_array = priv->serialized_swapped;
    }
    it.array_size = n32;
    items->items[items->n++] = it;
    n++;

    return n;
}

static void
gwy_mask_field_done(GwySerializable *serializable)
{
    GwyMaskField *field = GWY_MASK_FIELD(serializable);
    GWY_FREE(field->priv->serialized_swapped);
}

static gboolean
gwy_mask_field_construct(GwySerializable *serializable,
                         GwySerializableItems *items,
                         GwyErrorList **error_list)
{
    GwyMaskField *field = GWY_MASK_FIELD(serializable);
    MaskField *priv = field->priv;

    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyMaskField", error_list);

    if (!_gwy_check_data_dimension(error_list, "GwyMaskField", 2,
                                   its[0].value.v_uint32,
                                   its[1].value.v_uint32))
        goto fail;

    gsize n = stride_for_width(its[0].value.v_uint32) * its[1].value.v_uint32;
    if (G_UNLIKELY(n != its[3].array_size)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           // TRANSLATORS: Error message.
                           _("GwyMaskField dimensions %u×%u do not match "
                             "data size %lu."),
                           its[0].value.v_uint32, its[1].value.v_uint32,
                           (gulong)its[3].array_size);
        goto fail;
    }

    free_data(field);
    field->xres = its[0].value.v_uint32;
    field->yres = its[1].value.v_uint32;
    priv->name = its[2].value.v_string;
    field->stride = stride_for_width(field->xres);
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        field->data = its[3].value.v_uint32_array;
        its[3].value.v_uint32_array = NULL;
        its[3].array_size = 0;
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        field->data = g_memdup(its[3].value.v_uint32_array, n*sizeof(guint32));
        swap_bits_uint32_array(field->data, n);
    }
    priv->allocated = TRUE;

    return TRUE;

fail:
    GWY_FREE(its[2].value.v_string);
    GWY_FREE(its[3].value.v_uint32_array);
    return FALSE;
}

static GObject*
gwy_mask_field_duplicate_impl(GwySerializable *serializable)
{
    GwyMaskField *field = GWY_MASK_FIELD(serializable);
    //MaskField *priv = field->priv;

    GwyMaskField *duplicate = gwy_mask_field_new_sized(field->xres,
                                                       field->yres,
                                                       FALSE);
    //MaskField *dpriv = duplicate->priv;

    gsize n = field->stride * field->yres;
    gwy_assign(duplicate->data, field->data, n);
    gwy_assign_string(&duplicate->priv->name, field->priv->name);
    // TODO: Duplicate precalculated grain data too.

    return G_OBJECT(duplicate);
}

static void
gwy_mask_field_assign_impl(GwySerializable *destination,
                           GwySerializable *source)
{
    GwyMaskField *dest = GWY_MASK_FIELD(destination);
    GwyMaskField *src = GWY_MASK_FIELD(source);
    MaskField *spriv = src->priv, *dpriv = dest->priv;

    GParamSpec *notify[N_PROPS];
    guint nn = 0;
    if (dest->xres != src->xres)
        notify[nn++] = properties[PROP_XRES];
    if (dest->yres != src->yres)
        notify[nn++] = properties[PROP_YRES];
    if (dest->stride != src->stride)
        notify[nn++] = properties[PROP_STRIDE];
    if (gwy_assign_string(&dpriv->name, spriv->name))
        notify[nn++] = properties[PROP_NAME];

    gsize n = src->stride * src->yres;
    if (dest->stride * dest->yres != n) {
        free_data(dest);
        dest->data = g_new(guint32, n);
        dest->priv->allocated = TRUE;
    }
    gwy_assign(dest->data, src->data, n);
    dest->xres = src->xres;
    dest->yres = src->yres;
    dest->stride = src->stride;
    // TODO: Duplicate precalculated grain data too.
    _gwy_notify_properties_by_pspec(G_OBJECT(dest), notify, nn);
}

static void
gwy_mask_field_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyMaskField *field = GWY_MASK_FIELD(object);

    switch (prop_id) {
        case PROP_NAME:
        gwy_assign_string(&field->priv->name, g_value_get_string(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_mask_field_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    GwyMaskField *field = GWY_MASK_FIELD(object);

    switch (prop_id) {
        case PROP_XRES:
        g_value_set_uint(value, field->xres);
        break;

        case PROP_YRES:
        g_value_set_uint(value, field->yres);
        break;

        case PROP_STRIDE:
        g_value_set_uint(value, field->stride);
        break;

        case PROP_NAME:
        g_value_set_string(value, field->priv->name);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_mask_field_new:
 *
 * Creates a new two-dimensional mask field.
 *
 * The mask field dimensions will be 1×1 and it will be zero-filled.  This
 * parameterless constructor exists mainly for language bindings,
 * gwy_mask_field_new_sized() is usually more useful.
 *
 * Returns: (transfer full):
 *          A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new(void)
{
    return g_object_newv(GWY_TYPE_MASK_FIELD, 0, NULL);
}

/**
 * gwy_mask_field_new_sized:
 * @xres: Horizontal resolution (width).
 * @yres: Vertical resolution (height).
 * @clear: %TRUE to fill the new mask field with zeroes, %FALSE to leave it
 *         uninitialised.
 *
 * Creates a new two-dimensional mask field of specified dimensions.
 *
 * Returns: (transfer full):
 *          A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new_sized(guint xres,
                         guint yres,
                         gboolean clear)
{
    g_return_val_if_fail(xres && yres, NULL);

    GwyMaskField *field = g_object_newv(GWY_TYPE_MASK_FIELD, 0, NULL);
    free_data(field);
    field->xres = xres;
    field->yres = yres;
    alloc_data(field, clear);
    return field;
}

/**
 * gwy_mask_field_check_part:
 * @field: A two-dimensional mask field.
 * @fpart: (allow-none):
 *         Area in @field, possibly %NULL.
 * @col: Location to store the actual column index of the upper-left corner
 *       of the part.
 * @row: Location to store the actual row index of the upper-left corner
 *       of the part.
 * @width: Location to store the actual width (number of columns)
 *         of the part.
 * @height: Location to store the actual height (number of rows)
 *          of the part.
 *
 * Validates the position and dimensions of a mask field part.
 *
 * If @fpart is %NULL entire @field is to be used.  Otherwise @fpart must be
 * contained in @field.
 *
 * If the position and dimensions are valid @col, @row, @width and @height are
 * set to the actual rectangular part in @field.  If the function returns
 * %FALSE their values are undefined.
 *
 * This function is typically used in functions that operate on a part of a
 * field. Example (note gwy_mask_field_new_congruent() can create transposed,
 * rotated and flipped mask fields):
 * |[
 * GwyMaskField*
 * transpose_mask_field(const GwyMaskField *field,
 *                      const GwyFieldPart *fpart)
 * {
 *     guint col, row, width, height;
 *     if (!gwy_mask_field_check_part(field, fpart,
 *                                    &col, &row, &width, &height))
 *         return NULL;
 *
 *     // Perform the transposition of @field part given by @col, @row,
 *     // @width and @height...
 * }
 * ]|
 *
 * Returns: %TRUE if the position and dimensions are valid and the caller
 *          should proceed.  %FALSE if the caller should not proceed, either
 *          because @field is not a #GwyMaskField instance or the position or
 *          dimensions is invalid (a critical error is emitted in these cases)
 *          or the actual part is zero-sized.
 **/
gboolean
gwy_mask_field_check_part(const GwyMaskField *field,
                          const GwyFieldPart *fpart,
                          guint *col, guint *row,
                          guint *width, guint *height)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), FALSE);
    if (fpart) {
        if (!fpart->width || !fpart->height)
            return FALSE;
        // The two separate conditions are to catch integer overflows.
        g_return_val_if_fail(fpart->col < field->xres, FALSE);
        g_return_val_if_fail(fpart->width <= field->xres - fpart->col,
                             FALSE);
        g_return_val_if_fail(fpart->row < field->yres, FALSE);
        g_return_val_if_fail(fpart->height <= field->yres - fpart->row,
                             FALSE);
        *col = fpart->col;
        *row = fpart->row;
        *width = fpart->width;
        *height = fpart->height;
    }
    else {
        *col = *row = 0;
        *width = field->xres;
        *height = field->yres;
    }

    return TRUE;
}

/**
 * gwy_mask_field_limit_parts:
 * @src: A source two-dimensional mask field.
 * @srcpart: (allow-none):
 *           Area in @src, possibly %NULL.
 * @dest: A destination two-dimensional mask field.
 * @destcol: Column index for the upper-left corner of the part in @dest.
 * @destrow: Row index for the upper-left corner of the part in @dest.
 * @transpose: %TRUE to assume the area is transposed (rotated by 90 degrees)
 *             in the destination.
 * @col: Location to store the actual column index of the upper-left corner
 *       of the source part.
 * @row: Location to store the actual row index of the upper-left corner
 *       of the source part.
 * @width: Location to store the actual width (number of columns)
 *         of the source part.
 * @height: Location to store the actual height (number of rows)
 *          of the source part.
 *
 * Limits the dimensions of a mask field part for copying.
 *
 * The area is limited to make it contained both in @src and @dest and @col,
 * @row, @width and @height are set to the actual position and dimensions in
 * @src.  If the function returns %FALSE their values are undefined.
 *
 * If @src and @dest are the same field the source and destination parts should
 * not overlap.
 *
 * This function is typically used in copy-like functions that transfer a part
 * of a mask field into another mask field.
 * Example (note gwy_mask_field_copy() copies mask field parts):
 * |[
 * void
 * copy_mask_field(const GwyMaskField *src,
 *                 const GwyFieldPart *srcpart,
 *                 GwyMaskField *dest,
 *                 guint destcol, guint destrow)
 * {
 *     guint col, row, width, height;
 *     if (!gwy_mask_field_limit_parts(src, srcpart, dest, destcol, destrow,
 *                                     &col, &row, &width, &height))
 *         return;
 *
 *     // Copy area of size @width, @height at @col, @row in @src to an
 *     // equally-sized area at @destcol, @destrow in @dest...
 * }
 * ]|
 *
 * Returns: %TRUE if the caller should proceed.  %FALSE if the caller should
 *          not proceed, either because @field or @target is not a
 *          #GwyMaskField instance (a critical error is emitted in these cases)
 *          or the actual part is zero-sized.
 **/
gboolean
gwy_mask_field_limit_parts(const GwyMaskField *src,
                           const GwyFieldPart *srcpart,
                           const GwyMaskField *dest,
                           guint destcol, guint destrow,
                           gboolean transpose,
                           guint *col, guint *row,
                           guint *width, guint *height)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(src), FALSE);
    g_return_val_if_fail(GWY_IS_MASK_FIELD(dest), FALSE);

    if (srcpart) {
        *col = srcpart->col;
        *row = srcpart->row;
        *width = srcpart->width;
        *height = srcpart->height;
        if (*col >= src->xres || *row >= src->yres)
            return FALSE;
        *width = MIN(*width, src->xres - *col);
        *height = MIN(*height, src->yres - *row);
    }
    else {
        *col = *row = 0;
        *width = src->xres;
        *height = src->yres;
    }

    if (destcol >= dest->xres || destrow >= dest->yres)
        return FALSE;

    if (transpose) {
        *width = MIN(*width, dest->yres - destrow);
        *height = MIN(*height, dest->xres - destcol);
    }
    else {
        *width = MIN(*width, dest->xres - destcol);
        *height = MIN(*height, dest->yres - destrow);
    }

    if (src == dest) {
        if ((transpose
             && gwy_overlapping(*col, *width, destcol, *height)
             && gwy_overlapping(*row, *height, destrow, *width))
            || (gwy_overlapping(*col, *width, destcol, *width)
                && gwy_overlapping(*row, *height, destrow, *height))) {
            g_warning("Source and destination blocks overlap.  "
                      "Data corruption is imminent.");
        }
    }

    return *width && *height;
}

/**
 * gwy_mask_field_new_part:
 * @field: A two-dimensional mask field.
 * @fpart: (allow-none):
 *         Area in @field to extract to the new field.  Passing %NULL
 *         creates an identical copy of @field, similarly to
 *         gwy_mask_field_duplicate().
 *
 * Creates a new two-dimensional mask field as a part of another mask field.
 *
 * The rectangle specified by @fpart must be entirely contained in @field.
 * Both dimensions must be non-zero.
 *
 * Data are physically copied, i.e. changing the new mask field data does not
 * change @field's data and vice versa.
 *
 * Returns: (transfer full):
 *          A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new_part(const GwyMaskField *field,
                        const GwyFieldPart *fpart)
{
    guint col, row, width, height;
    if (!gwy_mask_field_check_part(field, fpart, &col, &row, &width, &height))
        return NULL;

    if (width == field->xres && height == field->yres)
        return gwy_mask_field_duplicate(field);

    GwyMaskField *part = gwy_mask_field_new_sized(width, height, FALSE);
    gwy_mask_field_copy(field, fpart, part, 0, 0);
    return part;
}

/**
 * gwy_mask_field_new_resampled:
 * @field: A two-dimensional mask field.
 * @xres: Desired X resolution.
 * @yres: Desired Y resolution.
 *
 * Creates a new two-dimensional mask field by resampling another mask field.
 *
 * Returns: (transfer full):
 *          A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new_resampled(const GwyMaskField *field,
                             guint xres,
                             guint yres)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), NULL);
    g_return_val_if_fail(xres && yres, NULL);
    if (xres == field->xres && yres == field->yres)
        return gwy_mask_field_duplicate(field);

    GwyMaskField *dest;
    dest = gwy_mask_field_new_sized(xres, yres, FALSE);

    guint xreq_bits, yreq_bits;
    gdouble xstep = (gdouble)field->xres/xres,
            ystep = (gdouble)field->yres/yres;
    GwyMaskScalingSegment *xsegments = gwy_mask_prepare_scaling(0.0, xstep,
                                                                xres,
                                                                &xreq_bits),
                          *ysegments = gwy_mask_prepare_scaling(0.0, ystep,
                                                                yres,
                                                                &yreq_bits);
    gdouble *row = g_new(gdouble, xres);
    g_assert(xreq_bits == field->xres);
    g_assert(yreq_bits == field->yres);

    GwyMaskScalingSegment *yseg = ysegments;
    for (guint i = 0, isrc = 0; i < yres; i++, yseg++) {
        GwyMaskIter iter;
        gwy_clear(row, xres);
        gwy_mask_field_iter_init(field, iter, 0, isrc);
        gwy_mask_scale_row_weighted(iter, xsegments, row, xres,
                                    xstep, yseg->w0);
        if (yseg->move) {
            for (guint k = yseg->move-1; k; k--) {
                isrc++;
                gwy_mask_field_iter_init(field, iter, 0, isrc);
                gwy_mask_scale_row_weighted(iter, xsegments, row, xres,
                                            xstep, 1.0/ystep);
            }
            isrc++;
            gwy_mask_field_iter_init(field, iter, 0, isrc);
            gwy_mask_scale_row_weighted(iter, xsegments, row, xres,
                                        xstep, yseg->w1);
        }
        gwy_mask_field_iter_init(dest, iter, 0, i);
        for (guint j = 0; j < xres; j++) {
            gwy_mask_iter_set(iter, row[j] >= MASK_ROUND_THRESHOLD);
            gwy_mask_iter_next(iter);
        }
    }

    g_free(row);
    g_free(ysegments);
    g_free(xsegments);

    return dest;
}

/**
 * gwy_mask_field_new_from_field:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to extract to the new mask field.  Passing %NULL
 *         processes entire @field.
 * @upper: Upper bound to compare the field values to.
 * @lower: Lower bound to compare the field values to.
 * @complement: %TRUE to negate the result of the comparison, i.e. create the
 *              mask of pixels with values in the complementary interval.
 *
 * Creates a new two-dimensional mask field by thresholding a field.
 *
 * If @complement is %FALSE and @lower ≤ @upper the mask is created of pixels
 * of the rectangle with values in the closed interval [@lower,@upper].  If
 * @lower is larger than @upper the mask is instead created of pixels with
 * values in [-∞,@upper] ∪ [@lower,∞].
 *
 * If @complement is %TRUE the mask is created of pixels in the complementary
 * open or semi-open intervals, namely [-∞,@lower) ∪ (@upper,∞] and
 * (@lower,@upper).
 *
 * So the most common two cases are:
 * |[
 * // Finite closed interval [a, b].
 * GwyMaskField *cmask = gwy_mask_field_new_from_field(field, NULL, a, b, FALSE);
 * // Finite open interval (a, b).
 * GwyMaskField *omask = gwy_mask_field_new_from_field(field, NULL, b, a, TRUE);
 * ]|
 *
 * Returns: (transfer full):
 *          A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new_from_field(const GwyField *field,
                              const GwyFieldPart *fpart,
                              gdouble lower,
                              gdouble upper,
                              gboolean complement)
{
    guint col, row, width, height;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height))
        return NULL;

    GwyMaskField *mfield = gwy_mask_field_new_sized(width, height, FALSE);
    const gdouble *fbase = field->data + field->xres*row + col;
    for (guint i = 0; i < height; i++) {
        const gdouble *p = fbase + i*field->xres;
        guint32 *q = mfield->data + i*mfield->stride;
        for (guint j = 0; j < (width >> 5); j++, q++) {
            guint32 v = 0;
            if (lower <= upper) {
                for (guint k = 0; k < 0x20; k++, p++) {
                    if (*p >= lower && *p <= upper)
                        v |= NTH_BIT(k);
                }
            }
            else {
                for (guint k = 0; k < 0x20; k++, p++) {
                    if (*p >= lower || *p <= upper)
                        v |= NTH_BIT(k);
                }
            }
            *q = complement ? ~v : v;
        }
        if (width % 0x20) {
            guint32 v = 0;
            if (lower <= upper) {
                for (guint k = 0; k < width % 0x20; k++, p++) {
                    if (*p >= lower && *p <= upper)
                        v |= NTH_BIT(k);
                }
            }
            else {
                for (guint k = 0; k < width % 0x20; k++, p++) {
                    if (*p >= lower || *p <= upper)
                        v |= NTH_BIT(k);
                }
            }
            *q = complement ? ~v : v;
        }
    }
    return mfield;
}

/**
 * gwy_mask_field_set_size:
 * @field: A two-dimensional mask field.
 * @xres: Desired X resolution.
 * @yres: Desired Y resolution.
 * @clear: %TRUE to clear the new field, %FALSE to leave it uninitialised.
 *
 * Resizes a two-dimensional mask field.
 *
 * If the new data size differs from the old data size this method is only
 * marginally more efficient than destroying the old field and creating a new
 * one.
 *
 * In no case the original data are preserved, not even if @xres and @yres are
 * equal to the current field dimensions.  Use gwy_mask_field_new_part() to
 * extract a part of a field into a new field.
 **/
void
gwy_mask_field_set_size(GwyMaskField *field,
                        guint xres,
                        guint yres,
                        gboolean clear)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    g_return_if_fail(xres && yres);

    GParamSpec *notify[N_PROPS];
    guint nn = 0;
    guint stride = stride_for_width(xres);
    if (field->xres != xres)
        notify[nn++] = properties[PROP_XRES];
    if (field->yres != yres)
        notify[nn++] = properties[PROP_YRES];
    if (field->stride != stride)
        notify[nn++] = properties[PROP_STRIDE];

    if (field->stride*field->yres != stride*yres) {
        free_data(field);
        field->xres = xres;
        field->yres = yres;
        alloc_data(field, clear);
    }
    else {
        field->xres = xres;
        field->yres = yres;
        field->stride = stride;
        gwy_clear(field->data, yres*stride);
    }
    gwy_mask_field_invalidate(field);
    _gwy_notify_properties_by_pspec(G_OBJECT(field), notify, nn);
}

/**
 * gwy_mask_field_data_changed:
 * @field: A two-dimensional mask field.
 * @fpart: (allow-none):
 *         Area in @field that has changed.  Passing %NULL means the entire
 *         mask field.
 *
 * Emits signal GwyMaskField::data-changed on a mask field.
 **/
void
gwy_mask_field_data_changed(GwyMaskField *field,
                            GwyFieldPart *fpart)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    g_signal_emit(field, signals[SGNL_DATA_CHANGED], 0, fpart);
}

/**
 * gwy_mask_field_invalidate:
 * @field: A two-dimensional mask field.
 *
 * Invalidates mask field grain data.
 *
 * All #GwyMaskField methods invalidate (or, in some cases, recalculate) cached
 * grain data if they modify the data.
 *
 * If you write to @field's data directly you may have to explicitly invalidate
 * the cached values as the methods have no means of knowing whether you
 * changed the data meanwhile or not.
 **/
void
gwy_mask_field_invalidate(GwyMaskField *field)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    free_caches(field);
}

/**
 * gwy_mask_field_count:
 * @field: A two-dimensional mask field.
 * @mask: (allow-none):
 *        A two-dimensional mask field determining to which bits of @field
 *        consider.  If it is %NULL entire @field is evaluated (as if
 *        all bits in @mask were set).
 * @value: %TRUE to count ones, %FALSE to count zeroes.
 *
 * Counts set or unset bits in a mask field.
 *
 * Returns: The number of bits equal to @value.
 **/
guint
gwy_mask_field_count(const GwyMaskField *field,
                     const GwyMaskField *mask,
                     gboolean value)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), 0);
    if (!mask)
        return gwy_mask_field_part_count(field, NULL, value);

    g_return_val_if_fail(GWY_IS_MASK_FIELD(mask), 0);
    g_return_val_if_fail(field->xres == mask->xres, 0);
    g_return_val_if_fail(field->yres == mask->yres, 0);
    g_return_val_if_fail(field->stride == mask->stride, 0);

    const guint end = field->xres & 0x1f;
    const guint32 m0 = MAKE_MASK(0, end);
    guint count = 0;

    for (guint i = 0; i < field->yres; i++) {
        const guint32 *m = mask->data + i*mask->stride;
        const guint32 *p = field->data + i*field->stride;
        if (value) {
            for (guint j = field->xres >> 5; j; j--, p++, m++)
                count += count_set_bits(*p & *m);
            if (end)
                count += count_set_bits(*p & *m & m0);
        }
        else {
            for (guint j = field->xres >> 5; j; j--, p++, m++)
                count += count_set_bits(~*p & *m);
            if (end)
                count += count_set_bits(~*p & *m & m0);
        }
    }
    return count;
}

/**
 * gwy_mask_field_part_count:
 * @field: A two-dimensional mask field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @value: %TRUE to count ones, %FALSE to count zeroes.
 *
 * Counts set or unset bits in a mask field.
 *
 * Returns: The number of bits equal to @value.
 **/
guint
gwy_mask_field_part_count(const GwyMaskField *field,
                          const GwyFieldPart *fpart,
                          gboolean value)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), 0);

    guint col, row, width, height;
    if (!gwy_mask_field_check_part(field, fpart, &col, &row, &width, &height))
        return 0;

    guint32 *base = field->data + field->stride*row + (col >> 5);
    const guint off = col & 0x1f;
    const guint end = (col + width) & 0x1f;
    guint count = 0;
    if (width <= 0x20 - off) {
        const guint32 m = MAKE_MASK(off, width);
        for (guint i = 0; i < height; i++)
            count += count_row_single(base + i*field->stride, m, value);
    }
    else {
        const guint32 m0 = MAKE_MASK(off, 0x20 - off);
        const guint32 m1 = MAKE_MASK(0, end);
        for (guint i = 0; i < height; i++)
            count += count_row(base + i*field->stride, width,
                               off, end, m0, m1, value);
    }
    return count;
}

/**
 * gwy_mask_field_part_count_masking:
 * @field: A two-dimensional mask field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @masking: Masking mode that would be used.
 *
 * Counts pixels that would be used in given masking mode.
 *
 * This is a convenience function used in other field functions that need to
 * know beforehand how many pixels they will process.
 *
 * Returns: The number of bits within the part that would be used if @field
 *          was used as a mask with masking mode @masking.
 **/
guint
gwy_mask_field_part_count_masking(const GwyMaskField *field,
                                  const GwyFieldPart *fpart,
                                  GwyMasking masking)
{
    g_return_val_if_fail(!field || GWY_IS_MASK_FIELD(field), 0);
    if (masking == GWY_MASK_IGNORE)
        return fpart ? fpart->width*fpart->height : field->xres*field->yres;
    if (masking == GWY_MASK_INCLUDE)
        return gwy_mask_field_part_count(field, fpart, TRUE);
    if (masking == GWY_MASK_EXCLUDE)
        return gwy_mask_field_part_count(field, fpart, FALSE);
    g_return_val_if_reached(0);
}

/**
 * gwy_mask_field_count_rows:
 * @field: A two-dimensional mask field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @value: %TRUE to count ones, %FALSE to count zeroes.
 * @counts: Array of length @height to store the counts in individual rows to.
 *
 * Counts set or unset bits in each row of a mask field.
 *
 * Returns: The number of bits equal to @value in the entire mask field part,
 *          i.e. the same value as gwy_mask_field_part_count() returns.
 **/
guint
gwy_mask_field_count_rows(const GwyMaskField *field,
                          const GwyFieldPart *fpart,
                          gboolean value,
                          guint *counts)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), 0);
    g_return_val_if_fail(counts, 0);

    guint col, row, width, height;
    if (!gwy_mask_field_check_part(field, fpart, &col, &row, &width, &height))
        return 0;

    guint32 *base = field->data + field->stride*row + (col >> 5);
    const guint off = col & 0x1f;
    const guint end = (col + width) & 0x1f;
    guint count = 0;
    if (width <= 0x20 - off) {
        const guint32 m = MAKE_MASK(off, width);
        for (guint i = 0; i < height; i++) {
            counts[i] = count_row_single(base + i*field->stride, m, value);
            count += counts[i];
        }
    }
    else {
        const guint32 m0 = MAKE_MASK(off, 0x20 - off);
        const guint32 m1 = MAKE_MASK(0, end);
        for (guint i = 0; i < height; i++) {
            counts[i] = count_row(base + i*field->stride, width,
                                  off, end, m0, m1, value);
            count += counts[i];
        }
    }
    return count;
}

/**
 * gwy_mask_field_set_name:
 * @field: A two-dimensional mask field.
 * @name: (allow-none):
 *        New field name.
 *
 * Sets the name of a two-dimensional mask field.
 **/
void
gwy_mask_field_set_name(GwyMaskField *field,
                        const gchar *name)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    if (!gwy_assign_string(&field->priv->name, name))
        return;

    g_object_notify_by_pspec(G_OBJECT(field), properties[PROP_NAME]);
}

/**
 * gwy_mask_field_get_name:
 * @field: A two-dimensional mask field.
 *
 * Gets the name of a two-dimensional mask field.
 *
 * Returns: (allow-none):
 *          Field name, owned by @field.
 **/
const gchar*
gwy_mask_field_get_name(const GwyMaskField *field)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), NULL);
    return field->priv->name;
}

/**
 * gwy_mask_field_get:
 * @field: A two-dimensional mask field.
 * @col: Column index in @field.
 * @row: Row index in @field.
 *
 * Obtains one bit value from a two-dimensional mask field.
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
#undef gwy_mask_field_get
gboolean
gwy_mask_field_get(const GwyMaskField *field,
                   guint col,
                   guint row)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), 0);
    g_return_val_if_fail(col < field->xres, 0);
    g_return_val_if_fail(row < field->yres, 0);
    return _gwy_mask_field_get(field, col, row);
}

/**
 * gwy_mask_field_set:
 * @field: A two-dimensional mask field.
 * @col: Column index in @field.
 * @row: Row index in @field.
 * @value: Nonzero value to set the bit, zero to clear it.
 *
 * Sets one bit value in a two-dimensional mask field.
 *
 * This is a low-level macro and it does not invalidate the mask field.
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
#undef gwy_mask_field_set
void
gwy_mask_field_set(const GwyMaskField *field,
                   guint col,
                   guint row,
                   gboolean value)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    g_return_if_fail(col < field->xres);
    g_return_if_fail(row < field->yres);
    _gwy_mask_field_set(field, col, row, value);
}

/**
 * SECTION: mask-field
 * @title: GwyMaskField
 * @short_description: Two-dimensional bit mask
 *
 * #GwyMaskField represents two-dimensional bit mask often used to specify the
 * area to include or exclude in #GwyField methods.
 *
 * The mask is stored in a flat array @data in #GwyMaskField-struct as #guint32
 * values, stored by rows.  This means the bits are packed into 32-bit integers
 * and the column index is the fast index, row index is the slow one.
 *
 * The bit order is native, i.e. on little-endian machines the lowest bit is
 * first, whereas on big-endian machines the highest bit is first.  This
 * convention matches the %CAIRO_FORMAT_A1 format and makes the masks directly
 * usable for Cairo drawing operations.
 *
 * Each row starts on a #guint32 boundary so, padding may be present at the
 * ends of rows.  The row stride is available as #GwyMaskField-struct.stride.
 * If padding is present, the unused bits have undefined value and mask field
 * methods may change them arbitrarily.
 *
 * Individual bits in the mask can be get and set with macros
 * gwy_mask_field_get() and gwy_mask_field_set().  These macros are relatively
 * slow and they are suitable for casual use or when the amount of work
 * otherwise done per pixel makes their cost negligible.  For the common
 * sequential access, the mask iterator #GwyMaskIter macros offers a good
 * compromise between complexity and efficiency.  In a really
 * performance-sensitive code, you also might wish to access the bits in
 * @data of #GwyMaskField directly.
 *
 * FIXME: Here should be something about invalidation, but let's get the grain
 * data implemented first.
 **/

/**
 * GwyMaskField:
 * @xres: X-resolution, i.e. width in pixels.
 * @yres: Y-resolution, i.e. height in pixels.
 * @stride: Row length in #guint32 items (not bytes, bear this in mind
 *          especially when passing the data to Cairo).
 * @data: Mask data.  See the introductory section for details.
 *
 * Object representing two-dimensional bit mask.
 *
 * The #GwyMaskField struct contains some public fields that can be
 * directly accessed for reading.  To set them, you must use the mask field
 * methods.
 **/

/**
 * GwyMaskFieldClass:
 *
 * Class of two-dimensional bit masks.
 **/

/**
 * gwy_mask_field_duplicate:
 * @field: A two-dimensional mask field.
 *
 * Duplicates a two-dimensional mask field.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_mask_field_assign:
 * @dest: Destination two-dimensional mask field.
 * @src: Source two-dimensional mask field.
 *
 * Copies the value of a two-dimensional mask field.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_mask_field_iter_init:
 * @field: A two-dimensional mask field.
 * @iter: Mask iterator to initialise.  It must be an identifier.
 * @col: Column index in @field.
 * @row: Row index in @field.
 *
 * Initialises a mask iterator to point to given pixel in a mask field.
 *
 * The iterator can be subsequently used to move back and forward within the
 * row @row (but not outside).
 *
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
