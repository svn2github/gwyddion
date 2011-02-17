/*
 *  $Id$
 *  Copyright (C) 2010 David Nečas (Yeti).
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

/*< private_header >*/

#ifndef __LIBGWY_OBJECT_INTERNAL_H__
#define __LIBGWY_OBJECT_INTERNAL_H__

#include "libgwy/error-list.h"
#include "libgwy/serializable.h"

G_BEGIN_DECLS

#define STATICP \
    (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)

#define ASSIGN_UNITS(dest, src) \
    do { \
        if (src && dest) \
            gwy_unit_assign(dest, src); \
        else if (dest) \
            GWY_OBJECT_UNREF(dest); \
        else if (src) \
            dest = gwy_unit_duplicate(src); \
    } while (0)

G_GNUC_INTERNAL
gboolean _gwy_assign_string(gchar **p,
                            const gchar *q);

G_GNUC_INTERNAL
void _gwy_notify_properties(GObject *object,
                            const gchar **properties,
                            guint nproperties);

G_GNUC_INTERNAL
void _gwy_notify_properties_by_pspec(GObject *object,
                                     GParamSpec **pspecs,
                                     guint npspecs);

G_GNUC_INTERNAL
gboolean _gwy_check_object_component(const GwySerializableItem *item,
                                     gpointer object,
                                     GType component_type,
                                     GwyErrorList **error_list);

G_GNUC_INTERNAL
guint _gwy_itemize_chain_to_parent(GwySerializable *serializable,
                                   GType parent_type,
                                   GwySerializableInterface *parent_iface,
                                   GwySerializableItems *items,
                                   guint child_items);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
