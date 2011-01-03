/*
 *  $Id$
 *  Copyright (C) 2009-2010 David Necas (Yeti).
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
#include "libgwy/mask-field.h"
#include "libgwy/math-internal.h"
#include "libgwy/line-internal.h"
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

// XXX: gwy_mask_field_grow() needs to invalidate the mask *except* @grains and
// @ngrains which it correctly updates.  It must be revised when fields are
// added here to invalidate them too.
struct _GwyMaskFieldPrivate {
    guint *grains;
    guint *graindata;
    guint ngrains;
    gboolean allocated;
    guint32 *serialized_swapped;    // serialization-only
};

typedef struct _GwyMaskFieldPrivate MaskField;

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

    g_object_class_install_property
        (gobject_class,
         PROP_XRES,
         g_param_spec_uint("x-res",
                           "X resolution",
                           "Pixel width of the mask field.",
                           1, G_MAXUINT, 1,
                           G_PARAM_READABLE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_YRES,
         g_param_spec_uint("y-res",
                           "Y resolution",
                           "Pixel height of the mask field.",
                           1, G_MAXUINT, 1,
                           G_PARAM_READABLE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_STRIDE,
         g_param_spec_uint("stride",
                           "Stride",
                           "Row stride of the mask field in items, i.e. "
                           "guint32s.",
                           1, G_MAXUINT, 2,
                           G_PARAM_READABLE | STATICP));

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

static inline guint
stride_for_width(guint width)
{
    // The return value is measured in sizeof(guint32), i.e. doublewords.
    // The row alignment is to start each row on 8byte boudnary.
    guint stride = (width + 0x1f) >> 5;
    return stride + (stride & 1);
}

static void
swap_bits_uint32_array(guint32 *data,
                       gsize n)
{
    while (n--) {
        *data = swap_bits_32(*data);
        data++;
    }
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
    else if (field->data) {
        g_slice_free1(field->stride * field->yres * sizeof(guint32),
                      field->data);
        field->data = NULL;
    }
}

static void
gwy_mask_field_init(GwyMaskField *field)
{
    field->priv = G_TYPE_INSTANCE_GET_PRIVATE(field,
                                              GWY_TYPE_MASK_FIELD,
                                              MaskField);
    field->xres = field->yres = 1;
    field->stride = stride_for_width(field->xres);
    field->data = g_slice_alloc(field->stride * field->yres * sizeof(guint32));
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
    memcpy(duplicate->data, field->data, n*sizeof(guint32));
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

    const gchar *notify[N_PROPS];
    guint nn = 0;
    if (dest->xres != src->xres)
        notify[nn++] = "x-res";
    if (dest->yres != src->yres)
        notify[nn++] = "y-res";
    if (dest->stride != src->stride)
        notify[nn++] = "stride";

    gsize n = src->stride * src->yres;
    if (dest->stride * dest->yres != n) {
        free_data(dest);
        dest->data = g_new(guint32, n);
        dest->priv->allocated = TRUE;
    }
    memcpy(dest->data, src->data, n*sizeof(guint32));
    dest->xres = src->xres;
    dest->yres = src->yres;
    dest->stride = src->stride;
    // TODO: Duplicate precalculated grain data too.
    _gwy_notify_properties(G_OBJECT(dest), notify, nn);
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
 * paremterless constructor exists mainly for language bindings,
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
_gwy_mask_field_check_rectangle(const GwyMaskField *field,
                                const GwyRectangle *rectangle,
                                guint *col, guint *row,
                                guint *width, guint *height)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), FALSE);
    if (rectangle) {
        if (!rectangle->width || !rectangle->height)
            return FALSE;
        // The two separate conditions are to catch integer overflows.
        g_return_val_if_fail(rectangle->col < field->xres, FALSE);
        g_return_val_if_fail(rectangle->width <= field->xres - rectangle->col,
                             FALSE);
        g_return_val_if_fail(rectangle->row < field->yres, FALSE);
        g_return_val_if_fail(rectangle->height <= field->yres - rectangle->row,
                             FALSE);
        *col = rectangle->col;
        *row = rectangle->row;
        *width = rectangle->width;
        *height = rectangle->height;
    }
    else {
        *col = *row = 0;
        *width = field->xres;
        *height = field->yres;
    }

    return TRUE;
}

gboolean
_gwy_mask_field_limit_rectangles(const GwyMaskField *src,
                                 const GwyRectangle *srcrectangle,
                                 const GwyMaskField *dest,
                                 guint destcol, guint destrow,
                                 gboolean transpose,
                                 guint *col, guint *row,
                                 guint *width, guint *height)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(src), FALSE);
    g_return_val_if_fail(GWY_IS_MASK_FIELD(dest), FALSE);

    if (srcrectangle) {
        *col = srcrectangle->col;
        *row = srcrectangle->row;
        *width = srcrectangle->width;
        *height = srcrectangle->height;
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
             && OVERLAPPING(*col, *width, destcol, *height)
             && OVERLAPPING(*row, *height, destrow, *width))
            || (OVERLAPPING(*col, *width, destcol, *width)
                && OVERLAPPING(*row, *height, destrow, *height))) {
            g_warning("Source and destination blocks overlap.  "
                      "Data corruption is imminent.");
        }
    }

    return *width && *height;
}

/**
 * gwy_mask_field_new_part:
 * @field: A two-dimensional mask field.
 * @rectangle: Area in @field to extract to the new field.  Passing %NULL
 *             creates an identical copy of @field, similarly to
 *             gwy_mask_field_duplicate().
 *
 * Creates a new two-dimensional mask field as a rectangular part of another
 * mask field.
 *
 * The rectangle specified by @rectangle must be entirely contained in @field.
 * Both dimensions must be non-zero.
 *
 * Data are physically copied, i.e. changing the new mask field data does not
 * change @field's data and vice versa.
 *
 * Returns: A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new_part(const GwyMaskField *field,
                        const GwyRectangle *rectangle)
{
    guint col, row, width, height;
    if (!_gwy_mask_field_check_rectangle(field, rectangle,
                                         &col, &row, &width, &height))
        return NULL;

    if (width == field->xres && height == field->yres)
        return gwy_mask_field_duplicate(field);

    GwyMaskField *part = gwy_mask_field_new_sized(width, height, FALSE);
    gwy_mask_field_copy(field, rectangle, part, 0, 0);
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
 * @rectangle: Area in @field to extract to the new mask field.  Passing %NULL
 *             processes entire @field.
 * @upper: Upper bound to compare the field values to.
 * @lower: Lower bound to compare the field values to.
 * @complement: %TRUE to negate the result of the comparison, i.e. create the
 *              mask of pixels with values in the complementary interval.
 *
 * Creates a new two-dimensional mask field by thresholding a rectangular part
 * of a data field.
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
                              const GwyRectangle *rectangle,
                              gdouble lower,
                              gdouble upper,
                              gboolean complement)
{
    guint col, row, width, height;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height))
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

    const gchar *notify[2];
    guint nn = 0;
    if (field->xres != xres)
        notify[nn++] = "x-res";
    if (field->yres != yres)
        notify[nn++] = "y-res";

    if (field->xres*field->yres != xres*yres) {
        free_data(field);
        field->xres = xres;
        field->yres = yres;
        alloc_data(field, clear);
    }
    else {
        field->xres = xres;
        field->yres = yres;
        if (clear)
            gwy_mask_field_fill(field, NULL, FALSE);
    }
    gwy_mask_field_invalidate(field);
    _gwy_notify_properties(G_OBJECT(field), notify, nn);
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

// Only one item is modified per row but the mask may need bits cut off from
// both sides (unlike in all other cases).
#define LOGICAL_OP_PART_SINGLE(masked) \
    do { \
        const guint32 m = MAKE_MASK(doff, width); \
        for (guint i = 0; i < height; i++) { \
            const guint32 *p = sbase + i*src->stride; \
            guint32 *q = dbase + i*dest->stride; \
            guint32 v0 = *p; \
            if (send && send <= soff) { \
                guint32 v1 = *(++p); \
                guint32 vp = (v0 SHL kk) | (v1 SHR k); \
                masked; \
            } \
            else if (doff > soff) { \
                guint32 vp = v0 SHR k; \
                masked; \
            } \
            else { \
                guint32 vp = v0 SHL kk; \
                masked; \
            } \
        } \
    } while (0)

// Multiple items are modified per row but the offsets match so no shifts are
// necessary, just masking at the ends.
#define LOGICAL_OP_PART_ALIGNED(simple, masked) \
    do { \
        const guint32 m0d = MAKE_MASK(doff, 0x20 - doff); \
        const guint32 m1d = MAKE_MASK(0, dend); \
        for (guint i = 0; i < height; i++) { \
            const guint32 *p = sbase + i*src->stride; \
            guint32 *q = dbase + i*dest->stride; \
            guint j = width; \
            guint32 vp = *p; \
            guint32 m = m0d; \
            masked; \
            j -= 0x20 - doff, p++, q++; \
            while (j >= 0x20) { \
                vp = *p; \
                simple; \
                j -= 0x20, p++, q++; \
            } \
            if (!dend) \
                continue; \
            vp = *p; \
            m = m1d; \
            masked; \
        } \
    } while (0)

// The general case, multi-item transfer and different offsets.
#define LOGICAL_OP_PART_GENERAL(simple, masked) \
    do { \
        const guint32 m0d = MAKE_MASK(doff, 0x20 - doff); \
        const guint32 m1d = MAKE_MASK(0, dend); \
        for (guint i = 0; i < height; i++) { \
            const guint32 *p = sbase + i*src->stride; \
            guint32 *q = dbase + i*dest->stride; \
            guint j = width; \
            guint32 v0 = *p; \
            if (doff > soff) { \
                guint32 vp = v0 SHR k; \
                guint32 m = m0d; \
                masked; \
            } \
            else { \
                guint32 v1 = *(++p); \
                guint32 vp = (v0 SHL kk) | (v1 SHR k); \
                guint32 m = m0d; \
                masked; \
                v0 = v1; \
            } \
            j -= (0x20 - doff), q++; \
            while (j >= 0x20) { \
                guint32 v1 = *(++p); \
                guint32 vp = (v0 SHL kk) | (v1 SHR k); \
                simple; \
                j -= 0x20, q++; \
                v0 = v1; \
            } \
            if (!dend) \
                continue; \
            if (send && dend > send) { \
                guint32 v1 = *(++p); \
                guint32 vp = (v0 SHL kk) | (v1 SHR k); \
                guint32 m = m1d; \
                masked; \
            } \
            else { \
                guint32 vp = (v0 SHL kk); \
                guint32 m = m1d; \
                masked; \
            } \
        } \
    } while (0)

#define LOGICAL_OP_PART(simple, masked) \
    if (width <= 0x20 - doff) \
        LOGICAL_OP_PART_SINGLE(masked); \
    else if (doff == soff) \
        LOGICAL_OP_PART_ALIGNED(simple, masked); \
    else \
        LOGICAL_OP_PART_GENERAL(simple, masked) \

static void
copy_part(const GwyMaskField *src,
          guint col,
          guint row,
          guint width,
          guint height,
          GwyMaskField *dest,
          guint destcol,
          guint destrow)
{
    const guint32 *sbase = src->data + src->stride*row + (col >> 5);
    guint32 *dbase = dest->data + dest->stride*destrow + (destcol >> 5);
    const guint soff = col & 0x1f;
    const guint doff = destcol & 0x1f;
    const guint send = (col + width) & 0x1f;
    const guint dend = (destcol + width) & 0x1f;
    const guint k = (doff + 0x20 - soff) & 0x1f;
    const guint kk = 0x20 - k;
    LOGICAL_OP_PART(*q = vp, *q = (*q & ~m) | (vp & m));
}

/**
 * gwy_mask_field_copy:
 * @src: Source two-dimensional mask field.
 * @srcrectangle: Area in field @src to copy.  Pass %NULL to copy entire @src.
 * @dest: Destination two-dimensional mask field.
 * @destcol: Destination column in @dest.
 * @destrow: Destination row in @dest.
 *
 * Copies data from one mask field to another.
 *
 * The copied rectangle is defined by @srcrectangle and it is copied to @dest
 * starting from (@destcol, @destrow).
 *
 * There are no limitations on the row and column indices or dimensions.  Only
 * the part of the rectangle that is corresponds to data inside @src and @dest
 * is copied.  This can also mean nothing is copied at all.
 *
 * If @src is equal to @dest the areas may <emphasis>not</emphasis> overlap.
 **/
