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

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include "app.h"
#include "file.h"
#include "settings.h"

#define THUMBNAIL_SIZE 16

typedef enum {
    GWY_ARITH_ADD,
    GWY_ARITH_SUBSTRACT,
    GWY_ARITH_MULTIPLY,
    GWY_ARITH_DIVIDE,
    GWY_ARITH_MINIMUM,
    GWY_ARITH_MAXIMUM,
    GWY_ARITH_LAST
} GwyArithOperation;

typedef struct {
    GwyArithOperation operation;
    gdouble scalar1;
    gdouble scalar2;
    GwyDataWindow *win1;
    GwyDataWindow *win2;
} GwyArithArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *operation;
    GtkWidget *scalar1;
    GtkWidget *scalar2;
    GtkWidget *win1;
    GtkWidget *win2;
} GwyArithControls;

static void       gwy_data_arith_load_args         (GwyContainer *settings,
                                                    GwyArithArgs *args);
static void       gwy_data_arith_save_args         (GwyContainer *settings,
                                                    GwyArithArgs *args);
static GtkWidget* gwy_data_arith_window_construct  (GwyArithArgs *args);
static GtkWidget* gwy_data_arith_data_option_menu  (GtkWidget *entry,
                                                    GwyDataWindow **operand);
static void       gwy_data_arith_append_line       (GwyDataWindow *data_window,
                                                    GtkWidget *menu);
static void       gwy_data_arith_menu_set_history  (GtkWidget *omenu,
                                                    gpointer current);
static void       gwy_data_arith_operation_cb      (GtkWidget *item,
                                                    GwyArithArgs *args);
static void       gwy_data_arith_data_cb           (GtkWidget *item);
static void       gwy_data_arith_entry_cb          (GtkWidget *entry,
                                                    gpointer data);
static gboolean   gwy_data_arith_do                (GwyArithArgs *args,
                                                    GtkWidget *arith_window);

static void       gwy_data_field_add2              (GwyDataField *dfield1,
                                                    GwyDataField *dfield2);
static void       gwy_data_field_subtract2         (GwyDataField *dfield1,
                                                    GwyDataField *dfield2);
static void       gwy_data_field_multiply2         (GwyDataField *dfield1,
                                                    GwyDataField *dfield2);
static void       gwy_data_field_divide2           (GwyDataField *dfield1,
                                                    GwyDataField *dfield2);
static void       gwy_data_field_minimum2          (GwyDataField *dfield1,
                                                    GwyDataField *dfield2);
static void       gwy_data_field_maximum2          (GwyDataField *dfield1,
                                                    GwyDataField *dfield2);

static const GwyEnum operations[] = {
    { "Add",       GWY_ARITH_ADD },
    { "Subtract",  GWY_ARITH_SUBSTRACT },
    { "Multiply",  GWY_ARITH_MULTIPLY },
    { "Divide",    GWY_ARITH_DIVIDE },
    { "Minimum",   GWY_ARITH_MINIMUM },
    { "Maximum",   GWY_ARITH_MAXIMUM },
};

static const GwyArithArgs gwy_data_arith_defaults = {
    GWY_ARITH_ADD, 0.0, 0.0, NULL, NULL
};

