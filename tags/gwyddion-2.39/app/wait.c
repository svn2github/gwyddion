/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <gtk/gtk.h>
#include "wait.h"

static void gwy_app_wait_create_dialog (GtkWindow *window,
                                        const gchar *message);
static void gwy_app_wait_cancelled      (void);

static GtkWidget *dialog = NULL;
static GtkWidget *progress = NULL;
static GtkWidget *label = NULL;
static gchar *message_prefix = NULL;
static gboolean cancelled = FALSE;

/**
 * gwy_app_wait_start:
 * @window: A window.
 * @message: A message to show in the wait dialog.
 *
 * Starts waiting for a window @window, creating a dialog with a progress bar.
 *
 * Waiting is global, there can be only one at a time.
 **/
void
gwy_app_wait_start(GtkWindow *window,
                   const gchar *message)
{
    if (window && !GTK_IS_WINDOW(window))
        g_warning("Widget is not a window");

    if (dialog) {
        g_critical("Waiting is modal, cannot wait on more than one thing "
                   "at once.");
        return;
    }

    cancelled = FALSE;
    gwy_app_wait_create_dialog(window, message);
}

/**
 * gwy_app_wait_finish:
 *
 * Finishes waiting, closing the dialog.
 *
 * No function like gwy_app_wait_set_message() should be call after that.
 *
 * This function must be called even if user cancelled the operation.
 **/
void
gwy_app_wait_finish(void)
{
    if (cancelled) {
        cancelled = FALSE;
        return;
    }

    g_return_if_fail(dialog != NULL);
    gtk_widget_destroy(dialog);
    g_free(message_prefix);

    dialog = NULL;
    progress = NULL;
    label = NULL;
    message_prefix = NULL;
}

static void
gwy_app_wait_create_dialog(GtkWindow *window,
                           const gchar *message)
{
    dialog = gtk_dialog_new_with_buttons(_("Please wait"),
                                         window,
                                         GTK_DIALOG_DESTROY_WITH_PARENT
                                         | GTK_DIALOG_NO_SEPARATOR
                                         | GTK_DIALOG_MODAL,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
    if (!window) {
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    }

    label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_markup(GTK_LABEL(label), message);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label,
                       FALSE, FALSE, 4);

    progress = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 0.0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), progress,
                       FALSE, FALSE, 4);

    g_signal_connect(dialog, "response",
                     G_CALLBACK(gwy_app_wait_cancelled), NULL);

    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
    while (gtk_events_pending())
        gtk_main_iteration();
}

/**
 * gwy_app_wait_set_message:
 * @message: A mesage to show in the progress dialog.
 *
 * Sets the message shown on the progress dialog.
 *
 * See also gwy_app_wait_set_message_prefix() which makes this function more
 * usable directly as a callback.
 *
 * Returns: %TRUE if the operation can continue, %FALSE if user cancelled it
 *          meanwhile.
 **/
gboolean
gwy_app_wait_set_message(const gchar *message)
{
    g_return_val_if_fail(dialog, FALSE);

    while (gtk_events_pending())
        gtk_main_iteration();
    if (cancelled)
        return FALSE;

    g_return_val_if_fail(dialog, FALSE);
    if (message_prefix) {
        gchar *s = g_strconcat(message_prefix, message, NULL);
        gtk_label_set_markup(GTK_LABEL(label), s);
        g_free(s);
    }
    else
        gtk_label_set_markup(GTK_LABEL(label), message);

    while (gtk_events_pending())
        gtk_main_iteration();

    return !cancelled;
}

/**
 * gwy_app_wait_set_message_prefix:
 * @prefix: The prefix for new messages.
 *
 * Sets prefix for the messages shown in the progress dialog.
 *
 * The prefix will take effect in the next gwy_app_wait_set_message() call.
 *
 * Returns: %TRUE if the operation can continue, %FALSE if user cancelled it
 *          meanwhile.
 **/
gboolean
gwy_app_wait_set_message_prefix(const gchar *prefix)
{
    g_return_val_if_fail(dialog, FALSE);

    if (cancelled)
        return FALSE;

    g_return_val_if_fail(dialog, FALSE);
    g_free(message_prefix);
    message_prefix = g_strdup(prefix);

    while (gtk_events_pending())
        gtk_main_iteration();

    return !cancelled;
}

/**
 * gwy_app_wait_set_fraction:
 * @fraction: The progress of the operation, as a number from 0 to 1.
 *
 * Sets the amount of progress the progress bar on the dialog displays.
 *
 * Returns: %TRUE if the operation can continue, %FALSE if user cancelled it
 *          meanwhile.
 **/
gboolean
gwy_app_wait_set_fraction(gdouble fraction)
{
    gchar buf[8];

    g_return_val_if_fail(dialog, FALSE);

    while (gtk_events_pending())
        gtk_main_iteration();
    if (cancelled)
        return FALSE;

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), fraction);
    if (fraction < 0.0 || fraction > 1.0) {
        g_warning("Fraction outside [0, 1] range");
        fraction = CLAMP(fraction, 0.0, 1.0);
    }
    g_snprintf(buf, sizeof(buf), "%d %%", (gint)(100*fraction + 0.4));
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress), buf);

    while (gtk_events_pending())
        gtk_main_iteration();

    return !cancelled;
}

static void
gwy_app_wait_cancelled(void)
{
    gwy_app_wait_finish();
    cancelled = TRUE;
}

/**
 * gwy_app_wait_cursor_start:
 * @window: A window.
 *
 * Changes the cursor for a window to indicate work.
 *
 * This function lets the Gtk+ main loop to run.
 *
 * Since: 2.3
 **/
void
gwy_app_wait_cursor_start(GtkWindow *window)
{
    GdkDisplay *display;
    GdkCursor *wait_cursor;
    GdkWindow *wait_window;
    GtkWidget *widget;

    g_return_if_fail(GTK_IS_WINDOW(window));
    widget = GTK_WIDGET(window);

    if (!GTK_WIDGET_REALIZED(widget)) {
        g_warning("Window must be realized to change the cursor");
        return;
    }

    wait_window = widget->window;

    display = gtk_widget_get_display(widget);
    wait_cursor = gdk_cursor_new_for_display(display, GDK_WATCH);
    gdk_window_set_cursor(wait_window, wait_cursor);
    gdk_cursor_unref(wait_cursor);

    while (gtk_events_pending())
        gtk_main_iteration_do(FALSE);
}

/**
 * gwy_app_wait_cursor_finish:
 * @window: A window.
 *
 * Resets the cursor for a window.
 *
 * This function lets the Gtk+ main loop to run.
 *
 * If the window cursor was non-default before gwy_app_wait_cursor_start(),
 * it is not restored and has to be set manually.  This limitation is due to
 * the nonexistence of a method to obtain the current cursor.
 *
 * Since: 2.3
 **/
void
gwy_app_wait_cursor_finish(GtkWindow *window)
{
    GdkWindow *wait_window;
    GtkWidget *widget;

    g_return_if_fail(GTK_IS_WINDOW(window));
    widget = GTK_WIDGET(window);

    if (!GTK_WIDGET_REALIZED(widget)) {
        g_warning("Window must be realized to change the cursor");
        return;
    }

    wait_window = widget->window;

    gdk_window_set_cursor(wait_window, NULL);

    while (gtk_events_pending())
        gtk_main_iteration_do(FALSE);
}

/************************** Documentation ****************************/

/**
 * SECTION:wait
 * @title: wait
 * @short_description: Informing the world we are busy
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
