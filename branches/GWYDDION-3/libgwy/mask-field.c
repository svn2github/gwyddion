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

#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/serialize.h"
#include "libgwy/mask-field.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/processing-internal.h"

/* Note we use run-time conditions for endianess-branching even though it is
 * known at compile time.  This is to get the big-endian branch at least
 * syntax-checked.  A good optimizing compiler then eliminates the unused
 * branch entirely so we do not need to care. */
#if (G_BYTE_ORDER != G_LITTLE_ENDIAN && G_BYTE_ORDER != G_BIG_ENDIAN)
#error Byte order used on this system is not supported.
#endif

#define STATIC \
    (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)

#define ALL_SET ((guint32)0xffffffffu)
#define ALL_CLEAR ((guint32)0x00000000u)

/* SHR moves the bits right in the mask field, which means towards the higher
 * bits on little-endian and towards the lower bits on big endian. */
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define FIRST_BIT ((guint32)0x1u)
#define SHR <<
#define SHL >>
#endif

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
#define FIRST_BIT ((guint32)0x80000000u)
#define SHR >>
#define SHL <<
#endif

/* Make a 32bit bit mask with nbits set, starting from bit firstbit.  The
 * lowest bit is 0, the highest 0x1f for little endian and the reverse for
 * big endian. */
#define NTH_BIT(n) (FIRST_BIT SHR (n))
#define MAKE_MASK(firstbit, nbits) \
    (nbits ? ((ALL_SET SHL (0x20 - (nbits))) SHR (firstbit)) : 0u)

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

struct _GwyMaskFieldPrivate {
    guint *grains;
    guint *graindata;
    guint ngrains;
    guint32 *serialized_swapped;
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
                           G_PARAM_READABLE | STATIC));

    g_object_class_install_property
        (gobject_class,
         PROP_YRES,
         g_param_spec_uint("y-res",
                           "Y resolution",
                           "Pixel height of the mask field.",
                           1, G_MAXUINT, 1,
                           G_PARAM_READABLE | STATIC));

    g_object_class_install_property
        (gobject_class,
         PROP_STRIDE,
         g_param_spec_uint("stride",
                           "Stride",
                           "Row stride of the mask field in items, i.e. "
                           "guint32s.",
                           1, G_MAXUINT, 2,
                           G_PARAM_READABLE | STATIC));

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
    return (width + 0x3f) >> 5;
}

static void
swap_bits_uint32(guint32 *data,
                 gsize n)
{
    while (n--) {
        guint32 v = *data;
        v = ((v >> 1) & 0x55555555u) | ((v & 0x55555555u) << 1);
        v = ((v >> 2) & 0x33333333u) | ((v & 0x33333333u) << 2);
        v = ((v >> 4) & 0x0f0f0f0fu) | ((v & 0x0f0f0f0fu) << 4);
        *(data++) = GUINT32_SWAP_LE_BE(v);
    }
}

static void
gwy_mask_field_init(GwyMaskField *maskfield)
{
    maskfield->priv = G_TYPE_INSTANCE_GET_PRIVATE(maskfield,
                                                  GWY_TYPE_MASK_FIELD,
                                                  MaskField);
    maskfield->xres = maskfield->yres = 1;
    maskfield->stride = stride_for_width(maskfield->xres);
    maskfield->data = g_new0(guint32, maskfield->stride * maskfield->yres);
}

static void
gwy_mask_field_finalize(GObject *object)
{
    GwyMaskField *maskfield = GWY_MASK_FIELD(object);
    GWY_FREE(maskfield->data);
    GWY_FREE(maskfield->priv->grains);
    GWY_FREE(maskfield->priv->graindata);
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

    GwyMaskField *maskfield = GWY_MASK_FIELD(serializable);
    GwySerializableItem *it = items->items + items->n;
    gsize n = maskfield->stride * maskfield->yres;

    *it = serialize_items[0];
    it->value.v_uint32 = maskfield->xres;
    it++, items->n++;

    *it = serialize_items[1];
    it->value.v_uint32 = maskfield->yres;
    it++, items->n++;

    *it = serialize_items[2];
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        it->value.v_uint32_array = maskfield->data;
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        MaskField *priv = maskfield->priv;
        priv->serialized_swapped = g_new(guint32, n);
        swap_bits_uint32(priv->serialized_swapped, n);
        it->value.v_uint32_array = priv->serialized_swapped;
    }
    it->array_size = n;
    it++, items->n++;

    return N_ITEMS;
}