void
gwy_app_data_arith(void)
{
    static GwyArithArgs *args = NULL;
    static GtkWidget *arith_window = NULL;
    static gpointer win1 = NULL, win2 = NULL;
    GwyContainer *settings;
    gboolean ok = FALSE;

    if (!args) {
        args = g_new(GwyArithArgs, 1);
        *args = gwy_data_arith_defaults;
    }
    settings = gwy_app_settings_get();
    if (!arith_window) {
        args->win1 = win1 ? win1 : gwy_app_data_window_get_current();
        args->win2 = win2 ? win2 : gwy_app_data_window_get_current();

        /* this may set win1, win2 back to NULL is operands are to be scalars */
        gwy_data_arith_load_args(settings, args);
        arith_window = gwy_data_arith_window_construct(args);
    }
    gtk_window_present(GTK_WINDOW(arith_window));
    do {
        switch (gtk_dialog_run(GTK_DIALOG(arith_window))) {
            case GTK_RESPONSE_CLOSE:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            ok = TRUE;
            break;

            case GTK_RESPONSE_APPLY:
            ok = gwy_data_arith_do(args, arith_window);
            if (ok) {
                gwy_data_arith_save_args(settings, args);
                if (win1)
                    g_object_remove_weak_pointer(G_OBJECT(win1), &win1);
                win1 = args->win1;
                if (win1)
                    g_object_add_weak_pointer(G_OBJECT(win1), &win1);
                if (win2)
                    g_object_remove_weak_pointer(G_OBJECT(win2), &win2);
                win2 = args->win2;
                if (win2)
                    g_object_add_weak_pointer(G_OBJECT(args->win2), &win2);
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    gtk_widget_destroy(arith_window);
    arith_window = NULL;
}

static GtkWidget*
gwy_data_arith_window_construct(GwyArithArgs *args)
{
    GtkWidget *dialog, *table, *omenu, *entry, *label;
    gchar *text;

    dialog = gtk_dialog_new_with_buttons(_("Data Arithmetic"),
                                         GTK_WINDOW(gwy_app_main_window_get()),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         NULL);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 8);

    table = gtk_table_new(3, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);

    /***** First operand *****/
    label = gtk_label_new_with_mnemonic(_("_First operand:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

    entry = gtk_entry_new();
    gtk_table_attach_defaults(GTK_TABLE(table), entry, 2, 3, 0, 1);
    gtk_entry_set_max_length(GTK_ENTRY(entry), 16);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 16);
    text = g_strdup_printf("%g", args->scalar1);
    gtk_entry_set_text(GTK_ENTRY(entry), text);
    g_free(text);
    gtk_widget_set_sensitive(entry, args->win1 == NULL);
    g_object_set_data(G_OBJECT(entry), "scalar", &args->scalar1);
    g_signal_connect(entry, "changed",
                     G_CALLBACK(gwy_data_arith_entry_cb), NULL);

    omenu = gwy_data_arith_data_option_menu(entry, &args->win1);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 0, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);

    /***** Operation *****/
    label = gtk_label_new_with_mnemonic(_("_Operation:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);

    omenu = gwy_option_menu_create(operations, G_N_ELEMENTS(operations),
                                   "operation",
                                   G_CALLBACK(gwy_data_arith_operation_cb),
                                   args,
                                   args->operation);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 1, 2);

    /***** Second operand *****/
    label = gtk_label_new_with_mnemonic(_("_Second operand:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);

    entry = gtk_entry_new();
    gtk_table_attach_defaults(GTK_TABLE(table), entry, 2, 3, 2, 3);
    gtk_entry_set_max_length(GTK_ENTRY(entry), 16);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 16);
    gtk_widget_set_sensitive(entry, args->win2 == NULL);
    text = g_strdup_printf("%g", args->scalar2);
    gtk_entry_set_text(GTK_ENTRY(entry), text);
    g_free(text);
    g_object_set_data(G_OBJECT(entry), "scalar", &args->scalar2);
    g_signal_connect(entry, "changed",
                     G_CALLBACK(gwy_data_arith_entry_cb), NULL);

    omenu = gwy_data_arith_data_option_menu(entry, &args->win2);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 2, 3);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);

    gtk_widget_show_all(dialog);

    return dialog;
}

GtkWidget*
gwy_data_arith_data_option_menu(GtkWidget *entry,
                                GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu, *item;

    omenu = gtk_option_menu_new();
    menu = gtk_menu_new();
    g_object_set_data(G_OBJECT(menu), "entry", entry);
    g_object_set_data(G_OBJECT(menu), "operand", operand);
    gwy_app_data_window_foreach((GFunc)gwy_data_arith_append_line, menu);
    item = gtk_menu_item_new_with_label(_("(scalar)"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    gwy_data_arith_menu_set_history(omenu, *operand);
    g_signal_connect(item, "activate",
                     G_CALLBACK(gwy_data_arith_data_cb), menu);

    return omenu;
}

static void
gwy_data_arith_append_line(GwyDataWindow *data_window,
                           GtkWidget *menu)
{
    GtkWidget *item, *data_view, *image;
    GdkPixbuf *pixbuf;
    gchar *filename;

    data_view = gwy_data_window_get_data_view(data_window);
    filename = gwy_data_window_get_base_name(data_window);

    pixbuf = gwy_data_view_get_thumbnail(GWY_DATA_VIEW(data_view),
                                         THUMBNAIL_SIZE);
    image = gtk_image_new_from_pixbuf(pixbuf);
    gwy_object_unref(pixbuf);
    item = gtk_image_menu_item_new_with_label(filename);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_data(G_OBJECT(item), "data-window", data_window);
    g_signal_connect(item, "activate",
                     G_CALLBACK(gwy_data_arith_data_cb), menu);
    g_free(filename);
}

static void
gwy_data_arith_menu_set_history(GtkWidget *omenu,
                                gpointer current)
{
    GtkWidget *menu;
    GList *l;
    gpointer p;
    gint i;

    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    l = GTK_MENU_SHELL(menu)->children;
    i = 0;
    while (l) {
        p = g_object_get_data(G_OBJECT(l->data), "data-window");
        if (p == current) {
            gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), i);
            return;
        }
        l = g_list_next(l);
        i++;
    }
    g_warning("Cannot select data window %p", current);
}

static void
gwy_data_arith_operation_cb(GtkWidget *item, GwyArithArgs *args)
{
    args->operation
        = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "operation"));
}

