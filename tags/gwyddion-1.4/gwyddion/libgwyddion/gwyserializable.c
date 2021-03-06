/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <string.h>
#include <glib-object.h>
#include <glib/gutils.h>

#include <libgwyddion/gwymacros.h>
#include "gwyserializable.h"

#define GWY_SERIALIZABLE_TYPE_NAME "GwySerializable"

static void     gwy_serializable_base_init          (gpointer g_class);
static GObject* gwy_serializable_duplicate_hard_way (GObject *object);

static inline gsize ctype_size     (guchar ctype);

GType
gwy_serializable_get_type(void)
{
    static GType gwy_serializable_type = 0;

    if (!gwy_serializable_type) {
        static const GTypeInfo gwy_serializable_info = {
            sizeof(GwySerializableIface),
            (GBaseInitFunc)gwy_serializable_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL,
            NULL,
        };

        gwy_serializable_type
            = g_type_register_static(G_TYPE_INTERFACE,
                                     GWY_SERIALIZABLE_TYPE_NAME,
                                     &gwy_serializable_info,
                                     0);
        g_type_interface_add_prerequisite(gwy_serializable_type, G_TYPE_OBJECT);
    }

    gwy_debug("%lu", gwy_serializable_type);
    return gwy_serializable_type;
}

static void
gwy_serializable_base_init(G_GNUC_UNUSED gpointer g_class)
{
    static gboolean initialized = FALSE;

    gwy_debug("initialized = %d", initialized);
    if (initialized)
        return;
    initialized = TRUE;
}

/**
 * gwy_serializable_serialize:
 * @serializable: A #GObject implementing #GwySerializable interface.
 * @buffer: A buffer to which the serialized object should be appended,
 *          or %NULL.
 *
 * Serializes an object implementing #GwySerializable interface.
 *
 * Returns: @buffer or a newly allocated #GBaseInitFunc with serialized
 *          object appended.
 **/
GByteArray*
gwy_serializable_serialize(GObject *serializable,
                           GByteArray *buffer)
{
    GwySerializeFunc serialize_method;

    g_return_val_if_fail(serializable, NULL);
    g_return_val_if_fail(GWY_IS_SERIALIZABLE(serializable), NULL);
    gwy_debug("serializing a %s",
              g_type_name(G_TYPE_FROM_INSTANCE(serializable)));

    serialize_method = GWY_SERIALIZABLE_GET_IFACE(serializable)->serialize;
    if (!serialize_method) {
        g_error("%s doesn't implement serialize()",
                g_type_name(G_TYPE_FROM_INSTANCE(serializable)));
        return NULL;
    }
    return serialize_method(serializable, buffer);
}

/**
 * gwy_serializable_deserialize:
 * @buffer: A block of memory of size @size contaning object representation.
 * @size: The size of @buffer.
 * @position: The position of the object in @buffer, it's updated to
 *            point after it.
 *
 * Restores a serialized object.
 *
 * The newly created object has reference count according to its nature, thus
 * a #GtkObject will have a floating reference, a #GObject will have a
 * refcount of 1, etc.
 *
 * Returns: A newly created object.
 **/
GObject*
gwy_serializable_deserialize(const guchar *buffer,
                             gsize size,
                             gsize *position)
{
    GType type;
    GwyDeserializeFunc deserialize_method;
    GObject *object;

    g_return_val_if_fail(buffer, NULL);
    if (!gwy_serialize_check_string(buffer, size, *position, NULL)) {
        g_error("memory contents at %p doesn't look as an serialized object",
                buffer);
        return NULL;
    }

    type = g_type_from_name((gchar*)(buffer + *position));
    g_type_class_ref(type);
    gwy_debug("deserializing a %s", g_type_name(type));
    g_return_val_if_fail(type, NULL);
    g_return_val_if_fail(G_TYPE_IS_INSTANTIATABLE(type), NULL);
    g_return_val_if_fail(g_type_is_a(type, GWY_TYPE_SERIALIZABLE), NULL);

    /* FIXME: this horrible construct gets interface class from a mere GType;
     * deserialize() is a class method, not an object method, there already
     * has to be some macro for it in gobject... */
    deserialize_method
        = ((GwySerializableIface*)
                g_type_interface_peek(g_type_class_peek(type),
                                      GWY_TYPE_SERIALIZABLE))->deserialize;
    if (!deserialize_method) {
        g_error("%s doesn't implement deserialize()", buffer);
        return NULL;
    }
    object = deserialize_method(buffer, size, position);
    if (object)
        g_type_class_unref(G_OBJECT_GET_CLASS(object));
    else
        g_warning("Cannot unref class after failed %s deserialization",
                  g_type_name(type));
    return object;
}

