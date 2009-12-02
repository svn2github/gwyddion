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

#ifndef __LIBGWY_SERIALIZABLE_H__
#define __LIBGWY_SERIALIZABLE_H__

#include <glib-object.h>
#include <libgwy/error-list.h>

G_BEGIN_DECLS

typedef union  _GwySerializableValue GwySerializableValue;
typedef struct _GwySerializableItem  GwySerializableItem;
typedef struct _GwySerializableItems GwySerializableItems;

typedef enum {
    GWY_SERIALIZABLE_HEADER        = 0,
    GWY_SERIALIZABLE_INT8          = 'c',
    GWY_SERIALIZABLE_INT8_ARRAY    = 'C',
    GWY_SERIALIZABLE_BOOLEAN       = 'b',
    GWY_SERIALIZABLE_INT16         = 'h',
    GWY_SERIALIZABLE_INT16_ARRAY   = 'H',
    GWY_SERIALIZABLE_INT32         = 'i',
    GWY_SERIALIZABLE_INT32_ARRAY   = 'I',
    GWY_SERIALIZABLE_INT64         = 'q',
    GWY_SERIALIZABLE_INT64_ARRAY   = 'Q',
    GWY_SERIALIZABLE_DOUBLE        = 'd',
    GWY_SERIALIZABLE_DOUBLE_ARRAY  = 'D',
    GWY_SERIALIZABLE_STRING        = 's',
    GWY_SERIALIZABLE_STRING_ARRAY  = 'S',
    GWY_SERIALIZABLE_OBJECT        = 'o',
    GWY_SERIALIZABLE_OBJECT_ARRAY  = 'O',
    GWY_SERIALIZABLE_BOXED         = 'x',
    GWY_SERIALIZABLE_BOXED_ARRAY   = 'X',
} GwySerializableCType;

union _GwySerializableValue {
    gboolean v_boolean;
    gint8 v_int8;
    guint8 v_uint8;
    gint16 v_int16;
    guint16 v_uint16;
    gint32 v_int32;
    guint32 v_uint32;
    gint64 v_int64;
    guint64 v_uint64;
    gdouble v_double;
    gchar *v_string;
    guchar *v_ustring;
    GObject *v_object;
    gpointer v_boxed;
    gsize v_size;
    gint8 *v_int8_array;
    guint8 *v_uint8_array;
    gint16 *v_int16_array;
    guint16 *v_uint16_array;
    gint32 *v_int32_array;
    guint32 *v_uint32_array;
    gint64 *v_int64_array;
    guint64 *v_uint64_array;
    gdouble *v_double_array;
    gchar **v_string_array;
    guchar **v_ustring_array;
    GObject **v_object_array;
    gpointer *v_boxed_array;
};

struct _GwySerializableItem {
    GwySerializableValue value;
    const gchar *name;
    gsize array_size;
    guchar ctype;
};

struct _GwySerializableItems {
    gsize len;
    gsize n_items;
    GwySerializableItem *items;
};

typedef struct _GwySerializableInterface GwySerializableInterface;
typedef struct _GwySerializable          GwySerializable;        /* dummy */

#define GWY_TYPE_SERIALIZABLE \
    (gwy_serializable_get_type())
#define GWY_SERIALIZABLE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SERIALIZABLE, GwySerializable))
#define GWY_IS_SERIALIZABLE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SERIALIZABLE))
#define GWY_SERIALIZABLE_GET_INTERFACE(obj) \
    (G_TYPE_INSTANCE_GET_INTERFACE((obj), GWY_TYPE_SERIALIZABLE, GwySerializableInterface))

#define GWY_IMPLEMENT_SERIALIZABLE(interface_init) \
    { \
        static const GInterfaceInfo gwy_serializable_interface_info = { \
            (GInterfaceInitFunc)interface_init, NULL, NULL \
        }; \
        g_type_add_interface_static(g_define_type_id, \
                                    GWY_TYPE_SERIALIZABLE, \
                                    &gwy_serializable_interface_info); \
    }

struct _GwySerializableInterface {
    GTypeInterface g_interface;

    /* virtual table */
    gsize                 (*n_items)  (GwySerializable *serializable);
    gsize                 (*itemize)  (GwySerializable *serializable,
                                       GwySerializableItems *items);
    void                  (*done)     (GwySerializable *serializable);

    GObject*              (*construct)(GwySerializableItems *items,
                                       GwyErrorList **error_list);

    GObject*              (*duplicate)(GwySerializable *serializable);
    void                  (*assign)   (GwySerializable *destination,
                                       GwySerializable *source);
};

GType    gwy_serializable_get_type (void)                           G_GNUC_CONST;
GObject* gwy_serializable_duplicate(GwySerializable *serializable)  G_GNUC_MALLOC;
void     gwy_serializable_assign   (GwySerializable *destination,
                                    GwySerializable *source);
gsize    gwy_serializable_n_items  (GwySerializable *serializable);
void     gwy_serializable_itemize  (GwySerializable *serializable,
                                    GwySerializableItems *items);
void     gwy_serializable_done     (GwySerializable *serializable);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