static void
gwy_data_arith_data_cb(GtkWidget *item)
{
    GtkWidget *menu, *entry;
    gpointer p, *pp;

    menu = gtk_widget_get_parent(item);
    entry = GTK_WIDGET(g_object_get_data(G_OBJECT(menu), "entry"));

    p = g_object_get_data(G_OBJECT(item), "data-window");
    gtk_widget_set_sensitive(entry, p == NULL);
    pp = (gpointer*)g_object_get_data(G_OBJECT(menu), "operand");
    g_return_if_fail(pp);
    *pp = p;
}

static void
gwy_data_arith_entry_cb(GtkWidget *entry,
                        gpointer data)
{
    GtkEditable *editable;
    gint pos;
    gchar *s, *end;
    gdouble *scalar;

    scalar = (gdouble*)g_object_get_data(G_OBJECT(entry), "scalar");
    editable = GTK_EDITABLE(entry);
    /* validate whether it looks as a number of something like start of a
     * number */
    s = end = gtk_editable_get_chars(editable, 0, -1);
    for (pos = 0; s[pos]; pos++)
        s[pos] = g_ascii_tolower(s[pos]);
    *scalar = strtod(s, &end);
    if (*end == '-' && end == s)
        end++;
    else if (*end == 'e' && strchr(s, 'e') == end) {
        end++;
        if (*end == '-')
            end++;
    }
    /*gwy_debug("<%s> <%s>", s, end);*/
    if (!*end) {
        g_free(s);
        return;
    }

    g_signal_handlers_block_by_func(editable,
                                    G_CALLBACK(gwy_data_arith_entry_cb),
                                    data);
    gtk_editable_delete_text(editable, 0, -1);
    pos = 0;
    gtk_editable_insert_text(editable, s, end - s, &pos);
    g_signal_handlers_unblock_by_func(editable,
                                      G_CALLBACK(gwy_data_arith_entry_cb),
                                      data);
    g_free(s);
    g_signal_stop_emission_by_name(editable, "changed");
}

