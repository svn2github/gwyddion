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

static gboolean activate_on_unfocus(GtkWidget *widget);
static void     sync_sensitivity   (GtkWidget *master,
                                    GtkStateType state,
                                    GtkWidget *slave);
static void     disconnect_slave   (GtkWidget *slave,
                                    GtkWidget *master);
static void     disconnect_master  (GtkWidget *master,
                                    GtkWidget *slave);

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
 * gwy_widget_add_sensitivity_slave:
 * @master: Master widget which will determine sensitvity of @slave.
 * @slave: Slave widget which will follow sensitvity of @master.
 *
 * Make a widget's sensitivity follow the sensitivity of another widget.
 *
 * The sensitivity of @slave is set according to @master's effective
 * sensitivity (as returned by GTK_WIDGET_IS_SENSITIVE()), i.e. it does not
 * just synchronize GtkWidget:sensitive property.
 *
 * Obviously, it does not make sense for one slave widget to follow more than
 * one master widget.  So if @slave already has a master widget it will stop
 * following it and start following @master. The number of slave wigets that
 * can follow one master is unlimited.
 *
 * The connection between the widgets ceases when either widget is destroyed.
 * They can also be disconnected explicitly with
 * gwy_widget_remove_sensitivity_slave().
 **/
void
gwy_widget_add_sensitivity_slave(GtkWidget *master,
                                 GtkWidget *slave)
{
    g_object_set_data(G_OBJECT(slave), "gwy-follow-sensitivity-master", master);
    g_signal_connect(master, "state-changed",
                     G_CALLBACK(sync_sensitivity), slave);
    g_signal_connect(slave, "destroy",
                     G_CALLBACK(disconnect_slave), master);
    g_signal_connect(master, "destroy",
                     G_CALLBACK(disconnect_master), slave);
    gtk_widget_set_sensitive(slave, gtk_widget_get_sensitive(master));
}

/**
 * gwy_widget_remove_sensitivity_slave:
 * @master: Master widget which determines sensitvity of @slave.
 * @slave: Slave widget which follows sensitvity of @master.
 *
 * Stops a widget's sensitivity following the sensitivity of another widget.
 *
 * The slave widget is set to be sensitive.
 *
 * Widgets @slave and @master must have been connected with
 * gwy_widget_follow_sensitivity(); otherwise, the results are undefined.
 *
 **/
void
gwy_widget_remove_sensitivity_slave(GtkWidget *master,
                                    GtkWidget *slave)
{
    disconnect_master(master, slave);
    disconnect_slave(slave, master);
    gtk_widget_set_sensitive(slave, TRUE);
}

/**
 * gwy_widget_get_sensitivity_master:
 * @slave: Widget currently following the sensitivity of another widget.
 *
 * Gets the master widget the sensitivity of a widget follows.
 *
 * Returns: (allow-none) (transfer none):
 *          The master widget of @slave, or %NULL if its sensitivity does not
 *          follow any other widget.
 **/
GtkWidget*
gwy_widget_get_sensitivity_master(GtkWidget *slave)
{
    return (GtkWidget*)g_object_get_data(G_OBJECT(slave),
                                         "gwy-follow-sensitivity-master");
}

static void
sync_sensitivity(GtkWidget *master,
                 G_GNUC_UNUSED GtkStateType state,
                 GtkWidget *slave)
{
    gtk_widget_set_sensitive(slave, gtk_widget_get_sensitive(master));
}

static void
disconnect_slave(GtkWidget *slave,
                 GtkWidget *master)
{
    g_signal_handlers_disconnect_by_func(master, sync_sensitivity, slave);
    g_signal_handlers_disconnect_by_func(master, disconnect_master, slave);
    g_object_set_data(G_OBJECT(slave), "gwy-follow-sensitivity-master", NULL);
}

static void
disconnect_master(GtkWidget *master,
                  GtkWidget *slave)
{
    g_signal_handlers_disconnect_by_func(master, disconnect_slave, slave);
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
