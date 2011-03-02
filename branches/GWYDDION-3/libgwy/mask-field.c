/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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
#include "libgwy/macros.h"
#include "libgwy/serialize.h"
#include "libgwy/math.h"
#include "libgwy/mask-field.h"
#include "libgwy/object-internal.h"
#include "libgwy/mask-field-internal.h"
#include "libgwy/field-internal.h"

enum { N_ITEMS = 3 };

enum {
    DATA_CHANGED,
    N_SIGNALS
};

enum {
    PROP_0,
    PROP_XRES,
    PROP_YRES,
    PROP_STRIDE,
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
    /*0*/ { .name = "xres", .ctype = GWY_SERIALIZABLE_INT32,       },
    /*1*/ { .name = "yres", .ctype = GWY_SERIALIZABLE_INT32,       },
    /*2*/ { .name = "data", .ctype = GWY_SERIALIZABLE_INT32_ARRAY, },
};

static guint mask_field_signals[N_SIGNALS];
static GParamSpec *mask_field_pspecs[N_PROPS];

G_DEFINE_TYPE_EXTENDED
    (GwyMaskField, gwy_mask_field, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_mask_field_serializable_init))

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

    mask_field_pspecs[PROP_XRES]
        = g_param_spec_uint("x-res",
                            "X resolution",
                            "Pixel width of the mask field.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | STATICP);

    mask_field_pspecs[PROP_YRES]
        = g_param_spec_uint("y-res",
                            "Y resolution",
                            "Pixel height of the mask field.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | STATICP);

    mask_field_pspecs[PROP_STRIDE]
        = g_param_spec_uint("stride",
                            "Stride",
                            "Row stride of the mask field in items, i.e. "
                            "guint32s.",
                            1, G_MAXUINT, 2,
                            G_PARAM_READABLE | STATICP);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, mask_field_pspecs[i]);

    /**
     * GwyMaskField::data-changed:
     * @gwymaskfield: The #GwyMaskField which received the signal.
     *
     * The ::data-changed signal is emitted whenever mask field data changes.
     **/
    mask_field_signals[DATA_CHANGED]
        = g_signal_new_class_handler("data-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
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
    free_data(field);
    GWY_FREE(field->priv->grains);
    GWY_FREE(field->priv->graindata);
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
    GwySerializableItem *it = items->items + items->n;
    gsize n = field->stride * field->yres;

    *it = serialize_items[0];
    it->value.v_uint32 = field->xres;
    it++, items->n++;

    *it = serialize_items[1];
    it->value.v_uint32 = field->yres;
    it++, items->n++;

    *it = serialize_items[2];
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        it->value.v_uint32_array = field->data;
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        MaskField *priv = field->priv;
        priv->serialized_swapped = g_new(guint32, n);
        swap_bits_uint32_array(priv->serialized_swapped, n);
        it->value.v_uint32_array = priv->serialized_swapped;
    }
    it->array_size = n;
    it++, items->n++;

    return N_ITEMS;
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

    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyMaskField", error_list);

    if (G_UNLIKELY(!its[0].value.v_uint32 || !its[1].value.v_uint32)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Mask field dimensions %u×%u are invalid."),
                           its[0].value.v_uint32, its[1].value.v_uint32);
        goto fail;
    }

    gsize n = stride_for_width(its[0].value.v_uint32) * its[1].value.v_uint32;
    if (G_UNLIKELY(n != its[2].array_size)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Mask field dimensions %u×%u do not match "
                             "data size %lu."),
                           its[0].value.v_uint32, its[1].value.v_uint32,
                           (gulong)its[2].array_size);
        goto fail;
    }

    free_data(field);
    field->xres = its[0].value.v_uint32;
    field->yres = its[1].value.v_uint32;
    field->stride = stride_for_width(field->xres);
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        field->data = its[2].value.v_uint32_array;
        its[2].value.v_uint32_array = NULL;
        its[2].array_size = 0;
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        field->data = g_memdup(its[2].value.v_uint32_array, n*sizeof(guint32));
        swap_bits_uint32_array(field->data, n);
    }
    field->priv->allocated = TRUE;

    return TRUE;

