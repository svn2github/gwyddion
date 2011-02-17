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

#ifndef __LIBGWY_SERIALIZABLE_BOXED_H__
#define __LIBGWY_SERIALIZABLE_BOXED_H__

#include <glib-object.h>
#include <libgwy/serializable.h>

G_BEGIN_DECLS

typedef struct {
    gsize                 size;
    gsize                 n_items;
    gsize                 (*itemize)  (gpointer boxed,
                                       GwySerializableItems *items);

    gpointer              (*construct)(GwySerializableItems *items,
                                       GwyErrorList **error_list);

    void                  (*assign)   (gpointer destination,
                                       gconstpointer source);
    gboolean              (*equal)    (gconstpointer a,
                                       gconstpointer b);
} GwySerializableBoxedInfo;

gboolean gwy_boxed_type_is_serializable  (GType type);
void     gwy_serializable_boxed_register_static(GType type,
                                                const GwySerializableBoxedInfo *info);
void     gwy_serializable_boxed_assign   (GType type,
                                          gpointer destination,
                                          gconstpointer source);
gboolean gwy_serializable_boxed_equal    (GType type,
                                          gconstpointer a,
                                          gconstpointer b);
gsize    gwy_serializable_boxed_n_items  (GType type);
void     gwy_serializable_boxed_itemize  (GType type,
                                          gpointer boxed,
                                          GwySerializableItems *items);
gpointer gwy_serializable_boxed_construct(GType type,
                                          GwySerializableItems *items,
                                          GwyErrorList **error_list);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
