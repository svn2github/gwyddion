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

#include "libgwy/math.h"
#include "libgwyui/widget-utils.h"

/**
 * gwy_scroll_wheel_delta:
 * @adjustment: Adjustment to get the scrolling delta for.
 * @event: A scrolling event.
 * @orientation: Preferred orientation of the scrolling.
 *
 * Calculates a suitable step for mouse wheel scrolling.
 *
 * This is a similar function to an _gtk_range_get_wheel_delta() which,
 * however, is not public.
 *
 * Returns: A suitable step value for mouse wheel.
 **/
gdouble
gwy_scroll_wheel_delta(GtkAdjustment *adjustment,
                       GdkEventScroll *event,
                       GtkOrientation orientation)
{
    g_return_val_if_fail(GTK_IS_ADJUSTMENT(adjustment), 0.0);
    g_return_val_if_fail(event, 0.0);

    gdouble page_size = gtk_adjustment_get_page_size(adjustment);
    gdouble page_increment = gtk_adjustment_get_page_increment(adjustment);
    gdouble scroll_unit = (page_size
                           ? cbrt(page_size*page_size)
                           : page_increment);
    gdouble dx, dy, delta;

    if (gdk_event_get_scroll_deltas((GdkEvent*)event, &dx, &dy)) {
      if (dx != 0 && orientation == GTK_ORIENTATION_HORIZONTAL)
          delta = dx*scroll_unit;
      else
          delta = dy*scroll_unit;
    }
    else {
      if (event->direction == GDK_SCROLL_UP
          || event->direction == GDK_SCROLL_LEFT)
          delta = -scroll_unit;
      else
          delta = scroll_unit;
    }

    return delta;
}

/**
 * SECTION: widget-utils
 * @section_id: libgwyui-widget-utils
 * @title: Widget utils
 * @short_description: Utility widget functions missing from Gtk+
 **/

/**
 * GWY_IMPLEMENT_TREE_MODEL:
 * @interface_init: The interface init function.
 *
 * Declares that a type implements #GtkTreeModel.
 *
 * This is a specialization of G_IMPLEMENT_INTERFACE() for
 * #GtkTreeModelIface.  It is intended to be used in last
 * G_DEFINE_TYPE_EXTENDED() argument:
 * |[
 * G_DEFINE_TYPE_EXTENDED
 *     (GwyFoo, gwy_foo, G_TYPE_OBJECT, 0,
 *      GWY_IMPLEMENT_TREE_MODEL(gwy_foo_tree_model_init));
 * ]|
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
