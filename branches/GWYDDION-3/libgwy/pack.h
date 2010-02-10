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

#ifdef __LIBGWY_ALIASES_H__
#error Public headers must be included before the aliasing header.
#endif

#ifndef __LIBGWY_PACK_H__
#define __LIBGWY_PACK_H__

#include <glib.h>

G_BEGIN_DECLS

#define GWY_PACK_ERROR gwy_pack_error_quark()

typedef enum {
    GWY_PACK_ERROR_FORMAT = 1,
    GWY_PACK_ERROR_SIZE,
    GWY_PACK_ERROR_ARGUMENTS,
    GWY_PACK_ERROR_DATA,
} GwyPackError;

GQuark gwy_pack_error_quark(void);
gsize  gwy_pack_size       (const gchar *format,
                            GError **error);
gsize  gwy_pack            (const gchar *format,
                            guchar *buffer,
                            gsize size,
                            GError **error,
                            ...);
gsize  gwy_unpack          (const gchar *format,
                            const guchar *buffer,
                            gsize size,
                            GError **error,
                            ...);
void   gwy_unpack_data     (const gchar *format,
                            gconstpointer buffer,
                            gsize len,
                            gdouble *data,
                            gint data_stride,
                            gdouble factor,
                            gdouble shift);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