void
gwy_mask_field_copy(const GwyMaskField *src,
                    const GwyRectangle *srcrectangle,
                    GwyMaskField *dest,
                    guint destcol,
                    guint destrow)
{
    guint col, row, width, height;
    if (!_gwy_mask_field_limit_rectangles(src, srcrectangle,
                                          dest, destcol, destrow,
                                          FALSE, &col, &row, &width, &height))
        return;

    if (width == src->xres
        && width == dest->xres
        && dest->stride == src->stride) {
        g_assert(col == 0 && destcol == 0);
        guint rowsize = src->stride * sizeof(guint32);
        memcpy(dest->data + dest->stride*destrow,
               src->data + src->stride*row, height*rowsize);
    }
    else
        copy_part(src, col, row, width, height, dest, destcol, destrow);
    gwy_mask_field_invalidate(dest);
}

/**
 * gwy_mask_field_copy_full:
 * @src: Source two-dimensional mask field.
 * @dest: Destination two-dimensional mask field.
 *
 * Copies entire data from one mask field to another.
 *
 * The two fields must be of identical dimensions.
 **/
void
gwy_mask_field_copy_full(const GwyMaskField *src,
                         GwyMaskField *dest)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(src));
    g_return_if_fail(GWY_IS_MASK_FIELD(dest));
    // This is a sanity check as gwy_mask_field_copy() can handle anything.
    g_return_if_fail(src->xres == dest->xres && src->yres == dest->yres);
    gwy_mask_field_copy(src, NULL, dest, 0, 0);
}

