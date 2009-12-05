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
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/main.h"
#include "libgwy/serializable.h"
#include "libgwy/serializable-boxed.h"
#include "libgwy/serialize.h"
#include "libgwy/libgwy-aliases.h"

/*
 * XXX assertions:
 * sizeof(gdouble) == sizeof(guint64)
 */

#if (GLIB_SIZEOF_SIZE_T > 8)
#error Serialization is not implemented for size_t longer than 8 bytes.
#endif

/* Note we use run-time conditions for endianess-branching even though it is
 * known at compile time.  This is to get the big-endian branch at least
 * syntax-checked.  A good optimizing compiler then eliminates the unused
 * branch entirely so we do not need to care. */
#if (G_BYTE_ORDER != G_LITTLE_ENDIAN && G_BYTE_ORDER != G_BIG_ENDIAN)
#error Byte order used on this system is not supported.
#endif

/* This is acceptable for 4k page size, not that bad for 4M page size and
 * of course good for the sizes between too. */
enum {
    GWY_SERIALIZE_BUFFER_SIZE = 65536,
};

typedef struct {
    GOutputStream *output;
    GError **error;
    gsize len;
    gsize bfree;
    guchar *data;
} GwySerializableBuffer;

static gsize                 calculate_sizes       (GwySerializableItems *items,
                                                    gsize *pos);
static void                  items_done            (const GwySerializableItems *items);
static gboolean              dump_to_stream        (const GwySerializableItems *items,
                                                    GwySerializableBuffer *buffer);
static GObject*              deserialize_memory    (const guchar *buffer,
                                                    gsize size,
                                                    gsize *bytes_consumed,
                                                    gboolean exact_size,
                                                    GwyErrorList **error_list);
static GType                 get_serializable      (const gchar *typename,
                                                    gpointer *classref,
                                                    const GwySerializableInterface **iface,
                                                    GwyErrorList **error_list);
static GType                 get_serializable_boxed(const gchar *typename,
                                                    GwyErrorList **error_list);
static GwySerializableItems* unpack_items          (const guchar *buffer,
                                                    gsize size,
                                                    GwyErrorList **error_list);
static void                  free_items            (GwySerializableItems *items);

/**
 * gwy_deserialize_error_quark:
 *
 * Gets the error domain for deserialization.
 *
 * See and use %GWY_DESERIALIZE_ERROR.
 *
 * Returns: The error domain.
 **/
GQuark
gwy_deserialize_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain = g_quark_from_static_string("gwy-deserialize-error-quark");

    return error_domain;
}

static void
buffer_alloc(GwySerializableBuffer *buffer,
             gsize size)
{
    if (!size)
        size = GWY_SERIALIZE_BUFFER_SIZE;

    if (size % 8 != 0)
        size += size % 8;

    buffer->len = buffer->bfree = size;
    buffer->data = g_malloc(size);
}

static gboolean
buffer_finish(GwySerializableBuffer *buffer)
{
    gsize size = buffer->len - buffer->bfree;

    if (!size)
        return TRUE;

    buffer->data -= size;
    buffer->bfree = buffer->len;
    return g_output_stream_write_all(buffer->output,
                                     buffer->data, size,
                                     NULL, NULL, buffer->error);
}

static void
buffer_dealloc(GwySerializableBuffer *buffer)
{
    buffer->data -= buffer->len - buffer->bfree;
    buffer->bfree = buffer->len;
    GWY_FREE(buffer->data);
}

static gboolean
buffer_write(GwySerializableBuffer *buffer,
             gconstpointer data,
             gsize size)
{
    while (size >= buffer->bfree) {
        memcpy(buffer->data, data, buffer->bfree);
        data = (const guchar*)data + buffer->bfree;
        size -= buffer->bfree;
        buffer->data -= buffer->len - buffer->bfree;
        buffer->bfree = buffer->len;
        if (!g_output_stream_write_all(buffer->output,
                                       buffer->data, buffer->len,
                                       NULL, NULL, buffer->error))
            return FALSE;
    }

    memcpy(buffer->data, data, size);
    buffer->data += size;
    buffer->bfree -= size;

    return TRUE;
}

static gboolean
buffer_write_size(GwySerializableBuffer *buffer,
                  gsize size)
{
    guint64 size64 = size;

    size64 = GUINT64_TO_LE(size64);
    return buffer_write(buffer, &size64, sizeof(guint64));
}

static gboolean
buffer_write16(GwySerializableBuffer *buffer,
               const guint16 *data16,
               gsize n)
{
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
        return buffer_write(buffer, data16, sizeof(guint16)*n);

    /* Only swap aligned data.
     * First, we do not want to mess with the leftover bytes, second, the
     * mem-swapping instructions usually work much better on aligned data.*/
    if (buffer->bfree % sizeof(guint16) != 0) {
        if (!buffer_finish(buffer))
            return FALSE;
    }

    while (n >= buffer->bfree/sizeof(guint16)) {
        guint16 *bdata16 = (guint16*)buffer->data;
        for (gsize i = buffer->bfree/sizeof(guint16); i; i--) {
            /* The default swapping macros evaulate the value multiple times. */
            guint16 v = *(data16++);
            *(bdata16++) = GUINT16_SWAP_LE_BE(v);
        }
        n -= buffer->bfree/sizeof(guint16);
        buffer->data -= buffer->len - buffer->bfree;
        buffer->bfree = buffer->len;
        if (!g_output_stream_write_all(buffer->output,
                                       buffer->data, buffer->len,
                                       NULL, NULL, buffer->error))
            return FALSE;
    }

    guint16 *bdata16 = (guint16*)buffer->data;
    for (gsize i = buffer->bfree/sizeof(guint16); i; i--) {
        /* The default swapping macros evaulate the value multiple times. */
        guint16 v = *(data16++);
        *(bdata16++) = GUINT16_SWAP_LE_BE(v);
    }
    buffer->data += sizeof(guint16)*n;
    buffer->bfree -= sizeof(guint16)*n;

    return TRUE;
}

