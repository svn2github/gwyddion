/*
 *  $Id$
 *  Copyright (C) 2011-2013 David Nečas (Yeti).
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

#ifndef __LIBGWY_OBJECT_UTILS_H__
#define __LIBGWY_OBJECT_UTILS_H__

#include <glib-object.h>
#include <libgwy/macros.h>

G_BEGIN_DECLS

#define GWY_OBJECT_UNREF(obj) \
    do { if (obj) { g_object_unref(obj); (obj) = NULL; } } while (0)

#define GWY_SIGNAL_HANDLER_DISCONNECT(obj, hid) \
    do { \
        if (hid && obj) { g_signal_handler_disconnect(obj, hid); (hid) = 0; } \
    } while (0)

gboolean     gwy_set_user_func             (gpointer func,
                                            gpointer data,
                                            GDestroyNotify destroy,
                                            gpointer func_field,
                                            gpointer data_field,
                                            GDestroyNotify *destroy_field);
gboolean     gwy_set_member_object         (gpointer instance,
                                            gpointer member_object,
                                            GType expected_type,
                                            gpointer member_field,
                                            ...)                            G_GNUC_NULL_TERMINATED;
gboolean     gwy_assign_string             (gchar **target,
                                            const gchar *newvalue);
gboolean     gwy_assign_boxed              (gpointer *target,
                                            gconstpointer newvalue,
                                            GType boxed_type);
void         gwy_override_class_properties (GObjectClass *oclass,
                                            GParamSpec **properties,
                                            ...)                            G_GNUC_NULL_TERMINATED;
guint        gwy_replicate_class_properties(GObjectClass *klass,
                                            GType fromtype,
                                            GParamSpec **properties,
                                            guint id,
                                            ...)                            G_GNUC_NULL_TERMINATED;
GType*       gwy_all_type_children         (GType type,
                                            gboolean concrete,
                                            guint *n)                       G_GNUC_MALLOC;
const gchar* gwy_genum_value_nick          (GType enumtype,
                                            gint value)                     G_GNUC_PURE;
const gchar* gwy_gflags_value_nick         (GType flagstype,
                                            guint value)                    G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
