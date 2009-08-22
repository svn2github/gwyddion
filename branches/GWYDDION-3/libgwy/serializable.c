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
#include "libgwy/serializable.h"
#include "libgwy/libgwy-aliases.h"

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
    if (!serializable)
        return NULL;

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

#define __GWY_SERIALIZABLE_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: serializable
 * @title: GwySerializable
 * @short_description: Serializable value-like object interface
 * @see_also: <link linkend="libgwy-serialize">serialize</link>
 *
 * #GwySerializable is an abstract interface for data-like objects that can be
 * serialized and deserialized.  You can serialize any object implementing this
 * interface with gwy_serializable_serialize() and the restore (deserialize) it
 * with gwy_serializable_deserialize().
 *
 * Gwyddion implements a simple serialization model: only tree-like structures
 * of objects, formed by ownership, can be serialized and restored.  Any
 * inter-object relations other than plain ownership must be stored in some
 * weak form and there is no explicit support for this.  Moreover, only object
 * values are preserved, their identities are lost (in particular, signals,
 * user data and similar attributes are not subject to serialization and
 * deserialization).  This, on the other hand, means that any saved object can
 * be restored individually and independently and still be in a meaningful
 * state.
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
 * <para>You can implement serialization and deserialization in your
 * classes...</para>
 * </refsect2>
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
 * @n_items: <para>Returns the number of items the object will flatten to,
 *           including items of all contained objects (but excluding the object
 *           header items of self).  It should use gwy_serializable_n_items()
 *           to calculate the numbers of items the contained objects will
 *           flatten to.</para>
 *           <para>This method may return a reasonable upper estimate instead
 *           of the exact number; for instance, if objects of some class are
 *           known to need 5 to 7 items, n_items() can simply return 7.</para>
 * @itemize: <para>Appends flattened representation of the object data to the
 *           @items list.  It should use gwy_serializable_itemize() to add the
 *           data of contained objects.</para>
 *           <para>The number of items corresponding to this object is
 *           returned, <emphasis>not including</emphasis> items of contained
 *           objects: every contained object counts only as one item.</para>
 * @done: <para>Frees all temporary data created by itemize().  This method
 *        is optional but if it is defined it is guaranteed to be called after
 *        each itemize().</para>
 * @construct: <para>Deserializes an object from array of flattened data
 *             items.</para>
 *             <para>All strings, objects and arrays in the item list are newly
 *             allocated.  The method can (and, generally, should) take
 *             ownership by filling corresponding item values with %NULL and
 *             setting their @array_size to zero.  The caller takes care of
 *             freeing them if they are not consumed by construct().</para>
 *             <para>This method must make any assumptions about the contents
 *             of @items.  Commonly, however, only items from a small
 *             predefined set are expected and then
 *             gwy_deserialize_filter_items() can be used to simplify the
 *             processing of the item list.</para>
 * @duplicate: <para>Creates a new object with all data identical to this
 *             object.  This method is expected to create a deep copy.
 *             Classes may provide other methods for shallow copies.</para>
 * @assign: <para>Makes all data of this object identical to the data of
 *          another object of the same class.  Implementations may assume that
 *          the is-a relation is satisfied for the source object.  This method
 *          is expected to perform a deep copy.  Classes may provide other
 *          methods for shallow copies.</para>
 *
 * Interface implemented by serializable objects.
 *
 * The object class must implement all the methods, except request() and done()
 * that are optional.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