static gboolean
buffer_write32(GwySerializableBuffer *buffer,
               const guint32 *data32,
               gsize n)
{
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
        return buffer_write(buffer, data32, sizeof(guint32)*n);

    /* Only swap aligned data.
     * First, we do not want to mess with the leftover bytes, second, the
     * mem-swapping instructions usually work much better on aligned data.*/
    if (buffer->bfree % sizeof(guint32) != 0) {
        if (!buffer_finish(buffer))
            return FALSE;
    }

    while (n >= buffer->bfree/sizeof(guint32)) {
        guint32 *bdata32 = (guint32*)buffer->data;
        for (gsize i = buffer->bfree/sizeof(guint32); i; i--) {
            /* The default swapping macros evaulate the value multiple times. */
            guint32 v = *(data32++);
            *(bdata32++) = GUINT32_SWAP_LE_BE(v);
        }
        n -= buffer->bfree/sizeof(guint32);
        buffer->data -= buffer->len - buffer->bfree;
        buffer->bfree = buffer->len;
        if (!g_output_stream_write_all(buffer->output,
                                       buffer->data, buffer->len,
                                       NULL, NULL, buffer->error))
            return FALSE;
    }

    guint32 *bdata32 = (guint32*)buffer->data;
    for (gsize i = buffer->bfree/sizeof(guint32); i; i--) {
        /* The default swapping macros evaulate the value multiple times. */
        guint32 v = *(data32++);
        *(bdata32++) = GUINT32_SWAP_LE_BE(v);
    }
    buffer->data += sizeof(guint32)*n;
    buffer->bfree -= sizeof(guint32)*n;

    return TRUE;
}

static gboolean
buffer_write64(GwySerializableBuffer *buffer,
               const guint64 *data64,
               gsize n)
{
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
        return buffer_write(buffer, data64, sizeof(guint64)*n);

    /* Only swap aligned data.
     * First, we do not want to mess with the leftover bytes, second, the
     * mem-swapping instructions usually work much better on aligned data.*/
    if (buffer->bfree % sizeof(guint64) != 0) {
        if (!buffer_finish(buffer))
            return FALSE;
    }

    while (n >= buffer->bfree/sizeof(guint64)) {
        guint64 *bdata64 = (guint64*)buffer->data;
        for (gsize i = buffer->bfree/sizeof(guint64); i; i--) {
            /* The default swapping macros evaulate the value multiple times. */
            guint64 v = *(data64++);
            *(bdata64++) = GUINT64_SWAP_LE_BE(v);
        }
        n -= buffer->bfree/sizeof(guint64);
        buffer->data -= buffer->len - buffer->bfree;
        buffer->bfree = buffer->len;
        if (!g_output_stream_write_all(buffer->output,
                                       buffer->data, buffer->len,
                                       NULL, NULL, buffer->error))
            return FALSE;
    }

    guint64 *bdata64 = (guint64*)buffer->data;
    for (gsize i = buffer->bfree/sizeof(guint64); i; i--) {
        /* The default swapping macros evaulate the value multiple times. */
        guint64 v = *(data64++);
        *(bdata64++) = GUINT64_SWAP_LE_BE(v);
    }
    buffer->data += sizeof(guint64)*n;
    buffer->bfree -= sizeof(guint64)*n;

    return TRUE;
}

/**
 * gwy_serialize_gio:
 * @serializable: A serializable object.
 * @output: Output stream to write the serialized object to.
 * @error: Location to store the error occuring, %NULL to ignore.  Errors from
 *         %G_IO_ERROR domain can occur.
 *
 * Serializes an object to GIO stream in GWY format.
 *
 * The data writing employs internal buffering.  If the output stream is
 * already buffered (e.g., #GBufferedOutputStream), the output will be
 * unnecessarily buffered twice.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE if it failed.
 **/
gboolean
gwy_serialize_gio(GwySerializable *serializable,
                  GOutputStream *output,
                  GError **error)
{
    GwySerializableItems items;
    GwySerializableBuffer buffer;
    gboolean ok;
    gsize i = 0;

    g_return_val_if_fail(GWY_IS_SERIALIZABLE(serializable), FALSE);
    g_return_val_if_fail(G_IS_OUTPUT_STREAM(output), FALSE);

    items.len = gwy_serializable_n_items(serializable);
    items.items = g_slice_alloc0(sizeof(GwySerializableItem)*items.len);
    items.n_items = 0;

    gwy_serializable_itemize(serializable, &items);
    calculate_sizes(&items, &i);
    buffer_alloc(&buffer, 0);
    buffer.output = output;
    buffer.error = error;
    ok = dump_to_stream(&items, &buffer);
    buffer_dealloc(&buffer);
    items_done(&items);
    gwy_serializable_done(serializable);
    g_slice_free1(sizeof(GwySerializableItem)*items.len, items.items);

    return ok;
}

/**
 * ctype_size:
 * @ctype: Component type.
 *
 * Computes type size based on type letter.
 *
 * Returns: Size in bytes, 0 for arrays and other nonatomic types.
 **/
static inline gsize G_GNUC_CONST
ctype_size(GwySerializableCType ctype)
{
    switch (ctype) {
        case GWY_SERIALIZABLE_INT8:
        case GWY_SERIALIZABLE_BOOLEAN:
        return sizeof(guint8);
        break;

        case GWY_SERIALIZABLE_INT16:
        return sizeof(guint16);
        break;

        case GWY_SERIALIZABLE_INT32:
        return sizeof(gint32);
        break;

        case GWY_SERIALIZABLE_INT64:
        return sizeof(gint64);
        break;

        case GWY_SERIALIZABLE_DOUBLE:
        return sizeof(gdouble);
        break;

        default:
        return 0;
        break;
    }
}

/* The value is returned for convenience, it permits us to declare item as
 * const even when recusring, because we do not access the fields after the
 * size changes. */