/**
 * gwy_mask_field_get_data:
 * @field: A two-dimensional mask field.
 *
 * Obtains the data of a two-dimensional mask field.
 *
 * This is the preferred method to obtain the data array for writing as it
 * invalidates numbered grains and segmented grain data.
 *
 * Do not use it if you only want to read data because it invalidates numbered
 * grains and segmented grain data.
 *
 * Returns: #GwyMaskField-struct.data, but it invalidates the mask field for
 * you.
 **/
guint32*
gwy_mask_field_get_data(GwyMaskField *field)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), NULL);
    gwy_mask_field_invalidate(field);
    return field->data;
}

/**
 * gwy_mask_field_invalidate:
 * @field: A two-dimensional mask field.
 *
 * Invalidates mask field grain data.
 *
 * User code should seldom need this method since all #GwyMaskField methods
 * correctly invalidate grain data when they change the mask, also
 * gwy_mask_field_get_data() does.
 **/
void
gwy_mask_field_invalidate(GwyMaskField *field)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    GWY_FREE(field->priv->grains);
    GWY_FREE(field->priv->graindata);
}

static void
set_part(GwyMaskField *field,
         guint col,
         guint row,
         guint width,
         guint height)
{
    guint32 *base = field->data + field->stride*row + (col >> 5);
    const guint off = col & 0x1f;
    const guint end = (col + width) & 0x1f;
    if (width <= 0x20 - off) {
        const guint32 m = MAKE_MASK(off, width);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*field->stride;
            *p |= m;
        }
    }
    else {
        const guint32 m0 = MAKE_MASK(off, 0x20 - off);
        const guint32 m1 = MAKE_MASK(0, end);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*field->stride;
            guint j = width;
            *p |= m0;
            j -= 0x20 - off, p++;
            while (j >= 0x20) {
                *p = ALL_SET;
                j -= 0x20, p++;
            }
            if (!end)
                continue;
            *p |= m1;
        }
    }
}

static void
clear_part(GwyMaskField *field,
           guint col,
           guint row,
           guint width,
           guint height)
{
    guint32 *base = field->data + field->stride*row + (col >> 5);
    const guint off = col & 0x1f;
    const guint end = (col + width) & 0x1f;
    if (width <= 0x20 - off) {
        const guint32 m = ~MAKE_MASK(off, width);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*field->stride;
            *p &= m;
        }
    }
    else {
        const guint32 m0 = ~MAKE_MASK(off, 0x20 - off);
        const guint32 m1 = ~MAKE_MASK(0, end);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*field->stride;
            guint j = width;
            *p &= m0;
            j -= 0x20 - off, p++;
            while (j >= 0x20) {
                *p = ALL_CLEAR;
                j -= 0x20, p++;
            }
            if (!end)
                continue;
            *p &= m1;
        }
    }
}

/**
 * gwy_mask_field_fill:
 * @field: A two-dimensional mask field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @value: Value to fill the rectangle with.
 *
 * Fills a rectangular part of a mask field with the specified value.
 **/