/**
 * gwy_serializable_duplicate:
 * @object: A #GObject implementing #GwySerializable interface.
 *
 * Creates a copy of object @object.
 *
 * If the object doesn't support duplication natively, it's brute-force
 * serialized and then deserialized, this may be quite inefficient,
 * namely for large objects.
 *
 * You can duplicate a %NULL, too, but you are discouraged from doing it.
 *
 * Returns: The newly created object copy.  However if the object is a
 *          singleton, @object itself (with incremented reference count)
 *          can be returned, too.
 **/
GObject*
gwy_serializable_duplicate(GObject *object)
{
    GwyDuplicateFunc duplicate_method;

    if (!object) {
        g_warning("trying to duplicate a NULL");
        return NULL;
    }
    g_return_val_if_fail(GWY_IS_SERIALIZABLE(object), NULL);

    duplicate_method = GWY_SERIALIZABLE_GET_IFACE(object)->duplicate;
    if (duplicate_method)
        return duplicate_method(object);

    return gwy_serializable_duplicate_hard_way(object);
}

static GObject*
gwy_serializable_duplicate_hard_way(GObject *object)
{
    GByteArray *buffer = NULL;
    gsize position = 0;
    GObject *duplicate;

    g_warning("%s doesn't have its own duplicate() method, "
              "forced to duplicate it the hard way.",
              g_type_name(G_TYPE_FROM_INSTANCE(object)));

    buffer = gwy_serializable_serialize(object, NULL);
    if (!buffer) {
        g_critical("%s serialization failed",
                   g_type_name(G_TYPE_FROM_INSTANCE(object)));
        return NULL;
    }
    duplicate = gwy_serializable_deserialize(buffer->data, buffer->len,
                                             &position);
    g_byte_array_free(buffer, TRUE);

    return duplicate;
}

/**
 * gwy_serialize_store_int32:
 * @buffer: A buffer to which the value should be stored.
 * @position: Position in the buffer to store @value to.
 * @value: A 32bit integer.
 *
 * Stores a 32bit integer to a buffer.
 **/
void
gwy_serialize_store_int32(GByteArray *buffer,
                          gsize position,
                          guint32 value)
{
    value = GINT32_TO_LE(value);
    memcpy(buffer->data + position, &value, sizeof(guint32));
}

/**
 * gwy_serialize_pack:
 * @buffer: A buffer to which the serialized values should be appended,
 *          or %NULL.
 * @templ: A template string.
 * @...: A list of atomic values to serialize.
 *
 * Serializes a list of plain atomic types.
 *
 * The @templ string can contain following characters:
 *
 * 'b' for a a boolean, 'c' for a character, 'i' for a 32bit integer,
 * 'q' for a 64bit integer, 'd' for a double, 's' for a null-terminated string.
 *
 * 'C' for a character array (a #gsize length followed by a pointer to the
 * array), 'I' for a 32bit integer array, 'Q' for a 64bit integer array,
 * 'D' for a double array.
 *
 * 'o' for a serializable object.
 *
 * FIXME: this function currently doesn't create architecture-independent
 * representations, it just copies the memory.
 *
 * Returns: @buffer or a newly allocated #GByteArray with serialization of
 *          given values appended.
 **/
