/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GWY_HMARKER_BOX_H__
#define __GWY_HMARKER_BOX_H__

#include <libgwydgets/gwymarkerbox.h>

G_BEGIN_DECLS

#define GWY_TYPE_HMARKER_BOX            (gwy_hmarker_box_get_type())
#define GWY_HMARKER_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_HMARKER_BOX, GwyHMarkerBox))
#define GWY_HMARKER_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_HMARKER_BOX, GwyHMarkerBoxClass))
#define GWY_IS_HMARKER_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_HMARKER_BOX))
#define GWY_IS_HMARKER_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_HMARKER_BOX))
#define GWY_HMARKER_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_HMARKER_BOX, GwyHMarkerBoxClass))

typedef struct _GwyHMarkerBox      GwyHMarkerBox;
typedef struct _GwyHMarkerBoxClass GwyHMarkerBoxClass;

struct _GwyHMarkerBox {
    GwyMarkerBox mbox;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyHMarkerBoxClass {
    GwyMarkerBoxClass parent_class;

    void (*reserved1)(void);
    void (*reserved2)(void);
};

GType       gwy_hmarker_box_get_type(void) G_GNUC_CONST;
GtkWidget*  gwy_hmarker_box_new     (void);

G_END_DECLS

#endif /* __GWY_HMARKER_BOX_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