void
gwy_mask_field_fill(GwyMaskField *field,
                    const GwyRectangle *rectangle,
                    gboolean value)
{
    guint col, row, width, height;
    if (!_gwy_mask_field_check_rectangle(field, rectangle,
                                         &col, &row, &width, &height))
        return;

    if (width == field->xres) {
        g_assert(col == 0);
        memset(field->data + row*field->stride,
               value ? 0xff : 0x00,
               height*field->stride*sizeof(guint32));
    }
    else {
        if (value)
            set_part(field, col, row, width, height);
        else
            clear_part(field, col, row, width, height);
    }
    gwy_mask_field_invalidate(field);
}

static void
invert_part(GwyMaskField *field,
            guint col,
            guint row,
            guint width,
            guint height)
{
    guint32 *base = field->data + field->stride*row + (col >> 5);
    const guint off = col & 0x1f;
    const guint end = (col + width) & 0x1f;
    if (width <= 0x20 - off) {
        const guint32 m = MAKE_MASK(off, width);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*field->stride;
            *p ^= m;
        }
    }
    else {
        const guint32 m0 = MAKE_MASK(off, 0x20 - off);
        const guint32 m1 = MAKE_MASK(0, end);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*field->stride;
            guint j = width;
            *p ^= m0;
            j -= 0x20 - off, p++;
            while (j >= 0x20) {
                *p ^= ALL_SET;
                j -= 0x20, p++;
            }
            if (!end)
                continue;
            *p ^= m1;
        }
    }
}

// FIXME: Does it worth publishing?  One usually inverts the complete mask.
/**
 * gwy_mask_field_invert:
 * @field: A two-dimensional mask field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 *
 * Inverts values in a mask field.
 **/
static void
gwy_mask_field_invert(GwyMaskField *field,
                      const GwyRectangle *rectangle)
{
    guint col, row, width, height;
    if (!_gwy_mask_field_check_rectangle(field, rectangle,
                                         &col, &row, &width, &height))
        return;
    invert_part(field, col, row, width, height);
    gwy_mask_field_invalidate(field);
}

#define LOGICAL_OP_LOOP(simple, masked) \
    do { \
        if (mask) { \
            const guint32 *m = mask->data; \
            for (guint i = n; i; i--, p++, q++, m++) \
                masked; \
        } \
        else { \
            for (guint i = n; i; i--, p++, q++) \
                simple; \
        } \
    } while (0)

/**
 * gwy_mask_field_logical:
 * @field: A two-dimensional mask field to modify and the first operand of
 *         the logical operation.
 * @operand: A two-dimensional mask field representing second operand of the
 *           logical operation.  It can be %NULL for degenerate operations that
 *           do not depend on the second operand.
 * @mask: A two-dimensional mask field determining to which bits of
 *        @field to apply the logical operation to.  If it is %NULL the
 *        opperation is applied to all bits (as if all bits in @mask were set).
 * @op: Logical operation to perform.
 *
 * Modifies a mask field by logical operation with another mask field.
 **/
void
gwy_mask_field_logical(GwyMaskField *field,
                       const GwyMaskField *operand,
                       const GwyMaskField *mask,
                       GwyLogicalOperator op)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    g_return_if_fail(op <= GWY_LOGICAL_ONE);
    if (op == GWY_LOGICAL_A)
        return;
    if (mask) {
        g_return_if_fail(GWY_IS_MASK_FIELD(mask));
        g_return_if_fail(field->xres == mask->xres);
        g_return_if_fail(field->yres == mask->yres);
        g_return_if_fail(field->stride == mask->stride);
    }
    if (op == GWY_LOGICAL_ZERO) {
        if (mask) {
            op = GWY_LOGICAL_NIMPL;
            operand = mask;
            mask = NULL;
        }
    }
    else if (op == GWY_LOGICAL_ONE) {
        if (mask) {
            op = GWY_LOGICAL_OR;
            operand = mask;
            mask = NULL;
        }
    }
    else if (op == GWY_LOGICAL_NA) {
        if (mask) {
            op = GWY_LOGICAL_XOR;
            operand = mask;
            mask = NULL;
        }
    }
    else {
        g_return_if_fail(GWY_IS_MASK_FIELD(operand));
        g_return_if_fail(field->xres == operand->xres);
        g_return_if_fail(field->yres == operand->yres);
        g_return_if_fail(field->stride == operand->stride);
    }

    guint n = field->stride * field->yres;
    const guint32 *p = operand->data;
    guint32 *q = field->data;

    // GWY_LOGICAL_ZERO cannot have mask.
    if (op == GWY_LOGICAL_ZERO)
        gwy_mask_field_fill(field, NULL, FALSE);
    else if (op == GWY_LOGICAL_AND)
        LOGICAL_OP_LOOP(*q &= *p, *q &= ~*m | (*p & *m));
    else if (op == GWY_LOGICAL_NIMPL)
        LOGICAL_OP_LOOP(*q &= ~*p, *q &= ~*m | (~*p & *m));
    // GWY_LOGICAL_A cannot get here.
    else if (op == GWY_LOGICAL_NCIMPL)
        LOGICAL_OP_LOOP(*q = ~*q & *p, *q = (*q & ~*m) | (~*q & *p & *m));
    else if (op == GWY_LOGICAL_B)
        LOGICAL_OP_LOOP(*q = *p, *q = (*q & ~*m) | (*p & *m));
    else if (op == GWY_LOGICAL_XOR)
        LOGICAL_OP_LOOP(*q ^= *p, *q ^= *m & *p);
    else if (op == GWY_LOGICAL_OR)
        LOGICAL_OP_LOOP(*q |= *p, *q |= *m & *p);
    else if (op == GWY_LOGICAL_NOR)
        LOGICAL_OP_LOOP(*q = ~(*q | *p), *q = (*q & ~*m) | (~(*q | *p) & *m));
    else if (op == GWY_LOGICAL_NXOR)
        LOGICAL_OP_LOOP(*q = ~(*q ^ *p), *q = (*q & ~*m) | (~(*q ^ *p) & *m));
    else if (op == GWY_LOGICAL_NB)
        LOGICAL_OP_LOOP(*q = ~*p, *q = (*q & ~*m) | (~*p & *m));
    else if (op == GWY_LOGICAL_CIMPL)
        LOGICAL_OP_LOOP(*q |= ~*p, *q |= ~*p & *m);
    // GWY_LOGICAL_NA cannot have mask.
    else if (op == GWY_LOGICAL_NA)
        LOGICAL_OP_LOOP(*q = ~*q, g_assert_not_reached());
    else if (op == GWY_LOGICAL_IMPL)
        LOGICAL_OP_LOOP(*q = ~*q | *p, *q = (*q & ~*m) | ((~*q | *p) & *m));
    else if (op == GWY_LOGICAL_NAND)
        LOGICAL_OP_LOOP(*q = ~(*q & *p),  *q = (*q & ~*m) | (~(*q & *p) & *m));
    // GWY_LOGICAL_ONE cannot have mask.
    else if (op == GWY_LOGICAL_ONE)
        gwy_mask_field_fill(field, NULL, TRUE);
    else {
        g_assert_not_reached();
    }
    gwy_mask_field_invalidate(field);
}

