/* @(#) $Id$ */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include "app.h"
#include "file.h"

static GtkWidget* gwy_data_arith_window_construct  (void);
static GtkWidget* gwy_data_arith_data_option_menu  (GtkWidget *entry,
                                                    GwyDataWindow **operand);
static void       gwy_data_arith_append_line       (GwyDataWindow *data_window,
                                                    GtkWidget *menu);
static void       gwy_data_arith_operation_cb      (GtkWidget *item);
static void       gwy_data_arith_data_cb           (GtkWidget *item);
static void       gwy_data_arith_entry_cb          (GtkWidget *entry,
                                                    gpointer data);
static gboolean   gwy_data_arith_do                (void);

typedef enum {
    GWY_ARITH_ADD,
    GWY_ARITH_SUBSTRACT,
    GWY_ARITH_MULTIPLY,
    GWY_ARITH_DIVIDE,
    GWY_ARITH_MINIMUM,
    GWY_ARITH_MAXIMUM,
    GWY_ARITH_LAST
} GwyArithOperation;

static const GwyOptionMenuEntry operations[] = {
    { "Add",       GWY_ARITH_ADD },
    { "Substract", GWY_ARITH_SUBSTRACT },
    { "Multiply",  GWY_ARITH_MULTIPLY },
    { "Divide",    GWY_ARITH_DIVIDE },
    { "Minimum",   GWY_ARITH_MINIMUM },
    { "Maximum",   GWY_ARITH_MAXIMUM },
};

static GtkWidget *arith_window = NULL;

static gdouble scalar1, scalar2;
static GwyArithOperation operation;
static GwyDataWindow *operand1, *operand2;

void
gwy_app_data_arith(void)
{
    gboolean ok = FALSE;

    if (!arith_window)
        arith_window = gwy_data_arith_window_construct();
    operand1 = operand2 = gwy_app_data_window_get_current();
    scalar1 = scalar2 = 0.0;
    operation = GWY_ARITH_ADD;
    gtk_window_present(GTK_WINDOW(arith_window));
    do {
        switch (gtk_dialog_run(GTK_DIALOG(arith_window))) {
            case GTK_RESPONSE_CLOSE:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            ok = TRUE;
            break;

            case GTK_RESPONSE_APPLY:
            ok = gwy_data_arith_do();
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
gwy_data_arith_window_construct(void)
{
    GtkWidget *dialog, *table, *omenu, *entry, *label;

    dialog = gtk_dialog_new_with_buttons(_("Data Arithmetic"),
                                         GTK_WINDOW(gwy_app_main_window),
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
    g_signal_connect(entry, "changed",
                     G_CALLBACK(gwy_data_arith_entry_cb), NULL);
    gtk_entry_set_max_length(GTK_ENTRY(entry), 16);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 16);
    gtk_widget_set_sensitive(entry, FALSE);
    g_object_set_data(G_OBJECT(entry), "scalar", &scalar1);

    omenu = gwy_data_arith_data_option_menu(entry, &operand1);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 0, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);

    /***** Operation *****/
    label = gtk_label_new_with_mnemonic(_("_Operation:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);

    omenu = gwy_option_menu_create(operations, G_N_ELEMENTS(operations),
                                   "operation",
                                   G_CALLBACK(gwy_data_arith_operation_cb),
                                   NULL,
                                   -1);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 1, 2);

    /***** Second operand *****/
    label = gtk_label_new_with_mnemonic(_("_Second operand:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);

    entry = gtk_entry_new();
    gtk_table_attach_defaults(GTK_TABLE(table), entry, 2, 3, 2, 3);
    g_signal_connect(entry, "changed",
                     G_CALLBACK(gwy_data_arith_entry_cb), NULL);
    gtk_entry_set_max_length(GTK_ENTRY(entry), 16);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 16);
    gtk_widget_set_sensitive(entry, FALSE);
    g_object_set_data(G_OBJECT(entry), "scalar", &scalar2);

    omenu = gwy_data_arith_data_option_menu(entry, &operand2);
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
    g_signal_connect(item, "activate",
                     G_CALLBACK(gwy_data_arith_data_cb), menu);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);

    return omenu;
}

static void
gwy_data_arith_append_line(GwyDataWindow *data_window,
                           GtkWidget *menu)
{
    GwyContainer *data;
    GtkWidget *data_view, *item;
    gchar *filename;

    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));

    if (gwy_container_contains_by_name(data, "/filename")) {
        const gchar *fnm = gwy_container_get_string_by_name(data, "/filename");

        filename = g_path_get_basename(fnm);
    }
    else {
        gint u = gwy_container_get_int32_by_name(data, "/filename/untitled");

        filename = g_strdup_printf(_("Untitled-%d"), u);
    }

    item = gtk_menu_item_new_with_label(filename);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_data(G_OBJECT(item), "data-window", data_window);
    g_signal_connect(item, "activate",
                     G_CALLBACK(gwy_data_arith_data_cb), menu);
}

static void
gwy_data_arith_operation_cb(GtkWidget *item)
{
    operation = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "operation"));
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
    /*gwy_debug("%s: <%s> <%s>", __FUNCTION__, s, end);*/
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
gwy_data_arith_do(void)
{
    GtkWidget *dialog, *data_window, *data_view;
    GwyContainer *data;
    GwyDataField *dfield, *dfield1, *dfield2;

    /***** scalar x scalar (silly) *****/
    if (!operand1 && !operand2) {
        gdouble value = 0.0;

        switch (operation) {
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
        data_view = gwy_data_window_get_data_view(operand1);
        data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
        data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        switch (operation) {
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
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window));

        return TRUE;
    }

    /***** scalar x datafield (always possible) *****/
    if (!operand1 && operand2) {
        data_view = gwy_data_window_get_data_view(operand2);
        data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
        data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        switch (operation) {
            case GWY_ARITH_ADD:
            gwy_data_field_add(dfield, scalar2);
            break;

            case GWY_ARITH_SUBSTRACT:
            gwy_data_field_invert(dfield, FALSE, FALSE, TRUE);
            gwy_data_field_add(dfield, scalar2);
            break;

            case GWY_ARITH_MULTIPLY:
            gwy_data_field_multiply(dfield, scalar2);
            break;

            case GWY_ARITH_DIVIDE:
            g_warning("Implement me!?");
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
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window));

        return TRUE;
    }

    /***** scalar x datafield (always possible) *****/
    if (operand1 && operand2) {
        data_view = gwy_data_window_get_data_view(operand2);
        data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
        dfield2 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                  "/0/data"));
        data_view = gwy_data_window_get_data_view(operand1);
        data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
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
        data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
        switch (operation) {
            case GWY_ARITH_ADD:
            g_warning("Implement me!?");
            break;

            case GWY_ARITH_SUBSTRACT:
            g_warning("Implement me!?");
            break;

            case GWY_ARITH_MULTIPLY:
            g_warning("Implement me!?");
            break;

            case GWY_ARITH_DIVIDE:
            g_warning("Implement me!?");
            break;

            case GWY_ARITH_MAXIMUM:
            g_warning("Implement me!?");
            break;

            case GWY_ARITH_MINIMUM:
            g_warning("Implement me!?");
            break;

            default:
            g_assert_not_reached();
            break;
        }
        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window));

        return TRUE;
    }

    g_assert_not_reached();
    return FALSE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

