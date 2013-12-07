/*
 *  $Id$
 *  Copyright (C) 2009-2013 David Nečas (Yeti).
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

#ifndef __LIBGWY_DICT_H__
#define __LIBGWY_DICT_H__

#include <glib-object.h>
#include <libgwy/serializable.h>

G_BEGIN_DECLS

#define GWY_DICT_PATHSEP      '/'
#define GWY_DICT_PATHSEP_STR  "/"

typedef struct _GwyDict GwyDict;
typedef struct _GwyDictClass GwyDictClass;

#define GWY_TYPE_DICT \
    (gwy_dict_get_type())
#define GWY_DICT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DICT, GwyDict))
#define GWY_DICT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DICT, GwyDictClass))
#define GWY_IS_DICT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DICT))
#define GWY_IS_DICT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DICT))
#define GWY_DICT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DICT, GwyDictClass))

struct _GwyDict {
    GObject g_object;
    struct _GwyDictPrivate *priv;
};

struct _GwyDictClass {
    /*<private>*/
    GObjectClass g_object_class;
};

typedef void (*GwyDictForeachFunc)(GQuark key,
                                   const GValue *value,
                                   gpointer user_data);

#define gwy_dict_duplicate(dict) \
    (GWY_DICT(gwy_serializable_duplicate(GWY_SERIALIZABLE(dict))))