static void
logical_part(const GwyMaskField *src,
             guint col,
             guint row,
             guint width,
             guint height,
             GwyMaskField *dest,
             guint destcol,
             guint destrow,
             GwyLogicalOperator op)
{
    const guint32 *sbase = src->data + src->stride*row + (col >> 5);
    guint32 *dbase = dest->data + dest->stride*destrow + (destcol >> 5);
    const guint soff = col & 0x1f;
    const guint doff = destcol & 0x1f;
    const guint send = (col + width) & 0x1f;
    const guint dend = (destcol + width) & 0x1f;
    const guint k = (doff + 0x20 - soff) & 0x1f;
    const guint kk = 0x20 - k;
    // GWY_LOGICAL_ZERO cannot get here.
    if (op == GWY_LOGICAL_AND)
        LOGICAL_OP_PART(*q &= vp, *q &= ~m | (vp & m));
    else if (op == GWY_LOGICAL_NIMPL)
        LOGICAL_OP_PART(*q &= ~vp, *q &= ~m | (~vp & m));
    // GWY_LOGICAL_A cannot get here.
    else if (op == GWY_LOGICAL_NCIMPL)
        LOGICAL_OP_PART(*q = ~*q & vp, *q = (*q & ~m) | (~*q & vp & m));
    // GWY_LOGICAL_B cannot get here.
    else if (op == GWY_LOGICAL_XOR)
        LOGICAL_OP_PART(*q ^= vp, *q ^= m & vp);
    else if (op == GWY_LOGICAL_OR)
        LOGICAL_OP_PART(*q |= vp, *q |= m & vp);
    else if (op == GWY_LOGICAL_NOR)
        LOGICAL_OP_PART(*q = ~(*q | vp), *q = (*q & ~m) | (~(*q | vp) & m));
    else if (op == GWY_LOGICAL_NXOR)
        LOGICAL_OP_PART(*q = ~(*q ^ vp), *q = (*q & ~m) | (~(*q ^ vp) & m));
    else if (op == GWY_LOGICAL_NB)
        LOGICAL_OP_PART(*q = ~vp, *q = (*q & ~m) | (~vp & m));
    else if (op == GWY_LOGICAL_CIMPL)
        LOGICAL_OP_PART(*q |= ~vp, *q |= ~vp & m);
    // GWY_LOGICAL_NA cannot get here.
    else if (op == GWY_LOGICAL_IMPL)
        LOGICAL_OP_PART(*q = ~*q | vp, *q = (*q & ~m) | ((~*q | vp) & m));
    else if (op == GWY_LOGICAL_NAND)
        LOGICAL_OP_PART(*q = ~(*q & vp),  *q = (*q & ~m) | (~(*q & vp) & m));
    // GWY_LOGICAL_ONE cannot get here.
    else {
        g_assert_not_reached();
    }
}

/**
 * gwy_mask_field_part_logical:
 * @field: A two-dimensional mask field to modify and the first operand of
 *         the logical operation.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @operand: A two-dimensional mask field representing second operand of the
 *           logical operation.  It can be %NULL for degenerate operations that
 *           do not depend on the second operand.
 * @opcol: Operand column in @dest.
 * @oprow: Operand row in @dest.
 * @op: Logical operation to perform.
 *
 * Modifies a rectangular part of a mask field by logical operation with
 * another mask field.
 *
 * The copied rectangle is defined by @rectangle and it is modified using data
 * in @operand starting from (@opcol, @oprow).  Note that although this method
 * resembles gwy_mask_field_copy() the arguments convention is different: the
 * destination comes first then the operand, similarly to in
 * gwy_mask_field_logical().
 *
 * There are no limitations on the row and column indices or dimensions.  Only
 * the part of the rectangle that is corresponds to data inside @field and
 * @operand is modified.  This can also mean nothing is modified at all.
 *
 * If @operand is equal to @field, the areas may not overlap.
 **/