static void
gwy_mask_field_done(GwySerializable *serializable)
{
    GwyMaskField *maskfield = GWY_MASK_FIELD(serializable);
    GWY_FREE(maskfield->priv->serialized_swapped);
}

static gboolean
gwy_mask_field_construct(GwySerializable *serializable,
                         GwySerializableItems *items,
                         GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, "GwyMaskField",
                                 error_list);

    GwyMaskField *maskfield = GWY_MASK_FIELD(serializable);

    if (G_UNLIKELY(!its[0].value.v_uint32 || !its[1].value.v_uint32)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Mask field dimensions %u×%u are invalid."),
                           its[0].value.v_uint32, its[1].value.v_uint32);
        return FALSE;
    }

    gsize n = stride_for_width(its[0].value.v_uint32) * its[1].value.v_uint32;
    if (G_UNLIKELY(n != its[2].array_size)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Mask field dimensions %u×%u do not match "
                             "data size %lu."),
                           its[0].value.v_uint32, its[1].value.v_uint32,
                           (gulong)its[2].array_size);
        return FALSE;
    }

    maskfield->xres = its[0].value.v_uint32;
    maskfield->yres = its[1].value.v_uint32;
    maskfield->stride = stride_for_width(maskfield->xres);
    g_free(maskfield->data);
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        maskfield->data = its[2].value.v_uint32_array;
        its[2].value.v_uint32_array = NULL;
        its[2].array_size = 0;
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        maskfield->data = g_memdup(its[2].value.v_uint32_array,
                                   n*sizeof(guint32));
        swap_bits_uint32(maskfield->data, n);
    }

    return TRUE;
}

static GObject*
gwy_mask_field_duplicate_impl(GwySerializable *serializable)
{
    GwyMaskField *maskfield = GWY_MASK_FIELD(serializable);
    //MaskField *priv = maskfield->priv;

    GwyMaskField *duplicate = gwy_mask_field_new_sized(maskfield->xres,
                                                       maskfield->yres,
                                                       FALSE);
    //MaskField *dpriv = duplicate->priv;

    gsize n = maskfield->stride * maskfield->yres;
    memcpy(duplicate->data, maskfield->data, n*sizeof(guint32));
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
        g_free(dest->data);
        dest->data = g_new(guint32, n);
    }
    memcpy(dest->data, src->data, n*sizeof(guint32));
    dest->xres = src->xres;
    dest->yres = src->yres;
    dest->stride = src->stride;
    // TODO: Duplicate precalculated grain data too.

    GObject *object = G_OBJECT(dest);
    g_object_freeze_notify(object);
    for (guint i = 0; i < nn; i++)
        g_object_notify(object, notify[i]);
    g_object_thaw_notify(object);
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
    GwyMaskField *maskfield = GWY_MASK_FIELD(object);

    switch (prop_id) {
        case PROP_XRES:
        g_value_set_uint(value, maskfield->xres);
        break;

        case PROP_YRES:
        g_value_set_uint(value, maskfield->yres);
        break;

        case PROP_STRIDE:
        g_value_set_uint(value, maskfield->stride);
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
 *         unitialized.
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

    GwyMaskField *maskfield = g_object_newv(GWY_TYPE_MASK_FIELD, 0, NULL);
    g_free(maskfield->data);
    maskfield->xres = xres;
    maskfield->yres = yres;
    maskfield->stride = stride_for_width(maskfield->xres);
    if (clear)
        maskfield->data = g_new0(guint32, maskfield->stride * maskfield->yres);
    else
        maskfield->data = g_new(guint32, maskfield->stride * maskfield->yres);
    return maskfield;
}

