/* @(#) $Id$ */

#include <string.h>
#include <glib-object.h>
#include <glib/gutils.h>

#include "gwyserializable.h"

#define GWY_SERIALIZABLE_TYPE_NAME "GwySerializable"

static void    gwy_serializable_base_init     (GwySerializableClass *klass);
static void    gwy_serializable_base_finalize (GwySerializableClass *klass);

static inline gsize ctype_size     (guchar ctype);

GType
gwy_serializable_get_type(void)
{
    static GType gwy_serializable_type = 0;

    if (!gwy_serializable_type) {
        static const GTypeInfo gwy_serializable_info = {
            sizeof(GwySerializableClass),
            (GBaseInitFunc)gwy_serializable_base_init,
            (GBaseFinalizeFunc)gwy_serializable_base_finalize,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL,
            NULL,
        };

        gwy_serializable_type = g_type_register_static(G_TYPE_INTERFACE,
                                                       GWY_SERIALIZABLE_TYPE_NAME,
                                                       &gwy_serializable_info,
                                                       0);
        g_type_interface_add_prerequisite(gwy_serializable_type, G_TYPE_OBJECT);
    }

    return gwy_serializable_type;
}

/* XXX: WTF is this exactly good for?  I took it from the examples... ;-) */
static guint gwy_serializable_base_init_count = 0;

static void
gwy_serializable_base_init(GwySerializableClass *klass)
{
    gwy_serializable_base_init_count++;
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s (base init count = %d)",
          __FUNCTION__, gwy_serializable_base_init_count);
    #endif
    if (gwy_serializable_base_init_count == 1) {
        /* add signals... */
    }
}

static void
gwy_serializable_base_finalize(GwySerializableClass *klass)
{
    gwy_serializable_base_init_count--;
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s (base init count = %d)",
          __FUNCTION__, gwy_serializable_base_init_count);
    #endif
    if (gwy_serializable_base_init_count == 0) {
        /* destroy signals... */
    }
}

/**
 * gwy_serializable_serialize:
 * @serializable: A #GObject implementing #GwySerializable interface.
 * @buffer: A buffer to which the serialized object should be appended.
 * @size: Current size of @buffer, new size is returned here.
 *
 * Serializes an object implementing #GwySerializable interface.
 *
 * Returns: A reallocated block of memory of size @size containing the
 *          current contents of @buffer with object representation appended.
 **/
guchar*
gwy_serializable_serialize(GObject *serializable,
                           guchar *buffer,
                           gsize *size)
{
    GwySerializeFunc serialize_method;

    g_return_val_if_fail(serializable, NULL);
    g_return_val_if_fail(GWY_IS_SERIALIZABLE(serializable), NULL);
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "serializing a %s",
          g_type_name(G_TYPE_FROM_INSTANCE(serializable)));
    #endif

    serialize_method = GWY_SERIALIZABLE_GET_CLASS(serializable)->serialize;
    if (!serialize_method) {
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
              "%s doesn't implement serialize()",
              g_type_name(G_TYPE_FROM_INSTANCE(serializable)));
        return NULL;
    }
    return serialize_method(serializable, buffer, size);
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
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
              "memory contents at %p doesn't look as an serialized object",
              buffer);
        return NULL;
    }

    type = g_type_from_name((gchar*)(buffer + *position));
    g_type_class_ref(type);
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "deserializing a %s",
          g_type_name(type));
    #endif
    g_return_val_if_fail(type, NULL);
    g_return_val_if_fail(G_TYPE_IS_INSTANTIATABLE(type), NULL);
    g_return_val_if_fail(g_type_is_a(type, GWY_TYPE_SERIALIZABLE), NULL);

    /* FIXME: this horrible construct gets interface class from a mere GType;
     * deserialize() is a class method, not an object method, there already
     * has to be some macro for it in gobject... */
    deserialize_method
        = ((GwySerializableClass*)
                g_type_interface_peek(g_type_class_peek(type),
                                      GWY_TYPE_SERIALIZABLE))->deserialize;
    if (!deserialize_method) {
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
              "%s doesn't implement deserialize()",
              buffer);
        return NULL;
    }
    object = deserialize_method(buffer, size, position);
    if (object)
        g_type_class_unref(G_OBJECT_GET_CLASS(object));
    else
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
              "Cannot unref class after failed %s deserialization",
              g_type_name(type));
    return object;
}