void
gwy_mask_field_part_logical(GwyMaskField *field,
                            const GwyRectangle *rectangle,
                            const GwyMaskField *operand,
                            guint opcol,
                            guint oprow,
                            GwyLogicalOperator op)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    g_return_if_fail(op <= GWY_LOGICAL_ONE);

    guint col, row, width, height;
    if (rectangle) {
        col = rectangle->col;
        row = rectangle->row;
        width = rectangle->width;
        height = rectangle->height;
        if (col >= field->xres || row >= field->yres)
            return;
        width = MIN(width, field->xres - col);
        height = MIN(height, field->yres - row);
    }
    else {
        col = row = 0;
        width = field->xres;
        height = field->yres;
    }

    if (opcol >= operand->xres || oprow >= operand->yres)
        return;
    width = MIN(width, operand->xres - opcol);
    height = MIN(height, operand->yres - oprow);
    if (!width || !height)
        return;

    if (op == GWY_LOGICAL_A)
        return;

    GwyRectangle rect = { col, row, width, height };
    if (op == GWY_LOGICAL_ZERO) {
        gwy_mask_field_fill(field, &rect, FALSE);
        return;
    }
    if (op == GWY_LOGICAL_B) {
        rect.col = opcol;
        rect.row = oprow;
        gwy_mask_field_copy(operand, &rect, field, col, row);
        return;
    }
    if (op == GWY_LOGICAL_NA) {
        gwy_mask_field_invert(field, &rect);
        return;
    }
    if (op == GWY_LOGICAL_ONE) {
        gwy_mask_field_fill(field, &rect, TRUE);
        return;
    }

    g_return_if_fail(GWY_IS_MASK_FIELD(operand));

    logical_part(operand, opcol, oprow, width, height, field, col, row, op);
    gwy_mask_field_invalidate(field);
}

static void
shrink_row(const guint32 *u,
           const guint32 *p,
           const guint32 *d,
           guint32 m0,
           guint len,
           guint end,
           gboolean from_borders,
           guint32 *q)
{
    guint32 v, vl, vr;

    if (!len) {
        v = *p & m0;
        vl = (v SHR 1) | (from_borders ? 0 : (v & NTH_BIT(0)));
        vr = (v SHL 1) | (from_borders ? 0 : (v & NTH_BIT(end-1)));
        *q = v & vl & vr & *u & *d;
        return;
    }

    v = *p;
    vl = (v SHR 1) | (from_borders ? 0 : (v & NTH_BIT(0)));
    vr = (v SHL 1) | (*(p+1) SHR 0x1f);
    *q = v & vl & vr & *u & *d;
    q++, d++, p++, u++;

    for (guint j = 1; j < len; j++, p++, q++, u++, d++) {
        v = *p;
        vl = (v SHR 1) | (*(p-1) SHL 0x1f);
        vr = (v SHL 1) | (*(p+1) SHR 0x1f);
        *q = v & vl & vr & *u & *d;
    }

    v = *p & m0;
    vl = (v SHR 1) | (*(p-1) SHL 0x1f);
    vr = (v SHL 1) | (from_borders ? 0 : (v & NTH_BIT(end-1)));
    *q = v & vl & vr & *u & *d;
}

/**
 * gwy_mask_field_shrink:
 * @field: A two-dimensional mask field.
 * @from_borders: %TRUE to shrink grains from field borders.
 *
 * Shrinks grains in a mask field by one pixel from all four directions.
 **/
void
gwy_mask_field_shrink(GwyMaskField *field,
                      gboolean from_borders)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));

    guint stride = field->stride;
    guint rowsize = stride * sizeof(guint32);
    if (from_borders && field->yres <= 2) {
        memset(field->data, 0x00, rowsize * field->yres);
        gwy_mask_field_invalidate(field);
        return;
    }

    const guint end = (field->xres & 0x1f) ? field->xres & 0x1f : 0x20;
    const guint32 m0 = MAKE_MASK(0, end);
    const guint len = (field->xres >> 5) - (end == 0x20 ? 1 : 0);

    if (field->yres == 1) {
        guint32 *row = g_slice_alloc(rowsize);
        memcpy(row, field->data, rowsize);
        shrink_row(row, row, row, m0, len, end, from_borders, field->data);
        g_slice_free1(rowsize, row);
        gwy_mask_field_invalidate(field);
        return;
    }

    guint32 *prev = g_slice_alloc(rowsize);
    guint32 *row = g_slice_alloc(rowsize);

    memcpy(prev, field->data, rowsize);
    if (from_borders)
        memset(field->data, 0x00, rowsize);
    else {
        guint32 *q = field->data;
        guint32 *next = q + stride;
        shrink_row(prev, prev, next, m0, len, end, from_borders, q);
    }

    for (guint i = 1; i+1 < field->yres; i++) {
        guint32 *q = field->data + i*stride;
        guint32 *next = q + stride;
        memcpy(row, q, rowsize);
        shrink_row(prev, row, next, m0, len, end, from_borders, q);
        GWY_SWAP(guint32*, prev, row);
    }

    if (from_borders)
        memset(field->data + (field->yres - 1)*stride, 0x00, rowsize);
    else {
        guint32 *q = field->data + (field->yres - 1)*stride;
        memcpy(row, q, rowsize);
        shrink_row(prev, row, row, m0, len, end, from_borders, q);
    }

    g_slice_free1(rowsize, row);
    g_slice_free1(rowsize, prev);
    gwy_mask_field_invalidate(field);
}

static void
grow_row(const guint32 *u,
         const guint32 *p,
         const guint32 *d,
         guint32 m0,
         guint len,
         guint end,
         guint32 *q)
{
    guint32 v, vl, vr;

    if (!len) {
        v = *p & m0;
        vl = v SHR 1;
        vr = v SHL 1;
        *q = v | vl | vr | *u | *d;
        return;
    }

    v = *p;
    vl = v SHR 1;
    vr = (v SHL 1) | (*(p+1) SHR 0x1f);
    *q = v | vl | vr | *u | *d;
    q++, d++, p++, u++;

    for (guint j = 1; j < len; j++, p++, q++, u++, d++) {
        v = *p;
        vl = (v SHR 1) | (*(p-1) SHL 0x1f);
        vr = (v SHL 1) | (*(p+1) SHR 0x1f);
        *q = v | vl | vr | *u | *d;
    }

    if (!end)
        return;

    v = *p & m0;
    vl = (v SHR 1) | (*(p-1) SHL 0x1f);
    vr = v SHL 1;
    *q = v | vl | vr | *u | *d;
}

