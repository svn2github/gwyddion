/*
 *  @(#) $Id$
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
 * sizeof(gsize) <= sizeof(guint64)
 * sizeof(gchar) == 1
 * sizeof(gdouble) == sizeof(guint64)
 */

#include <string.h>
#include "libgwy/serializable.h"

enum {
    GWY_SERIALIZE_BUFFER_SIZE = 65536,
    GWY_SERIALIZE_BUFFER_SIZE32 = GWY_SERIALIZE_BUFFER_SIZE/sizeof(guint32),
    GWY_SERIALIZE_BUFFER_SIZE64 = GWY_SERIALIZE_BUFFER_SIZE/sizeof(guint64),
};

static gsize    gwy_serializable_calculate_sizes(GwySerializableItems *items,
                                                 gsize *pos);
static void     gwy_serializable_items_done     (const GwySerializableItems *items);
static gboolean gwy_serializable_dump_to_stream (const GwySerializableItems *items,
                                                 gpointer *buffer,
                                                 GOutputStream *output,
                                                 GError **error);

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

    iface = GWY_SERIALIZABLE_GET_INTERFACE(serializable);
    g_return_if_fail(iface && iface->itemize);

    g_return_if_fail(items->n_items >= items->len);
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
 * The data will be written in many small chunks (generally, 3 writes per an
 * item).  Therefore if @output is not a plain memory buffer, it should be
 * wrapped by #GBufferedOutputStream to reduce the number of syscalls.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE if it failed.
 **/
gboolean
gwy_serializable_serialize(GwySerializable *serializable,
                           GOutputStream *output,
                           GError **error)
{
    GwySerializableItems items;
    gboolean ok = FALSE;
    gpointer buffer = NULL;
    gsize i = 0;

    g_return_val_if_fail(GWY_IS_SERIALIZABLE(serializable), FALSE);
    g_return_val_if_fail(G_IS_OUTPUT_STREAM(output), FALSE);

    items.len = gwy_serializable_n_items(serializable);
    items.items = g_new(GwySerializableItem, items.len);
    items.n_items = 0;

    gwy_serializable_itemize(serializable, &items);
    gwy_serializable_calculate_sizes(&items, &i);
    ok = gwy_serializable_dump_to_stream(&items, &buffer, output, error);
    g_free(buffer);
    gwy_serializable_items_done(&items);
    g_free(items.items);

    return ok;
}

/**
 * gwy_serializable_ctype_size:
 * @ctype: Component type.
 *
 * Computes type size based on type letter.
 *
 * Returns: Size in bytes, 0 for arrays and other nonatomic types.
 **/
