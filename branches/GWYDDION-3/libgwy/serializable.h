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

#ifndef __GWY_SERIALIZABLE_H__
#define __GWY_SERIALIZABLE_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef union  _GwySerializableValue GwySerializableValue;
typedef struct _GwySerializableItem  GwySerializableItem;
typedef struct _GwySerializableItems GwySerializableItems;

union _GwySerializableValue {
    gboolean v_boolean;
    guchar v_char;
    guint32 v_int32;
    guint64 v_int64;
    gdouble v_double;
    guchar *v_string;
    GObject *v_object;
    gboolean *v_boolean_array;
    guchar *v_char_array;
    guint32 *v_int32_array;
    guint64 *v_int64_array;
    gdouble *v_double_array;
    guchar **v_string_array;
    GObject **v_object_array;
};

struct _GwySerializableItem {
    const gchar *name;
    GwySerializableValue value;
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
    gsize    (*n_items)  (GwySerializable *serializable);
    void     (*add_items)(GwySerializable *serializable,
                          GwySerializableItems *items);
    void     (*done)     (GwySerializable *serializable);
             
    GObject* (*construct)(const GwySerializableItems *items);
              
    GObject* (*duplicate)(GwySerializable *serializable);
    void     (*assign)   (GwySerializable *source,
                          GwySerializable *destination);
};

GType gwy_serializable_get_type(void);

/* NOTE: We may need to use error lists here. */
gboolean gwy_serializable_serialize  (GwySerializable *serializable,
                                      GInputStream *input,
                                      GError **error);
GObject* gwy_serializable_deserialize(GInputStream *input,
                                      GError **error);
GObject* gwy_serializable_duplicate  (GwySerializable *serializable);
void     gwy_serializable_assign     (GwySerializable *source,
                                      GwySerializable *destination);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