GByteArray*
gwy_serialize_pack(GByteArray *buffer,
                   const gchar *templ,
                   ...)
{
    va_list ap;
    gsize nargs, i;

    gwy_debug("templ: %s", templ);
    nargs = strlen(templ);
    if (!nargs)
        return buffer;

    gwy_debug("nargs = %d, buffer = %p", nargs, buffer);
    if (!buffer)
        buffer = g_byte_array_new();

    va_start(ap, templ);
    for (i = 0; i < nargs; i++) {
        gwy_debug("<%c> %lu", templ[i], buffer->len);
        switch (templ[i]) {
            case 'b':
            {
                char value = va_arg(ap, gboolean);  /* store it as char */

                g_byte_array_append(buffer, &value, 1);
            }
            break;

            case 'c':
            {
                char value = va_arg(ap, int);

                g_byte_array_append(buffer, &value, 1);
            }
            break;

            case 'C':
            {
                gint32 alen = va_arg(ap, gsize);
                guchar *value = va_arg(ap, guchar*);
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
                g_byte_array_append(buffer, (guint8*)&alen, sizeof(gint32));
#else
                gint32 lealen = GINT32_TO_LE(alen);
                g_byte_array_append(buffer, (guint8*)&lealen, sizeof(gint32));
#endif
                g_byte_array_append(buffer, value, alen*sizeof(char));
            }
            break;

            case 'i':
            {
                gint32 value = va_arg(ap, gint32);

                value = GINT32_TO_LE(value);
                g_byte_array_append(buffer, (guint8*)&value, sizeof(gint32));
            }
            break;

            case 'I':
            {
                guint32 alen = va_arg(ap, gsize);
                gint32 *value = va_arg(ap, gint32*);
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
                /* optimize for the common case */
                g_byte_array_append(buffer, (guint8*)&alen, sizeof(gint32));
                g_byte_array_append(buffer, (guint8*)value,
                                    alen*sizeof(gint32));
#else
#warning FIXME
                /* we can't operate on value in-place as it may be read-only
                 * we can't simply operate on buffer, as it may not be
                 * aligned
                 * is there anything better than a copy? */
                guint32 lealen = GINT32_TO_LE(alen);
                gint32 *levalue = g_new(gint32, alen);
                gsize j;

                g_byte_array_append(buffer, (guint8*)&lealen, sizeof(gint32));
                for (j = 0; j < alen; j++)
                    levalue[j] = GINT32_TO_LE(value[j]);
                g_byte_array_append(buffer, (guint8*)levalue,
                                    alen*sizeof(gint32));
                g_free(levalue);
#endif
            }
            break;

            case 'q':
            {
                gint64 value = va_arg(ap, gint64);

                g_byte_array_append(buffer, (guint8*)&value, sizeof(gint64));
            }
            break;

            case 'Q':
            {
                guint32 alen = va_arg(ap, gsize);
                gint64 *value = va_arg(ap, gint64*);
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
                /* optimize for the common case */
                g_byte_array_append(buffer, (guint8*)&alen, sizeof(gint32));
                g_byte_array_append(buffer, (guint8*)value,
                                    alen*sizeof(gint64));
#else
#warning FIXME
                /* we can't operate on value in-place as it may be read-only
                 * we can't simply operate on buffer, as it may not be
                 * aligned
                 * is there anything better than a copy? */
                guint32 lealen = GUINT32_TO_LE(alen);
                gint64 *levalue = g_new(gint64, alen);
                gsize j;

                g_byte_array_append(buffer, (guint8*)&lealen, sizeof(gint32));
                for (j = 0; j < alen; j++)
                    levalue[j] = GINT64_TO_LE(value[j]);
                g_byte_array_append(buffer, (guint8*)levalue,
                                    alen*sizeof(gint64));
                g_free(levalue);
#endif
            }
            break;

            case 'd':
            {
                double value = va_arg(ap, double);

                g_byte_array_append(buffer, (guint8*)&value, sizeof(double));
            }
            break;

            case 'D':
            {
                guint32 alen = va_arg(ap, gsize);
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
                double *value = va_arg(ap, double*);
                /* optimize for the common case */
                g_byte_array_append(buffer, (guint8*)&alen, sizeof(gint32));
                g_byte_array_append(buffer, (guint8*)value,
                                    alen*sizeof(double));
#else
#warning FIXME FIXME FIXME
                /* we can't operate on value in-place as it may be read-only
                 * we can't simply operate on buffer, as it may not be
                 * aligned
                 * is there anything better than a copy? */
                /* XXX: we swap bytes like it was a gint64, since it should
                 * not matter (one octuple like another), but who knows... */
                gint64 *value = va_arg(ap, gint64*);
                guint32 lealen = GUINT32_TO_LE(alen);
                gint64 *levalue = g_new(gint64, alen);
                gsize j;

                g_byte_array_append(buffer, (guint8*)&lealen, sizeof(gint32));
                for (j = 0; j < alen; j++)
                    levalue[j] = GINT64_TO_LE(value[j]);
                g_byte_array_append(buffer, (guint8*)levalue,
                                    alen*sizeof(gint64));
                g_free(levalue);
#endif
            }
            break;

            case 's':
            {
                guchar *value = va_arg(ap, guchar*);

                if (!value) {
                    g_warning("representing NULL string as an empty string");
                    g_byte_array_append(buffer, "", 1);
                }
                else
                    g_byte_array_append(buffer, value, strlen(value) + 1);
            }
            break;

            case 'o':
            {
                GObject *value = va_arg(ap, GObject*);

                g_assert(value);
                g_assert(GWY_IS_SERIALIZABLE(value));
                gwy_serializable_serialize(value, buffer);
            }
            break;

            default:
            g_error("wrong spec `%c' in templ `%s'", templ[i], templ);
            va_end(ap);
            return buffer;
            break;
        }
    }

    va_end(ap);

    return buffer;
}

GByteArray*
gwy_serialize_pack_object_struct(GByteArray *buffer,
                                 const guchar *object_name,
                                 gsize nspec,
                                 const GwySerializeSpec *spec)
{
    gsize before_obj;

    gwy_debug("init size: %lu, buffer = %p", buffer ? buffer->len : 0, buffer);
    buffer = gwy_serialize_pack(buffer, "si", object_name, 0);
    before_obj = buffer->len;
    gwy_debug("+head size: %lu", buffer->len);

    gwy_serialize_pack_struct(buffer, nspec, spec);
    gwy_debug("+body size: %lu", buffer->len);
    gwy_serialize_store_int32(buffer, before_obj - sizeof(guint32),
                              buffer->len - before_obj);
    return buffer;
}