fail:
    GWY_FREE(its[2].value.v_uint32_array);
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
    // TODO: Duplicate precalculated grain data too.

    return G_OBJECT(duplicate);
}

static void
gwy_mask_field_assign_impl(GwySerializable *destination,
                           GwySerializable *source)
{
    GwyMaskField *dest = GWY_MASK_FIELD(destination);
    GwyMaskField *src = GWY_MASK_FIELD(source);
    //MaskField *spriv = src->priv, *dpriv = dest->priv;

    GParamSpec *notify[N_PROPS];
    guint nn = 0;
    if (dest->xres != src->xres)
        notify[nn++] = mask_field_pspecs[PROP_XRES];
    if (dest->yres != src->yres)
        notify[nn++] = mask_field_pspecs[PROP_YRES];
    if (dest->stride != src->stride)
        notify[nn++] = mask_field_pspecs[PROP_STRIDE];

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
 * Returns: A new two-dimensional mask field.
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
 * Returns: A new two-dimensional mask field.
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

gboolean
_gwy_mask_field_check_part(const GwyMaskField *field,
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

gboolean
_gwy_mask_field_limit_parts(const GwyMaskField *src,
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
 * Returns: A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new_part(const GwyMaskField *field,
                        const GwyFieldPart *fpart)
{
    guint col, row, width, height;
    if (!_gwy_mask_field_check_part(field, fpart, &col, &row, &width, &height))
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
 * Returns: A new two-dimensional mask field.
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
    g_warning("Implement me!");
    // TODO

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
 * Returns: A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new_from_field(const GwyField *field,
                              const GwyFieldPart *fpart,
                              gdouble lower,
                              gdouble upper,
                              gboolean complement)
{
    guint col, row, width, height;
    if (!_gwy_field_check_part(field, fpart, &col, &row, &width, &height))
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
        notify[nn++] = mask_field_pspecs[PROP_XRES];
    if (field->yres != yres)
        notify[nn++] = mask_field_pspecs[PROP_YRES];
    if (field->stride != stride)
        notify[nn++] = mask_field_pspecs[PROP_STRIDE];

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
 *
 * Emits signal GwyMaskField::data-changed on a mask field.
 **/
void
gwy_mask_field_data_changed(GwyMaskField *field)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    g_signal_emit(field, mask_field_signals[DATA_CHANGED], 0);
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
    GWY_FREE(field->priv->grains);
    GWY_FREE(field->priv->graindata);
}

/**
 * gwy_mask_field_count:
 * @field: A two-dimensional mask field.
 * @mask: A two-dimensional mask field determining to which bits of @field
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

static inline guint
count_row_single(const guint32 *row,
                 guint32 m,
                 gboolean value)
{
    return count_set_bits((value ? *row : ~*row) & m);
}

static guint
count_row(const guint32 *row,
          guint width,
          guint off, guint end,
          guint32 m0, guint32 m1,
          gboolean value)
{
    guint j = width;
    guint count = count_set_bits(*row & m0);
    j -= 0x20 - off, row++;
    while (j >= 0x20) {
        count += count_set_bits(*row);
        j -= 0x20, row++;
    }
    if (end)
        count += count_set_bits(*row & m1);
    return value ? count : width - count;
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
    if (!_gwy_mask_field_check_part(field, fpart, &col, &row, &width, &height))
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
    if (!_gwy_mask_field_check_part(field, fpart, &col, &row, &width, &height))
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

/* Merge grains i and j in map with full resolution */
static inline void
resolve_grain_map(guint *m, guint i, guint j)
{
    guint ii, jj, k;

    /* Find what i and j fully resolve to */
    for (ii = i; m[ii] != ii; ii = m[ii])
        ;
    for (jj = j; m[jj] != jj; jj = m[jj])
        ;
    k = MIN(ii, jj);

    /* Fix partial resultions to full */
    for (ii = m[i]; m[ii] != ii; ii = m[ii]) {
        m[i] = k;
        i = ii;
    }
    m[ii] = k;
    for (jj = m[j]; m[jj] != jj; jj = m[jj]) {
        m[j] = k;
        j = jj;
    }
    m[jj] = k;
}

static inline guint32*
ensure_map(guint max_no, guint *map, guint *mapsize)
{
    if (G_UNLIKELY(max_no == *mapsize)) {
        *mapsize *= 2;
        return g_renew(guint, map, *mapsize);
    }
    return map;
}

/**
 * gwy_mask_field_number_grains:
 * @field: A two-dimensional mask field.
 * @ngrains: Location to store the number of the last grain, or %NULL.
 *
 * Numbers grains in a mask field.
 *
 * Returns: Array of integers of the same number of items as @field
 *          (without padding) filled with grain numbers of each pixel.  Empty
 *          space is set to 0, pixels inside a grain are set to the grain
 *          number.  Grains are numbered sequentially 1, 2, 3, ...
 *          The returned array is owned by @field and become invalid when
 *          the data change, gwy_mask_field_invalidate() is called or the
 *          mask field is finalized.
 **/
const guint*
gwy_mask_field_number_grains(GwyMaskField *field,
                             guint *ngrains)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), NULL);
    MaskField *priv = field->priv;
    if (priv->grains) {
        GWY_MAYBE_SET(ngrains, priv->ngrains);
        return priv->grains;
    }

    guint xres = field->xres, yres = field->yres;
    priv->grains = g_new(guint, xres*yres);

    // A reasonable initial size of the grain map.
    guint msize = 4*(field->xres + field->yres);
    guint *m = g_new0(guint, msize);

    /* Number grains with simple unidirectional grain number propagation,
     * updating map m for later full grain join */
    guint max_id = 0;
    for (guint i = 0; i < yres; i++) {
        guint *g = priv->grains + i*xres;
        guint *gprev = g - xres;
        GwyMaskIter iter;
        gwy_mask_field_iter_init(field, iter, 0, i);
        guint grain_id = 0;
        for (guint j = xres; j; j--, g++, gprev++) {
            if (gwy_mask_iter_get(iter)) {
                /* Grain number is kept from the top or left neighbour
                 * unless it does not exist (a new number is assigned) or a
                 * join with top neighbour occurs (m is updated) */
                guint id;
                if (i && (id = *gprev)) {
                    if (!grain_id)
                        grain_id = id;
                    else if (id != grain_id) {
                        resolve_grain_map(m, id, grain_id);
                        grain_id = m[id];
                    }
                }
                if (!grain_id) {
                    grain_id = ++max_id;
                    m = ensure_map(grain_id, m, &msize);
                    m[grain_id] = grain_id;
                }
            }
            else
                grain_id = 0;
            *g = grain_id;
            gwy_mask_iter_next(iter);
        }
    }

    /* Resolve remianing grain number links in map */
    for (guint i = 1; i <= max_id; i++)
        m[i] = m[m[i]];

    /* Compactify grain numbers */
    guint *mm = g_new0(guint, max_id + 1);
    guint id = 0;
    for (guint i = 1; i <= max_id; i++) {
        if (!mm[m[i]]) {
            id++;
            mm[m[i]] = id;
        }
        m[i] = mm[m[i]];
    }
    g_free(mm);

    /* Renumber grains (we make use of the fact m[0] = 0) */
    guint *g = priv->grains;
    for (guint i = 0; i < xres*yres; i++)
        g[i] = m[g[i]];

    g_free(m);

    priv->ngrains = id;
    GWY_MAYBE_SET(ngrains, priv->ngrains);
    return priv->grains;
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
 *
 * <refsect2 id='GwyMaskField-grains'>
 * <title>Grains</title>
 * <para>Several mask field methods deal with grains.  In this context, grain
 * simply means a contiguous part of the mask, not touching other parts of the
 * mask (two pixels with just a common corner are considered separate).  The
 * term grain has the origin in the common use of these methods on the result
 * of a grain marking function.</para>
 * </refsect2>
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
 * Returns: Nonzero value (not necessarily 1) if the bit is set, zero if it's
 *          unset.
 **/

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
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
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