/**
 * gwy_serialize_store_int32:
 * @buffer: A buffer to which the value should be stored.
 * @value: A 32bit integer.
 *
 * Stored a 32bit integer to a buffer.
 **/
void
gwy_serialize_store_int32(guchar *buffer, guint32 value)
{
    memcpy(buffer, &value, sizeof(guint32));
}

/**
 * gwy_serialize_pack:
 * @buffer: A buffer to which the serialized values should be appended.
 * @size: Current size of @buffer, new size is returned here.
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
 * The buffer @buffer may be %NULL (and @size should be zero then), or it
 * can contain some data.  In the former case a new one will be allocated,
 * in the latter case the existing buffer will be extended to be able to keep
 * both the old and the new data; @size will be updated.
 *
 * FIXME: this function currently doesn't create architecture-independent
 * representations, it just copies the memory.
 *
 * Returns: The buffer with serialization of given values appended.
 **/
guchar*
gwy_serialize_pack(guchar *buffer,
                   gsize *size,
                   const gchar *templ,
                   ...)
{
    va_list ap;
    gsize nargs, i, pos;
    guchar *p = NULL;
    gboolean do_copy = FALSE;
    gboolean did_copy = FALSE;
    gsize nobjs;  /* number of items which are objects */
    struct o { gsize size; guchar *buffer; } *objects;  /* serialized objects */

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s (templ: %s)",
          __FUNCTION__, templ);
    #endif
    nargs = strlen(templ);
    if (!nargs)
        return buffer;

    for (nobjs = i = 0; i < nargs; i++) {
        if (templ[i] == 'o')
            nobjs++;
    }
    objects = g_new(struct o, nobjs);

    while (!did_copy) {
        va_start(ap, templ);
        nobjs = 0;
        pos = 0;

        for (i = 0; i < nargs; i++) {
            switch (templ[i]) {
                case 'b':
                {
                    char value = va_arg(ap, gboolean);  /* store it as char */

                    if (do_copy)
                        memcpy(p + pos, &value, sizeof(char));
                    pos += sizeof(char);
                }
                break;

                case 'c':
                {
                    char value = va_arg(ap, int);

                    if (do_copy)
                        memcpy(p + pos, &value, sizeof(char));
                    pos += sizeof(char);
                }
                break;

                case 'C':
                {
                    gint32 alen = va_arg(ap, gsize);
                    guchar *value = va_arg(ap, guchar*);

                    if (do_copy) {
                        memcpy(p + pos, &alen, sizeof(gint32));
                        memcpy(p + pos + sizeof(gint32), value,
                               alen*sizeof(char));
                    }
                    pos += sizeof(gint32) + alen*sizeof(char);
                }
                break;

                case 'i':
                {
                    gint32 value = va_arg(ap, gint32);

                    if (do_copy)
                        memcpy(p + pos, &value, sizeof(gint32));
                    pos += sizeof(gint32);
                }
                break;

                case 'I':
                {
                    gint32 alen = va_arg(ap, gsize);
                    gint32 *value = va_arg(ap, gint32*);

                    if (do_copy) {
                        memcpy(p + pos, &alen, sizeof(gint32));
                        memcpy(p + pos + sizeof(gint32), value,
                               alen*sizeof(gint32));
                    }
                    pos += sizeof(gint32) + alen*sizeof(gint32);
                }
                break;

                case 'q':
                {
                    gint64 value = va_arg(ap, gint64);

                    if (do_copy)
                        memcpy(p + pos, &value, sizeof(gint64));
                    pos += sizeof(gint64);
                }
                break;

                case 'Q':
                {
                    gint32 alen = va_arg(ap, gsize);
                    gint64 *value = va_arg(ap, gint64*);

                    if (do_copy) {
                        memcpy(p + pos, &alen, sizeof(gint32));
                        memcpy(p + pos + sizeof(gint32), value,
                               alen*sizeof(gint64));
                    }
                    pos += sizeof(gint32) + alen*sizeof(gint64);
                }
                break;

                case 'd':
                {
                    double value = va_arg(ap, double);

                    if (do_copy)
                        memcpy(p + pos, &value, sizeof(double));
                    pos += sizeof(double);
                }
                break;

                case 'D':
                {
                    gint32 alen = va_arg(ap, gsize);
                    double *value = va_arg(ap, double*);

                    if (do_copy) {
                        memcpy(p + pos, &alen, sizeof(gint32));
                        memcpy(p + pos + sizeof(gint32), value,
                               alen*sizeof(double));
                    }
                    pos += sizeof(gint32) + alen*sizeof(double);
                }
                break;

                case 's':
                {
                    guchar *value = va_arg(ap, guchar*);

                    if (!value) {
                        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                              "representing NULL string as an empty string");
                        if (do_copy)
                            p[pos] = '\0';
                        p++;
                    }
                    else {
                        gsize l = strlen(value) + 1;

                        if (do_copy)
                            memcpy(p + pos, value, l);
                        pos += l;
                    }
                }
                break;

                case 'o':
                {
                    GObject *value = va_arg(ap, GObject*);

                    g_assert(value);
                    g_assert(GWY_IS_SERIALIZABLE(value));
                    if (do_copy) {
                        memcpy(p + pos,
                               objects[nobjs].buffer, objects[nobjs].size);
                        g_free(objects[nobjs].buffer);
                    }
                    else {
                        objects[nobjs].size = 0;
                        objects[nobjs].buffer =
                            gwy_serializable_serialize(value, NULL,
                                                       &objects[nobjs].size);
                    }
                    pos += objects[nobjs].size;
                    nobjs++;
                }
                break;

                default:
                g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
                      "wrong spec `%c' in templ `%s'",
                      templ[i], templ);
                g_assert(!do_copy);
                g_free(p);
                va_end(ap);
                /* FIXME: we may leak some objects[] here */
                return buffer;
                break;
            }
        }

        va_end(ap);
        if (do_copy)
            did_copy = TRUE;
        else {
            buffer = g_renew(guchar, buffer, *size + pos);
            p = buffer + *size;
            *size += pos;
            do_copy = TRUE;
        }
    }
    g_free(objects);

    return buffer;
}