static void
grow_field(GwyMaskField *field)
{
    guint stride = field->stride;
    guint rowsize = stride * sizeof(guint32);

    const guint end = field->xres & 0x1f;
    const guint32 m0 = MAKE_MASK(0, end);
    const guint len = field->xres >> 5;

    if (field->yres == 1) {
        guint32 *row = g_slice_alloc(rowsize);
        memcpy(row, field->data, rowsize);
        grow_row(row, row, row, m0, len, end, field->data);
        g_slice_free1(rowsize, row);
        return;
    }

    guint32 *prev = g_slice_alloc(rowsize);
    guint32 *row = g_slice_alloc(rowsize);
    guint32 *q, *next;

    q = field->data;
    memcpy(prev, q, rowsize);
    next = q + stride;
    grow_row(prev, prev, next, m0, len, end, q);

    for (guint i = 1; i+1 < field->yres; i++) {
        q = field->data + i*stride;
        next = q + stride;
        memcpy(row, q, rowsize);
        grow_row(prev, row, next, m0, len, end, q);
        GWY_SWAP(guint32*, prev, row);
    }

    q = field->data + (field->yres - 1)*stride;
    memcpy(row, q, rowsize);
    grow_row(prev, row, row, m0, len, end, q);

    g_slice_free1(rowsize, row);
    g_slice_free1(rowsize, prev);
}

static void
prevent_grain_merging(GwyMaskField *field)
{
    /* We know in the grains did not touch before the growth step.
     * Therefore, we examine pixels that are in the mask now but were not
     * in the last iteration, i.e. grains[k] == 0 but data[k] is set */
    guint xres = field->xres, yres = field->yres;
    guint *grains = field->priv->grains;
    for (guint i = 0; i < yres; i++) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(field, iter, 0, i);
        for (guint j = 0; j < xres; j++) {
            guint k = i*xres + j;
            if (!grains[k] && gwy_mask_iter_get(iter)) {
                guint g1 = i          ? grains[k-xres] : 0;
                guint g2 = j          ? grains[k-1]    : 0;
                guint g3 = j+1 < xres ? grains[k+1]    : 0;
                guint g4 = i+1 < yres ? grains[k+xres] : 0;
                /* If all nonzero grain numbers are equal they are also equal
                 * to this value. */
                guint gno = g1 | g2 | g3 | g4;
                if ((!g1 || g1 == gno)
                    && (!g2 || g2 == gno)
                    && (!g3 || g3 == gno)
                    && (!g4 || g4 == gno))
                    grains[k] = gno;
                else {
                    /* Now we have a conflict and it has to be resolved.
                     * We just get rid of this pixel. */
                    gwy_mask_iter_set(iter, FALSE);
                }
            }
            gwy_mask_iter_next(iter);
        }
    }
}

/**
 * gwy_mask_field_grow:
 * @field: A two-dimensional mask field.
 * @separate_grains: %TRUE to prevent merging of grains, their growth stops
 *                   if they should touch another grain.
 *
 * Grows grains in a mask field by one pixel from all four directions.
 **/
void
gwy_mask_field_grow(GwyMaskField *field,
                    gboolean separate_grains)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    // Separated grain growth needs numbered grains but it also updates the
    // numbers to the valid state again.
    if (separate_grains)
        gwy_mask_field_number_grains(field, NULL);
    grow_field(field);
    if (separate_grains) {
        prevent_grain_merging(field);
        GWY_FREE(field->priv->graindata);
    }
    else
        gwy_mask_field_invalidate(field);
}

// GCC has a built-in __builtin_popcount() but for some reason it does not
// inline the code.  So whatever clever it does, this makes the built-in to be
// more than three times slower than this expanded implementation.  See
// tests/mask-field.c if you want to play with the table size and/or table item
// size.
static inline guint
count_set_bits(guint32 x)
{
    static const guint16 table[256] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
    };
    guint count = 0;
    count += table[x & 0xff];
    x >>= 8;
    count += table[x & 0xff];
    x >>= 8;
    count += table[x & 0xff];
    x >>= 8;
    count += table[x & 0xff];
    return count;
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
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @value: %TRUE to count ones, %FALSE to count zeroes.
 *
 * Counts set or unset bits in a rectangular part of a mask field.
 *
 * Returns: The number of bits equal to @value.
 **/
guint
gwy_mask_field_part_count(const GwyMaskField *field,
                          const GwyRectangle *rectangle,
                          gboolean value)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), 0);

    guint col, row, width, height;
    if (rectangle) {
        col = rectangle->col;
        row = rectangle->row;
        width = rectangle->width;
        height = rectangle->height;
        g_return_val_if_fail(col + width <= field->xres, 0);
        g_return_val_if_fail(row + height <= field->yres, 0);
        if (!width || !height)
            return 0;
    }
    else {
        col = row = 0;
        width = field->xres;
        height = field->yres;
    }

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
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @value: %TRUE to count ones, %FALSE to count zeroes.
 * @counts: Array of length @height to store the counts in individual rows to.
 *
 * Counts set or unset bits in each row of a rectangular part of a mask field.
 *
 * Returns: The number of bits equal to @value in the entire mask field part,
 *          i.e. the same value as gwy_mask_field_part_count() returns.
 **/
