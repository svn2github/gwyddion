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

#include "libgwy/serializable.h"

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
 * @items: List of flattened object tree to append the representation of
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
 * <title>Implementing GwySerializable</title>
 * You can implement serialization and deserialization in your classes...
 * </refsect2>
 **/

/**
 * GwySerializableCType:
 * @GWY_SERIALIZABLE_HEADER: Denotes object header item.  This has no use in
 *                           interface impementation.
 * @GWY_SERIALIZABLE_CHAR: Denotes a character.
 * @GWY_SERIALIZABLE_CHAR_ARRAY: Denotes a character array.
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
 * The type is a single character, i.e. a value smaller than 256.  It is the
 * same as the character used in gwy files to denote the corresponding type.
 **/

/**
 * GwySerializableValue:
 * @v_boolean: Boolean value member.
 * @v_char: Character value member.
 * @v_int32: 32bit integer value member.
 * @v_int64: 64bit integer value member.
 * @v_double: IEEE double value member.
 * @v_string: C string value member.
 * @v_object: Object type member.
 * @v_size: Size type member.  This member type has no use in interface
 *          implementations (it corresponds to %GWY_SERIALIZABLE_HEADER).
 * @v_char_array: Character array value member.
 * @v_int32_array: 32bit integer array value member.
 * @v_int64_array: 64bit integer array value member.
 * @v_double_array: IEEE double array value member.
 * @v_string_array: C string array value member.
 * @v_object_array: Object array value member.
 *
 * Representation of individual values of object serializable data.
 *
 * See #GwySerializableCType for corresponding type specifiers.
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
 * gwy_serializable_serialize:
 * @serializable: A serializable object.
 * @output: Output stream to write the serialized object to.
 * @error: Location to store the error occuring, %NULL to ignore.
 *
 * Serializes an object.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE if it failed.
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
 *           estimate instead of the exact number.  It should use
 *           gwy_serializable_n_items() to calculate the numbers of items
 *           the contained objects will flatten to.
 * @itemize: Appends flattened representation of the object data to the @items
 *           array.  It should use gwy_serializable_itemize() to add the data
 *           of contained objects.  It returns the number of items
 *           corresponding to this object, <emphasis>not including</emphasis>
 *           items of contained objects: every contained object counts only as
 *           one item.
 * @done: Frees all temporary data created by itemize().  It is guaranteed to
 *        be called after each itemize().
 * @construct: Deserializes an object from array of flattened data items.
 * @duplicate: Creates a new object with all data identical to this object.
 * @assign: Makes all data of an object of the same class identical to the
 *          data of this object.
 *
 * Interface implemented by serializable objects.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