/**
 * gwy_serialize_pack_struct:
 * @buffer: A buffer to which the serialized components should be appended.
 * @size: Current size of @buffer, new size is returned here.
 * @nspec: The number of items in @spec.
 * @spec: The components to serialize.
 *
 *
 *
 * Returns: The buffer with serialization of @spec components appended.
 **/
guchar*
gwy_serialize_pack_object_struct(guchar *buffer,
                                 gsize *size,
                                 const guchar *object_name,
                                 gsize nspec,
                                 const GwySerializeSpec *spec)
{
    gsize oldsize;

    buffer = gwy_serialize_pack(buffer, size, "si", object_name, 0);
    oldsize = *size;

    buffer = gwy_serialize_pack_struct(buffer, size, nspec, spec);
    gwy_serialize_store_int32(buffer + oldsize - sizeof(guint32),
                              *size - oldsize);
    return buffer;
}

/**
 * gwy_serialize_pack_struct:
 * @buffer: A buffer to which the serialized components should be appended.
 * @size: Current size of @buffer, new size is returned here.
 * @nspec: The number of items in @spec.
 * @spec: The components to serialize.
 *
 * Serializes a struct with named and somewhat typed fields.
 *
 * For object serialization gwy_serialize_pack_object_struct() should be more
 * convenient and less error prone.
 *
 * Returns: The buffer with serialization of @spec components appended.
 **/