/**
 * gwy_serialize_pack_struct:
 * @buffer: A buffer to which the serialized components should be appended,
 *          or %NULL.
 * @nspec: The number of items in @spec.
 * @spec: The components to serialize.
 *
 * Serializes a struct with named and somewhat typed fields.
 *
 * For object serialization gwy_serialize_pack_object_struct() should be more
 * convenient and less error prone.
 *
 * Returns: @buffer or a newly allocated #GByteArray with serialization of
 *          @spec components appended.
 **/
GByteArray*
gwy_serialize_pack_struct(GByteArray *buffer,
                          gsize nspec,
                          const GwySerializeSpec *spec)
{
    const GwySerializeSpec *sp;
    guint32 asize = 0;
    guint8 *arr = NULL;
    gsize i;

    gwy_debug("nspec = %d, buffer = %p", nspec, buffer);
    if (!nspec)
        return buffer;

    if (!buffer)
        buffer = g_byte_array_new();

    sp = spec;
    for (i = 0; i < nspec; i++) {
        sp = spec + i;
        g_assert(sp->value);
        if (g_ascii_isupper(sp->ctype)) {
            g_assert(sp->array_size);
            g_assert(*(gpointer*)sp->value);
            asize = *sp->array_size;
            arr = *(guint8**)sp->value;
        }
        g_byte_array_append(buffer, sp->name, strlen(sp->name) + 1);
        g_byte_array_append(buffer, &sp->ctype, 1);
        gwy_debug("%d <%s> <%c> %lu", i, sp->name, sp->ctype, buffer->len);
        switch (sp->ctype) {
            case 'b':
            {
                /* store it as char */
                char value = *(gboolean*)sp->value;

                g_byte_array_append(buffer, &value, 1);
            }
            break;

            case 'c':
            {
                g_byte_array_append(buffer, sp->value, 1);
            }
            break;

            case 'C':
            {
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
                g_byte_array_append(buffer, (guint8*)sp->array_size,
                                    sizeof(gint32));
#else
#warning FIXME
                guint32 leasize = GUINT32_TO_LE(sp->array_size);
                g_byte_array_append(buffer, (guint8*)leasize, sizeof(gint32));
#endif
                g_byte_array_append(buffer, arr, asize*sizeof(char));
            }
            break;

            case 'i':
            {
                g_byte_array_append(buffer, sp->value, sizeof(gint32));
            }
            break;

            case 'I':
            {
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
                /* optimize for the common case */
                g_byte_array_append(buffer, (guint8*)sp->array_size,
                                    sizeof(gint32));
                g_byte_array_append(buffer, arr, asize*sizeof(gint32));
#else
#warning FIXME
                /* we can't operate on value in-place as it may be read-only
                 * we can't simply operate on buffer, as it may not be
                 * aligned
                 * is there anything better than a copy? */
                guint32 leasize = GINT32_TO_LE(sp->array_size);
                gint32 *plarr = (gint32*)arr;
                gint32 *learr = g_new(gint32, sp->array_size);
                gsize j;

                g_byte_array_append(buffer, (guint8*)&leasize, sizeof(gint32));
                for (j = 0; j < sp->array_size; j++)
                    learr[j] = GINT32_TO_LE(plarr[j]);
                g_byte_array_append(buffer, (guint8*)learr,
                                    sp->array_size*sizeof(gint32));
                g_free(learr);
#endif
            }
            break;

            case 'q':
            {
                g_byte_array_append(buffer, sp->value, sizeof(gint64));
            }
            break;

            case 'Q':
            {
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
                /* optimize for the common case */
                g_byte_array_append(buffer, (guint8*)sp->array_size,
                                    sizeof(gint32));
                g_byte_array_append(buffer, arr, asize*sizeof(gint64));
#else
#warning FIXME
                /* we can't operate on value in-place as it may be read-only
                 * we can't simply operate on buffer, as it may not be
                 * aligned
                 * is there anything better than a copy? */
                guint32 leasize = GINT32_TO_LE(sp->array_size);
                gint64 *plarr = (gint64*)arr;
                gint64 *learr = g_new(gint64, sp->array_size);
                gsize j;

                g_byte_array_append(buffer, (guint8*)&leasize, sizeof(gint32));
                for (j = 0; j < sp->array_size; j++)
                    learr[j] = GINT64_TO_LE(plarr[j]);
                g_byte_array_append(buffer, (guint8*)learr,
                                    sp->array_size*sizeof(gint64));
                g_free(learr);
#endif
            }
            break;

            case 'd':
            {
                g_byte_array_append(buffer, sp->value, sizeof(double));
            }
            break;

            case 'D':
            {
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
                /* optimize for the common case */
                g_byte_array_append(buffer, (guint8*)sp->array_size,
                                    sizeof(gint32));
                g_byte_array_append(buffer, arr, asize*sizeof(double));
#else
#warning FIXME FIXME FIXME
                /* we can't operate on value in-place as it may be read-only
                 * we can't simply operate on buffer, as it may not be
                 * aligned
                 * is there anything better than a copy? */
                /* XXX: we swap bytes like it was a gint64, since it should
                 * not matter (one octuple like another), but who knows... */
                guint32 leasize = GINT32_TO_LE(sp->array_size);
                gint64 *plarr = (gint64*)arr;
                gint64 *learr = g_new(gint64, sp->array_size);
                gsize j;

                g_byte_array_append(buffer, (guint8*)&leasize, sizeof(gint32));
                for (j = 0; j < sp->array_size; j++)
                    learr[j] = GINT64_TO_LE(plarr[j]);
                g_byte_array_append(buffer, (guint8*)learr,
                                    sp->array_size*sizeof(gint64));
                g_free(learr);
#endif
            }
            break;

            case 's':
            {
                guchar *value = *(guchar**)sp->value;

                if (!value) {
                    g_warning("representing NULL string as an empty string");
                    g_byte_array_append(buffer, "", 1);
                }
                else
                    g_byte_array_append(buffer, value, strlen(value) + 1);
            }
            break;

            case 'o':
            {
                GObject *value = *(GObject**)sp->value;

                g_assert(value);
                g_assert(GWY_IS_SERIALIZABLE(value));
                gwy_serializable_serialize(value, buffer);
            }
            break;

            default:
            g_error("wrong spec `%c' at pos %d", sp->ctype, sp - spec);
            return buffer;
            break;
        }
    }

    return buffer;
}

