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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
