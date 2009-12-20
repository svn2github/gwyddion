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
static void         gwy_mask_field_done             (GwySerializable *serializable);
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
                           "Row stride of the mask field in #guint32.",
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
        v = ((v >> 4) & 0x0F0F0F0Fu) | ((v & 0x0F0F0F0Fu) << 4);
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
    MaskField *priv = maskfield->priv;

    GwyMaskField *duplicate = gwy_mask_field_new_sized(maskfield->xres,
                                                       maskfield->yres,
                                                       FALSE);
    MaskField *dpriv = duplicate->priv;

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
    MaskField *spriv = src->priv, *dpriv = dest->priv;

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
 * @dest: Destination two-dimensional mask field.
 * @src: Source two-dimensional mask field.
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
 * the part of the rectangle that is corrsponds to data inside @src and @dest
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
    else {
        // How many bits we shift right from src to dest
        const guint32 *sbase = src->data + src->stride*row + (col >> 5);
        guint32 *dbase = dest->data + dest->stride*destrow + (destcol >> 5);
        guint soff = col & 0x1f, doff = destcol & 0x1f;
        guint k = (doff + 0x20 - soff) & 0x1f;
        guint kk = 0x20 - k;
        guint32 v;
        if (doff == soff) {
            const guint32 m0 = ((0xffffffff >> (0x20 - doff - width))
                                & (~((1 << doff) - 1)));
            const guint32 m1 = 0xffffffff & ~((1 << doff) - 1);
            const guint32 m2 = (1 << ((width & 0x1f) - 0x20 + doff)) - 1;
            for (guint i = 0; i < height; i++) {
                const guint32 *p = sbase + i*src->stride;
                guint32 *q = dbase + i*dest->stride;
                guint j = width;
                if (doff + width <= 0x20) {
                    // All within one word, mask possibly incomplete from both
                    // sides
                    *q = (*q & ~m0) | (*p & m0);
                }
                else {
                    // Incomplete leftmost word
                    *q = (*q & ~m1) | (*p & m1);
                    j -= 0x20 - doff, p++, q++;
                    // Complete words
                    while (j >= 0x20) {
                        *q = *p;
                        j -= 0x20 - doff, p++, q++;
                    }
                    // Incomplete rightmost word
                    if (j)
                        *q = (*q & ~m2) | (*p & m2);
                }
            }
        }
        else if (doff < soff) {
            const guint32 m0 = ~((1 << soff) - 1) >> kk;
            const guint32 m1 = ((1 << kk) - 1) << k;
            const guint32 m2 = (1 << k) - 1;
            for (guint i = 0; i < height; i++) {
                const guint32 *p = sbase + i*src->stride;
                guint32 *q = dbase + i*dest->stride;
                guint j = width;
                // Incomplete leftmost word in the source
                // XXX: Fails if (width < 0x20 - soff) because the soruce has
                // even less bits than we copy here
                v = *p >> kk;
                *q = (*q & ~m0) | (*p & m0);
                j -= (0x20 - soff), p++;
                // Complete words in the source
                while (j >= 0x20) {
                    v = *p << k;
                    *q = (*q & ~m1) | (*p & m1);
                    j -= kk, q++;
                    v = *p >> kk;
                    *q = (*q & ~m2) | (*p & m2);
                    j -= k, p++;
                }
                // Incomplete rightmost word in the source
                if (j < kk) {
                    if (j) {
                        guint32 m = ((1 << j) - 1) << k;
                        v = *p << k;
                        *q = (*q & ~m) | (*p & m);
                    }
                }
                else {
                    v = *p << k;
                    *q = (*q & ~m1) | (*p & m1);
                    j -= kk, q++;
                    if (j) {
                        guint32 m = (1 << j) - 1;
                        v = *p >> kk;
                        *q = (*q & ~m) | (*p & m);
                    }
                }
            }
        }
        else {
            const guint32 m0 = 0xffffffff & ~((1 << (0x20 - doff)) - 1);
            const guint32 m1 = (1 << k) - 1;
            const guint32 m2 = ((1 << kk) - 1) << k;
            for (guint i = 0; i < height; i++) {
                const guint32 *p = sbase + i*src->stride;
                guint32 *q = dbase + i*dest->stride;
                guint j = width;
                // Incomplete leftmost word in the destination
                // XXX: Fails if (width < 0x20 - doff) because the destination
                // has even less bits than we copy here
                v = *p << k;
                *q = (*q & ~m0) | (*p & m0);
                j -= (0x20 - doff), q++;
                // Complete words in the destination
                while (j >= 0x20) {
                    v = *p >> kk;
                    *q = (*q & ~m1) | (*p & m1);
                    j -= k, p++;
                    v = *p << k;
                    *q = (*q & ~m2) | (*p & m2);
                    j -= kk, q++;
                }
                // Incomplete rightmost word in the source
                if (j < k) {
                    if (j) {
                        guint32 m = (1 << j) - 1;
                        v = *p >> kk;
                        *q = (*q & ~m) | (*p & m);
                    }
                }
                else {
                    v = *p >> kk;
                    *q = (*q & ~m1) | (*p & m1);
                    j -= k, p++;
                    if (j) {
                        guint32 m = ((1 << j) - 1) << k;
                        v = *p << k;
                        *q = (*q & ~m) | (*p & m);
                    }
                }
            }
        }
    }
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
    // This is much faster.
    if (width == maskfield->xres && height == maskfield->yres) {
        gwy_mask_field_fill(maskfield, value);
        return;
    }

    // TODO:
}

#define __LIBGWY_MASK_FIELD_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: maskfield
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
 *
 * It is possible to get and set individual bits with macros
 * gwy_mask_field_get() and gwy_mask_field_set().  However, in
 * performance-sensitive calculations it is preferable to directly obtain
 * the packed values and iterate over bits.
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
 * No argument validation is performed.  If you process the data in a loop,
 * you are encouraged to access #GwyMaskField-struct.data directly.
 *
 * Returns: Nonzero value (not necessarily 1) if the bit is set, zero it it's
 *          unset.
 **/

/**
 * gwy_mask_field_get:
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
 * No argument validation is performed.  If you process the data in a loop,
 * you are encouraged to access #GwyMaskField-struct.data directly.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
