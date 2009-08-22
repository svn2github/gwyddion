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

/*
 * XXX assertions:
 * sizeof(gdouble) == sizeof(guint64)
 */
#include <string.h>
#include <glib/gi18n.h>
#include "libgwy/serializable.h"
#include "libgwy/macros.h"

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

static gsize                 calculate_sizes (GwySerializableItems *items,
                                              gsize *pos);
static void                  items_done      (const GwySerializableItems *items);
static gboolean              dump_to_stream  (const GwySerializableItems *items,
                                              GwySerializableBuffer *buffer);
static GType                 get_serializable(const gchar *typename,
                                              gpointer *classref,
                                              const GwySerializableInterface **iface,
                                              GwyErrorList **error_list);
static GwySerializableItems* unpack_items    (const guchar *buffer,
                                              gsize size,
                                              GwyErrorList **error_list);
static void                  free_items      (GwySerializableItems *items);

GType
gwy_serializable_get_type(void)
{
    static GType serializable_type = 0;

    if (G_UNLIKELY(!serializable_type))
        serializable_type
            = g_type_register_static_simple(G_TYPE_INTERFACE,
                                            "GwySerializable",
                                            sizeof(GwySerializableInterface),
                                            NULL, 0, NULL, 0);

    return serializable_type;
}

/**
 * gwy_serializable_error_quark:
 *
 * Gets the error domain for serialization and deserialization.
 *
 * See and use %GWY_SERIALIZABLE_ERROR.
 *
 * Returns: The error domain.
 **/