#define gwy_dict_assign(dest, src) \
    (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType         gwy_dict_get_type      (void)                        G_GNUC_CONST;
GwyDict*      gwy_dict_new           (void)                        G_GNUC_MALLOC;
guint         gwy_dict_size          (const GwyDict *dict)         G_GNUC_PURE;
GQuark*       gwy_dict_keys          (const GwyDict *dict)         G_GNUC_MALLOC;
const gchar** gwy_dict_keys_n        (const GwyDict *dict)         G_GNUC_MALLOC;
GType         gwy_dict_item_type     (const GwyDict *dict,
                                      GQuark key)                  G_GNUC_PURE;
GType         gwy_dict_item_type_n   (const GwyDict *dict,
                                      const gchar *name)           G_GNUC_PURE;
gboolean      gwy_dict_contains      (const GwyDict *dict,
                                      GQuark key)                  G_GNUC_PURE;
gboolean      gwy_dict_contains_n    (const GwyDict *dict,
                                      const gchar *name)           G_GNUC_PURE;
gboolean      gwy_dict_get_value     (const GwyDict *dict,
                                      GQuark key,
                                      GValue *value);
gboolean      gwy_dict_get_value_n   (const GwyDict *dict,
                                      const gchar *name,
                                      GValue *value);
void          gwy_dict_set_value     (GwyDict *dict,
                                      GQuark key,
                                      const GValue *value);
gboolean      gwy_dict_remove        (GwyDict *dict,
                                      GQuark key);
gboolean      gwy_dict_remove_n      (GwyDict *dict,
                                      const gchar *name);
guint         gwy_dict_remove_prefix (GwyDict *dict,
                                      const gchar *prefix);
gboolean      gwy_dict_rename        (GwyDict *dict,
                                      GQuark key,
                                      GQuark newkey,
                                      gboolean force);
gboolean      gwy_dict_rename_n      (GwyDict *dict,
                                      const gchar *name,
                                      const gchar *newname,
                                      gboolean force);
guint         gwy_dict_transfer      (GwyDict *source,
                                      GwyDict *dest,
                                      const gchar *source_prefix,
                                      const gchar *dest_prefix,
                                      gboolean deep,
                                      gboolean force);
guint         gwy_dict_foreach       (GwyDict *dict,
                                      const gchar *prefix,
                                      GwyDictForeachFunc function,
                                      gpointer user_data);
void          gwy_dict_set_boolean   (GwyDict *dict,
                                      GQuark key,
                                      gboolean value);
void          gwy_dict_set_boolean_n (GwyDict *dict,
                                      const gchar *name,
                                      gboolean value);
gboolean      gwy_dict_get_boolean   (const GwyDict *dict,
                                      GQuark key)                  G_GNUC_PURE;
gboolean      gwy_dict_get_boolean_n (const GwyDict *dict,
                                      const gchar *name)           G_GNUC_PURE;
gboolean      gwy_dict_pick_boolean  (const GwyDict *dict,
                                      GQuark key,
                                      gboolean *value);
gboolean      gwy_dict_pick_boolean_n(const GwyDict *dict,
                                      const gchar *name,
                                      gboolean *value);
void          gwy_dict_set_schar     (GwyDict *dict,
                                      GQuark key,
                                      gint8 value);
void          gwy_dict_set_schar_n   (GwyDict *dict,
                                      const gchar *name,
                                      gint8 value);
gint8         gwy_dict_get_schar     (const GwyDict *dict,
                                      GQuark key)                  G_GNUC_PURE;
gint8         gwy_dict_get_schar_n   (const GwyDict *dict,
                                      const gchar *name)           G_GNUC_PURE;
gboolean      gwy_dict_pick_schar    (const GwyDict *dict,
                                      GQuark key,
                                      gint8 *value);
gboolean      gwy_dict_pick_schar_n  (const GwyDict *dict,
                                      const gchar *name,
                                      gint8 *value);
void          gwy_dict_set_int32     (GwyDict *dict,
                                      GQuark key,
                                      gint32 value);
void          gwy_dict_set_int32_n   (GwyDict *dict,
                                      const gchar *name,
                                      gint32 value);
gint32        gwy_dict_get_int32     (const GwyDict *dict,
                                      GQuark key)                  G_GNUC_PURE;
gint32        gwy_dict_get_int32_n   (const GwyDict *dict,
                                      const gchar *name)           G_GNUC_PURE;
gboolean      gwy_dict_pick_int32    (const GwyDict *dict,
                                      GQuark key,
                                      gint32 *value);
gboolean      gwy_dict_pick_int32_n  (const GwyDict *dict,
                                      const gchar *name,
                                      gint32 *value);
void          gwy_dict_set_enum      (GwyDict *dict,
                                      GQuark key,
                                      guint32 value);
void          gwy_dict_set_enum_n    (GwyDict *dict,
                                      const gchar *name,
                                      guint32 value);
guint32       gwy_dict_get_enum      (const GwyDict *dict,
                                      GQuark key)                  G_GNUC_PURE;
guint32       gwy_dict_get_enum_n    (const GwyDict *dict,
                                      const gchar *name)           G_GNUC_PURE;
gboolean      gwy_dict_pick_enum     (const GwyDict *dict,
                                      GQuark key,
                                      guint32 *value);
gboolean      gwy_dict_pick_enum_n   (const GwyDict *dict,
                                      const gchar *name,
                                      guint32 *value);
void          gwy_dict_set_int64     (GwyDict *dict,
                                      GQuark key,
                                      gint64 value);
void          gwy_dict_set_int64_n   (GwyDict *dict,
                                      const gchar *name,
                                      gint64 value);
gint64        gwy_dict_get_int64     (const GwyDict *dict,
                                      GQuark key)                  G_GNUC_PURE;
gint64        gwy_dict_get_int64_n   (const GwyDict *dict,
                                      const gchar *name)           G_GNUC_PURE;
gboolean      gwy_dict_pick_int64    (const GwyDict *dict,
                                      GQuark key,
                                      gint64 *value);
gboolean      gwy_dict_pick_int64_n  (const GwyDict *dict,
                                      const gchar *name,
                                      gint64 *value);
void          gwy_dict_set_double    (GwyDict *dict,
                                      GQuark key,
                                      gdouble value);
void          gwy_dict_set_double_n  (GwyDict *dict,
                                      const gchar *name,
                                      gdouble value);
gdouble       gwy_dict_get_double    (const GwyDict *dict,
                                      GQuark key)                  G_GNUC_PURE;
gdouble       gwy_dict_get_double_n  (const GwyDict *dict,
                                      const gchar *name)           G_GNUC_PURE;
gboolean      gwy_dict_pick_double   (const GwyDict *dict,
                                      GQuark key,
                                      gdouble *value);
gboolean      gwy_dict_pick_double_n (const GwyDict *dict,
                                      const gchar *name,
                                      gdouble *value);
void          gwy_dict_set_string    (GwyDict *dict,
                                      GQuark key,
                                      const gchar *value);
void          gwy_dict_set_string_n  (GwyDict *dict,
                                      const gchar *name,
                                      const gchar *value);
void          gwy_dict_take_string   (GwyDict *dict,
                                      GQuark key,
                                      gchar *value);
void          gwy_dict_take_string_n (GwyDict *dict,
                                      const gchar *name,
                                      gchar *value);
const gchar*  gwy_dict_get_string    (const GwyDict *dict,
                                      GQuark key)                  G_GNUC_PURE;
const gchar*  gwy_dict_get_string_n  (const GwyDict *dict,
                                      const gchar *name)           G_GNUC_PURE;
gboolean      gwy_dict_pick_string   (const GwyDict *dict,
                                      GQuark key,
                                      const gchar **value);
gboolean      gwy_dict_pick_string_n (const GwyDict *dict,
                                      const gchar *name,
                                      const gchar **value);
void          gwy_dict_set_object    (GwyDict *dict,
                                      GQuark key,
                                      gpointer value);
void          gwy_dict_set_object_n  (GwyDict *dict,
                                      const gchar *name,
                                      gpointer value);
void          gwy_dict_take_object   (GwyDict *dict,
                                      GQuark key,
                                      gpointer value);
void          gwy_dict_take_object_n (GwyDict *dict,
                                      const gchar *name,
                                      gpointer value);
gpointer      gwy_dict_get_object    (const GwyDict *dict,
                                      GQuark key)                  G_GNUC_PURE;
gpointer      gwy_dict_get_object_n  (const GwyDict *dict,
                                      const gchar *name)           G_GNUC_PURE;
gboolean      gwy_dict_pick_object   (const GwyDict *dict,
                                      GQuark key,
                                      gpointer value);
gboolean      gwy_dict_pick_object_n (const GwyDict *dict,
                                      const gchar *name,
                                      gpointer value);
void          gwy_dict_set_boxed     (GwyDict *dict,
                                      GQuark key,
                                      GType type,
                                      gpointer value);
void          gwy_dict_set_boxed_n   (GwyDict *dict,
                                      const gchar *name,
                                      GType type,
                                      gpointer value);
gconstpointer gwy_dict_get_boxed     (const GwyDict *dict,
                                      GQuark key,
                                      GType type)                  G_GNUC_PURE;
gconstpointer gwy_dict_get_boxed_n   (const GwyDict *dict,
                                      const gchar *name,
                                      GType type)                  G_GNUC_PURE;
gboolean      gwy_dict_pick_boxed    (const GwyDict *dict,
                                      GQuark key,
                                      GType type,
                                      gpointer value);
gboolean      gwy_dict_pick_boxed_n  (const GwyDict *dict,
                                      const gchar *name,
                                      GType type,
                                      gpointer value);
gchar**       gwy_dict_dump_to_text  (const GwyDict *dict)         G_GNUC_MALLOC;
GwyDict*      gwy_dict_new_from_text (const gchar *text)           G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