guchar*
gwy_serialize_pack_struct(guchar *buffer,
                          gsize *size,
                          gsize nspec,
                          const GwySerializeSpec *spec)
{
    const GwySerializeSpec *sp;
    gsize i, pos;
    guint32 asize;
    guchar *p = NULL;
    gboolean do_copy = FALSE;
    gboolean did_copy = FALSE;
    gsize nobjs;  /* number of items which are objects */
    struct o { gsize size; guchar *buffer; } *objects;  /* serialized objects */

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s (nspec = %d)",
          __FUNCTION__, nspec);
    #endif
    if (!nspec)
        return buffer;

    for (nobjs = i = 0; i < nspec; i++) {
        if (spec[i].ctype == 'o')
            nobjs++;
    }
    objects = g_new(struct o, nobjs);

    while (!did_copy) {
        nobjs = 0;
        pos = 0;

        for (sp = spec; sp - spec < nspec; sp++) {
            g_assert(sp->value);
            if (g_ascii_isupper(sp->ctype)) {
                g_assert(sp->array_size);
                g_assert(*(gpointer*)sp->value);
                asize = *sp->array_size;
            }
            i = strlen(sp->name) + 1;
            if (do_copy)
                memcpy(p + pos, sp->name, i);
            pos += i;
            if (do_copy)
                *(p + pos) = sp->ctype;
            pos++;
            switch (sp->ctype) {
                case 'b':
                {
                    /* store it as char */
                    char value = *(gboolean*)sp->value;

                    if (do_copy)
                        memcpy(p + pos, &value, sizeof(char));
                    pos += sizeof(char);
                }
                break;

                case 'c':
                {
                    if (do_copy)
                        *(p + pos) = *(guchar*)sp->value;
                    pos += sizeof(char);
                }
                break;

                case 'C':
                {
                    if (do_copy)
                        memcpy(p + pos, sp->array_size, sizeof(guint32));
                    pos += sizeof(guint32);
                    if (do_copy)
                        memcpy(p + pos, *(guchar**)sp->value,
                               asize*sizeof(char));
                    pos += asize*sizeof(char);
                }
                break;

                case 'i':
                {
                    if (do_copy)
                        memcpy(p + pos, sp->value, sizeof(gint32));
                    pos += sizeof(gint32);
                }
                break;

                case 'I':
                {
                    if (do_copy)
                        memcpy(p + pos, sp->array_size, sizeof(gint32));
                    pos += sizeof(guint32);
                    if (do_copy)
                        memcpy(p + pos, *(guint32**)sp->value,
                               asize*sizeof(gint32));
                    pos += asize*sizeof(gint32);
                }
                break;

                case 'q':
                {
                    if (do_copy)
                        memcpy(p + pos, sp->value, sizeof(gint64));
                    pos += sizeof(gint64);
                }
                break;

                case 'Q':
                {
                    if (do_copy)
                        memcpy(p + pos, sp->array_size, sizeof(gint32));
                    pos += sizeof(guint32);
                    if (do_copy)
                        memcpy(p + pos, *(guint64**)sp->value,
                               asize*sizeof(gint64));
                    pos += asize*sizeof(gint64);
                }
                break;

                case 'd':
                {
                    if (do_copy)
                        memcpy(p + pos, sp->value, sizeof(double));
                    pos += sizeof(double);

                }
                break;

                case 'D':
                {
                    if (do_copy)
                        memcpy(p + pos, sp->array_size, sizeof(gint32));
                    pos += sizeof(guint32);
                    if (do_copy)
                        memcpy(p + pos, *(gdouble**)sp->value,
                               asize*sizeof(double));
                    pos += asize*sizeof(double);
                }
                break;

                case 's':
                {
                    guchar *value = *(guchar**)sp->value;

                    if (!value) {
                        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                              "representing NULL string as an empty string");
                        if (do_copy)
                            p[pos] = '\0';
                        p++;
                    }
                    else {
                        asize = strlen(value) + 1;
                        if (do_copy)
                            memcpy(p + pos, value, asize);
                        pos += asize;
                    }
                }
                break;

                case 'o':
                {
                    GObject *value = *(GObject**)sp->value;

                    g_assert(value);
                    g_assert(GWY_IS_SERIALIZABLE(value));
                    if (do_copy) {
                        memcpy(p + pos,
                               objects[nobjs].buffer, objects[nobjs].size);
                        g_free(objects[nobjs].buffer);
                    }
                    else {
                        objects[nobjs].size = 0;
                        objects[nobjs].buffer =
                            gwy_serializable_serialize(value, NULL,
                                                       &objects[nobjs].size);
                    }
                    pos += objects[nobjs].size;
                    nobjs++;
                }
                break;

                default:
                g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
                      "wrong spec `%c' at pos %d", sp->ctype, sp - spec);
                g_assert(!do_copy);
                g_free(p);
                /* FIXME: we may leak some objects[] here */
                return buffer;
                break;
            }
        }

        if (do_copy)
            did_copy = TRUE;
        else {
            buffer = g_renew(guchar, buffer, *size + pos);
            p = buffer + *size;
            *size += pos;
            do_copy = TRUE;
        }
    }
    g_free(objects);

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
    }

    g_assert_not_reached();
}


