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

#ifndef __LIBGWY_CONTAINER_H__
#define __LIBGWY_CONTAINER_H__

#include <glib-object.h>
#include <libgwy/serializable.h>

G_BEGIN_DECLS

#define GWY_CONTAINER_PATHSEP      '/'
#define GWY_CONTAINER_PATHSEP_STR  "/"

typedef struct _GwyContainer GwyContainer;
typedef struct _GwyContainerClass GwyContainerClass;

#define GWY_TYPE_CONTAINER \
    (gwy_container_get_type())
#define GWY_CONTAINER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_CONTAINER, GwyContainer))
#define GWY_CONTAINER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_CONTAINER, GwyContainerClass))
#define GWY_IS_CONTAINER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_CONTAINER))
#define GWY_IS_CONTAINER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_CONTAINER))
#define GWY_CONTAINER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_CONTAINER, GwyContainerClass))

struct _GwyContainer {
    GObject g_object;
    struct _GwyContainerPrivate *priv;
};

struct _GwyContainerClass {
    /*<private>*/
    GObjectClass g_object_class;
};

typedef void (*GwyContainerForeachFunc)(GQuark key,
                                        const GValue *value,
                                        gpointer user_data);

#define gwy_container_duplicate(container) \
        (GWY_CONTAINER(gwy_serializable_duplicate(GWY_SERIALIZABLE(container))))
#define gwy_container_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType         gwy_container_get_type     (void)                       G_GNUC_CONST;
GwyContainer* gwy_container_new          (void)                       G_GNUC_MALLOC;
guint         gwy_container_n_items      (GwyContainer *container)    G_GNUC_PURE;
GQuark*       gwy_container_keys         (GwyContainer *container)    G_GNUC_MALLOC;
const gchar** gwy_container_keys_n       (GwyContainer *container)    G_GNUC_MALLOC;
GType         gwy_container_item_type    (GwyContainer *container,
                                          GQuark key)                 G_GNUC_PURE;
gboolean      gwy_container_contains     (GwyContainer *container,
                                          GQuark key)                 G_GNUC_PURE;
gboolean      gwy_container_get_value    (GwyContainer *container,
                                          GQuark key,
                                          GValue *value);
void          gwy_container_set_value    (GwyContainer *container,
                                          GQuark key,
                                          const GValue *value);
gboolean      gwy_container_remove       (GwyContainer *container,
                                          GQuark key);
guint         gwy_container_remove_prefix(GwyContainer *container,
                                          const gchar *prefix);
gboolean      gwy_container_rename       (GwyContainer *container,
                                          GQuark key,
                                          GQuark newkey,
                                          gboolean force);
guint         gwy_container_transfer     (GwyContainer *source,
                                          GwyContainer *dest,
                                          const gchar *source_prefix,
                                          const gchar *dest_prefix,
                                          gboolean deep,
                                          gboolean force);
guint         gwy_container_foreach      (GwyContainer *container,
                                          const gchar *prefix,
                                          GwyContainerForeachFunc function,
                                          gpointer user_data);
void          gwy_container_set_boolean  (GwyContainer *container,
                                          GQuark key,
                                          gboolean value);
gboolean      gwy_container_get_boolean  (GwyContainer *container,
                                          GQuark key)                 G_GNUC_PURE;
gboolean      gwy_container_gis_boolean  (GwyContainer *container,
                                          GQuark key,
                                          gboolean *value);
void          gwy_container_set_char     (GwyContainer *container,
                                          GQuark key,
                                          gchar value);
gchar         gwy_container_get_char     (GwyContainer *container,
                                          GQuark key)                 G_GNUC_PURE;
gboolean      gwy_container_gis_char     (GwyContainer *container,
                                          GQuark key,
                                          gchar *value);
void          gwy_container_set_int32    (GwyContainer *container,
                                          GQuark key,
                                          gint32 value);
gint32        gwy_container_get_int32    (GwyContainer *container,
                                          GQuark key)                 G_GNUC_PURE;
gboolean      gwy_container_gis_int32    (GwyContainer *container,
                                          GQuark key,
                                          gint32 *value);
void          gwy_container_set_enum     (GwyContainer *container,
                                          GQuark key,
                                          guint value);
guint         gwy_container_get_enum     (GwyContainer *container,
                                          GQuark key)                 G_GNUC_PURE;
gboolean      gwy_container_gis_enum     (GwyContainer *container,
                                          GQuark key,
                                          guint *value);
void          gwy_container_set_int64    (GwyContainer *container,
                                          GQuark key,
                                          gint64 value);
gint64        gwy_container_get_int64    (GwyContainer *container,
                                          GQuark key)                 G_GNUC_PURE;
gboolean      gwy_container_gis_int64    (GwyContainer *container,
                                          GQuark key,
                                          gint64 *value);
void          gwy_container_set_double   (GwyContainer *container,
                                          GQuark key,
                                          gdouble value);
gdouble       gwy_container_get_double   (GwyContainer *container,
                                          GQuark key)                 G_GNUC_PURE;
gboolean      gwy_container_gis_double   (GwyContainer *container,
                                          GQuark key,
                                          gdouble *value);
void          gwy_container_set_string   (GwyContainer *container,
                                          GQuark key,
                                          const gchar *value);
void          gwy_container_take_string  (GwyContainer *container,
                                          GQuark key,
                                          gchar *value);
const gchar*  gwy_container_get_string   (GwyContainer *container,
                                          GQuark key)                 G_GNUC_PURE;
