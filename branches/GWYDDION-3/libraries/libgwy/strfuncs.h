/*
 *  $Id$
 *  Copyright (C) 2009,2011-2014 David Nečas (Yeti).
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

#ifndef __LIBGWY_STRFUNCS_H__
#define __LIBGWY_STRFUNCS_H__

#include <glib.h>
#include <string.h>

G_BEGIN_DECLS

#define gwy_strequal(a, b) \
    (!strcmp((a), (b)))

gboolean gwy_ascii_strisident    (const gchar *s,
                                  const gchar *more,
                                  const gchar *startmore)    G_GNUC_PURE;
gboolean gwy_utf8_strisident     (const gchar *s,
                                  const gunichar *more,
                                  const gunichar *startmore) G_GNUC_PURE;
gboolean gwy_ascii_strcase_equal (gconstpointer v1,
                                  gconstpointer v2)          G_GNUC_PURE;
guint    gwy_ascii_strcase_hash  (gconstpointer v)           G_GNUC_PURE;
guint    gwy_stramong            (const gchar *str,
                                  ...)                       G_GNUC_NULL_TERMINATED G_GNUC_PURE;
guint    gwy_str_remove_prefix   (gchar *str,
                                  ...)                       G_GNUC_NULL_TERMINATED;
guint    gwy_str_remove_suffix   (gchar *str,
                                  ...)                       G_GNUC_NULL_TERMINATED;
gchar*   gwy_str_next_line       (gchar **buffer);
gpointer gwy_memmem              (gconstpointer haystack,
                                  gsize haystack_len,
                                  gconstpointer needle,
                                  gsize needle_len)          G_GNUC_PURE;
void     gwy_utf8_append_exponent(GString *str,
                                  gint power);
guint    gwy_gstring_replace     (GString *str,
                                  const gchar *old,
                                  const gchar *replacement,
                                  gint count);

typedef struct _GwyStrLineIter GwyStrLineIter;

GwyStrLineIter* gwy_str_line_iter_new     (gchar *buffer)              G_GNUC_MALLOC;
GwyStrLineIter* gwy_str_line_iter_new_take(gchar *buffer)              G_GNUC_MALLOC;
void            gwy_str_line_iter_free    (GwyStrLineIter *iter);
gchar*          gwy_str_line_iter_next    (GwyStrLineIter *iter);
guint           gwy_str_line_iter_lineno  (const GwyStrLineIter *iter) G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
