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

static gboolean activate_on_unfocus          (GtkWidget *widget);
static void     sync_sensitivity             (GtkWidget *leader,
                                              GtkStateType state,
                                              GtkWidget *follower);
static void     disconnect_sensitivity_follower (GtkWidget *follower,
                                              GtkWidget *leader);
static void     disconnect_sensitivity_leader(GtkWidget *leader,
                                              GtkWidget *follower);

/**
 * gwy_scroll_wheel_delta:
 * @adjustment: Adjustment to get the scrolling delta for.
 * @event: A scrolling event.
 * @orientation: Preferred orientation of the scrolling.
 *
 * Calculates a suitable step for mouse wheel scrolling.
 *
 * This is a similar function to
 * <function>_gtk_range_get_wheel_delta<!-- -->()</function> which,
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
 * gwy_list_store_row_changed:
 * @store: A list store.
 * @iter: (allow-none):
 *        Tree model iterator in @store, or %NULL for none.
 * @path: (allow-none):
 *        Tree model path in @store, or %NULL for none.
 * @row: Row number in @store, or -1 for none.
 *
 * Emits GtkTreeModel::row-changed signal on a tree store using whatever row
 * identification is given.
 *
 * At least one of @iter, @path, @row must be set to identify the row to emit
 * "row-changed" on.  Usually you would provide exactly one of them.  The
 * remaining information necessary to call gtk_tree_model_row_changed() is
 * inferred from what is provided.
 *
 * The behaviour of this function is undefined for given, but inconsistent
 * @iter, @path, and @row.
 **/
void
gwy_list_store_row_changed(GtkListStore *store,
                           GtkTreeIter *iter,
                           GtkTreePath *path,
                           gint row)
{
    g_return_if_fail(GTK_IS_LIST_STORE(store));
    g_return_if_fail(iter || path || row >= 0);

    GtkTreeModel *model = GTK_TREE_MODEL(store);
    if (iter && path) {
        gtk_tree_model_row_changed(model, path, iter);
        return;
    }

    GtkTreeIter myiter;

    if (!iter && row >= 0) {
        gboolean iter_ok = gtk_tree_model_iter_nth_child(model, &myiter, NULL,
                                                         row);
        g_return_if_fail(iter_ok);
        iter = &myiter;
    }

    if (!iter) {
        gboolean iter_ok = gtk_tree_model_get_iter(model, &myiter, path);
        g_return_if_fail(iter_ok);
        iter = &myiter;
    }

    if (path) {
        gtk_tree_model_row_changed(model, path, iter);
        return;
    }

    path = gtk_tree_model_get_path(model, iter);
    gtk_tree_model_row_changed(model, path, iter);
    gtk_tree_path_free(path);
}

/**
 * gwy_widget_get_activate_on_unfocus:
 * @widget: A widget.
 *
 * Obtains the activate-on-unfocus state of a widget.
 *
 * Returns: %TRUE if signal "GtkWidget::activate" is emitted when focus leaves
 *          the widget.
 **/
gboolean
gwy_widget_get_activate_on_unfocus(GtkWidget *widget)
{
    g_return_val_if_fail(GTK_IS_WIDGET(widget), FALSE);
    return !!g_signal_handler_find(widget, G_SIGNAL_MATCH_FUNC,
                                   0, 0, NULL, activate_on_unfocus, NULL);
}

/**
 * gwy_widget_set_activate_on_unfocus:
 * @widget: A widget.
 * @activate: %TRUE to enable activate-on-unfocus, %FALSE disable it.
 *
 * Sets the activate-on-unfocus state of a widget.
 *
 * When it is enabled, signal "GtkWidget::activate" is emited whenever focus
 * leaves the widget.
 **/
void
gwy_widget_set_activate_on_unfocus(GtkWidget *widget,
                                   gboolean activate)
{
    g_return_if_fail(GTK_IS_WIDGET(widget));
    g_return_if_fail(GTK_WIDGET_GET_CLASS(widget)->activate_signal);
    gulong id = g_signal_handler_find(widget, G_SIGNAL_MATCH_FUNC,
                                      0, 0, NULL, activate_on_unfocus, NULL);
    if (id && !activate)
        g_signal_handler_disconnect(widget, id);
    if (!id && activate)
        g_signal_connect(widget, "focus-out-event",
                         G_CALLBACK(activate_on_unfocus), NULL);
}