static inline gsize G_GNUC_CONST
ctype_size(guchar ctype)
{
    switch (ctype) {
        case 'c':
        case 'b':
        return sizeof(guchar);
        break;

        case 'i':
        return sizeof(gint);
        break;

        case 'q':
        return sizeof(gint64);
        break;

        case 'd':
        return sizeof(gdouble);
        break;

        default:
        return 0;
        break;
    }
}

void
gwy_serialize_skip_type(const guchar *buffer,
                        gsize size,
                        gsize *position,
                        guchar ctype)
{
    gsize tsize;

    tsize = ctype_size(ctype);
    if (tsize) {
        *position += tsize;
        return;
    }

    if (ctype == 's') {
        tsize = gwy_serialize_check_string(buffer, size, *position, NULL);
        *position += tsize;
        return;
    }

    if (ctype == 'o') {
        tsize = gwy_serialize_check_string(buffer, size, *position, NULL);
        *position += tsize;
        tsize = gwy_serialize_unpack_int32(buffer, size, position);
        *position += tsize;
        return;
    }

    /* arrays */
    if (g_ascii_isupper(ctype)) {
        ctype = g_ascii_tolower(ctype);
        tsize = gwy_serialize_unpack_int32(buffer, size, position);
        position += tsize*ctype_size(ctype);
        return;
    }

    g_assert_not_reached();
}


gboolean
gwy_serialize_unpack_object_struct(const guchar *buffer,
                                   gsize size,
                                   gsize *position,
                                   const guchar *object_name,
                                   gsize nspec,
                                   const GwySerializeSpec *spec)
{
    gsize mysize;
    gboolean ok;

    mysize = gwy_serialize_check_string(buffer, size, *position, object_name);
    g_return_val_if_fail(mysize, FALSE);
    *position += mysize;

    mysize = gwy_serialize_unpack_int32(buffer, size, position);
    ok = gwy_serialize_unpack_struct(buffer + *position, mysize, nspec, spec);
    *position += mysize;

    return ok;
}

/**
 * gwy_serialize_unpack_struct:
 * @buffer: A memory location containing a serialized structure.
 * @size: The size of @buffer.
 * @nspec: The number of items in @spec.
 * @spec: The components to deserialize.
 *
 * Deserializes a structure with named components packed by
 * gwy_serialize_pack_struct().
 *
 * Extra components are ignored, components of different type than expected
 * cause failure, missing components are not detected.
 *
 * For object deserialization gwy_serialize_unpack_object_struct() should be
 * more convenient and less error prone.
 *
 * Returns: TRUE if the unpacking succeeded, FALSE otherwise (some fields may
 * be unpacked in this case).
 **/
