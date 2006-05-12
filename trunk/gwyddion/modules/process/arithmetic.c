/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyexpr.h>
#include <libprocess/datafield.h>
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#define ARITH_RUN_MODES GWY_RUN_INTERACTIVE

enum {
    WIN_ARGS = 5
};

enum {
    ARITHMETIC_OK = 0,
    ARITHMETIC_DATA = 1,
    ARITHMETIC_EXPR = 2
};

typedef struct {
    GwyExpr *expr;
    gchar *expression;
    guint err;
    GwyDataWindow *win[WIN_ARGS];
    gchar *name[WIN_ARGS];
    guint pos[WIN_ARGS];
} ArithmeticArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *expression;
    GtkWidget *result;
    ArithmeticArgs *args;
    GtkWidget *win[WIN_ARGS];
} ArithmeticControls;

static gboolean     module_register           (void);
static void         arithmetic                (GwyContainer *data,
                                               GwyRunType run);
static void         arithmetic_load_args      (GwyContainer *settings,
                                               ArithmeticArgs *args);
static void         arithmetic_save_args      (GwyContainer *settings,
                                               ArithmeticArgs *args);
static gboolean     arithmetic_dialog         (ArithmeticArgs *args);
static void         arithmetic_data_cb        (GtkWidget *item,
                                               ArithmeticControls *controls);
static void         arithmetic_expr_cb        (GtkWidget *entry,
                                               ArithmeticControls *controls);
static void         arithmetic_maybe_preview  (ArithmeticControls *controls);
static const gchar* arithmetic_check          (ArithmeticArgs *args);
static void         arithmetic_do             (ArithmeticArgs *args);


static const gchar default_expression[] = "d1 - d2";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Simple arithmetic operations with two data fields "
       "(or a data field and a scalar)."),
    "Yeti <yeti@gwyddion.net>",
    "2.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("arithmetic",
                              (GwyProcessFunc)&arithmetic,
                              N_("/M_ultidata/_Arithmetic..."),
                              NULL,
                              ARITH_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Arithmetic operations on data"));

    return TRUE;
}

/* FIXME: we ignore the Container argument and use current data window */
void
arithmetic(GwyContainer *data, GwyRunType run)
{
    ArithmeticArgs args;
    guint i;
    GwyContainer *settings;

    g_return_if_fail(run & ARITH_RUN_MODES);
    settings = gwy_app_settings_get();
    for (i = 0; i < WIN_ARGS; i++)
        args.win[i] = gwy_app_data_window_get_current();
    arithmetic_load_args(settings, &args);
    args.expr = gwy_expr_new();

    g_assert(gwy_data_window_get_data(args.win[0]) == data);
    if (arithmetic_dialog(&args)) {
        arithmetic_do(&args);
    }
    arithmetic_save_args(settings, &args);
    gwy_expr_free(args.expr);
}

static gboolean
arithmetic_dialog(ArithmeticArgs *args)
{
    ArithmeticControls controls;
    GtkWidget *dialog, *table, *omenu, *entry, *label, *menu;
    guint i, row, response;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Arithmetic"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4 + WIN_ARGS, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(_("Operands:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    for (i = 0; i < WIN_ARGS; i++) {
        args->name[i] = g_strdup_printf("d_%d", i+1);
        label = gtk_label_new_with_mnemonic(args->name[i]);
        gwy_strkill(args->name[i], "_");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);

        omenu = gwy_option_menu_data_window(G_CALLBACK(arithmetic_data_cb),
                                            &controls, NULL,
                                            GTK_WIDGET(args->win[i]));
        menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
        g_object_set_data(G_OBJECT(menu), "index", GUINT_TO_POINTER(i));
        gtk_table_attach(GTK_TABLE(table), omenu, 1, 2, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
        controls.win[i] = omenu;

        row++;
    }
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gtk_label_new_with_mnemonic(_("_Expression:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.expression = entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), args->expression);
    gtk_table_attach(GTK_TABLE(table), entry, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(entry, "changed",
                     G_CALLBACK(arithmetic_expr_cb), &controls);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    row++;

    controls.result = label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    gtk_widget_grab_focus(controls.expression);
    gtk_widget_show_all(dialog);
    arithmetic_expr_cb(entry, &controls);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
arithmetic_data_cb(GtkWidget *item,
                   ArithmeticControls *controls)
{
    ArithmeticArgs *args;
    GtkWidget *menu;
    guint i;

    args = controls->args;
    menu = gtk_widget_get_parent(item);
    i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(menu), "index"));
    args->win[i] = g_object_get_data(G_OBJECT(item), "data-window");
    if (!(args->err & ARITHMETIC_EXPR))
        arithmetic_maybe_preview(controls);
}

static void
arithmetic_expr_cb(GtkWidget *entry,
                   ArithmeticControls *controls)
{
    ArithmeticArgs *args;
    guint nvars;
    gdouble v;
    gchar *s;

    args = controls->args;
    g_free(args->expression);
    args->expression = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
    args->err = ARITHMETIC_OK;

    if (gwy_expr_compile(args->expr, args->expression, NULL)) {
        nvars = gwy_expr_get_variables(args->expr, NULL);
        g_return_if_fail(nvars);
        if (nvars == 1) {
            v = gwy_expr_execute(args->expr, NULL);
            s = g_strdup_printf("%g", v);
            gtk_label_set_text(GTK_LABEL(controls->result), s);
            g_free(s);
            gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                              GTK_RESPONSE_OK, FALSE);
        }
        else {
            if (!gwy_expr_resolve_variables(args->expr, WIN_ARGS,
                                            (const gchar*const*)args->name,
                                            args->pos)) {
                arithmetic_maybe_preview(controls);
            }
            else {
                args->err = ARITHMETIC_EXPR;
                gtk_label_set_text(GTK_LABEL(controls->result), "");
                gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                                  GTK_RESPONSE_OK, FALSE);
            }
        }
    }
    else {
        args->err = ARITHMETIC_EXPR;
        gtk_label_set_text(GTK_LABEL(controls->result), "");
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                          GTK_RESPONSE_OK, FALSE);
    }
}