static gsize
calculate_sizes(GwySerializableItems *items,
                gsize *pos)
{
    GwySerializableItem *header = items->items + (*pos)++;

    g_return_val_if_fail(header->ctype == GWY_SERIALIZABLE_HEADER, 0);

    gsize n = header->array_size;
    gsize size = strlen(header->name)+1 + 8 /* object size */;
    for (gsize i = 0; i < n; i++) {
        /* It is important to increment pos now because recusive invocations
         * expect pos already pointing to the header item. */
        const GwySerializableItem *item = items->items + (*pos)++;
        const GwySerializableCType ctype = item->ctype;
        gsize typesize;

        size += strlen(item->name)+1 + 1 /* ctype */;

        if ((typesize = ctype_size(ctype)))
            size += typesize;
        else if ((typesize = ctype_size(g_ascii_tolower(ctype)))) {
            g_warn_if_fail(item->array_size != 0);
            size += typesize*item->array_size + sizeof(guint64);
        }
        else if (ctype == GWY_SERIALIZABLE_STRING)
            size += strlen((const gchar*)item->value.v_string)+1;
        else if (ctype == GWY_SERIALIZABLE_OBJECT
                 || ctype == GWY_SERIALIZABLE_BOXED)
            size += calculate_sizes(items, pos);
        else if (ctype == GWY_SERIALIZABLE_STRING_ARRAY) {
            size += sizeof(guint64);
            gsize alen = item->array_size;
            for (gsize j = 0; j < alen; j++)
                size += strlen((const gchar*)item->value.v_string_array[j])+1;
        }
        else if (ctype == GWY_SERIALIZABLE_OBJECT_ARRAY) {
            size += sizeof(guint64);
            gsize alen = item->array_size;
            for (gsize j = 0; j < alen; j++)
                size += calculate_sizes(items, pos);
        }
        else {
            g_return_val_if_reached(0);
        }
        g_return_val_if_fail(*pos <= items->len, 0);
    }

    return header->value.v_size = size;
}

/**
 * dump_to_stream:
 * @items: List of flattened object tree items.
 * @buffer: Serialization output buffer.
 *
 * Write itemized object tree list into an output stream.
 *
 * Byte-swapping and similar transforms are done on-the-fly, as necessary.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE if it failed.
 **/
static gboolean
dump_to_stream(const GwySerializableItems *items,
               GwySerializableBuffer *buffer)
{
    for (gsize i = 0; i < items->n_items; i++) {
        const GwySerializableItem *item = items->items + i;
        /* Use the single-byte type here to faciliate writing. */
        const guint8 ctype = item->ctype;
        gsize len = strlen(item->name) + 1;

        if (!buffer_write(buffer, item->name, len))
            return FALSE;

        if (ctype == GWY_SERIALIZABLE_HEADER) {
            /* The size stored in GWY files exludes the name and itself. */
            guint64 v = item->value.v_size - len - sizeof(guint64);

            v = GUINT64_TO_LE(v);
            if (!buffer_write(buffer, &v, sizeof(guint64)))
                return FALSE;
            continue;
        }

        if (!buffer_write(buffer, &ctype, sizeof(guint8)))
            return FALSE;

        /* Serializable object/boxed follows... */
        if (ctype == GWY_SERIALIZABLE_OBJECT
            || ctype == GWY_SERIALIZABLE_BOXED)
            continue;
        else if (ctype == GWY_SERIALIZABLE_INT8) {
            if (!buffer_write(buffer, &item->value.v_uint8, sizeof(guint8)))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_BOOLEAN) {
            guint8 v = !!item->value.v_boolean;
            if (!buffer_write(buffer, &v, sizeof(guint8)))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_INT16) {
            guint16 v = GUINT16_TO_LE(item->value.v_uint16);
            if (!buffer_write(buffer, &v, sizeof(guint16)))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_INT32) {
            guint32 v = GUINT32_TO_LE(item->value.v_uint32);
            if (!buffer_write(buffer, &v, sizeof(guint32)))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_INT64
                 || ctype == GWY_SERIALIZABLE_DOUBLE) {
            guint64 v = GUINT64_TO_LE(item->value.v_uint64);
            if (!buffer_write(buffer, &v, sizeof(guint64)))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_STRING) {
            const gchar *s = item->value.v_string;
            if (!buffer_write(buffer, s, strlen(s)+1))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_INT8_ARRAY) {
            if (!buffer_write_size(buffer, item->array_size)
                || !buffer_write(buffer, item->value.v_uint8_array,
                                 item->array_size))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_INT16_ARRAY) {
            if (!buffer_write_size(buffer, item->array_size)
                || !buffer_write16(buffer, item->value.v_uint16_array,
                                   item->array_size))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_INT32_ARRAY) {
            if (!buffer_write_size(buffer, item->array_size)
                || !buffer_write32(buffer, item->value.v_uint32_array,
                                   item->array_size))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_INT64_ARRAY
                 || ctype == GWY_SERIALIZABLE_DOUBLE_ARRAY) {
            if (!buffer_write_size(buffer, item->array_size)
                || !buffer_write64(buffer, item->value.v_uint64_array,
                                   item->array_size))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_STRING_ARRAY) {
            guint64 v = GUINT64_TO_LE(item->array_size);
            if (!buffer_write(buffer, &v, sizeof(guint64)))
                return FALSE;
            for (gsize j = 0; j < item->array_size; j++) {
                const gchar *s = item->value.v_string_array[j];
                if (!buffer_write(buffer, s, strlen(s)+1))
                    return FALSE;
            }
        }
        else if (ctype == GWY_SERIALIZABLE_OBJECT_ARRAY) {
            guint64 v = GUINT64_TO_LE(item->array_size);
            if (!buffer_write(buffer, &v, sizeof(guint64)))
                return FALSE;
            /* Serialized objects/boxed follow... */
        }
        else {
            g_return_val_if_reached(FALSE);
        }
    }

    return buffer_finish(buffer);
}

/**
 * items_done:
 * @items: List of flattened object tree items.
 *
 * Call method done on all objects that need it, in reverse order.
 **/
static void
items_done(const GwySerializableItems *items)
{
    /* The zeroth item is always an object header. */
    for (gsize i = items->n_items-1; i > 0; i--) {
        const GwySerializableItem *item = items->items + i;
        if (item->ctype == GWY_SERIALIZABLE_OBJECT)
            gwy_serializable_done(GWY_SERIALIZABLE(item->value.v_object));
    }
}

static inline gboolean
check_size(gsize size,
           gsize required_size,
           const gchar *what,
           GwyErrorList **error_list)
{
    if (G_LIKELY(size >= required_size))
        return TRUE;

    gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                       GWY_DESERIALIZE_ERROR_TRUNCATED,
                       _("End of data was reached while reading value of type "
                         "‘%s’. It requires %" G_GSIZE_FORMAT " bytes but only "
                         "%" G_GSIZE_FORMAT " bytes remain."),
                       what, required_size, size);
    return FALSE;
}