gboolean
gwy_serialize_unpack_struct(const guchar *buffer,
                            gsize size,
                            gsize nspec,
                            const GwySerializeSpec *spec)
{
    gsize nlen, position;
    const GwySerializeSpec *sp;
    const guchar *name;
    gpointer p;
    gsize *a;
    guchar ctype;

    position = 0;
    while (position < size) {
        nlen = gwy_serialize_check_string(buffer, size, position, NULL);
        if (!nlen) {
            g_error("Expected a component name to deserialize, got garbage");
            return FALSE;
        }

        for (sp = spec; (gsize)(sp - spec) < nspec; sp++) {
            if (strcmp(sp->name, buffer + position) == 0)
                break;
        }
        name = buffer + position;
        position += nlen;
        ctype = gwy_serialize_unpack_char(buffer, size, &position);
        if ((gsize)(sp - spec) == nspec) {
            g_warning("Extra component %s of type `%c'", name, ctype);
            gwy_serialize_skip_type(buffer, size, &position, ctype);
            continue;
        }

        if (ctype != sp->ctype) {
            g_warning("Bad or unknown type `%c' of %s (expected `%c')",
                      ctype, name, sp->ctype);
            return FALSE;
        }

        p = sp->value;
        a = sp->array_size;
        switch (ctype) {
            case 'o':
            if (*(GObject**)p)
                g_object_unref(*(GObject**)p);
            *(GObject**)p = gwy_serializable_deserialize(buffer, size,
                                                         &position);
            break;

            case 'b':
            *(gboolean*)p = gwy_serialize_unpack_boolean(buffer, size,
                                                         &position);
            break;

            case 'c':
            *(guchar*)p = gwy_serialize_unpack_char(buffer, size, &position);
            break;

            case 'i':
            *(gint32*)p = gwy_serialize_unpack_int32(buffer, size, &position);
            break;

            case 'q':
            *(gint64*)p = gwy_serialize_unpack_int64(buffer, size, &position);
            break;

            case 'd':
            *(gdouble*)p = gwy_serialize_unpack_double(buffer, size, &position);
            break;

            case 's':
            g_free(*(guchar**)p);
            *(guchar**)p = gwy_serialize_unpack_string(buffer, size, &position);
            break;

            case 'C':
            g_free(*(guchar**)p);
            *(guchar**)p = gwy_serialize_unpack_char_array(buffer, size,
                                                            &position, a);
            break;

            case 'I':
            g_free(*(guint32**)p);
            *(gint32**)p = gwy_serialize_unpack_int32_array(buffer, size,
                                                            &position, a);
            break;

            case 'Q':
            g_free(*(guint64**)p);
            *(gint64**)p = gwy_serialize_unpack_int64_array(buffer, size,
                                                            &position, a);
            break;

            case 'D':
            g_free(*(gdouble**)p);
            *(gdouble**)p = gwy_serialize_unpack_double_array(buffer, size,
                                                              &position, a);
            break;

            default:
            g_error("Type `%c' of %s is unknown "
                    "(though known to application?!)",
                    ctype, name);
            return FALSE;
            break;
        }
    }
    return TRUE;
}

/**
 * gwy_serialize_unpack_boolean:
 * @buffer: A memory location containing a serialized boolean at position
 *          @position.
 * @size: The size of @buffer.
 * @position: The position of the character in @buffer, it's updated to
 *            point after it.
 *
 * Deserializes a one boolean.
 *
 * Returns: The boolean as gboolean.
 **/