static inline gsize G_GNUC_CONST
gwy_serializable_ctype_size(GwySerializableCType ctype)
{
    switch (ctype) {
        case GWY_SERIALIZABLE_INT8:
        case GWY_SERIALIZABLE_BOOLEAN:
        return sizeof(guint8);
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
gsize
gwy_serializable_calculate_sizes(GwySerializableItems *items,
                                 gsize *pos)
{
    GwySerializableItem *header = items->items + (*pos)++;
    gsize i, n, size;

    g_return_val_if_fail(header->ctype == GWY_SERIALIZABLE_HEADER, 0);

    n = header->array_size;
    size = strlen(header->name)+1 + 8 /* object size */;
    for (i = 1; i < n; i++) {
        /* It is important to increment pos now because recusive invocations
         * expect pos already pointing to the header item. */
        const GwySerializableItem *item = items->items + (*pos)++;
        const GwySerializableCType ctype = item->ctype;
        gsize typesize;

        size += strlen(item->name)+1 + 1 /* ctype */;

        if ((typesize = gwy_serializable_ctype_size(ctype)))
            size += typesize;
        else if ((typesize
                  = gwy_serializable_ctype_size(g_ascii_tolower(ctype)))) {
            g_warn_if_fail(item->array_size != 0);
            size += typesize*item->array_size;
        }
        else if (ctype == GWY_SERIALIZABLE_STRING)
            size += strlen((const gchar*)item->value.v_string)+1;
        else if (ctype == GWY_SERIALIZABLE_OBJECT)
            size += gwy_serializable_calculate_sizes(items, pos);
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
                size += gwy_serializable_calculate_sizes(items, pos);
        }
        else {
            g_return_val_if_reached(0);
        }
        g_return_val_if_fail(*pos <= items->len, 0);
    }

    return header->value.v_size = size;
}

static gboolean
gwy_serializable_write_swap32(const guint32 *values,
                              gsize len,
                              gpointer *buffer,
                              GOutputStream *output,
                              GError **error)
{
    const gsize n = len/GWY_SERIALIZE_BUFFER_SIZE32;
    guint32 *bvalues;
    gsize i, j;

    if (!*buffer)
        *buffer = g_malloc(GWY_SERIALIZE_BUFFER_SIZE);

    bvalues = (guint32*)buffer;
    for (i = 0; i < n; i++) {
        for (j = 0; j < GWY_SERIALIZE_BUFFER_SIZE32; j++)
            bvalues[j] = GUINT32_TO_LE(values[j]);

        if (!g_output_stream_write_all(output, bvalues,
                                       GWY_SERIALIZE_BUFFER_SIZE,
                                       NULL, NULL, error))
            return FALSE;

        values += GWY_SERIALIZE_BUFFER_SIZE32;
    }

    i = len % GWY_SERIALIZE_BUFFER_SIZE32;
    for (j = 0; j < i; j++)
        bvalues[j] = GUINT32_SWAP_LE_BE(values[j]);

    return g_output_stream_write_all(output, bvalues, sizeof(guint32)*i,
                                     NULL, NULL, error);
}

static gboolean
gwy_serializable_write_swap64(const guint64 *values,
                              gsize len,
                              gpointer *buffer,
                              GOutputStream *output,
                              GError **error)
{
    const gsize n = len/GWY_SERIALIZE_BUFFER_SIZE64;
    guint64 *bvalues;
    gsize i, j;

    if (!*buffer)
        *buffer = g_malloc(GWY_SERIALIZE_BUFFER_SIZE);

    bvalues = (guint64*)buffer;
    for (i = 0; i < n; i++) {
        for (j = 0; j < GWY_SERIALIZE_BUFFER_SIZE64; j++)
            bvalues[j] = GUINT64_TO_LE(values[j]);

        if (!g_output_stream_write_all(output, bvalues,
                                       GWY_SERIALIZE_BUFFER_SIZE,
                                       NULL, NULL, error))
            return FALSE;

        values += GWY_SERIALIZE_BUFFER_SIZE64;
    }

    i = len % GWY_SERIALIZE_BUFFER_SIZE64;
    for (j = 0; j < i; j++)
        bvalues[j] = GUINT64_SWAP_LE_BE(values[j]);

    return g_output_stream_write_all(output, bvalues, sizeof(guint64)*i,
                                     NULL, NULL, error);
}

/**
 * gwy_serializable_dump_to_stream:
 * @items: List of flattened object tree items.
 * @output: Output stream to write the serialized object to.
 * @error: Location to store the error occuring, %NULL to ignore.
 *
 * Write itemized object tree list into an output stream.
 *
 * Byte-swapping and similar transforms are done on-the-fly, as necessary.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE if it failed.
 **/
static gboolean
gwy_serializable_dump_to_stream(const GwySerializableItems *items,
                                gpointer *buffer,
                                GOutputStream *output,
                                GError **error)
{
    gsize i;

    for (i = 0; i < items->n_items; i++) {
        const GwySerializableItem *item = items->items + i;
        /* Use the single-byte type here to faciliate writing. */
        const guint8 ctype = item->ctype;
        gsize len = strlen(item->name) + 1;

        if (!g_output_stream_write(output, item->name, len, NULL, error))
            return FALSE;

        if (ctype == GWY_SERIALIZABLE_HEADER) {
            /* The size stored in GWY files exludes the name and itself. */
            guint64 v = item->value.v_size - len - sizeof(guint64);

            v = GUINT64_TO_LE(v);
            if (!g_output_stream_write_all(output, &v, sizeof(guint64),
                                           NULL, NULL, error))
                return FALSE;
            continue;
        }

        if (!g_output_stream_write(output, &ctype, sizeof(guint8), NULL, error))
            return FALSE;

        /* More stuff is in the following GWY_SERIALIZABLE_HEADER item. */
        if (ctype == GWY_SERIALIZABLE_OBJECT)
            continue;

        if (ctype == GWY_SERIALIZABLE_INT8) {
            if (!g_output_stream_write(output,
                                       &item->value.v_uint8,
                                       sizeof(guint8),
                                       NULL, error))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_BOOLEAN) {
            guint8 v = !!item->value.v_boolean;
            if (!g_output_stream_write(output, &v, sizeof(guint8), NULL, error))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_INT32) {
            guint32 v = GUINT32_TO_LE(item->value.v_uint32);
            if (!g_output_stream_write_all(output, &v, sizeof(guint32),
                                           NULL, NULL, error))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_INT64
                 || ctype == GWY_SERIALIZABLE_DOUBLE) {
            guint64 v = GUINT64_TO_LE(item->value.v_uint64);
            if (!g_output_stream_write_all(output, &v, sizeof(guint64),
                                           NULL, NULL, error))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_STRING) {
            const gchar *s = item->value.v_string;
            if (!g_output_stream_write_all(output, s, strlen(s)+1,
                                           NULL, NULL, error))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_INT8_ARRAY) {
            guint64 v = GUINT64_TO_LE(item->array_size);
            if (!g_output_stream_write_all(output, &v, sizeof(guint64),
                                           NULL, NULL, error))
                return FALSE;
            if (!g_output_stream_write_all(output,
                                           item->value.v_uint8_array,
                                           item->array_size,
                                           NULL, NULL, error))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_INT32_ARRAY) {
            guint64 v = GUINT64_TO_LE(item->array_size);
            if (!g_output_stream_write_all(output, &v, sizeof(guint64),
                                           NULL, NULL, error))
                return FALSE;
            if (!gwy_serializable_write_swap32(item->value.v_uint32_array,
                                               item->array_size,
                                               buffer, output, error))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_INT64_ARRAY) {
            guint64 v = GUINT64_TO_LE(item->array_size);
            if (!g_output_stream_write_all(output, &v, sizeof(guint64),
                                           NULL, NULL, error))
                return FALSE;
            if (!gwy_serializable_write_swap64(item->value.v_uint64_array,
                                               item->array_size,
                                               buffer, output, error))
                return FALSE;
        }
        else if (ctype == GWY_SERIALIZABLE_STRING_ARRAY) {
            guint64 v = GUINT64_TO_LE(item->array_size);
            gsize j;
            if (!g_output_stream_write_all(output, &v, sizeof(guint64),
                                           NULL, NULL, error))
                return FALSE;

            for (j = 0; j < item->array_size; j++) {
                const gchar *s = item->value.v_string_array[j];
                if (!g_output_stream_write_all(output, s, strlen(s)+1,
                                               NULL, NULL, error))
                    return FALSE;
            }
        }
        else if (ctype == GWY_SERIALIZABLE_OBJECT_ARRAY) {
            guint64 v = GUINT64_TO_LE(item->array_size);
            if (!g_output_stream_write_all(output, &v, sizeof(guint64),
                                           NULL, NULL, error))
                return FALSE;
            /* Serialized objects follow... */
        }
        else {
            g_return_val_if_reached(FALSE);
        }
    }

    return TRUE;
}

/**
 * gwy_serializable_items_done:
 * @items: List of flattened object tree items.
 *
 * Call method done on all objects that need it, in reverse order.
 **/
static void
gwy_serializable_items_done(const GwySerializableItems *items)
{
    gsize i;

    /* The zeroth item is always an object header. */
    for (i = items->len-1; i > 0; i--) {
        const GwySerializableItem *item = items->items + i;
        if (item->ctype == GWY_SERIALIZABLE_OBJECT)
            gwy_serializable_done(GWY_SERIALIZABLE(item->value.v_object));
    }
}

/**
 * SECTION:serializable
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
 * GwySerializableCType:
 * @GWY_SERIALIZABLE_HEADER: Denotes object header item.  This has no use in
 *                           interface impementation.
 * @GWY_SERIALIZABLE_INT8: Denotes a character (8bit integer).
 * @GWY_SERIALIZABLE_INT8_ARRAY: Denotes a character (8bit integer) array.
 * @GWY_SERIALIZABLE_BOOLEAN: Denotes a one-byte boolean.
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
 * gwy_serializable_deserialize:
 * @input: Input stream to read the serialized object from.
 * @error: Location to store the error occuring, %NULL to ignore.
 *
 * Deserializes an object.
 *
 * The initial reference count of the restored object is according to its
 * nature.  For instance, a #GInitiallyUnowned will have a floating reference,
 * a plain #GObject will have a reference count of 1, etc.
 *
 * Returns: Newly created object on success, %NULL on failure.
 **/

/**
 * gwy_serializable_assign:
 * @source: A serializable object.
 * @destination: An object of the same type as @source. More precisely,
 *               @source may be subclass of @destination (the extra 
 *               information is lost then).
 *
 * Copies the value of an object to another object.
 **/

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
 * @construct: Deserializes an object from array of flattened data items.
 * @duplicate: Creates a new object with all data identical to this object.
 * @assign: Makes all data of an object of the same class identical to the
 *          data of this object.
 *
 * Interface implemented by serializable objects.
 *
 * The object class must implement all the methods, except done() which is
 * optional.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