/* This is the only function that knows what integral is used to represent
 * sizes. */
static inline gsize
unpack_size(const guchar *buffer,
            gsize size,
            gsize item_size,
            gsize *array_size,
            const gchar *name,
            GwyErrorList **error_list)
{
    guint64 raw_size;

    if (!check_size(size, sizeof(guint64), "data-size", error_list))
        return 0;
    memcpy(&raw_size, buffer, sizeof(guint64));
    raw_size = GINT64_FROM_LE(raw_size);
    size -= sizeof(guint64);

    /* Check whether the array length is representable and whether the total
     * size is representable. */
    *array_size = raw_size;
    if (G_UNLIKELY(*array_size != raw_size
                   || *array_size >= G_MAXSIZE/item_size)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_SIZE_T,
                           _("Data of size larger than what can be "
                             "represented on this machine was encountered."));
        return 0;
    }

    /* And if the airhtmetic works, whether the data fits. */
    if (!check_size(size, *array_size * item_size, name, error_list))
        return 0;

    return sizeof(guint64);
}

static inline gsize
unpack_boolean(const guchar *buffer,
               gsize size,
               gboolean *value,
               GwyErrorList **error_list)
{
    guint8 byte;

    if (!check_size(size, sizeof(guint8), "boolean", error_list))
        return 0;
    byte = *buffer;
    *value = !!byte;
    return sizeof(guint8);
}

static inline gsize
unpack_uint8(const guchar *buffer,
             gsize size,
             guint8 *value,
             GwyErrorList **error_list)
{
    if (!check_size(size, sizeof(guint8), "int8", error_list))
        return 0;
    *value = *buffer;
    return sizeof(guint8);
}

static inline gsize
unpack_uint8_array(const guchar *buffer,
                   gsize size,
                   gsize *array_size,
                   guint8 **value,
                   GwyErrorList **error_list)
{
    gsize rbytes;

    if (!(rbytes = unpack_size(buffer, size, sizeof(guint8),
                               array_size, "int8-array", error_list)))
        return 0;

    buffer += rbytes;
    if (*array_size) {
        *value = g_new(guint8, *array_size);
        memcpy(*value, buffer, *array_size*sizeof(guint8));
    }
    return rbytes + *array_size*sizeof(guint8);
}

static inline gsize
unpack_uint16(const guchar *buffer,
              gsize size,
              guint16 *value,
              GwyErrorList **error_list)
{
    if (!check_size(size, sizeof(guint16), "int16", error_list))
        return 0;
    memcpy(value, buffer, sizeof(guint16));
    *value = GINT16_FROM_LE(*value);
    return sizeof(guint16);
}

static inline gsize
unpack_uint16_array(const guchar *buffer,
                    gsize size,
                    gsize *array_size,
                    guint16 **value,
                    GwyErrorList **error_list)
{
    gsize rbytes;

    if (!(rbytes = unpack_size(buffer, size, sizeof(guint16),
                               array_size, "int16-array", error_list)))
        return 0;
    buffer += rbytes;

    if (*array_size) {
        *value = g_new(guint16, *array_size);
        memcpy(*value, buffer, *array_size*sizeof(guint16));
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        gsize i;

        for (i = 0; i < *array_size; i++) {
            /* The default swapping macros evaulate the value multiple times. */
            guint16 v = (*value)[i];
            (*value)[i] = GUINT16_SWAP_LE_BE(v);
        }
    }
    return rbytes + *array_size*sizeof(guint16);
}

static inline gsize
unpack_uint32(const guchar *buffer,
              gsize size,
              guint32 *value,
              GwyErrorList **error_list)
{
    if (!check_size(size, sizeof(guint32), "int32", error_list))
        return 0;
    memcpy(value, buffer, sizeof(guint32));
    *value = GINT32_FROM_LE(*value);
    return sizeof(guint32);
}

static inline gsize
unpack_uint32_array(const guchar *buffer,
                    gsize size,
                    gsize *array_size,
                    guint32 **value,
                    GwyErrorList **error_list)
{
    gsize rbytes;

    if (!(rbytes = unpack_size(buffer, size, sizeof(guint32),
                               array_size, "int32-array", error_list)))
        return 0;
    buffer += rbytes;

    if (*array_size) {
        *value = g_new(guint32, *array_size);
        memcpy(*value, buffer, *array_size*sizeof(guint32));
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        gsize i;

        for (i = 0; i < *array_size; i++) {
            /* The default swapping macros evaulate the value multiple times. */
            guint32 v = (*value)[i];
            (*value)[i] = GUINT32_SWAP_LE_BE(v);
        }
    }
    return rbytes + *array_size*sizeof(guint32);
}

static inline gsize
unpack_uint64(const guchar *buffer,
              gsize size,
              guint64 *value,
              GwyErrorList **error_list)
{
    if (!check_size(size, sizeof(guint64), "int64", error_list))
        return 0;
    memcpy(value, buffer, sizeof(guint64));
    *value = GINT64_FROM_LE(*value);
    return sizeof(guint64);
}

static inline gsize
unpack_uint64_array(const guchar *buffer,
                    gsize size,
                    gsize *array_size,
                    guint64 **value,
                    GwyErrorList **error_list)
{
    gsize rbytes;

    if (!(rbytes = unpack_size(buffer, size, sizeof(guint64),
                               array_size, "int64-array", error_list)))
        return 0;
    buffer += rbytes;

    if (*array_size) {
        *value = g_new(guint64, *array_size);
        memcpy(*value, buffer, *array_size*sizeof(guint64));
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        gsize i;

        for (i = 0; i < *array_size; i++) {
            /* The default swapping macros evaulate the value multiple times. */
            guint64 v = (*value)[i];
            (*value)[i] = GUINT64_SWAP_LE_BE(v);
        }
    }
    return rbytes + *array_size*sizeof(guint64);
}

/* The code says `int64', that is correct we only care about the size. */
static inline gsize
unpack_double(const guchar *buffer,
              gsize size,
              guint64 *value,
              GwyErrorList **error_list)
{
    if (!check_size(size, sizeof(gdouble), "double", error_list))
        return 0;
    memcpy(value, buffer, sizeof(gdouble));
    *value = GINT64_FROM_LE(*value);
    return sizeof(gdouble);
}

