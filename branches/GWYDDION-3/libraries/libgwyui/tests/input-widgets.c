/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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

#include <stdlib.h>
#include <libgwyui.h>

static void
attach_adjustment(GtkGrid *grid,
                  guint row,
                  GtkAdjustment *adj,
                  const gchar *name,
                  const gchar *units,
                  GFunc function,
                  gpointer user_data)
{
    GtkWidget *bar = gwy_adjust_bar_new();
    gwy_adjust_bar_set_adjustment(GWY_ADJUST_BAR(bar), adj);
    GtkWidget *namelabel = gtk_bin_get_child(GTK_BIN(bar));
    gtk_label_set_text_with_mnemonic(GTK_LABEL(namelabel), name);
    gtk_grid_attach(grid, bar, 0, row, 1, 1);

    GtkWidget *spin = gwy_spin_button_new(adj, 0.0, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(namelabel), spin);
    gtk_grid_attach(grid, spin, 1, row, 1, 1);

    GtkWidget *unitlabel = gtk_label_new(units);
    gtk_widget_set_halign(unitlabel, GTK_ALIGN_START);
    gtk_grid_attach(grid, unitlabel, 2, row, 1, 1);

    if (function) {
        function(bar, user_data);
        function(spin, user_data);
        function(unitlabel, user_data);
    }
}

static void
set_sensitive(gpointer widget, gpointer user_data)
{
    gtk_widget_set_sensitive(widget, GPOINTER_TO_INT(user_data));
}

static void
fix_adj_bar3(gpointer widget, G_GNUC_UNUSED gpointer user_data)
{
    if (GWY_IS_ADJUST_BAR(widget)) {
        gwy_adjust_bar_set_snap_to_ticks(GWY_ADJUST_BAR(widget), TRUE);
        gwy_adjust_bar_set_mapping(GWY_ADJUST_BAR(widget),
                                   GWY_SCALE_MAPPING_LINEAR);
    }

    if (GWY_IS_SPIN_BUTTON(widget))
        gwy_spin_button_set_digits(GWY_SPIN_BUTTON(widget), 0);
}

static GtkWidget*
create_widget_test(void)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Gwy3 Widget Test");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkGrid *grid = GTK_GRID(gtk_grid_new());
    gtk_grid_set_column_spacing(grid, 2);
    gtk_grid_set_row_spacing(grid, 2);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(grid));

    GwyAdjustment *adj1 = gwy_adjustment_new_set(10.0, 10.0,
                                                 0.0, 1000.0, 1.0, 10.0);
    attach_adjustment(grid, 0, GTK_ADJUSTMENT(adj1),
                      "_Value with a long name:", "µm",
                      NULL, NULL);

    GwyAdjustment *adj2 = gwy_adjustment_new_set(10.0, 100.0,
                                                 0.0, 1000.0, 1.0, 10.0);
    attach_adjustment(grid, 1, GTK_ADJUSTMENT(adj2),
                      "_Short value:", "px",
                      set_sensitive, GINT_TO_POINTER(FALSE));

    GwyAdjustment *adj3 = gwy_adjustment_new_set(0.0, 0.0,
                                                 0.0, 20.0, 1.0, 5.0);
    attach_adjustment(grid, 2, GTK_ADJUSTMENT(adj3),
                      "S_napped value:", "count",
                      fix_adj_bar3, NULL);

    GtkWidget *entry = gtk_entry_new();
    gtk_grid_attach(grid, entry, 1, 3, 2, 1);
    GtkWidget *label = gtk_label_new_with_mnemonic("_Entry:");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(grid, label, 0, 3, 1, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

    GwyChoiceOption options[] = {
        { NULL, "First", "First option", 1 },
        { NULL, "Second", "Yes, another option", 2 },
        { NULL, "Third", "You have so many choices!", 3 },
    };
    GwyChoice *choice = gwy_choice_new_with_options(options,
                                                    G_N_ELEMENTS(options));
    gwy_choice_set_active(choice, 2);
    GtkWidget *combo = gwy_choice_create_combo(choice);
    gtk_grid_attach(grid, combo, 1, 4, 2, 1);
    label = gtk_label_new_with_mnemonic("_Choice:");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(grid, label, 0, 4, 1, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);

    return window;
}

int
main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    GtkWidget *twindow = create_widget_test();
    gtk_widget_show_all(twindow);

    gtk_main();

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