/**
 * gwy_mask_field_new_part:
 * @maskfield: A two-dimensional mask field.
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 *
 * Creates a new two-dimensional mask field as a rectangular part of another
 * mask field.
 *
 * The rectangle of size @width×@height starting at (@col,@row) must be
 * entirely contained in @maskfield.  Both dimensions must be non-zero.
 *
 * Data are physically copied, i.e. changing the new mask field data does not
 * change @maskfield's data and vice versa.
 *
 * Returns: A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new_part(const GwyMaskField *maskfield,
                        guint col,
                        guint row,
                        guint width,
                        guint height)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(maskfield), NULL);
    g_return_val_if_fail(width && height, NULL);
    g_return_val_if_fail(col + width <= maskfield->xres, NULL);
    g_return_val_if_fail(row + height <= maskfield->yres, NULL);

    if (width == maskfield->xres && height == maskfield->yres)
        return gwy_mask_field_duplicate(maskfield);

    GwyMaskField *part = gwy_mask_field_new_sized(width, height, FALSE);
    gwy_mask_field_part_copy(maskfield, col, row, width, height, part, 0, 0);
    return part;
}

/**
 * gwy_mask_field_new_resampled:
 * @maskfield: A two-dimensional mask field.
 * @xres: Desired X resolution.
 * @yres: Desired Y resolution.
 *
 * Creates a new two-dimensional mask field by resampling another mask field.
 *
 * Returns: A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new_resampled(const GwyMaskField *maskfield,
                             guint xres,
                             guint yres)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(maskfield), NULL);
    g_return_val_if_fail(xres && yres, NULL);
    if (xres == maskfield->xres && yres == maskfield->yres)
        return gwy_mask_field_duplicate(maskfield);

    GwyMaskField *dest;
    dest = gwy_mask_field_new_sized(xres, yres, FALSE);
    // TODO

    return dest;
}

/**
 * gwy_mask_field_new_from_field:
 * @field: A two-dimensional data field.
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
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
                              guint col,
                              guint row,
                              guint width,
                              guint height,
                              gdouble lower,
                              gdouble upper,
                              gboolean complement)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    g_return_val_if_fail(width && height, NULL);
    g_return_val_if_fail(col + width <= field->xres, NULL);
    g_return_val_if_fail(row + height <= field->yres, NULL);

    GwyMaskField *maskfield = gwy_mask_field_new_sized(width, height, FALSE);
    const gdouble *fbase = field->data + field->xres*row + col;
    for (guint i = 0; i < height; i++) {
        const gdouble *p = fbase + i*field->xres;
        guint32 *q = maskfield->data + i*maskfield->stride;
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
    return maskfield;
}

/**
 * gwy_mask_field_data_changed:
 * @maskfield: A two-dimensional mask field.
 *
 * Emits signal GwyMaskField::data-changed on a mask field.
 **/
void
gwy_mask_field_data_changed(GwyMaskField *maskfield)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(maskfield));
    g_signal_emit(maskfield, mask_field_signals[DATA_CHANGED], 0);
}

/**
 * gwy_mask_field_copy:
 * @src: Source two-dimensional mask field.
 * @dest: Destination two-dimensional mask field.
 *
 * Copies the data of a mask field to another mask field of the same
 * dimensions.
 *
 * Only the data are copied.  To make a mask field completely identical to
 * another, including change of dimensions, you can use
 * gwy_mask_field_assign().
 **/