gboolean      gwy_container_gis_string   (GwyContainer *container,
                                          GQuark key,
                                          const gchar **value);
void          gwy_container_set_object   (GwyContainer *container,
                                          GQuark key,
                                          gpointer value);
void          gwy_container_take_object  (GwyContainer *container,
                                          GQuark key,
                                          gpointer value);
gpointer      gwy_container_get_object   (GwyContainer *container,
                                          GQuark key)                 G_GNUC_PURE;
gboolean      gwy_container_gis_object   (GwyContainer *container,
                                          GQuark key,
                                          gpointer value);
void          gwy_container_set_boxed    (GwyContainer *container,
                                          GQuark key,
                                          GType type,
                                          gpointer value);
gconstpointer gwy_container_get_boxed    (GwyContainer *container,
                                          GQuark key,
                                          GType type)                 G_GNUC_PURE;
gboolean      gwy_container_gis_boxed    (GwyContainer *container,
                                          GQuark key,
                                          GType type,
                                          gpointer value);
gchar**       gwy_container_dump_to_text (GwyContainer *container)    G_GNUC_MALLOC;
GwyContainer* gwy_container_new_from_text(const gchar *text)          G_GNUC_MALLOC;

#define gwy_container_item_type_n(container,name) \
    gwy_container_item_type(container,g_quark_try_string(name))
#define gwy_container_contains_n(container,name) \
    gwy_container_contains(container,g_quark_try_string(name))
#define gwy_container_set_value_n(container,name,value) \
    gwy_container_set_value(container,g_quark_from_string(name),value)
#define gwy_container_get_value_n(container,name,value) \
    gwy_container_get_value(container,g_quark_try_string(name),value)
#define gwy_container_remove_n(container,name) \
    gwy_container_remove(container,g_quark_try_string(name))
#define gwy_container_rename_n(container,name,newname,force) \
    gwy_container_rename(container,g_quark_try_string(name),g_quark_from_string(newname),force)
#define gwy_container_set_boolean_n(container,name,value) \
    gwy_container_set_boolean(container,g_quark_from_string(name),value)
#define gwy_container_get_boolean_n(container,name) \
    gwy_container_get_boolean(container,g_quark_try_string(name))
#define gwy_container_gis_boolean_n(container,name,value) \
    gwy_container_gis_boolean(container,g_quark_try_string(name),value)
#define gwy_container_set_char_n(container,name,value) \
    gwy_container_set_char(container,g_quark_from_string(name),value)
#define gwy_container_get_char_n(container,name) \
    gwy_container_get_char(container,g_quark_try_string(name))
#define gwy_container_gis_char_n(container,name,value) \
    gwy_container_gis_char(container,g_quark_try_string(name),value)
#define gwy_container_set_int32_n(container,name,value) \
    gwy_container_set_int32(container,g_quark_from_string(name),value)
#define gwy_container_get_int32_n(container,name) \
    gwy_container_get_int32(container,g_quark_try_string(name))
#define gwy_container_gis_int32_n(container,name,value) \
    gwy_container_gis_int32(container,g_quark_try_string(name),value)
#define gwy_container_set_enum_n(container,name,value) \
    gwy_container_set_enum(container,g_quark_from_string(name),value)
#define gwy_container_get_enum_n(container,name) \
    gwy_container_get_enum(container,g_quark_try_string(name))
#define gwy_container_gis_enum_n(container,name,value) \
    gwy_container_gis_enum(container,g_quark_try_string(name),value)
#define gwy_container_set_int64_n(container,name,value) \
    gwy_container_set_int64(container,g_quark_from_string(name),value)
#define gwy_container_get_int64_n(container,name) \
    gwy_container_get_int64(container,g_quark_try_string(name))
#define gwy_container_gis_int64_n(container,name,value) \
    gwy_container_gis_int64(container,g_quark_try_string(name),value)
#define gwy_container_set_double_n(container,name,value) \
    gwy_container_set_double(container,g_quark_from_string(name),value)
#define gwy_container_get_double_n(container,name) \
    gwy_container_get_double(container,g_quark_try_string(name))
#define gwy_container_gis_double_n(container,name,value) \
    gwy_container_gis_double(container,g_quark_try_string(name),value)
#define gwy_container_set_string_n(container,name,value) \
    gwy_container_set_string(container,g_quark_from_string(name),value)
#define gwy_container_take_string_n(container,name,value) \
    gwy_container_take_string(container,g_quark_from_string(name),value)
#define gwy_container_get_string_n(container,name) \
    gwy_container_get_string(container,g_quark_try_string(name))
#define gwy_container_gis_string_n(container,name,value) \
    gwy_container_gis_string(container,g_quark_try_string(name),value)
#define gwy_container_set_object_n(container,name,value) \
    gwy_container_set_object(container,g_quark_from_string(name),value)
#define gwy_container_take_object_n(container,name,value) \
    gwy_container_take_object(container,g_quark_from_string(name),value)
#define gwy_container_get_object_n(container,name) \
    gwy_container_get_object(container,g_quark_try_string(name))
#define gwy_container_gis_object_n(container,name,value) \
    gwy_container_gis_object(container,g_quark_try_string(name),value)
#define gwy_container_set_boxed_n(container,name,type,value) \
    gwy_container_set_boxed(container,g_quark_from_string(name),type,value)
#define gwy_container_get_boxed_n(container,name,type) \
    gwy_container_get_boxed(container,g_quark_try_string(name),type)
#define gwy_container_gis_boxed_n(container,name,type,value) \
    gwy_container_gis_boxed(container,g_quark_try_string(name),type,value)

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