static gboolean
activate_on_unfocus(GtkWidget *widget)
{
    gtk_widget_activate(widget);
    return FALSE;
}

/**
 * gwy_widget_add_sensitivity_follower:
 * @leader: Leader widget which will determine sensitvity of @follower.
 * @follower: Follower widget which will follow sensitvity of @leader.
 *
 * Make a widget's sensitivity follow the sensitivity of another widget.
 *
 * The sensitivity of @follower is set according to @leader's effective
 * sensitivity (as returned by GTK_WIDGET_IS_SENSITIVE()), i.e. it does not
 * just synchronize GtkWidget:sensitive property.
 *
 * Obviously, it does not make sense for one follower widget to follow more than
 * one leader widget.  So if @follower already has a leader widget it will stop
 * following it and start following @leader. The number of follower wigets that
 * can follow one leader is unlimited.
 *
 * The connection between the widgets ceases when either widget is destroyed.
 * They can also be disconnected explicitly with
 * gwy_widget_remove_sensitivity_follower().
 **/
void
gwy_widget_add_sensitivity_follower(GtkWidget *leader,
                                    GtkWidget *follower)
{
    GtkWidget *oldleader = g_object_get_data(G_OBJECT(follower),
                                             "gwy-follow-sensitivity-leader");
    if (oldleader) {
        disconnect_sensitivity_follower(follower, oldleader);
        disconnect_sensitivity_leader(oldleader, follower);
    }
    g_object_set_data(G_OBJECT(follower), "gwy-follow-sensitivity-leader",
                      leader);
    g_signal_connect(leader, "state-changed",
                     G_CALLBACK(sync_sensitivity), follower);
    g_signal_connect(follower, "destroy",
                     G_CALLBACK(disconnect_sensitivity_follower), leader);
    g_signal_connect(leader, "destroy",
                     G_CALLBACK(disconnect_sensitivity_leader), follower);
    gtk_widget_set_sensitive(follower, gtk_widget_get_sensitive(leader));
}

/**
 * gwy_widget_remove_sensitivity_follower:
 * @leader: Leader widget which determines sensitvity of @follower.
 * @follower: Follower widget which follows sensitvity of @leader.
 *
 * Stops a widget's sensitivity following the sensitivity of another widget.
 *
 * The follower widget is set to be sensitive.
 *
 * Widgets @follower and @leader must have been connected with
 * gwy_widget_follow_sensitivity(); otherwise, the results are undefined.
 *
 **/
void
gwy_widget_remove_sensitivity_follower(GtkWidget *leader,
                                       GtkWidget *follower)
{
    disconnect_sensitivity_leader(leader, follower);
    disconnect_sensitivity_follower(follower, leader);
    gtk_widget_set_sensitive(follower, TRUE);
}

/**
 * gwy_widget_get_sensitivity_leader:
 * @follower: Widget currently following the sensitivity of another widget.
 *
 * Gets the leader widget the sensitivity of a widget follows.
 *
 * Returns: (allow-none) (transfer none):
 *          The leader widget of @follower, or %NULL if its sensitivity does not
 *          follow any other widget.
 **/
GtkWidget*
gwy_widget_get_sensitivity_leader(GtkWidget *follower)
{
    return (GtkWidget*)g_object_get_data(G_OBJECT(follower),
                                         "gwy-follow-sensitivity-leader");
}

static void
sync_sensitivity(GtkWidget *leader,
                 G_GNUC_UNUSED GtkStateType state,
                 GtkWidget *follower)
{
    gtk_widget_set_sensitive(follower, gtk_widget_get_sensitive(leader));
}

static void
disconnect_sensitivity_follower(GtkWidget *follower,
                                GtkWidget *leader)
{
    g_signal_handlers_disconnect_by_func(leader, sync_sensitivity, follower);
    g_signal_handlers_disconnect_by_func(leader, disconnect_sensitivity_leader,
                                         follower);
    g_object_set_data(G_OBJECT(follower), "gwy-follow-sensitivity-leader",
                      NULL);
}

static void
disconnect_sensitivity_leader(GtkWidget *leader,
                              GtkWidget *follower)
{
    g_signal_handlers_disconnect_by_func(leader,
                                         disconnect_sensitivity_follower,
                                         follower);
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