void
gwy_mask_field_copy(const GwyMaskField *src,
                    GwyMaskField *dest)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(src));
    g_return_if_fail(GWY_IS_MASK_FIELD(dest));
    g_return_if_fail(dest->xres == src->xres && dest->yres == src->yres);
    gsize n = src->stride * src->yres;
    memcpy(dest->data, src->data, n*sizeof(guint32));
}

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
    if (width <= 0x20 - doff) {
        const guint32 m0d = MAKE_MASK(doff, width);
        for (guint i = 0; i < height; i++) {
            const guint32 *p = sbase + i*src->stride;
            guint32 *q = dbase + i*dest->stride;
            guint32 v0 = *p;
            if (send && send <= soff) {
                guint32 v1 = *(++p);
                *q = (*q & ~m0d) | (((v0 SHL kk) | (v1 SHR k)) & m0d);
            }
            else if (doff > soff)
                *q = (*q & ~m0d) | ((v0 SHR k) & m0d);
            else
                *q = (*q & ~m0d) | ((v0 SHL kk) & m0d);
        }
    }
    else if (doff == soff) {
        const guint32 m0d = MAKE_MASK(doff, 0x20 - doff);
        const guint32 m1d = MAKE_MASK(0, dend);
        for (guint i = 0; i < height; i++) {
            const guint32 *p = sbase + i*src->stride;
            guint32 *q = dbase + i*dest->stride;
            guint j = width;
            *q = (*q & ~m0d) | (*p & m0d);
            j -= 0x20 - doff, p++, q++;
            while (j >= 0x20) {
                *q = *p;
                j -= 0x20, p++, q++;
            }
            if (!dend)
                continue;
            *q = (*q & ~m1d) | (*p & m1d);
        }
    }
    else {
        const guint32 m0d = MAKE_MASK(doff, 0x20 - doff);
        const guint32 m1d = MAKE_MASK(0, dend);
        for (guint i = 0; i < height; i++) {
            const guint32 *p = sbase + i*src->stride;
            guint32 *q = dbase + i*dest->stride;
            guint j = width;
            guint32 v0 = *p;
            if (doff > soff) {
                *q = (*q & ~m0d) | ((v0 SHR k) & m0d);
            }
            else {
                guint32 v1 = *(++p);
                *q = (*q & ~m0d) | (((v0 SHL kk) | (v1 SHR k)) & m0d);
                v0 = v1;
            }
            j -= (0x20 - doff), q++;
            while (j >= 0x20) {
                guint32 v1 = *(++p);
                *q = (v0 SHL kk) | (v1 SHR k);
                j -= 0x20, q++;
                v0 = v1;
            }
            if (!dend)
                continue;
            if (dend > send) {
                guint32 v1 = *(++p);
                *q = (*q & ~m1d) | (((v0 SHL kk) | (v1 SHR k)) & m1d);
            }
            else {
                *q = (*q & ~m1d) | ((v0 SHL kk) & m1d);
            }
        }
    }
}

/**
 * gwy_mask_field_part_copy:
 * @src: Source two-dimensional data mask field.
 * @col: Column index of the upper-left corner of the rectangle in @src.
 * @row: Row index of the upper-left corner of the rectangle in @src.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 * @dest: Destination two-dimensional mask field.
 * @destcol: Destination column in @dest.
 * @destrow: Destination row in @dest.
 *
 * Copies a rectangular part from one mask field to another.
 *
 * The rectangle starts at (@col, @row) in @src and its dimensions are
 * @width×@height. It is copied to @dest starting from (@destcol, @destrow).
 *
 * There are no limitations on the row and column indices or dimensions.  Only
 * the part of the rectangle that is corresponds to data inside @src and @dest
 * is copied.  This can also mean nothing is copied at all.
 *
 * If @src is equal to @dest, the areas may not overlap.
 **/
void
gwy_mask_field_part_copy(const GwyMaskField *src,
                         guint col,
                         guint row,
                         guint width,
                         guint height,
                         GwyMaskField *dest,
                         guint destcol,
                         guint destrow)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(src));
    g_return_if_fail(GWY_IS_MASK_FIELD(dest));

    if (col >= src->xres || destcol >= dest->xres
        || row >= src->yres || destrow >= dest->yres)
        return;

    width = MIN(width, src->xres - col);
    height = MIN(height, src->yres - row);
    width = MIN(width, dest->xres - destcol);
    height = MIN(height, dest->yres - destrow);
    if (!width || !height)
        return;

    if (width == src->xres
        && width == dest->xres
        && dest->stride == src->stride) {
        /* make it as fast as gwy_data_mask_field_copy() if possible */
        g_assert(col == 0 && destcol == 0);
        memcpy(dest->data + dest->stride*destrow,
               src->data + src->stride*row, src->stride*height);
    }
    else
        copy_part(src, col, row, width, height, dest, destcol, destrow);
    gwy_mask_field_invalidate(dest);
}

/**
 * gwy_mask_field_get_data:
 * @maskfield: A two-dimensional mask field.
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
gwy_mask_field_get_data(GwyMaskField *maskfield)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(maskfield), NULL);
    gwy_mask_field_invalidate(maskfield);
    return maskfield->data;
}

/**
 * gwy_mask_field_invalidate:
 * @maskfield: A two-dimensional mask field.
 *
 * Invalidates mask field grain data.
 *
 * User code should seldom need this method since all #GwyMaskField methods
 * correctly invalidate grain data when they change the mask, also
 * gwy_mask_field_get_data() does.
 **/
