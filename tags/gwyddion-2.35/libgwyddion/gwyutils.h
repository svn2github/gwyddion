/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2011 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef __GWY_UTILS_H__
#define __GWY_UTILS_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
    GWY_RAW_DATA_SINT8,
    GWY_RAW_DATA_UINT8,
    GWY_RAW_DATA_SINT16,
    GWY_RAW_DATA_UINT16,
    GWY_RAW_DATA_SINT32,
    GWY_RAW_DATA_UINT32,
    GWY_RAW_DATA_SINT64,
    GWY_RAW_DATA_UINT64,
    GWY_RAW_DATA_HALF,
    GWY_RAW_DATA_FLOAT,
    GWY_RAW_DATA_REAL,
    GWY_RAW_DATA_DOUBLE,
} GwyRawDataType;

typedef enum {
    GWY_BYTE_ORDER_NATIVE        = 0,
    GWY_BYTE_ORDER_LITTLE_ENDIAN = G_LITTLE_ENDIAN,
    GWY_BYTE_ORDER_BIG_ENDIAN    = G_BIG_ENDIAN,
} GwyByteOrder;

void         gwy_hash_table_to_slist_cb(gpointer unused_key,
                                        gpointer value,
                                        gpointer user_data);
void         gwy_hash_table_to_list_cb (gpointer unused_key,
                                        gpointer value,
                                        gpointer user_data);
gchar*       gwy_strkill               (gchar *s,
                                        const gchar *killchars);
gchar*       gwy_strreplace            (const gchar *haystack,
                                        const gchar *needle,
                                        const gchar *replacement,
                                        gsize maxrepl);
gint         gwy_strdiffpos            (const gchar *s1,
                                        const gchar *s2);
gboolean     gwy_strisident            (const gchar *s,
                                        const gchar *more,
                                        const gchar *startmore);
gboolean     gwy_ascii_strcase_equal   (gconstpointer v1,
                                        gconstpointer v2);
guint        gwy_ascii_strcase_hash    (gconstpointer v);
guint        gwy_stramong              (const gchar *str,
                                        ...);
gpointer     gwy_memmem                (gconstpointer haystack,
                                        gsize haystack_len,
                                        gconstpointer needle,
                                        gsize needle_len);
gboolean     gwy_file_get_contents     (const gchar *filename,
                                        guchar **buffer,
                                        gsize *size,
                                        GError **error);
gboolean     gwy_file_abandon_contents (guchar *buffer,
                                        gsize size,
                                        GError **error);
gchar*       gwy_find_self_dir         (const gchar *dirname);
const gchar* gwy_get_user_dir          (void);
const gchar* gwy_get_home_dir          (void);
gchar*       gwy_canonicalize_path     (const gchar *path);
gboolean     gwy_filename_ignore       (const gchar *filename_sys);
gchar*       gwy_sgettext              (const gchar *msgid);
gchar*       gwy_str_next_line         (gchar **buffer);
void         gwy_memcpy_byte_swap      (const guint8 *source,
                                        guint8 *dest,
                                        gsize item_size,
                                        gsize nitems,
                                        gsize byteswap);
void         gwy_convert_raw_data      (gconstpointer data,
                                        gsize nitems,
                                        gssize stride,
                                        GwyRawDataType datatype,
                                        GwyByteOrder byteorder,
                                        gdouble *target,
                                        gdouble scale,
                                        gdouble offset);
guint        gwy_raw_data_size         (GwyRawDataType datatype);
void         gwy_object_set_or_reset   (gpointer object,
                                        GType type,
                                        ...);

G_END_DECLS

#endif /* __GWY_UTILS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