static void
arithmetic_maybe_preview(ArithmeticControls *controls)
{
    ArithmeticArgs *args;
    const gchar *message;

    args = controls->args;
    message = arithmetic_check(args);
    if (args->err) {
        gtk_label_set_text(GTK_LABEL(controls->result), message);
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                          GTK_RESPONSE_OK, FALSE);
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls->result), "");
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                          GTK_RESPONSE_OK, TRUE);
        /* TODO: preview */
    }
}

static const gchar*
arithmetic_check(ArithmeticArgs *args)
{
    guint first = 0, i;
    GwyContainer *data;
    GwyDataField *dfirst, *dfield;
    gdouble xreal1, xreal2, yreal1, yreal2;

    if (args->err & ARITHMETIC_EXPR)
        return NULL;

    for (i = 0; i < WIN_ARGS; i++) {
        if (args->pos[i]) {
            first = i;
            break;
        }
    }
    if (i == WIN_ARGS) {
        /* no variables */
        args->err &= ~ARITHMETIC_DATA;
        return NULL;
    }

    /* each window must match with first, this is transitive */
    data = gwy_data_window_get_data(args->win[first]);
    dfirst = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    for (i = first+1; i < WIN_ARGS; i++) {
        if (!args->pos[i])
            continue;

        data = gwy_data_window_get_data(args->win[i]);
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));

        if ((gwy_data_field_get_xres(dfirst)
             != gwy_data_field_get_xres(dfield))
            || (gwy_data_field_get_yres(dfirst)
                != gwy_data_field_get_yres(dfield))) {
            args->err |= ARITHMETIC_DATA;
            return _("Pixel dimensions differ");
        }
        xreal1 = gwy_data_field_get_xreal(dfirst);
        yreal1 = gwy_data_field_get_yreal(dfirst);
        xreal2 = gwy_data_field_get_xreal(dfield);
        yreal2 = gwy_data_field_get_yreal(dfield);
        if (fabs(log(xreal1/xreal2)) > 0.0001
            || fabs(log(yreal1/yreal2)) > 0.0001) {
            args->err |= ARITHMETIC_DATA;
            return _("Physical dimensions differ");
        }
    }

    args->err &= ~ARITHMETIC_DATA;
    return NULL;
}

static void
arithmetic_do(ArithmeticArgs *args)
{
    GwyContainer *data;
    GwyDataField *dfield, *result = NULL;
    /* We know the expression can't contain more variables than WIN_ARGS */
    const gdouble *d[WIN_ARGS + 1];
    gdouble *r = NULL;
    gboolean first = TRUE;
    guint n = 0, i;
    gint newid;

    g_return_if_fail(!args->err);

    d[0] = NULL;
    for (i = 0; i < WIN_ARGS; i++) {
        if (!args->pos[i])
            continue;

        data = gwy_data_window_get_data(args->win[i]);
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        d[args->pos[i]] = gwy_data_field_get_data_const(dfield);
        if (first) {
            first = FALSE;
            n = gwy_data_field_get_xres(dfield)*gwy_data_field_get_yres(dfield);
            result = gwy_data_field_new_alike(dfield, FALSE);
            r = gwy_data_field_get_data(result);
        }
    }
    g_return_if_fail(!first);

    gwy_expr_vector_execute(args->expr, n, d, r);

    /* FIXME FIXME */
    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
    newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
    g_object_unref(result);
    gwy_app_copy_data_items(data, data, 0, newid, GWY_DATA_ITEM_GRADIENT, 0);
}

static const gchar expression_key[] = "/module/arithmetic/expression";

static void
arithmetic_load_args(GwyContainer *settings,
                     ArithmeticArgs *args)
{
    const guchar *exprstr;

    exprstr = default_expression;
    gwy_container_gis_string_by_name(settings, expression_key, &exprstr);
    args->expression = g_strdup(exprstr);
}

static void
arithmetic_save_args(GwyContainer *settings,
                     ArithmeticArgs *args)
{
    gwy_container_set_string_by_name(settings, expression_key,
                                     args->expression);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