/* The code says `int64', that is correct we only care about the size. */
static inline gsize
unpack_double_array(const guchar *buffer,
                    gsize size,
                    gsize *array_size,
                    guint64 **value,
                    GwyErrorList **error_list)
{
    gsize rbytes;

    if (!(rbytes = unpack_size(buffer, size, sizeof(gdouble),
                               array_size, "double-array", error_list)))
        return 0;
    buffer += rbytes;

    if (*array_size) {
        *value = g_new(guint64, *array_size);
        memcpy(*value, buffer, *array_size*sizeof(gdouble));
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        gsize i;

        for (i = 0; i < *array_size; i++) {
            /* The default swapping macros evaulate the value multiple times. */
            guint64 v = (*value)[i];
            (*value)[i] = GUINT64_SWAP_LE_BE(v);
        }
    }
    return rbytes + *array_size*sizeof(gdouble);
}

static inline gsize
unpack_name(const guchar *buffer,
            gsize size,
            const gchar **value,
            GwyErrorList **error_list)
{
    const guchar *s = memchr(buffer, '\0', size);

    if (G_UNLIKELY(!s)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_TRUNCATED,
                           _("End of data was reached while looking for the "
                             "end of a string."));
        return 0;
    }
    *value = (const gchar*)buffer;
    return s-buffer + 1;
}

static inline gsize
unpack_string(const guchar *buffer,
              gsize size,
              gchar **value,
              GwyErrorList **error_list)
{
    const guchar *s = memchr(buffer, '\0', size);

    if (G_UNLIKELY(!s)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_TRUNCATED,
                           _("End of data was reached while looking for the "
                             "end of a string."));
        return 0;
    }
    *value = g_memdup(buffer, s-buffer + 1);
    return s-buffer + 1;
}

static inline gsize
unpack_string_array(const guchar *buffer,
                    gsize size,
                    gsize *array_size,
                    gchar ***value,
                    GwyErrorList **error_list)
{
    gsize rbytes, position = 0;

    if (!(rbytes = unpack_size(buffer, size, 1,
                               array_size, "string-array", error_list)))
        return 0;
    position += rbytes;

    if (*array_size) {
        *value = g_new0(gchar*, *array_size);
        for (gsize i = 0; i < *array_size; i++) {
            if (!(rbytes = unpack_string(buffer + position, size - position,
                                         *value + i, error_list))) {
                while (i--)
                    g_free((*value)[i]);
                GWY_FREE(*value);
                *array_size = 0;
                position = 0;
                break;
            }
            position += rbytes;
        }
    }

    return position;
}

static inline gsize
unpack_object(const guchar *buffer,
              gsize size,
              GObject **value,
              GwyErrorList **error_list)
{
    gsize rbytes;

    if (!(*value = deserialize_memory(buffer, size, &rbytes, FALSE,
                                      error_list)))
        return 0;

    return rbytes;
}

static inline gsize
unpack_object_array(const guchar *buffer,
                    gsize size,
                    gsize *array_size,
                    GObject ***value,
                    GwyErrorList **error_list)
{
    gsize rbytes, position = 0;

    /* 10 bytes is the minimum object size: 2 bytes name and 8 bytes size */
    if (!(rbytes = unpack_size(buffer, size, 10,
                               array_size, "object-array", error_list)))
        return 0;
    position += rbytes;

    if (*array_size) {
        *value = g_new0(GObject*, *array_size);
        for (gsize i = 0; i < *array_size; i++) {
            if (!((*value)[i] = gwy_deserialize_memory(buffer + position,
                                                       size - position, &rbytes,
                                                       error_list))) {
                while (i--)
                    GWY_OBJECT_UNREF((*value)[i]);
                GWY_FREE(*value);
                position = 0;
                *array_size = 0;
                break;
            }
            position += rbytes;
        }
    }

    return position;
}

static GObject*
deserialize_boxed(const guchar *buffer,
                  gsize size,
                  gsize *bytes_consumed,
                  gsize *gtype,                // GType, in fact
                  GwyErrorList **error_list)
{
    GwySerializableItems *items = NULL;
    const gchar *typename;
    gsize pos = 0, rbytes, boxed_size;
    gpointer boxed = NULL;
    GType type;

    /* Type name */
    if (!(rbytes = unpack_name(buffer + pos, size - pos, &typename,
                               error_list)))
        goto fail;
    pos += rbytes;

    /* Object size */
    if (!(rbytes = unpack_size(buffer + pos, size - pos, 1,
                               &boxed_size, "boxed", error_list)))
        goto fail;
    pos += rbytes;

    /* Check if we can deserialize such thing. */
    if (!(type = get_serializable_boxed(typename, error_list)))
        goto fail;

    /* We should no only fit, but the boxed size should be exactly size.
     * If there is extra stuff, add an error, but do not fail. */
    if (G_UNLIKELY(size - pos != boxed_size)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_PADDING,
                           _("Boxed type ‘%s’ data is smaller than the space "
                             "alloted for it.  The padding was ignored."),
                           typename);
    }
    size = boxed_size + pos;

    /* Unpack everything, call request() only after that so that is its
     * quaranteed construct() is called after request(). */
    items = unpack_items(buffer + pos, boxed_size, error_list);
    if (!items)
        goto fail;

    /* Finally, construct the boxed type. */
    boxed = gwy_serializable_boxed_construct(type, items, error_list);
    if (boxed)
        *gtype = type;

fail:
    free_items(items);
    if (bytes_consumed)
        *bytes_consumed = size;
    return boxed;
}

static inline gsize
unpack_boxed(const guchar *buffer,
             gsize size,
             gpointer *value,
             gsize *gtype,                // GType, in fact
             GwyErrorList **error_list)
{
    gsize rbytes;

    if (!(*value = deserialize_boxed(buffer, size, &rbytes, gtype, error_list)))
        return 0;

    return rbytes;
}