void
gwy_mask_field_invalidate(GwyMaskField *maskfield)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(maskfield));
    GWY_FREE(maskfield->priv->grains);
    GWY_FREE(maskfield->priv->graindata);
}

/**
 * gwy_mask_field_fill:
 * @maskfield: A two-dimensional mask field.
 * @value: Value to fill @maskfield with.
 *
 * Fills a mask field with the specified value.
 **/
void
gwy_mask_field_fill(GwyMaskField *maskfield,
                    gboolean value)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(maskfield));
    gsize n = maskfield->stride * maskfield->yres;
    memset(maskfield->data, value ? 0xff : 0x00, n*sizeof(guint32));
    gwy_mask_field_invalidate(maskfield);
}

static void
set_part(GwyMaskField *maskfield,
         guint col,
         guint row,
         guint width,
         guint height)
{
    guint32 *base = maskfield->data + maskfield->stride*row + (col >> 5);
    const guint off = col & 0x1f;
    const guint end = (col + width) & 0x1f;
    if (width <= 0x20 - off) {
        const guint32 m = MAKE_MASK(off, width);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*maskfield->stride;
            *p |= m;
        }
    }
    else {
        const guint32 m0 = MAKE_MASK(off, 0x20 - off);
        const guint32 m1 = MAKE_MASK(0, end);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*maskfield->stride;
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
clear_part(GwyMaskField *maskfield,
           guint col,
           guint row,
           guint width,
           guint height)
{
    guint32 *base = maskfield->data + maskfield->stride*row + (col >> 5);
    const guint off = col & 0x1f;
    const guint end = (col + width) & 0x1f;
    if (width <= 0x20 - off) {
        const guint32 m = ~MAKE_MASK(off, width);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*maskfield->stride;
            *p &= m;
        }
    }
    else {
        const guint32 m0 = ~MAKE_MASK(off, 0x20 - off);
        const guint32 m1 = ~MAKE_MASK(0, end);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*maskfield->stride;
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
 * gwy_mask_field_part_fill:
 * @maskfield: A two-dimensional mask field.
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 * @value: Value to fill the rectangle with.
 *
 * Fills a rectangular part of a mask field with the specified value.
 **/
void
gwy_mask_field_part_fill(GwyMaskField *maskfield,
                         guint col,
                         guint row,
                         guint width,
                         guint height,
                         gboolean value)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(maskfield));
    g_return_if_fail(col + width <= maskfield->xres);
    g_return_if_fail(row + height <= maskfield->yres);

    if (!width || !height)
        return;

    if (width == maskfield->xres) {
        g_assert(col == 0);
        memset(maskfield->data + row*maskfield->stride,
               value ? 0xff : 0x00,
               height*maskfield->stride*sizeof(guint32));
    }
    else {
        if (value)
            set_part(maskfield, col, row, width, height);
        else
            clear_part(maskfield, col, row, width, height);
    }
    gwy_mask_field_invalidate(maskfield);
}

#define LOGICAL_OP_LOOP(simple, masked) \
    do { \
        if (mask) {  \
            const guint32 *m = mask->data;  \
            for (guint i = n; i; i--, p++, q++, m++)  \
                masked; \
        }  \
        else {  \
            for (guint i = n; i; i--, p++, q++)  \
                simple;  \
        }  \
    } while (0)

/**
 * gwy_mask_field_logical:
 * @maskfield: A two-dimensional mask field to modify and the first operand of
 *             the logical operation.
 * @operand: A two-dimensional mask field representing second operand of the
 *           logical operation.  It can be %NULL for degenerate operations that
 *           do not depend on the second operand.
 * @mask: A two-dimensional mask field determining to which bits of
 *        @maskfield to apply the logical operation to.  If it is %NULL the
 *        opperation is applied to all bits (as if all bits in @mask were set).
 * @op: Logical operation to perform.
 *
 * Modifies a mask field by logical operation with another mask field.
 **/
