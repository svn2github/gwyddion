/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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

#ifndef __LIBGWY_SERIALIZE_H__
#define __LIBGWY_SERIALIZE_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <libgwy/serializable.h>

G_BEGIN_DECLS

#define GWY_DESERIALIZE_ERROR gwy_deserialize_error_quark()

typedef enum {
    GWY_DESERIALIZE_ERROR_PADDING = 1,
    GWY_DESERIALIZE_ERROR_ITEM,
    GWY_DESERIALIZE_ERROR_FATAL_MASK = 1024,
    GWY_DESERIALIZE_ERROR_TRUNCATED = GWY_DESERIALIZE_ERROR_FATAL_MASK + GWY_DESERIALIZE_ERROR_ITEM + 1,
    GWY_DESERIALIZE_ERROR_SIZE_T,
    GWY_DESERIALIZE_ERROR_OBJECT,
    GWY_DESERIALIZE_ERROR_DATA,
    GWY_DESERIALIZE_ERROR_INVALID,
} GwyDeserializeError;

GQuark   gwy_deserialize_error_quark (void)                           G_GNUC_CONST;
gboolean gwy_serialize_gio           (GwySerializable *serializable,
                                      GOutputStream *output,
                                      GError **error);
GObject* gwy_deserialize_memory      (const guchar *buffer,
                                      gsize size,
                                      gsize *bytes_consumed,
                                      GwyErrorList **error_list)      G_GNUC_MALLOC;
gsize    gwy_deserialize_filter_items(GwySerializableItem *template_,
                                      gsize n_items,
                                      GwySerializableItems *items,
                                      GwySerializableItems *parent_items,
                                      const gchar *type_name,
                                      GwyErrorList **error_list);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