/**
 * gwy_deserialize_memory:
 * @buffer: Memory containing the serialized representation of one object.
 * @size: Size of @buffer in bytes.  If the buffer is larger than the
 *        object representation, non-fatal %GWY_DESERIALIZE_ERROR_PADDING will
 *        be reported.
 * @bytes_consumed: Location to store the number of bytes actually consumed
 *                  from @buffer, or %NULL.
 * @error_list: Location to store the errors occuring, %NULL to ignore.
 *              Errors from %GWY_DESERIALIZE_ERROR domain can occur.
 *
 * Deserializes an object in GWY format from plain memory buffer.
 *
 * Returns: Newly created object on success, %NULL on failure.
 **/
GObject*
gwy_deserialize_memory(const guchar *buffer,
                       gsize size,
                       gsize *bytes_consumed,
                       GwyErrorList **error_list)
{
    /* This is a bit expensive.  But deserialization is expensive generally. */
    gwy_type_init();
    return deserialize_memory(buffer, size, bytes_consumed, TRUE, error_list);
}

static GObject*
deserialize_memory(const guchar *buffer,
                   gsize size,
                   gsize *bytes_consumed,
                   gboolean exact_size,
                   GwyErrorList **error_list)
{
    const GwySerializableInterface *iface;
    GwySerializableItems *items = NULL;
    const gchar *typename;
    gsize pos = 0, rbytes, object_size;
    gpointer classref;
    GObject *object = NULL;
    GType type;

    /* Type name */
    if (!(rbytes = unpack_name(buffer + pos, size - pos, &typename,
                               error_list)))
        goto fail;
    pos += rbytes;

    /* Object size */
    if (!(rbytes = unpack_size(buffer + pos, size - pos, 1,
                               &object_size, "object", error_list)))
        goto fail;
    pos += rbytes;

    /* Check if we can deserialize such thing. */
    if (!(type = get_serializable(typename, &classref, &iface, error_list)))
        goto fail;

    if (exact_size) {
        /* We should no only fit, but the object size should be exactly size.
         * If there is extra stuff, add an error, but do not fail. */
        if (G_UNLIKELY(size - pos != object_size)) {
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_PADDING,
                               _("Object ‘%s’ data is smaller than the space "
                                 "alloted for it.  The padding was ignored."),
                               typename);
        }
    }
    size = object_size + pos;

    /* Unpack everything, call request() only after that so that is its
     * quaranteed construct() is called after request(). */
    items = unpack_items(buffer + pos, object_size, error_list);
    if (!items)
        goto fail;

    /* Finally, construct the object. */
    object = iface->construct(items, error_list);

fail:
    free_items(items);
    if (bytes_consumed)
        *bytes_consumed = size;
    return object;
}

static GType
get_serializable(const gchar *typename,
                 gpointer *classref,
                 const GwySerializableInterface **iface,
                 GwyErrorList **error_list)
{
    GType type;

    type = g_type_from_name(typename);
    if (G_UNLIKELY(!type)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_OBJECT,
                           _("Object type ‘%s’ is not known. "
                             "It was ignored."),
                           typename);
        return 0;
    }

    *classref = g_type_class_ref(type);
    g_assert(*classref);
    if (G_UNLIKELY(!G_TYPE_IS_INSTANTIATABLE(type))) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_OBJECT,
                           _("Object type ‘%s’ is not instantiable. "
                             "It was ignored."),
                           typename);
        return 0;
    }
    if (G_UNLIKELY(!g_type_is_a(type, GWY_TYPE_SERIALIZABLE))) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_OBJECT,
                           _("Object type ‘%s’ is not serializable. "
                             "It was ignored."),
                           typename);
        return 0;
    }

    *iface = g_type_interface_peek(*classref, GWY_TYPE_SERIALIZABLE);
    if (G_UNLIKELY(!*iface || !(*iface)->construct)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_OBJECT,
                           _("Object type ‘%s’ does not implement "
                             "deserialization. It was ignored."),
                           typename);
        return 0;
    }

    return type;
}

static GType
get_serializable_boxed(const gchar *typename,
                       GwyErrorList **error_list)
{
    GType type;

    type = g_type_from_name(typename);
    if (G_UNLIKELY(!type)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_OBJECT,
                           _("Boxed type ‘%s’ is not known. "
                             "It was ignored."),
                           typename);
        return 0;
    }

    if (G_UNLIKELY(!gwy_boxed_type_is_serializable(type))) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_OBJECT,
                           _("Boxed type ‘%s’ is not serializable. "
                             "It was ignored."),
                           typename);
        return 0;
    }

    return type;
}

