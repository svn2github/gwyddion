/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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

G_DEFINE_INTERFACE(GwySerializable, gwy_serializable, G_TYPE_OBJECT);

static void
gwy_serializable_default_init(G_GNUC_UNUSED GwySerializableInterface *iface)
{
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
 * Creates the flattened representation of a serializable object.
 *
 * This function wraps the virtual table method itemize().  It deals with
 * recording of the object header item.  Method itemize() then only serialises
 * the actual data of the object.
 **/
void
gwy_serializable_itemize(GwySerializable *serializable,
                         GwySerializableItems *items)
{
    g_return_if_fail(GWY_IS_SERIALIZABLE(serializable));
    const GwySerializableInterface *iface
        = GWY_SERIALIZABLE_GET_INTERFACE(serializable);
    g_return_if_fail(iface && iface->itemize);

    g_return_if_fail(items->n < items->len);
    GwySerializableItem *item = items->items + items->n;
    items->n++;
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
 * Returns: (transfer full):
 *          The newly created object copy.
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


/**
 * SECTION: serializable
 * @title: GwySerializable
 * @short_description: Serialisable value-like object interface
 * @see_also: <link linkend="libgwy-serialize">serialize</link>
 *
 * #GwySerializable is an abstract interface for data-like objects that can be
 * serialised and deserialised.  You can serialise any object implementing this
 * interface with functions such as gwy_serialize_gio() and the restore
 * (deserialise) it with gwy_deserialize_memory().
 *
 * Gwyddion implements a simple serialisation model: only tree-like structures
 * of objects, formed by ownership, can be serialised and restored.  Any
 * inter-object relations other than plain ownership must be stored in some
 * weak form and there is no explicit support for this.  Moreover, only object
 * values are preserved, their identities are lost (in particular, signals,
 * user data and similar attributes are not subject to serialisation and
 * deserialisation).  This, on the other hand, means that any saved object can
 * be restored individually and independently and still be in a meaningful
 * state.
 *
 * Beside saving and restoration, all serialisable classes implement a
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
 * <para>You can implement serialisation and deserialisation in your
 * classes.  There is a convenience macro %GWY_IMPLEMENT_SERIALIZABLE available
 * declaring implementation of the interface in the type function.  The copy
 * constructor and assignment operators are usually easy so we focus on
 * serialisation and deserialisation.</para>
 * |[
 * // Standard object structure definitions
 * typedef struct _MyObject      MyObject;
 * typedef struct _MyObjectClass MyObjectClass;
 *
 * struct _MyObject {
 *     GObject g_object;
 *     gint32 data;
 * };
 *
 * struct _MyObjectClass {
 *     GObjectClass g_object_class;
 * };
 *
 * // Define the type
 * G_DEFINE_TYPE_EXTENDED
 *     (MyObject, my_object, G_TYPE_OBJECT, 0,
 *      GWY_IMPLEMENT_SERIALIZABLE(my_object_serializable_init));
 *
 * // Serialised data specification, used both in itemize() and construct()
 * static const GwySerializableItem default_items[] = {
 *     { .name = "data", .ctype = GWY_SERIALIZABLE_INT32, },
 * };
 *
 * // Implement the interface
 * static void
 * my_object_serializable_init(GwySerializableInterface *iface)
 * {
 *     iface->n_items   = my_object_n_items;
 *     iface->itemize   = my_object_itemize;
 *     iface->done      = my_object_done;
 *     iface->construct = my_object_construct;
 *     iface->duplicate = my_object_duplicate_impl;
 *     iface->assign =    my_object_assign_impl;
 * }
 *
 * // Duplication is easy, create a new object and set data
 * static GObject*
 * my_object_duplicate_impl(GwySerializable *serializable)
 * {
 *     MyObject *myobject = GWY_MY_OBJECT(serializable);
 *     MyObject *copy = g_object_newv(MY_TYPE_OBJECT, 0, NULL);
 *
 *     copy->data = myobject->data;
 * }
 *
 * // Assigning is even easier, just set data
 * static GObject*
 * my_object_assign_impl(GwySerializable *serializable,
 *                       GwySerializable *source)
 * {
 *     MyObject *myobject = GWY_MY_OBJECT(serializable);
 *     MyObject *src = GWY_MY_OBJECT(source);
 *
 *     src->data = myobject->data;
 * }
 *
 * // Our object needs at most one item and it does not contain any child
 * // objects.  Thus we can simply return a constant.
 * static gsize
 * my_object_n_items(G_GNUC_UNUSED GwySerializable *serializable)
 * {
 *     return 1;
 * }
 *
 * #define add_item(id) \
 *     g_return_val_if_fail(items->len - items->n, 0); \
 *     items->items[items->n++] = it[id]; \
 *     n_items++
 *
 * // Fill the item list with actual object data.  This is a bit
 * // overcomplicated for such a simple object as MyObject, of course.
 * static gsize
 * my_object_itemize(GwySerializable *serializable,
 *                   GwySerializableItems *items)
 * {
 *     MyObject *myobject = GWY_MY_OBJECT(serializable);
 *     GwySerializableItem it[G_N_ELEMENTS(default_items)];
 *     gsize n_items = 0;
 *
 *     memcpy(it, default_items, sizeof(default_items));
 *
 *     // Do not store the data if it has the default value
 *     if (myobject->data) {
 *         it[0].value.v_int32 = myobject->data;
 *         add_item(0);
 *     }
 *
 *     return n_items;
 * }
 *
 * // Construct the object from item list.  The data item does not need to be
 * // present, in this case the default zero value is used.  Also, for more
 * // complicated objects the deserialisation can fail or resources need to be
 * // released; see GwySerializableInterface for the clean-up rules.
 * static gboolean
 * my_object_construct(GwySerializable *serializable,
 *                     GwySerializableItems *items,
 *                     GwyErrorList **error_list)
 * {
 *     GwySerializableItem it[G_N_ELEMENTS(default_items)];
 *
 *     memcpy(it, default_items, sizeof(default_items));
 *     gwy_deserialize_filter_items(it, G_N_ELEMENTS(it), items, "MyObject",
 *                                  error_list);
 *
 *     MyObject *myobject = GWY_MY_OBJECT(serializable);
 *     myobject->data = it[0].value.v_int32;
 *
 *     return TRUE;
 * }
 * ]|
 * </refsect2>
 **/

/**
 * GwySerializable:
 *
 * Formal type of serialisable objects.
 **/

/**
 * GwySerializableCType:
 * @GWY_SERIALIZABLE_HEADER: Denotes object header item.  This has no use in
 *                           interface impementation and does not actually
 *                           appear in files (although this depends on the
 *                           serialiser/deserialiser implementation).  The
 *                           serialiser/deserialised sets @size to the object
 *                           data size and @array_size to the number of
 *                           items.
 * @GWY_SERIALIZABLE_PARENT: Denotes parent's data separator, used when
 *                           subclasses want to chain serialisation and
 *                           deserialisation to the parent class.  If they do
 *                           so then this item must be always included even if
 *                           the parent data turn out to be empty for a
 *                           speficif instance.  The item has no data and its
 *                           name is the parent type name.  For convenience,
 *                           the value is set to the corresponding #GType in
 *                           deserialisation.  The most derived child data
 *                           comes first, then the parents' in reverse order of
 *                           deriving.
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
 * @GWY_SERIALIZABLE_BOXED: Denotes a serialisable boxed type.
 *
 * Type of serialisable value.
 *
 * The type is a single byte, i.e. a value smaller than 256.  It is equal to
 * the character used in GWY files to denote the corresponding type.
 **/

/**
 * GwySerializableValue:
 * @v_boolean: The value as a #gboolean.
 * @v_int8: The value as a #gint8.
 * @v_uint8: The value as a #guint8.
 * @v_int16: The value as a #gint16.
 * @v_uint16: The value as a #guint16.
 * @v_int32: The value as a #gint32.
 * @v_uint32: The value as a #guint32.
 * @v_int64: The value as a #gint64.
 * @v_uint64: The value as a #guint64.
 * @v_double: The value as a #gdouble.
 * @v_string: The value as a #gchar pointer.
 * @v_ustring: The value as a #guchar pointer.
 * @v_object: The value as an object.
 * @v_boxed: The value as a boxed type.
 * @v_size: The value as a #gsize.
 * @v_type: The value as a #GType.
 * @v_int8_array: The value as an array of #gint8<!-- -->s.
 * @v_uint8_array: The value as an array of #guint8<!-- -->s.
 * @v_int16_array: The value as an array of #gint16<!-- -->s.
 * @v_uint16_array: The value as an array of #guint16<!-- -->s.
 * @v_int32_array: The value as an array of #gint32<!-- -->s.
 * @v_uint32_array: The value as an array of #guint32<!-- -->s.
 * @v_int64_array: The value as an array of #gint64<!-- -->s.
 * @v_uint64_array: The value as an array of #guint64<!-- -->s.
 * @v_double_array: The value as an array of #gdouble<!-- -->s.
 * @v_string_array: The value as an array of #gchar pointers.
 * @v_ustring_array: The value as an array of #guchar pointers.
 * @v_object_array: The value as an array of objects.
 *
 * Representation of individual values of object serialisable data.
 *
 * See #GwySerializableCType for corresponding type specifiers.
 *
 * Member @v_size has no use in interface implementations (it corresponds to
 * %GWY_SERIALIZABLE_HEADER), it is used only in object header items in the
 * flattened object tree.
 *
 * Signed and unsigned integer members are provided for convenience, the
 * serialisation does not distinguish between signed and unsigned integers.
 **/

/**
 * GwySerializableItem:
 * @name: Component name.
 * @array_size: Array size of the component if of an array type.  For boxed
 *              types that do not carry type information, deserialisation sets
 *              is to the #GType of the boxed type.  Unused otherwise.
 * @value: Item value, the interpreration depends on the @ctype member.
 * @ctype: Item type, one of the #GwySerializableCType enum.
 *
 * Information about one object component that is subject to serialisation or
 * deserialisation.
 **/

/**
 * GwySerializableItems:
 * @len: Allocated number of items.
 * @n: Number of items present.
 * @items: Array of items of total size @len with @n positions occupied.
 *
 * Flattened tree structure of object data used during serialisation and
 * deserialisation.
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
 *      GWY_IMPLEMENT_SERIALIZABLE(gwy_foo_serializable_init));
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
 * @done: <para>Frees all temporary data created by
 *        #GwySerializableInterface.itemize().  This method is optional but if
 *        it is defined it is guaranteed to be called after each
 *        #GwySerializableInterface.itemize().</para>
 * @construct: <para>Deserialises an object from array of flattened data
 *             items.  The first argument is the object of the final class
 *             (i.e. it may be a derived class), consructed with the default
 *             parameterless constructor.</para>
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
 *             <para>It is possible to chain this method to the parent class
 *             and pass it the the item list or a part of it, if you are
 *             careful.</para>
 * @duplicate: <para>Creates a new object with all data identical to this
 *             object.  This method is expected to create a deep copy.
 *             Classes may provide other methods for shallow copies.</para>
 * @assign: <para>Makes all data of this object identical to the data of
 *          another object of the same class.  Implementations may assume that
 *          the is-a relation is satisfied for the source object.  This method
 *          is expected to perform a deep copy.  Classes may provide other
 *          methods for shallow copies.</para>
 *
 * Interface implemented by serialisable objects.
 *
 * The object class must implement all the methods, except
 * #GwySerializableInterface.request() and #GwySerializableInterface.done()
 * that are optional.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
