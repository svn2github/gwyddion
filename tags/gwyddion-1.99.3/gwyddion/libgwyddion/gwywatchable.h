/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_WATCHABLE_H__
#define __GWY_WATCHABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#ifndef GWY_DISABLE_DEPRECATED

#define GWY_TYPE_WATCHABLE           (gwy_watchable_get_type())
#define GWY_WATCHABLE(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_WATCHABLE, GwyWatchable))
#define GWY_IS_WATCHABLE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_WATCHABLE))
#define GWY_WATCHABLE_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), GWY_TYPE_WATCHABLE, GwyWatchableIface))

typedef struct _GwyWatchableIface GwyWatchableIface;
typedef struct _GwyWatchable      GwyWatchable;       /* dummy */

struct _GwyWatchableIface {
    GTypeInterface parent_class;

    void (*value_changed)(GObject *watchable);
};

GType         gwy_watchable_get_type         (void) G_GNUC_CONST;
void          gwy_watchable_value_changed    (GObject *watchable);

#endif  /* GWY_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __GWY_WATCHABLE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