static GwySerializableItems*
unpack_items(const guchar *buffer,
             gsize size,
             GwyErrorList **error_list)
{
    GwySerializableItems *items;
    GwySerializableItem *item;

    items = g_new(GwySerializableItems, 1);
    items->len = 8;
    items->items = g_new0(GwySerializableItem, items->len);
    items->n_items = 0;

    while (size) {
        gsize rbytes;

        /* Grow items if necessary */
        if (items->n_items == items->len) {
            items->len = 2*items->len;
            items->items = g_renew(GwySerializableItem, items->items,
                                   items->len);
            gwy_memclear(items->items + items->n_items,
                         items->len - items->n_items);
        }
        item = items->items + items->n_items;

        /* Component name */
        if (!(rbytes = unpack_name(buffer, size, &item->name, error_list)))
            goto fail;
        buffer += rbytes;
        size -= rbytes;

        /* Component type */
        if (!(rbytes = unpack_uint8(buffer, size, &item->ctype, error_list)))
            goto fail;
        buffer += rbytes;
        size -= rbytes;

        GwySerializableCType ctype = item->ctype;

        /* The data */
        if (ctype == GWY_SERIALIZABLE_BOOLEAN) {
            if (!(rbytes = unpack_boolean(buffer, size, &item->value.v_boolean,
                                          error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_INT8) {
            if (!(rbytes = unpack_uint8(buffer, size, &item->value.v_uint8,
                                        error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_INT16) {
            if (!(rbytes = unpack_uint16(buffer, size, &item->value.v_uint16,
                                         error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_INT32) {
            if (!(rbytes = unpack_uint32(buffer, size, &item->value.v_uint32,
                                         error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_INT64) {
            if (!(rbytes = unpack_uint64(buffer, size, &item->value.v_uint64,
                                         error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_DOUBLE) {
            if (!(rbytes = unpack_double(buffer, size, &item->value.v_uint64,
                                         error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_STRING) {
            if (!(rbytes = unpack_string(buffer, size, &item->value.v_string,
                                         error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_OBJECT) {
            if (!(rbytes = unpack_object(buffer, size, &item->value.v_object,
                                         error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_BOXED) {
            if (!(rbytes = unpack_boxed(buffer, size, &item->value.v_boxed,
                                        &item->array_size, error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_INT8_ARRAY) {
            if (!(rbytes = unpack_uint8_array(buffer, size,
                                              &item->array_size,
                                              &item->value.v_uint8_array,
                                              error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_INT16_ARRAY) {
            if (!(rbytes = unpack_uint16_array(buffer, size,
                                               &item->array_size,
                                               &item->value.v_uint16_array,
                                               error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_INT32_ARRAY) {
            if (!(rbytes = unpack_uint32_array(buffer, size,
                                               &item->array_size,
                                               &item->value.v_uint32_array,
                                               error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_INT64_ARRAY) {
            if (!(rbytes = unpack_uint64_array(buffer, size,
                                               &item->array_size,
                                               &item->value.v_uint64_array,
                                               error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_DOUBLE_ARRAY) {
            if (!(rbytes = unpack_double_array(buffer, size,
                                               &item->array_size,
                                               &item->value.v_uint64_array,
                                               error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_STRING_ARRAY) {
            if (!(rbytes = unpack_string_array(buffer, size,
                                               &item->array_size,
                                               &item->value.v_string_array,
                                               error_list)))
                goto fail;
        }
        else if (ctype == GWY_SERIALIZABLE_OBJECT_ARRAY) {
            if (!(rbytes = unpack_object_array(buffer, size,
                                               &item->array_size,
                                               &item->value.v_object_array,
                                               error_list)))
                goto fail;
        }
        else {
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_DATA,
                               _("Data type 0x%02x is unknown. "
                                 "It could not be just skipped, complete "
                                 "object ‘%s’ was ignored."),
                               ctype, item->name);
            goto fail;
        }

        buffer += rbytes;
        size -= rbytes;
        items->n_items++;
    }

    return items;

fail:
    free_items(items);
    return NULL;
}

static inline void
warn_nonzero_array_size(const GwySerializableItem *item)
{
    g_warning("A NULL array ‘%s’ of type 0x%02x has still nonzero "
              "size %" G_GSIZE_FORMAT "."
              "Please set item->array_size to zero when clearing "
              "the array."
              "Hoping no memory was leaked...",
              item->name, item->ctype, item->array_size);
}

static void
free_items(GwySerializableItems *items)
{
    if (!items)
        return;

    for (gsize i = 0; i < items->n_items; i++) {
        GwySerializableItem *item = items->items + i;
        GwySerializableCType ctype = item->ctype;

        switch (ctype) {
            case 0:
            // Type not assigned yet.  Can happen.
            break;

            case GWY_SERIALIZABLE_OBJECT:
            GWY_OBJECT_UNREF(item->value.v_object);
            break;

            case GWY_SERIALIZABLE_INT8_ARRAY:
            case GWY_SERIALIZABLE_INT16_ARRAY:
            case GWY_SERIALIZABLE_INT32_ARRAY:
            case GWY_SERIALIZABLE_INT64_ARRAY:
            case GWY_SERIALIZABLE_DOUBLE_ARRAY:
            if (item->value.v_int8_array)
                GWY_FREE(item->value.v_int8_array);
            else if (G_UNLIKELY(item->array_size))
                warn_nonzero_array_size(item);
            break;

            case GWY_SERIALIZABLE_STRING:
            GWY_FREE(item->value.v_string);
            break;

            case GWY_SERIALIZABLE_BOXED:
            if (item->value.v_boxed) {
                g_boxed_free(item->array_size, item->value.v_boxed);
                item->value.v_boxed = NULL;
                item->array_size = 0;
            }
            break;

            case GWY_SERIALIZABLE_STRING_ARRAY:
            if (item->value.v_string_array) {
                for (gsize j = 0; j < item->array_size; j++)
                    GWY_FREE(item->value.v_string_array[j]);
                GWY_FREE(item->value.v_string_array);
            }
            else if (G_UNLIKELY(item->array_size))
                warn_nonzero_array_size(item);
            break;

            case GWY_SERIALIZABLE_OBJECT_ARRAY:
            if (item->value.v_object_array) {
                for (gsize j = 0; j < item->array_size; j++)
                    GWY_OBJECT_UNREF(item->value.v_object_array[j]);
                GWY_FREE(item->value.v_object_array);
            }
            else if (G_UNLIKELY(item->array_size))
                warn_nonzero_array_size(item);
            break;

            default:
            g_assert(g_ascii_islower(ctype));
            break;
        }
        item->array_size = 0;
    }

    g_free(items->items);
    g_free(items);
}

/**
 * gwy_deserialize_filter_items:
 * @template_: List of expected items.  Generally, the values should be set to
 *             defaults.
 * @n_items: The number of items in the template.
 * @items: Item list passed to construct().
 * @type_name: Name of the deserialized type for error messages.
 * @error_list: Location to store the errors occuring, %NULL to ignore.
 *              Only non-fatal error %GWY_DESERIALIZE_ERROR_ITEM can occur.
 *
 * Fills the template of expected items with values from received item list.
 *
 * This is a helper function for use in #GwySerializable construct() method.
 *
 * Expected values are moved from @items to @template.  This means the owner of
 * @template becomes the owner of dynamically allocated data in these items.
 * Unexpected values are left in @items for the owner of @items to free them
 * which normally means you do not need to concern yourself with them.
 *
 * An item template is identified by its name and type.  Items of the same
 * name but different type are permitted in @template (e.g. to accept old
 * serialized representations for compatibility).
 **/
void
gwy_deserialize_filter_items(GwySerializableItem *template_,
                             gsize n_items,
                             GwySerializableItems *items,
                             const gchar *type_name,
                             GwyErrorList **error_list)
{
    guint8 *seen = g_slice_alloc0(sizeof(guint8)*n_items);

    for (gsize i = 0; i < items->n_items; i++) {
        GwySerializableItem *item = items->items + i;
        const GwySerializableCType ctype = item->ctype;
        const gchar *name = item->name;
        gsize j;

        for (j = 0; j < n_items; j++) {
            if (template_[j].ctype == ctype
                && gwy_strequal(template_[j].name, name))
                break;
        }

        if (G_UNLIKELY(j == n_items)) {
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_ITEM,
                               _("Unexpected item ‘%s’ of type 0x%02x in the "
                                 "representation of object ‘%s’ was ignored"),
                               name, ctype, type_name);
            continue;
        }
        if (G_UNLIKELY(seen[j] == 1)) {
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_ITEM,
                               _("Item ‘%s’ of type 0x%02x is present multiple "
                                 "times in the representation of object ‘%s’.  "
                                 "Values other than the first were ignored."),
                               name, ctype, type_name);
            seen[j]++;
            continue;
        }
        seen[j]++;

        /* Boxed types can be also filtered using the type. */
        if (ctype == GWY_SERIALIZABLE_BOXED && template_[j].array_size) {
            if (G_UNLIKELY(item->array_size != template_[j].array_size)) {
                gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                                   GWY_DESERIALIZE_ERROR_ITEM,
                                   _("Item ‘%s’ in the representation of "
                                     "object ‘%s’ has type ‘%s’ "
                                     "instead of expected ‘%s’. "
                                     "It was ignored."),
                                   name, type_name,
                                   g_type_name(item->array_size),
                                   g_type_name(template_[j].array_size));
                continue;
            }
        }

        /* Give ownership to template. */
        memcpy(&template_[j].value, &item->value, sizeof(GwySerializableValue));
        item->value.v_uint8_array = NULL;

        template_[j].array_size = item->array_size;
        item->array_size = 0;
    }

    g_slice_free1(sizeof(guint8)*n_items, seen);
}

#define __LIBGWY_SERIALIZE_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: serialize
 * @title: serialize
 * @short_description: Serializers and deserializers
 * @see_also: #GwySerializable
 *
 * Methods of #GwySerializable abstract from the precise byte-for-byte
 * representation of serialized objects.  It is conceivable to imagine using
 * for instance HDF5 or XML as the binary format.  Functions available here at
 * this moment, however, implement the GWY binary data format, version 3.
 *
 * Note the initial reference counts of restored objects are according to their
 * nature.  For instance, a #GInitiallyUnowned will have a floating reference,
 * a plain #GObject will have a reference count of 1, etc.
 *
 * <refsect2 id='libgwy-serialize-details'>
 * <title>Details of Serialization</title>
 * <para>The following information is not necessary for implementing
 * #GwySerializable interface in your classes, but it can help prevent wrong
 * decision about serialized representation of your objects.  Also, it might
 * help implementing a different serialization backend than GWY files.</para>
 * <para>Serialization occurs in several steps.</para>
 * <para>First, all objects are recursively asked to calulcate the number of
 * named
 * data items they will serialize to (or provide a reasonable upper estimate of
 * this number).  This is done simply by calling gwy_serializable_n_items() on
 * the top-level object, objects that contain other objects must call
 * gwy_serializable_n_items() on these contained objects and sum the
 * results.</para>
 * <para>Second, a #GwySerializableItems item list is created, with size
 * calculated in the first step.  All objects are then recursively asked to add
 * items representing their data to this list.  This is done simply by calling
 * gwy_serializable_itemize() on the top-level object.  The objects may
 * sometimes need to represent certain data differently than the internal
 * representation is, however, expensive transforms should be avoided,
 * especially for arrays.  This step can allocate temporary structures.</para>
 * <para>Third, sizes of each object are calcuated and stored into the
 * object-header items created by gwy_serializable_itemize().  This again is
 * done recursively, but the objects do not participate, the calculation works
 * with the itemized list.  This step might not be necessary for other
 * storage formats.</para>
 * <para>Subsequently, the object tree flattened into an item list is written
 * to the output stream, byte-swapping or otherwise normalizing the data on the
 * fly if necessary.  This part strongly depends on the storage format.</para>
 * <para>Finally, virtual method done() is called for all objects.  This step
 * frees the temporary storage allocated in the itemization step, if any.  This
 * is not done recursively so that objects need not to implement this method,
 * even if they contain other objects, if they themselves do not create any
 * temporary data during itemize().  The methods are called in the inverse
 * order than the objects appear in the list, i.e. the most inner and last
 * objects are processed first.  This means that if done() of an object is
 * invoked, all its contained objects have been already processed.  At the very
 * end the item list is freed too.</para>
 * </refsect2>
 **/

/**
 * GWY_DESERIALIZE_ERROR:
 *
 * Error domain for deserialization.
 *
 * Errors in this domain will be from the #GwyDeserializeError enumeration.
 * See #GError for information on error domains.
 **/

/**
 * GwyDeserializeError:
 * @GWY_DESERIALIZE_ERROR_TRUNCATED: Data ends in the middle of some item or
 *                                   there is not enough data to represent
 *                                   given object.  This error is fatal.
 * @GWY_DESERIALIZE_ERROR_PADDING: There is too much data to represent given
 *                                 object.  This error is non-fatal: the extra
 *                                 data is just ignored.
 * @GWY_DESERIALIZE_ERROR_SIZE_T: Size is not representable on this system.
 *                                This can occur on legacy 32bit systems,
 *                                however, they are incapable of holding such
 *                                large data in core anyway.  This error is
 *                                fatal.
 * @GWY_DESERIALIZE_ERROR_OBJECT: Representation of an object of unknown or
 *                                deserialization-incapable type was found.
 *                                This error is fatal.
 * @GWY_DESERIALIZE_ERROR_ITEM: Unexpected item was encountered.  This error
 *                              is non-fatal: such item is just ignored.
 * @GWY_DESERIALIZE_ERROR_DATA: Uknown data type (#GwySerializableCType) was
 *                              encountered.  This error is fatal.
 * @GWY_DESERIALIZE_ERROR_FATAL_MASK: Distinguishes fatal and non-fatal errors
 *                                    (fatal give non-zero value when masked
 *                                    with this mask).
 *
 * Error codes returned by deserialization.
 *
 * In the error code descriptions, fatal error means aborting of
 * deserialization of the object that is just being unpacked.  Non-fatal errors
 * are competely recoverable.  If the deserialization of a contained object
 * is aborted, the unpacking of the container object still continues.
 * Therefore, fatal errors are truly fatal only if they occur for the
 * top-level object.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
