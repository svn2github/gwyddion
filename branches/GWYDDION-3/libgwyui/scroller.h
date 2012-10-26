/*
 *  $Id$
 *  Copyright (C) 2012 David Nečas (Yeti).
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

#ifndef __LIBGWYUI_SCROLLER_H__
#define __LIBGWYUI_SCROLLER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GWY_TYPE_SCROLLER \
    (gwy_scroller_get_type())
#define GWY_SCROLLER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SCROLLER, GwyScroller))
#define GWY_SCROLLER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SCROLLER, GwyScrollerClass))
#define GWY_IS_SCROLLER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SCROLLER))
#define GWY_IS_SCROLLER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SCROLLER))
#define GWY_SCROLLER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SCROLLER, GwyScrollerClass))

typedef struct _GwyScroller      GwyScroller;
typedef struct _GwyScrollerClass GwyScrollerClass;

struct _GwyScroller {
    GtkBin bin;
    struct _GwyScrollerPrivate *priv;
};

struct _GwyScrollerClass {
    /*<private>*/
    GtkBinClass bin_class;
};

GType          gwy_scroller_get_type       (void)                        G_GNUC_CONST;
GtkWidget*     gwy_scroller_new            (void)                        G_GNUC_MALLOC;
void           gwy_scroller_set_hadjustment(GwyScroller *scroller,
                                            GtkAdjustment *adjustment);
GtkAdjustment* gwy_scroller_get_hadjustment(const GwyScroller *scroller) G_GNUC_PURE;
void           gwy_scroller_set_vadjustment(GwyScroller *scroller,
                                            GtkAdjustment *adjustment);
GtkAdjustment* gwy_scroller_get_vadjustment(const GwyScroller *scroller) G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