GQuark
gwy_serializable_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain = g_quark_from_static_string("gwy-serializable-error-quark");

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
    guint16 *bdata16;
    gsize i;

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
        bdata16 = (guint16*)buffer->data;
        for (i = buffer->bfree/sizeof(guint16); i; i--) {
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

    bdata16 = (guint16*)buffer->data;
    for (i = buffer->bfree/sizeof(guint16); i; i--) {
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
    guint32 *bdata32;
    gsize i;

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
        bdata32 = (guint32*)buffer->data;
        for (i = buffer->bfree/sizeof(guint32); i; i--) {
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

    bdata32 = (guint32*)buffer->data;
    for (i = buffer->bfree/sizeof(guint32); i; i--) {
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
    guint64 *bdata64;
    gsize i;

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
        bdata64 = (guint64*)buffer->data;
        for (i = buffer->bfree/sizeof(guint64); i; i--) {
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

    bdata64 = (guint64*)buffer->data;
    for (i = buffer->bfree/sizeof(guint64); i; i--) {
        /* The default swapping macros evaulate the value multiple times. */
        guint64 v = *(data64++);
        *(bdata64++) = GUINT64_SWAP_LE_BE(v);
    }
    buffer->data += sizeof(guint64)*n;
    buffer->bfree -= sizeof(guint64)*n;

    return TRUE;
}

/**
 * gwy_serializable_n_items:
 * @serializable: A serializable object.
 *
 * Provides an upper bound of the number of items the flattened representation
 * of an object will take.
 *
 * This function wraps the virtual table method n_items().   It increases the
 * returned value by the number of item used to represent the object header
 * (which is 1).
 *
 * Returns: An upper bound of the number of items @serializable will serialize
 *          to.
 **/
gsize
gwy_serializable_n_items(GwySerializable *serializable)
{
    const GwySerializableInterface *iface;

    g_return_val_if_fail(GWY_IS_SERIALIZABLE(serializable), 0);
    iface = GWY_SERIALIZABLE_GET_INTERFACE(serializable);
    g_return_val_if_fail(iface && iface->n_items, 1);

    return iface->n_items(serializable) + 1;
}

/**
 * gwy_serializable_itemize:
 * @serializable: A serializable object.
 * @items: List of flattened object tree items to append the representation of
 *         @serializable to.
 *
 * Creates a flattened representation of a serializable object.
 *
 * This function wraps the virtual table method itemize().  It deals with
 * recording of the object header item.  Method itemize() then only serializes
 * the actual data of the object.
 **/
void
gwy_serializable_itemize(GwySerializable *serializable,
                         GwySerializableItems *items)
{
    const GwySerializableInterface *iface;
    GwySerializableItem *item;

    g_return_if_fail(GWY_IS_SERIALIZABLE(serializable));
    iface = GWY_SERIALIZABLE_GET_INTERFACE(serializable);
    g_return_if_fail(iface && iface->itemize);

    g_return_if_fail(items->n_items < items->len);
    item = items->items + items->n_items;
    items->n_items++;
    item->name = G_OBJECT_TYPE_NAME(G_OBJECT(serializable));
    item->value.v_size = 0;
    item->ctype = GWY_SERIALIZABLE_HEADER;
    item->array_size = iface->itemize(serializable, items);
}

/**
 * gwy_serializable_done:
 * @serializable: A serializable object.
 *
 * Frees temporary storage allocated by object itemization.
 *
 * This function calls the virtual table method done(), if the class has any.
 **/
void
gwy_serializable_done(GwySerializable *serializable)
{
    const GwySerializableInterface *iface;

    g_return_if_fail(GWY_IS_SERIALIZABLE(serializable));
    iface = GWY_SERIALIZABLE_GET_INTERFACE(serializable);
    g_return_if_fail(iface);

    if (iface->done)
        iface->done(serializable);
}

/**
 * gwy_serializable_serialize:
 * @serializable: A serializable object.
 * @output: Output stream to write the serialized object to.
 * @error: Location to store the error occuring, %NULL to ignore.
 *
 * Serializes an object.
 *
 * The data writing employs internal buffering to avoid too many syscalls.
 * If the output stream is already buffered (e.g., #GBufferedOutputStream),
 * the output will be unnecessarily buffered twice.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE if it failed.
 **/
gboolean
gwy_serializable_serialize(GwySerializable *serializable,
                           GOutputStream *output,
                           GError **error)
{
    GwySerializableItems items;
    GwySerializableBuffer buffer;
    gboolean ok = FALSE;
    gsize i = 0;

    g_return_val_if_fail(GWY_IS_SERIALIZABLE(serializable), FALSE);
    g_return_val_if_fail(G_IS_OUTPUT_STREAM(output), FALSE);

    items.len = gwy_serializable_n_items(serializable);
    items.items = g_new(GwySerializableItem, items.len);
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
    g_free(items.items);

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
    gsize i, n, size;

    g_return_val_if_fail(header->ctype == GWY_SERIALIZABLE_HEADER, 0);

    n = header->array_size;
    size = strlen(header->name)+1 + 8 /* object size */;
    for (i = 0; i < n; i++) {
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
        else if (ctype == GWY_SERIALIZABLE_OBJECT)
            size += calculate_sizes(items, pos);
        else if (ctype == GWY_SERIALIZABLE_STRING_ARRAY) {
            gsize j, alen;

            alen = item->array_size;
            for (j = 0; j < alen; j++)
                size += strlen((const gchar*)item->value.v_string_array[j])+1;
        }
        else if (ctype == GWY_SERIALIZABLE_OBJECT_ARRAY) {
            gsize j, alen;

            alen = item->array_size;
            for (j = 0; j < alen; j++)
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
    gsize i;

    for (i = 0; i < items->n_items; i++) {
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

        /* Serializable object follows... */
        if (ctype == GWY_SERIALIZABLE_OBJECT)
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
            gsize j;
            if (!buffer_write(buffer, &v, sizeof(guint64)))
                return FALSE;

            for (j = 0; j < item->array_size; j++) {
                const gchar *s = item->value.v_string_array[j];
                if (!buffer_write(buffer, s, strlen(s)+1))
                    return FALSE;
            }
        }
        else if (ctype == GWY_SERIALIZABLE_OBJECT_ARRAY) {
            guint64 v = GUINT64_TO_LE(item->array_size);
            if (!buffer_write(buffer, &v, sizeof(guint64)))
                return FALSE;
            /* Serialized objects follow... */
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
    gsize i;

    /* The zeroth item is always an object header. */
    for (i = items->len-1; i > 0; i--) {
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

    gwy_error_list_add(error_list, GWY_SERIALIZABLE_ERROR,
                       GWY_SERIALIZABLE_ERROR_TRUNCATED,
                       _("End of data was reached while reading value of type "
                         "`%s'. It requires %" G_GSIZE_FORMAT " bytes but only "
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
        gwy_error_list_add(error_list, GWY_SERIALIZABLE_ERROR,
                           GWY_SERIALIZABLE_ERROR_SIZE_T,
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
        memcpy(value, buffer, *array_size*sizeof(guint8));
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
        memcpy(value, buffer, *array_size*sizeof(guint16));
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
        memcpy(value, buffer, *array_size*sizeof(guint32));
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
        memcpy(value, buffer, *array_size*sizeof(guint64));
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
        memcpy(value, buffer, *array_size*sizeof(gdouble));
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

    if (!s) {
        gwy_error_list_add(error_list, GWY_SERIALIZABLE_ERROR,
                           GWY_SERIALIZABLE_ERROR_TRUNCATED,
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

    if (!s) {
        gwy_error_list_add(error_list, GWY_SERIALIZABLE_ERROR,
                           GWY_SERIALIZABLE_ERROR_TRUNCATED,
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
    gsize rbytes, i, position = 0;

    if (!(rbytes = unpack_size(buffer, size, 1,
                               array_size, "string-array", error_list)))
        return 0;
    position += rbytes;

    if (*array_size) {
        *value = g_new0(gchar*, *array_size);
        for (i = 0; i < *array_size; i++) {
            if (!(rbytes = unpack_string(buffer + position, size - position,
                                         *value + i, error_list))) {
                while (i--)
                    g_free((*value)[i]);
                GWY_FREE(*value);
                position = 0;
                break;
            }
            position += rbytes;
        }
    }

    return position;
}

/**
 * gwy_serializable_deserialize:
 * @buffer: Memory containing the serialized representation of one object.
 * @size: Size of @buffer in bytes.
 * @bytes_consumed: Location to store the number of bytes actually consumed
 *                  from @buffer, or %NULL.
 * @error_list: Location to store the errors occuring, %NULL to ignore.
 *
 * Deserializes an object.
 *
 * The initial reference count of the restored object is according to its
 * nature.  For instance, a #GInitiallyUnowned will have a floating reference,
 * a plain #GObject will have a reference count of 1, etc.
 *
 * Returns: Newly created object on success, %NULL on failure.
 **/
GObject*
gwy_serializable_deserialize(const guchar *buffer,
                             gsize size,
                             gsize *bytes_consumed,
                             GwyErrorList **error_list)
{
    const GwySerializableInterface *iface;
    GwySerializableItems *items = NULL, *requested_items = NULL;
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

    /* We should no only fit, but the object size should be exactly size.
     * If there is extra stuff, add an error, but do not fail. */
    if (size - pos != object_size)
        gwy_error_list_add(error_list, GWY_SERIALIZABLE_ERROR,
                           GWY_SERIALIZABLE_ERROR_PADDING,
                           _("Object `%s' data is smaller than the space "
                             "alloted for it.  The padding was ignored."));

    /* Unpack everything, call request() only after that so that is its
     * quaranteed construct() is called after request(). */
    items = unpack_items(buffer + pos, object_size, error_list);
    if (!items)
        goto fail;

    /* Allow the method return %NULL even it is implemented. */
    if (iface->request)
        requested_items = iface->request();

fail:
    if (items)
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
    if (!type) {
        gwy_error_list_add(error_list, GWY_SERIALIZABLE_ERROR,
                           GWY_SERIALIZABLE_ERROR_OBJECT,
                           _("Object type `%s' is not known. "
                             "It was ignored."),
                           typename);
        return 0;
    }

    *classref = g_type_class_ref(type);
    g_assert(*classref);
    if (!G_TYPE_IS_INSTANTIATABLE(type)) {
        gwy_error_list_add(error_list, GWY_SERIALIZABLE_ERROR,
                           GWY_SERIALIZABLE_ERROR_OBJECT,
                           _("Object type `%s' is not instantiable. "
                             "It was ignored."),
                           typename);
        return 0;
    }
    if (!g_type_is_a(type, GWY_TYPE_SERIALIZABLE)) {
        gwy_error_list_add(error_list, GWY_SERIALIZABLE_ERROR,
                           GWY_SERIALIZABLE_ERROR_OBJECT,
                           _("Object type `%s' is not serializable. "
                             "It was ignored."),
                           typename);
        return 0;
    }

    *iface = g_type_interface_peek(*classref, GWY_TYPE_SERIALIZABLE);
    if (!*iface || !(*iface)->construct) {
        gwy_error_list_add(error_list, GWY_SERIALIZABLE_ERROR,
                           GWY_SERIALIZABLE_ERROR_OBJECT,
                           _("Object type `%s' does not implement "
                             "deserialization. It was ignored."),
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
        GwySerializableCType ctype;
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
        ctype = item->ctype;

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
        else {
            gwy_error_list_add(error_list, GWY_SERIALIZABLE_ERROR,
                               GWY_SERIALIZABLE_ERROR_DATA,
                               _("Data type 0x%02x is unknown. "
                                 "It could not be just skipped, complete "
                                 "object %s was ignored."),
                               ctype, item->name);
            goto fail;
        }

        buffer += rbytes;
        size -= rbytes;
        items->n_items++;
    }

fail:
    /* TODO */
    return NULL;
}

static void
free_items(GwySerializableItems *items)
{
    gsize i;

    for (i = 0; i < items->n_items; i++) {
        GwySerializableItem *item = items->items + i;
        GwySerializableCType ctype = item->ctype;

        switch (ctype) {
            case 0:
            /* Type not assigned yet.  Can happen. */
            break;

            case GWY_SERIALIZABLE_OBJECT:
            GWY_OBJECT_UNREF(item->value.v_object);
            break;

            case GWY_SERIALIZABLE_INT8_ARRAY:
            case GWY_SERIALIZABLE_INT16_ARRAY:
            case GWY_SERIALIZABLE_INT32_ARRAY:
            case GWY_SERIALIZABLE_INT64_ARRAY:
            case GWY_SERIALIZABLE_DOUBLE_ARRAY:
            case GWY_SERIALIZABLE_STRING:
            GWY_FREE(item->value.v_int8_array);
            break;

            case GWY_SERIALIZABLE_STRING_ARRAY:
            /* TODO */
            break;

            case GWY_SERIALIZABLE_OBJECT_ARRAY:
            /* TODO */
            break;

            default:
            g_assert(g_ascii_islower(ctype));
            break;
        }
    }

    g_free(items->items);
    g_free(items);
}

/**
 * gwy_serializable_duplicate:
 * @serializable: A serializable object.
 *
 * Creates an object with identical value.
 *
 * This is a copy-constructor.  You can duplicate a %NULL, too, but you are
 * discouraged from doing it.
 *
 * Returns: The newly created object copy.
 **/
GObject*
gwy_serializable_duplicate(GwySerializable *serializable)
{
    const GwySerializableInterface *iface;

    g_return_val_if_fail(GWY_IS_SERIALIZABLE(serializable), NULL);
    iface = GWY_SERIALIZABLE_GET_INTERFACE(serializable);
    g_return_val_if_fail(iface && iface->duplicate, NULL);

    return iface->duplicate(serializable);
}

/**
 * gwy_serializable_assign:
 * @source: A serializable object.
 * @destination: An object of the same type as @source. More precisely,
 *               @source may be subclass of @destination (the extra 
 *               information is lost then).
 *
 * Copies the value of an object to another object.
 **/
void
gwy_serializable_assign(GwySerializable *destination,
                        GwySerializable *source)
{
    const GwySerializableInterface *iface;

    if (source == destination)
        return;

    g_return_if_fail(GWY_IS_SERIALIZABLE(destination));
    /* No need to check GWY_IS_SERIALIZABLE(source) */
    g_return_if_fail(g_type_is_a(G_TYPE_FROM_INSTANCE(source),
                                 G_TYPE_FROM_INSTANCE(destination)));

    iface = GWY_SERIALIZABLE_GET_INTERFACE(destination);
    g_return_if_fail(iface && iface->assign);

    return iface->assign(destination, source);
}

/**
 * SECTION: serializable
 * @title: GwySerializable
 * @short_description: Serializable value-like object interface
 *
 * GwySerializable is an abstract interface for data-like objects that can be
 * serialized and deserialized.  You can serialize any object implementing this
 * interface with gwy_serializable_serialize() and the restore (deserialize) it
 * with gwy_serializable_deserialize().
 *
 * Gwyddion implements a simple serialization model: only tree-like structures
 * of objects formed by ownership can be serialized and restored.  Any
 * inter-object relations other than plain ownership must be stored in some
 * weak form and there is no explicit support for this.  Moreover, only object
 * values are preserved, their identities are lost (in particular, signals,
 * user data and similar attributes are not subject of serialization and
 * deserialization).  This, on the other hand, permits to deserialize any saved
 * object individually and independently.
 *
 * Serialization and deserialization can fail for various reasons.  The errors
 * returned can be either from %GWY_SERIALIZABLE_ERROR or %G_IO_ERROR domain
 * (if GLib I/O streams are involved).  A number of errors that are not
 * considered fatal can occur during deserialization, these are reported in
 * the provided error list.
 *
 * Beside saving and restoration, all serializable classes implement a
 * copy-constructor that creates a duplicate of an existing object.  This
 * constructor is invoked with gwy_serializable_duplicate().  Furthermore, the
 * value of one such object can be transferred to another object of the same
 * class (or a superclass), similarly to overriden assignment operators in OO
 * languages.  This assignment is performed with gwy_serializable_assign().
 *
 * Most classes that implement #GwySerializable define convenience macros
 * for the copy-constructor and assignment operator, called
 * gwy_foo_duplicate() and gwy_foo_assign(), respectively, where
 * <literal>foo</literal> is the lowercase class name.
 *
 * <refsect2 id='libgwy-serializable-implementing'>
 * <title>Implementing #GwySerializable</title>
 * </refsect2>
 *
 * You can implement serialization and deserialization in your classes...
 *
 * <refsect2 id='libgwy-serializable-internals'>
 * <title>Gory Details of Serialization</title>
 * </refsect2>
 *
 * The following information is not strictly necessary for implementing
 * #GwySerializable interface in your classes, but it can help prevent wrong
 * decision about serialized representation of your objects.  Also, it might
 * help implementing a different serialization backend than GWY files, e.g.
 * HDF5.  Serialization occurs in several steps.
 *
 * First, all objects are recursively asked to calulcate the number named data
 * items they will serialize to (or provide a reasonable upper estimate of this
 * number).  This is done simply by calling gwy_serializable_n_items() on the
 * top-level object, objects that contain other objects must call their
 * gwy_serializable_n_items() 
 *
 * Second, a #GwySerializableItems item list is created, with fixed size
 * calculated in the first step.  All objects are then recursively asked to add
 * items representing their data to this list.  This is done simply by calling
 * gwy_serializable_itemize() on the top-level object.  The objects may
 * sometimes need to represent certain data differently than the internal
 * representation is, however, expensive transforms should be avoided,
 * especially for arrays.  This step can allocate temporary structures.
 *
 * Third, sizes of each object are calcuated and stored into the object-header
 * items created by gwy_serializable_itemize().  This again is done
 * recursively, but the objects do not participate, the calculation works with
 * the itemized list.  This step might not be necessary for different storage
 * formats.
 *
 * Subsequently, the object tree flattened into an item list is written to the
 * output stream, byte-swapping or otherwise normalizing the data on the fly if
 * necessary.  This part strongly depends on the storage format.
 *
 * Finally, virtual method done() is called for all objects.  This step frees
 * the temporary storage allocated in the itemization step, if any.  This is
 * not done recursively so that objects need not to implement this method, even
 * if they contain other objects, if they do not create any temporary data
 * during itemize().  The methods are called from the inverse order than the
 * objects, appear in the list, i.e. the most inner and last objects are
 * processed first.  This means that if done() of an object is invoked, all its
 * contained objects have been already process.  At the very end the item list
 * is freed too.
 **/

/**
 * GWY_SERIALIZABLE_ERROR:
 *
 * Error domain for serialization and deserialization.
 *
 * Errors in this domain will be from the #GwySerializableError enumeration.
 * See #GError for information on error domains.
 **/

/**
 * GwySerializable:
 *
 * Formal type of serializable objects.
 **/

/**
 * GwySerializableCType:
 * @GWY_SERIALIZABLE_HEADER: Denotes object header item.  This has no use in
 *                           interface impementation.
 * @GWY_SERIALIZABLE_INT8: Denotes a character (8bit integer).
 * @GWY_SERIALIZABLE_INT8_ARRAY: Denotes a character (8bit integer) array.
 * @GWY_SERIALIZABLE_BOOLEAN: Denotes a one-byte boolean.
 * @GWY_SERIALIZABLE_INT16: Denotes a 16bit integer.
 * @GWY_SERIALIZABLE_INT16_ARRAY: Denotes an array of 16bit integers.
 * @GWY_SERIALIZABLE_INT32: Denotes a 32bit integer.
 * @GWY_SERIALIZABLE_INT32_ARRAY: Denotes an array of 32bit integers.
 * @GWY_SERIALIZABLE_INT64: Denotes a 64bit integer.
 * @GWY_SERIALIZABLE_INT64_ARRAY: Denotes an array of 64bit integers.
 * @GWY_SERIALIZABLE_DOUBLE: Denotes a IEEE double.
 * @GWY_SERIALIZABLE_DOUBLE_ARRAY: Denotes an array of IEEE doubles.
 * @GWY_SERIALIZABLE_STRING: Denotes a C string.
 * @GWY_SERIALIZABLE_STRING_ARRAY: Denotes an array of C strings.
 * @GWY_SERIALIZABLE_OBJECT: Denotes an object.
 * @GWY_SERIALIZABLE_OBJECT_ARRAY: Denotes an array of objects.
 *
 * Type of serializable value.
 *
 * The type is a single byte, i.e. a value smaller than 256.  It is the
 * same as the character used in GWY files to denote the corresponding type.
 **/

/**
 * GwySerializableValue:
 *
 * Representation of individual values of object serializable data.
 *
 * See #GwySerializableCType for corresponding type specifiers.
 *
 * Member @v_size has no use in interface implementations (it corresponds to
 * %GWY_SERIALIZABLE_HEADER), it is used only in object header items in the
 * flattened object tree.
 *
 * Signed and unsigned integer members are provided for convenience, the
 * serialization does not distinguish between signed and unsigned integers.
 **/

/**
 * GwySerializableItem:
 * @name: Component name.
 * @array_size: Array size of the component if of an array type.  Unused
 *              otherwise.
 * @value: Item value, the interpreration depends on the @ctype member.
 * @ctype: Item type, one of the #GwySerializableCType enum.
 *
 * Information about one object component that is subject to serialization or
 * deserialization.
 **/

/**
 * GwySerializableItems:
 * @len: Allocated number of items.
 * @n_items: Number of items present.
 * @items: Array of items of total size @len with @n_items positions occupied.
 *
 * Flattened tree structure of object data used during serialization and
 * deserialization.
 **/

/**
 * GWY_IMPLEMENT_SERIALIZABLE:
 * @interface_init: The interface init function.
 *
 * Declares that a type implements #GwySerializable.
 *
 * This is a specialization of G_IMPLEMENT_INTERFACE() for
 * #GwySerializableInterface.  It is intended to be used in last
 * G_DEFINE_TYPE_EXTENDED() argument:
 * |[
 * G_DEFINE_TYPE_EXTENDED
 *     (GwyFoo, gwy_foo, G_TYPE_OBJECT, 0,
 *      GWY_IMPLEMENT_SERIALIZABLE(gwy_foo_serializable_init))
 * ]|
 **/

/**
 * GwySerializableInterface:
 * @g_interface: Parent class.
 * @n_items: Returns the number of items the object will flatten to, including
 *           items of all contained objects (but excluding the object header
 *           items of self).  This method may return a reasonable upper
 *           estimate instead of the exact number; for instance, if objects
 *           of some class are known to need 5 to 7 items, this method can
 *           simply return 7.  It should use gwy_serializable_n_items() to
 *           calculate the numbers of items the contained objects will flatten
 *           to.
 * @itemize: Appends flattened representation of the object data to the @items
 *           array.  It should use gwy_serializable_itemize() to add the data
 *           of contained objects.  It returns the number of items
 *           corresponding to this object, <emphasis>not including</emphasis>
 *           items of contained objects: every contained object counts only as
 *           one item.
 * @done: Frees all temporary data created by itemize().  It is optional but
 *        if it is defined it is guaranteed to be called after each itemize().
 * @request: Speficies what data items and of what types construct() expects
 *           to get.  It then gets this very item list as the first argument,
 *           filled with deserialized item values.  Extra items are omitted
 *           and construct() needs not to care about them.  If this method
 *           is unimplemented or returns %NULL, construct() gets a newly
 *           allocate item list that contains all the items found instead.
 * @construct: Deserializes an object from array of flattened data items.
 * @duplicate: Creates a new object with all data identical to this object.
 * @assign: Makes all data of an object identical to the data of another object
 *          of the same class.  Implementations may assume that the is-a
 *          relation is satisfied for the source object.
 *
 * Interface implemented by serializable objects.
 *
 * The object class must implement all the methods, except request() and done()
 * that are optional.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