void
gwy_mask_field_logical(GwyMaskField *maskfield,
                       const GwyMaskField *operand,
                       const GwyMaskField *mask,
                       GwyLogicalOperator op)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(maskfield));
    g_return_if_fail(op <= GWY_LOGICAL_ONE);
    if (op == GWY_LOGICAL_A)
        return;
    if (mask) {
        g_return_if_fail(GWY_IS_MASK_FIELD(mask));
        g_return_if_fail(maskfield->xres == mask->xres);
        g_return_if_fail(maskfield->yres == mask->yres);
        g_return_if_fail(maskfield->stride == mask->stride);
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
        g_return_if_fail(maskfield->xres == operand->xres);
        g_return_if_fail(maskfield->yres == operand->yres);
        g_return_if_fail(maskfield->stride == operand->stride);
    }

    guint n = maskfield->stride * maskfield->yres;
    const guint32 *p = operand->data;
    guint32 *q = maskfield->data;

    // GWY_LOGICAL_ZERO cannot have mask.
    if (op == GWY_LOGICAL_ZERO)
        gwy_mask_field_fill(maskfield, FALSE);
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
        gwy_mask_field_fill(maskfield, TRUE);
    else {
        g_assert_not_reached();
    }
    gwy_mask_field_invalidate(maskfield);
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
 * @maskfield: Data field containing positive values in grains, nonpositive
 *             in free space.
 * @ngrains: Location to store the number of the last grain, or %NULL.
 *
 * Numbers grains in a mask field.
 *
 * Returns: Array of integers of the same number of items as @maskfield
 *          (without padding) filled with grain numbers of each pixel.  Empty
 *          space is set to 0, pixels inside a grain are set to the grain
 *          number.  Grains are numbered sequentially 1, 2, 3, ...
 *          The returned array is owned by @maskfield and become invalid when
 *          the data change, gwy_mask_field_invalidate() is called or the
 *          mask field is finalized.
 **/
const guint*
gwy_mask_field_number_grains(GwyMaskField *maskfield,
                             guint *ngrains)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(maskfield), NULL);
    MaskField *priv = maskfield->priv;
    if (priv->grains) {
        if (ngrains)
            *ngrains = priv->ngrains;
        return priv->grains;
    }

    guint xres = maskfield->xres, yres = maskfield->yres;
    priv->grains = g_new(guint, xres*yres);

    // A reasonable initial size of the grain map.
    guint msize = 4*(maskfield->xres + maskfield->yres);
    guint *m = g_new0(guint, msize);

    /* Number grains with simple unidirectional grain number propagation,
     * updating map m for later full grain join */
    guint max_id = 0;
    guint end = xres & 0x1f;
    for (guint i = 0; i < yres; i++) {
        const guint32 *p = maskfield->data + i*maskfield->stride;
        guint *g = priv->grains + i*xres;
        guint *gprev = g - xres;
        guint grain_id = 0;
        for (guint j = 0; j < (xres >> 5); j++) {
            guint32 v = *(p++);
            for (guint k = 0; k < 0x20; k++, g++, gprev++) {
                if (v & FIRST_BIT) {
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
                v = v SHL 1;
            }
        }
        if (end) {
            guint32 v = *p;
            for (guint k = 0; k < end; k++, g++, gprev++) {
                if (v & FIRST_BIT) {
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
                v = v SHL 1;
            }
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
    if (ngrains)
        *ngrains = priv->ngrains;
    return priv->grains;
}

#define __LIBGWY_MASK_FIELD_C__
#include "libgwy/libgwy-aliases.c"

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
 * It is possible to get and set individual bits with macros
 * gwy_mask_field_get() and gwy_mask_field_set().  In performance-sensitive
 * code it might be preferable to directly obtain the packed values and iterate
 * over bits.  However, if you perform any non-trivial floating-point operation
 * in each pixel, the cost of the gwy_mask_field_get() bit extraction will
 * unlikely matter.
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
 * @g_object_class: Parent class.
 *
 * Class of two-dimensional bit masks.
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
 * @maskfield: A two-dimensional mask field.
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
 * @maskfield: A two-dimensional mask field.
 * @col: Column index in @maskfield.
 * @row: Row index in @maskfield.
 *
 * Obtains one bit value from a two-dimensional mask field.
 *
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
 *
 * Returns: Nonzero value (not necessarily 1) if the bit is set, zero it it's
 *          unset.
 **/

/**
 * gwy_mask_field_set:
 * @maskfield: A two-dimensional mask field.
 * @col: Column index in @maskfield.
 * @row: Row index in @maskfield.
 * @value: Nonzero value to set the bit, zero to clear it.
 *
 * Sets one bit value in a two-dimensional mask field.
 *
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