/**
 * gwy_serialize_unpack_object_struct:
 * @buffer: A memory location containing a serialized object at position
 *          @position.
 * @size: Current size of @buffer, new size is returned here.
 * @position: The position of the object in @buffer, it's updated to
 *            point after it.
 * @object_name: The g_type_name() of the object.
 * @nspec: The number of items in @spec.
 * @spec: The components to deserialize.
 *
 * Deserializes an object with named components packed by
 * gwy_serialize_pack_object_struct().
 *
 * Returns: Whether the unpacking succeeded
 * (see gwy_serialize_unpack_struct() for definition of success).
 **/
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
            g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
                  "Expected a component name to deserialize, got garbage");
            return FALSE;
        }

        for (sp = spec; sp - spec < nspec; sp++) {
            if (strcmp(sp->name, buffer + position) == 0)
                break;
        }
        name = buffer + position;
        position += nlen;
        ctype = gwy_serialize_unpack_char(buffer, size, &position);
        if (sp - spec == nspec) {
            g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                  "Extra component %s of type `%c'", name, ctype);
            gwy_serialize_skip_type(buffer, size, &position, ctype);
            continue;
        }

        if (ctype != sp->ctype) {
            g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                  "Bad or unknown type `%c' of %s (expected `%c')",
                  ctype, name, sp->ctype);
            return FALSE;
        }

        p = sp->value;
        a = sp->array_size;
        switch (ctype) {
            case 'o':
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
            *(guchar**)p = gwy_serialize_unpack_string(buffer, size, &position);
            break;

            case 'C':
            *(guchar**)p = gwy_serialize_unpack_char_array(buffer, size,
                                                            &position, a);
            break;

            case 'I':
            *(gint32**)p = gwy_serialize_unpack_int32_array(buffer, size,
                                                            &position, a);
            break;

            case 'Q':
            *(gint64**)p = gwy_serialize_unpack_int64_array(buffer, size,
                                                            &position, a);
            break;

            case 'D':
            *(gdouble**)p = gwy_serialize_unpack_double_array(buffer, size,
                                                              &position, a);
            break;

            default:
            g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
                  "Type `%c' of %s is unknown (though known to application?!)",
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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(buffer);
    g_assert(position);
    g_assert(*position + sizeof(guchar) <= size);
    value = buffer[*position];
    *position += sizeof(guchar);

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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(buffer);
    g_assert(position);
    g_assert(*position + sizeof(guchar) <= size);
    value = buffer[*position];
    *position += sizeof(guchar);

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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    *asize = gwy_serialize_unpack_int32(buffer, size, position);
    g_assert(*position + *asize*sizeof(guchar) <= size);
    value = g_memdup(buffer + *position, *asize*sizeof(guchar));
    *position += *asize*sizeof(guchar);

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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(buffer);
    g_assert(position);
    g_assert(*position + sizeof(gint32) <= size);
    memcpy(&value, buffer + *position, sizeof(gint32));
    *position += sizeof(gint32);

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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    *asize = gwy_serialize_unpack_int32(buffer, size, position);
    g_assert(*position + *asize*sizeof(gint32) <= size);
    value = g_memdup(buffer + *position, *asize*sizeof(gint32));
    *position += *asize*sizeof(gint32);

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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(buffer);
    g_assert(position);
    g_assert(*position + sizeof(gint64) <= size);
    memcpy(&value, buffer + *position, sizeof(gint64));
    *position += sizeof(gint64);

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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    *asize = gwy_serialize_unpack_int32(buffer, size, position);
    g_assert(*position + *asize*sizeof(gint64) <= size);
    value = g_memdup(buffer + *position, *asize*sizeof(gint64));
    *position += *asize*sizeof(gint64);

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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(buffer);
    g_assert(position);
    g_assert(*position + sizeof(gdouble) <= size);
    memcpy(&value, buffer + *position, sizeof(gdouble));
    *position += sizeof(gdouble);

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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    *asize = gwy_serialize_unpack_int32(buffer, size, position);
    g_assert(*position + *asize*sizeof(gdouble) <= size);
    value = g_memdup(buffer + *position, *asize*sizeof(gdouble));
    *position += *asize*sizeof(gdouble);

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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(buffer);
    g_assert(position);
    g_assert(*position < size);
    p = memchr(buffer + *position, 0, size - *position);
    g_assert(p);
    value = g_strdup(buffer + *position);
    *position += (p - buffer) - *position + 1;

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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(buffer);
    g_assert(size > 0);
    g_assert(position < size);
    p = (guchar*)memchr(buffer + position, 0, size - position);
    if (!p || (compare_to && strcmp(buffer + position, compare_to)))
        return 0;

    return (p - buffer) + 1 - position;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