guint
gwy_mask_field_count_rows(const GwyMaskField *field,
                          const GwyRectangle *rectangle,
                          gboolean value,
                          guint *counts)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), 0);

    guint col, row, width, height;
    if (rectangle) {
        col = rectangle->col;
        row = rectangle->row;
        width = rectangle->width;
        height = rectangle->height;
        g_return_val_if_fail(col + width <= field->xres, 0);
        g_return_val_if_fail(row + height <= field->yres, 0);
        if (!width || !height)
            return 0;
    }
    else {
        col = row = 0;
        width = field->xres;
        height = field->yres;
    }
    g_return_val_if_fail(counts, 0);

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
 * #GwyMaskField represents two dimensional bit mask often used to specify the
 * area to include or exclude in #GwyField methods.
 *
 * The mask is stored in a flat array #GwyMaskField-struct.data of #guint32
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
 * #GwyMaskField.data directly.
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
 * The #GwyMaskField struct contains some public mask_fields that can be
 * directly accessed for reading.  To set them, you must use the mask field
 * methods.
 **/

/**
 * GwyMaskFieldClass:
 *
 * Class of two-dimensional bit masks.
 **/

/**
 * GwyMaskingType:
 * @GWY_MASK_EXCLUDE: Exclude data under mask, i.e. take into account only
 *                    data not covered by the mask.
 * @GWY_MASK_INCLUDE: Take into account only data under the mask.
 * @GWY_MASK_IGNORE: Ignore mask, if present, and use all data.
 *
 * Mask interpretation in procedures that can apply masks.
 **/

/**
 * GwyLogicalOperator:
 * @GWY_LOGICAL_ZERO: Always zero, mask clearing.
 * @GWY_LOGICAL_AND: Logical conjuction @A ∧ @B, mask intersection.
 * @GWY_LOGICAL_NIMPL: Negated implication @A ⇏ @B, ¬@A ∧ @B.
 * @GWY_LOGICAL_A: First operand @A, no change to the mask.
 * @GWY_LOGICAL_NCIMPL: Negated converse implication @A ⇍ @B, @A ∧ ¬@B, mask
 *                      subtraction.
 * @GWY_LOGICAL_B: Second operand @B, mask copying.
 * @GWY_LOGICAL_XOR: Exclusive disjunction @A ⊻ @B, symmetrical mask
 *                   subtraction.
 * @GWY_LOGICAL_OR: Disjunction @A ∨ @B, mask union.
 * @GWY_LOGICAL_NOR: Negated disjunction @A ⊽ @B.
 * @GWY_LOGICAL_NXOR: Negated exclusive disjunction ¬(@A ⊻ @B).
 * @GWY_LOGICAL_NB: Negated second operand ¬@B, copying of inverted mask.
 * @GWY_LOGICAL_CIMPL: Converse implication @A ⇐ @B, @A ∨ ¬@B.
 * @GWY_LOGICAL_NA: Negated first operand ¬@A, mask inversion.
 * @GWY_LOGICAL_IMPL: Implication @A ⇒ @B, ¬@A ∨ @B.
 * @GWY_LOGICAL_NAND: Negated conjuction @A ⊼ @B.
 * @GWY_LOGICAL_ONE: Always one, mask filling.
 *
 * Logical operators applicable to masks.
 *
 * All possible 16 logical operators are available although some are not as
 * useful as others.  If a common mask operation corresponds to the logical
 * operator it is mentioned in the list.
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
 * GwyMaskIter:
 * @p: Pointer to the current mask data item.
 * @bit: The current bit, i.e. value with always exactly one bit set.
 *
 * Mask iterator.
 *
 * The mask iterator is another method of accessing individual mask pixels
 * within a contiguous block, suitable especially for sequential processing.
 * It can be used for reading and writing bits and it is possible to move it
 * one pixel forward or backward within the block with gwy_mask_iter_next() or
 * gwy_mask_iter_prev(), respectively.
 *
 * The following example demonstrates the typical use on finding the minimum of
 * masked two-dimensional data.  Notice how the iterator is initialised for
 * each row because the mask field rows do not form one contiguous block
 * although each individual row is contiguous.
 * |[
 * gdouble min = G_MAXDOUBLE;
 * for (guint i = 0; i < field->height; i++) {
 *     const gdouble *d = field->data + i*field->xres;
 *     GwyMaskIter iter;
 *     gwy_mask_field_iter_init(mask, iter, 0, i);
 *     for (guint j = 0; j < field->xres; j++) {
 *         if (gwy_mask_iter_get(iter)) {
 *             if (min > d[j])
 *                 min = d[j];
 *         }
 *         gwy_mask_iter_next(iter);
 *     }
 * }
 * ]|
 * The iterator is represented by a very simple structure that is supposed to
 * be allocated as an automatic variable and passed/copied by value.  It can be
 * re-initialised any number of times, even to iterate in completely different
 * mask objects.  It can be simply forgotten when no longer useful (i.e. there
 * is no teardown function).
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

/**
 * gwy_mask_iter_next:
 * @iter: Mask iterator.  It must be an identifier.
 *
 * Moves a mask iterator one pixel right.
 *
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
 * The caller must ensure the position does not leave the contiguous mask
 * block.
 **/

/**
 * gwy_mask_iter_prev:
 * @iter: Mask iterator.  It must be an identifier.
 *
 * Moves a mask iterator one pixel left.
 *
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
 * The caller must ensure the position does not leave the contiguous mask
 * block.
 **/

/**
 * gwy_mask_iter_get:
 * @iter: Mask iterator.  It must be an identifier.
 *
 * Obtains the value a mask iterator points to.
 *
 * No argument validation is performed.
 *
 * Returns: Nonzero value (not necessarily 1) if the mask bit is set, zero if
 *          it is unset.
 **/

/**
 * gwy_mask_iter_set:
 * @iter: Mask iterator.  It must be an identifier.
 * @value: Nonzero value to set the bit, zero to clear it.
 *
 * Sets the value a mask iterator points to.
 *
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
 *
 * This is a low-level macro and it does not invalidate the mask object.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