static gboolean
gwy_data_arith_do(GwyArithArgs *args,
                  GtkWidget *arith_window)
{
    GtkWidget *dialog, *data_window;
    GwyContainer *data;
    GwyDataField *dfield, *dfield1, *dfield2;
    GwyDataWindow *operand1, *operand2;
    gdouble scalar1, scalar2;

    operand1 = args->win1;
    operand2 = args->win2;
    scalar1 = args->scalar1;
    scalar2 = args->scalar2;

    /***** scalar x scalar (silly) *****/
    if (!operand1 && !operand2) {
        gdouble value = 0.0;

        switch (args->operation) {
            case GWY_ARITH_ADD:
            value = scalar1 + scalar2;
            break;

            case GWY_ARITH_SUBSTRACT:
            value = scalar1 - scalar2;
            break;

            case GWY_ARITH_MULTIPLY:
            value = scalar1 * scalar2;
            break;

            case GWY_ARITH_DIVIDE:
            value = scalar1/scalar2;
            break;

            case GWY_ARITH_MAXIMUM:
            value = MAX(scalar1, scalar2);
            break;

            case GWY_ARITH_MINIMUM:
            value = MIN(scalar1, scalar2);
            break;

            default:
            g_assert_not_reached();
            break;
        }
        dialog = gtk_message_dialog_new(GTK_WINDOW(arith_window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_INFO,
                                        GTK_BUTTONS_CLOSE,
                                        _("The result is %g, but no data "
                                          "to operate on were selected.\n"),
                                        value);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return FALSE;
    }

    /***** datafield x scalar (always possible) *****/
    if (operand1 && !operand2) {
        data = gwy_data_window_get_data(operand1);
        data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        switch (args->operation) {
            case GWY_ARITH_ADD:
            gwy_data_field_add(dfield, scalar2);
            break;

            case GWY_ARITH_SUBSTRACT:
            gwy_data_field_add(dfield, -scalar2);
            break;

            case GWY_ARITH_MULTIPLY:
            gwy_data_field_multiply(dfield, scalar2);
            break;

            case GWY_ARITH_DIVIDE:
            gwy_data_field_multiply(dfield, 1.0/scalar2);
            break;

            case GWY_ARITH_MAXIMUM:
            gwy_data_field_clamp(dfield, scalar2, G_MAXDOUBLE);
            break;

            case GWY_ARITH_MINIMUM:
            gwy_data_field_clamp(dfield, -G_MAXDOUBLE, scalar2);
            break;

            default:
            g_assert_not_reached();
            break;
        }
        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

        return TRUE;
    }

    /***** scalar x datafield (always possible) *****/
    if (!operand1 && operand2) {
        data = gwy_data_window_get_data(operand2);
        data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        switch (args->operation) {
            case GWY_ARITH_ADD:
            gwy_data_field_add(dfield, scalar1);
            break;

            case GWY_ARITH_SUBSTRACT:
            gwy_data_field_invert(dfield, FALSE, FALSE, TRUE);
            gwy_data_field_add(dfield, scalar1);
            break;

            case GWY_ARITH_MULTIPLY:
            gwy_data_field_multiply(dfield, scalar1);
            break;

            case GWY_ARITH_DIVIDE:
            g_warning("Implement me!?");
            break;

            case GWY_ARITH_MAXIMUM:
            gwy_data_field_clamp(dfield, scalar1, G_MAXDOUBLE);
            break;

            case GWY_ARITH_MINIMUM:
            gwy_data_field_clamp(dfield, -G_MAXDOUBLE, scalar1);
            break;

            default:
            g_assert_not_reached();
            break;
        }
        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

        return TRUE;
    }

    /***** scalar x datafield (always possible) *****/
    if (operand1 && operand2) {
        data = gwy_data_window_get_data(operand2);
        dfield2 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                  "/0/data"));
        data = gwy_data_window_get_data(operand1);
        dfield1 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                  "/0/data"));

        if ((gwy_data_field_get_xres(dfield1)
             != gwy_data_field_get_xres(dfield2))
            || (gwy_data_field_get_yres(dfield1)
                != gwy_data_field_get_yres(dfield2))) {
            dialog = gtk_message_dialog_new(GTK_WINDOW(arith_window),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_INFO,
                                            GTK_BUTTONS_CLOSE,
                                            _("The dimensions differ.\n"));
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            return FALSE;
        }
        if ((gwy_data_field_get_xreal(dfield1)
             != gwy_data_field_get_xreal(dfield2))
            || (gwy_data_field_get_yreal(dfield1)
                != gwy_data_field_get_yreal(dfield2))) {
            dialog = gtk_message_dialog_new(GTK_WINDOW(arith_window),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_INFO,
                                            GTK_BUTTONS_CLOSE,
                                            _("The real dimensions differ.\n"));
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            return FALSE;
        }
        data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        switch (args->operation) {
            case GWY_ARITH_ADD:
            gwy_data_field_add2(dfield, dfield2);
            break;

            case GWY_ARITH_SUBSTRACT:
            gwy_data_field_subtract2(dfield, dfield2);
            break;

            case GWY_ARITH_MULTIPLY:
            gwy_data_field_multiply2(dfield, dfield2);
            break;

            case GWY_ARITH_DIVIDE:
            gwy_data_field_divide2(dfield, dfield2);
            break;

            case GWY_ARITH_MAXIMUM:
            gwy_data_field_maximum2(dfield, dfield2);
            break;

            case GWY_ARITH_MINIMUM:
            gwy_data_field_minimum2(dfield, dfield2);
            break;

            default:
            g_assert_not_reached();
            break;
        }
        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

        return TRUE;
    }

    g_assert_not_reached();
    return FALSE;
}

/************************ Datafield arithmetic ***************************/
/* XXX: move to libprocess/datafield.c? */

static void
gwy_data_field_add2(GwyDataField *dfield1,
                    GwyDataField *dfield2)
{
    gdouble *p, *q;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield1));
    g_return_if_fail(GWY_IS_DATA_FIELD(dfield2));
    xres = gwy_data_field_get_xres(dfield1);
    yres = gwy_data_field_get_yres(dfield1);
    g_return_if_fail(xres == gwy_data_field_get_xres(dfield2));
    g_return_if_fail(yres == gwy_data_field_get_yres(dfield2));

    p = dfield1->data;
    q = dfield2->data;
    for (i = xres*yres; i; i--, p++, q++)
        *p += *q;
}