gboolean
gwy_serialize_unpack_boolean(const guchar *buffer,
                             gsize size,
                             gsize *position)
{
    gboolean value;

    gwy_debug("buf = %p, size = %u, pos = %u", buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_assert(*position + sizeof(guchar) <= size);
    value = buffer[*position];
    *position += sizeof(guchar);

    gwy_debug("value = <%s>", value ? "TRUE" : "FALSE");
    return value;
}

/**
 * gwy_serialize_unpack_char:
 * @buffer: A memory location containing a serialized character at position
 *          @position.
 * @size: The size of @buffer.
 * @position: The position of the character in @buffer, it's updated to
 *            point after it.
 *
 * Deserializes a one character.
 *
 * Returns: The character as guchar.
 **/
guchar
gwy_serialize_unpack_char(const guchar *buffer,
                          gsize size,
                          gsize *position)
{
    guchar value;

    gwy_debug("buf = %p, size = %u, pos = %u", buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_assert(*position + sizeof(guchar) <= size);
    value = buffer[*position];
    *position += sizeof(guchar);

    gwy_debug("value = <%c>", value);
    return value;
}

/**
 * gwy_serialize_unpack_char_array:
 * @buffer: A memory location containing a serialized character array at
 *          position @position.
 * @size: The size of @buffer.
 * @position: The position of the array in @buffer, it's updated to
 *            point after it.
 * @asize: Where the size of the array is to be returned.
 *
 * Deserializes a character array.
 *
 * Returns: The unpacked character array (newly allocated).
 **/
guchar*
gwy_serialize_unpack_char_array(const guchar *buffer,
                                gsize size,
                                gsize *position,
                                gsize *asize)
{
    guchar *value;

    gwy_debug("buf = %p, size = %u, pos = %u", buffer, size, *position);

    *asize = gwy_serialize_unpack_int32(buffer, size, position);
    g_assert(*position + *asize*sizeof(guchar) <= size);
    value = g_memdup(buffer + *position, *asize*sizeof(guchar));
    *position += *asize*sizeof(guchar);

    gwy_debug("|value| = %u", *asize);
    return value;
}

/**
 * gwy_serialize_unpack_int32:
 * @buffer: A memory location containing a serialized 32bit integer at position
 *          @position.
 * @size: The size of @buffer.
 * @position: The position of the integer in @buffer, it's updated to
 *            point after it.
 *
 * Deserializes a one 32bit integer.
 *
 * Returns: The integer as gint32.
 **/
gint32
gwy_serialize_unpack_int32(const guchar *buffer,
                           gsize size,
                           gsize *position)
{
    gint32 value;

    gwy_debug("buf = %p, size = %u, pos = %u", buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_assert(*position + sizeof(gint32) <= size);
    memcpy(&value, buffer + *position, sizeof(gint32));
    value = GINT32_FROM_LE(value);
    *position += sizeof(gint32);

    gwy_debug("value = <%d>", value);
    return value;
}

/**
 * gwy_serialize_unpack_int32_array:
 * @buffer: A memory location containing a serialized int32 array at
 *          position @position.
 * @size: The size of @buffer.
 * @position: The position of the array in @buffer, it's updated to
 *            point after it.
 * @asize: Where the size of the array is to be returned.
 *
 * Deserializes an int32 array.
 *
 * Returns: The unpacked 32bit integer array (newly allocated).
 **/
gint32*
gwy_serialize_unpack_int32_array(const guchar *buffer,
                                 gsize size,
                                 gsize *position,
                                 gsize *asize)
{
    gint32 *value;

    gwy_debug("buf = %p, size = %u, pos = %u", buffer, size, *position);

    *asize = gwy_serialize_unpack_int32(buffer, size, position);
    g_assert(*position + *asize*sizeof(gint32) <= size);
    value = g_memdup(buffer + *position, *asize*sizeof(gint32));
    *position += *asize*sizeof(gint32);

    gwy_debug("|value| = %u", *asize);
    return value;
}

/**
 * gwy_serialize_unpack_int64:
 * @buffer: A memory location containing a serialized 64bit integer at position
 *          @position.
 * @size: The size of @buffer.
 * @position: The position of the integer in @buffer, it's updated to
 *            point after it.
 *
 * Deserializes a one 64bit integer.
 *
 * Returns: The integer as gint64.
 **/
gint64
gwy_serialize_unpack_int64(const guchar *buffer,
                           gsize size,
                           gsize *position)
{
    gint64 value;

    gwy_debug("buf = %p, size = %u, pos = %u", buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_assert(*position + sizeof(gint64) <= size);
    memcpy(&value, buffer + *position, sizeof(gint64));
    *position += sizeof(gint64);

    gwy_debug("value = <%lld>", value);
    return value;
}

/**
 * gwy_serialize_unpack_int64_array:
 * @buffer: A memory location containing a serialized int64 array at
 *          position @position.
 * @size: The size of @buffer.
 * @position: The position of the array in @buffer, it's updated to
 *            point after it.
 * @asize: Where the size of the array is to be returned.
 *
 * Deserializes an int64 array.
 *
 * Returns: The unpacked 64bit integer array (newly allocated).
 **/
gint64*
gwy_serialize_unpack_int64_array(const guchar *buffer,
                                 gsize size,
                                 gsize *position,
                                 gsize *asize)
{
    gint64 *value;

    gwy_debug("buf = %p, size = %u, pos = %u", buffer, size, *position);

    *asize = gwy_serialize_unpack_int32(buffer, size, position);
    g_assert(*position + *asize*sizeof(gint64) <= size);
    value = g_memdup(buffer + *position, *asize*sizeof(gint64));
    *position += *asize*sizeof(gint64);

    gwy_debug("|value| = %u", *asize);
    return value;
}

/**
 * gwy_serialize_unpack_double:
 * @buffer: A memory location containing a serialized double at position
 *          @position.
 * @size: The size of @buffer.
 * @position: The position of the integer in @buffer, it's updated to
 *            point after it.
 *
 * Deserializes a one double.
 *
 * Returns: The integer as gdouble.
 **/
gdouble
gwy_serialize_unpack_double(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    gdouble value;

    gwy_debug("buf = %p, size = %u, pos = %u", buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_assert(*position + sizeof(gdouble) <= size);
    memcpy(&value, buffer + *position, sizeof(gdouble));
    *position += sizeof(gdouble);

    gwy_debug("value = <%g>", value);
    return value;
}

/**
 * gwy_serialize_unpack_double_array:
 * @buffer: A memory location containing a serialized double array at
 *          position @position.
 * @size: The size of @buffer.
 * @position: The position of the array in @buffer, it's updated to
 *            point after it.
 * @asize: Where the size of the array is to be returned.
 *
 * Deserializes an double array.
 *
 * Returns: The unpacked double array (newly allocated).
 **/
gdouble*
gwy_serialize_unpack_double_array(const guchar *buffer,
                                  gsize size,
                                  gsize *position,
                                  gsize *asize)
{
    gdouble *value;

    gwy_debug("buf = %p, size = %u, pos = %u", buffer, size, *position);

    *asize = gwy_serialize_unpack_int32(buffer, size, position);
    g_assert(*position + *asize*sizeof(gdouble) <= size);
    value = g_memdup(buffer + *position, *asize*sizeof(gdouble));
    *position += *asize*sizeof(gdouble);

    gwy_debug("|value| = %u", *asize);
    return value;
}

/**
 * gwy_serialize_unpack_string:
 * @buffer: A memory location containing a serialized nul-terminated string at
 *          position @position.
 * @size: The size of @buffer.
 * @position: The position of the string in @buffer, it's updated to
 *            point after it.
 *
 * Deserializes a one nul-terminated string.
 *
 * Returns: A newly allocated, nul-terminated string.
 **/
guchar*
gwy_serialize_unpack_string(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    guchar *value;
    const guchar *p;

    gwy_debug("buf = %p, size = %u, pos = %u", buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_assert(*position < size);
    p = memchr(buffer + *position, 0, size - *position);
    g_assert(p);
    value = g_strdup(buffer + *position);
    *position += (p - buffer) - *position + 1;

    gwy_debug("value = <%s>", value);
    return value;
}

/**
 * gwy_serialize_check_string:
 * @buffer: A memory location containing a nul-terminated string at position
 *          @position.
 * @size: The size of @buffer.
 * @position: The position of the string in @buffer.
 * @compare_to: String to compare @buffer to, or %NULL.
 *
 * Check whether @size bytes of memory in @buffer can be interpreted as a
 * nul-terminated string, and eventually whether it's equal to @compare_to.
 *
 * When @compare_to is %NULL, the comparsion is not performed.
 *
 * Returns: The length of the nul-terminated string including the nul
 * character; zero otherwise.
 **/
gsize
gwy_serialize_check_string(const guchar *buffer,
                           gsize size,
                           gsize position,
                           const guchar *compare_to)
{
    const guchar *p;

    gwy_debug("<%s> buf = %p, size = %u, pos = %u",
              compare_to, buffer, size, position);
    g_assert(buffer);
    g_assert(size > 0);
    g_assert(position < size);
    p = (guchar*)memchr(buffer + position, 0, size - position);
    if (!p || (compare_to && strcmp(buffer + position, compare_to)))
        return 0;

    return (p - buffer) + 1 - position;
}

/************************** Documentation ****************************/

/**
 * GwySerializeFunc:
 * @serializable: An object to serialize.
 * @buffer: A buffer to append the representation to, may be %NULL indicating
 *          a new one should be allocated.
 *
 * The type of serialization method, see gwy_serializable_serialize() for
 * description.
 *
 * Returns: @buffer with serialized object appended.
 */

/**
 * GwyDeserializeFunc:
 * @buffer: A buffer containing a serialized object.
 * @size: The size of @buffer.
 * @position: The current position in @buffer.
 *
 * The type of deserialization method, see gwy_serializable_deserialize() for
 * description.
 *
 * Returns: A newly created (restored) object.
 */

/**
 * GwyDuplicateFunc:
 * @object: An object to duplicate.
 *
 * The type of duplication method, see gwy_serializable_duplicate() for
 * description.
 *
 * Returns: A copy of @object.
 */

/**
 * GwySerializeSpec:
 * @ctype: Component type, as in gwy_serialize_pack().
 * @name: Component name as a null terminated string.
 * @value: Pointer to component (always add one level of indirection; for
 *         an object, a #GObject** pointer should be stored).
 * @array_size: Pointer to array size if component is an array, NULL
 *              otherwise.
 *
 * A structure containing information for one object/struct component
 * serialization or deserialization.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