static void
gwy_data_field_subtract2(GwyDataField *dfield1,
                         GwyDataField *dfield2)
{
    gdouble *p, *q;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield1));
    g_return_if_fail(GWY_IS_DATA_FIELD(dfield2));
    xres = gwy_data_field_get_xres(dfield1);
    yres = gwy_data_field_get_yres(dfield1);
    g_return_if_fail(xres == gwy_data_field_get_xres(dfield2));
    g_return_if_fail(yres == gwy_data_field_get_yres(dfield2));

    p = dfield1->data;
    q = dfield2->data;
    for (i = xres*yres; i; i--, p++, q++)
        *p -= *q;
}

static void
gwy_data_field_multiply2(GwyDataField *dfield1,
                         GwyDataField *dfield2)
{
    gdouble *p, *q;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield1));
    g_return_if_fail(GWY_IS_DATA_FIELD(dfield2));
    xres = gwy_data_field_get_xres(dfield1);
    yres = gwy_data_field_get_yres(dfield1);
    g_return_if_fail(xres == gwy_data_field_get_xres(dfield2));
    g_return_if_fail(yres == gwy_data_field_get_yres(dfield2));

    p = dfield1->data;
    q = dfield2->data;
    for (i = xres*yres; i; i--, p++, q++)
        *p *= *q;
}

static void
gwy_data_field_divide2(GwyDataField *dfield1,
                       GwyDataField *dfield2)
{
    gdouble *p, *q;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield1));
    g_return_if_fail(GWY_IS_DATA_FIELD(dfield2));
    xres = gwy_data_field_get_xres(dfield1);
    yres = gwy_data_field_get_yres(dfield1);
    g_return_if_fail(xres == gwy_data_field_get_xres(dfield2));
    g_return_if_fail(yres == gwy_data_field_get_yres(dfield2));

    p = dfield1->data;
    q = dfield2->data;
    for (i = xres*yres; i; i--, p++, q++)
        *p /= *q;
}

static void
gwy_data_field_minimum2(GwyDataField *dfield1,
                        GwyDataField *dfield2)
{
    gdouble *p, *q;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield1));
    g_return_if_fail(GWY_IS_DATA_FIELD(dfield2));
    xres = gwy_data_field_get_xres(dfield1);
    yres = gwy_data_field_get_yres(dfield1);
    g_return_if_fail(xres == gwy_data_field_get_xres(dfield2));
    g_return_if_fail(yres == gwy_data_field_get_yres(dfield2));

    p = dfield1->data;
    q = dfield2->data;
    for (i = xres*yres; i; i--, p++, q++)
        if (*p > *q)
            *p = *q;
}

static void
gwy_data_field_maximum2(GwyDataField *dfield1,
                        GwyDataField *dfield2)
{
    gdouble *p, *q;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield1));
    g_return_if_fail(GWY_IS_DATA_FIELD(dfield2));
    xres = gwy_data_field_get_xres(dfield1);
    yres = gwy_data_field_get_yres(dfield1);
    g_return_if_fail(xres == gwy_data_field_get_xres(dfield2));
    g_return_if_fail(yres == gwy_data_field_get_yres(dfield2));

    p = dfield1->data;
    q = dfield2->data;
    for (i = xres*yres; i; i--, p++, q++)
        if (*p < *q)
            *p = *q;
}

static const gchar *operation_key = "/app/arith/operation";
static const gchar *scalar1_key = "/app/arith/scalar1";
static const gchar *scalar2_key = "/app/arith/scalar2";
static const gchar *scalar_is1_key = "/app/arith/scalar_is1";
static const gchar *scalar_is2_key = "/app/arith/scalar_is2";

static void
gwy_data_arith_load_args(GwyContainer *settings,
                         GwyArithArgs *args)
{
    gboolean b;

    gwy_container_gis_enum_by_name(settings, operation_key, &args->operation);
    gwy_container_gis_double_by_name(settings, scalar1_key, &args->scalar1);
    gwy_container_gis_double_by_name(settings, scalar2_key, &args->scalar2);
    gwy_container_gis_boolean_by_name(settings, scalar_is1_key, &b);
    if (b)
        args->win1 = NULL;
    gwy_container_gis_boolean_by_name(settings, scalar_is2_key, &b);
    if (b)
        args->win2 = NULL;
}

static void
gwy_data_arith_save_args(GwyContainer *settings,
                         GwyArithArgs *args)
{
    gwy_container_set_enum_by_name(settings, operation_key, args->operation);
    gwy_container_set_double_by_name(settings, scalar1_key, args->scalar1);
    gwy_container_set_double_by_name(settings, scalar2_key, args->scalar2);
    gwy_container_set_boolean_by_name(settings, scalar_is1_key,
                                      args->win1 == NULL);
    gwy_container_set_boolean_by_name(settings, scalar_is2_key,
                                      args->win2 == NULL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

