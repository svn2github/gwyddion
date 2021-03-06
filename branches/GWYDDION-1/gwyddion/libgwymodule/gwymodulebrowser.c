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

#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>

#include "gwymodulebrowser.h"

static GtkWidget* gwy_module_browser_construct    (GtkWidget *parent);
static GtkWidget* gwy_module_browser_info_table   (GtkWidget *parent);
static void       attach_info_line                (GtkWidget *table,
                                                   gint row,
                                                   const gchar *name,
                                                   GtkWidget *parent,
                                                   const gchar *key);
static void       update_module_info_cb           (GtkWidget *tree,
                                                   GtkWidget *parent);
static gint       module_name_compare_cb          (const GwyModuleInfo *a,
                                                   const GwyModuleInfo *b);

enum {
    MODULE_NAME,
    MODULE_VERSION,
    MODULE_AUTHOR,
    MODULE_LAST
};

static GtkWidget* window = NULL;

/**
 * gwy_module_browser:
 *
 * Shows a simple module browser.
 **/
void
gwy_module_browser(void)
{
    GtkWidget *browser, *scroll, *paned, *info;

    if (window) {
        gtk_window_present(GTK_WINDOW(window));
        return;
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 480, 480);
    gtk_window_set_title(GTK_WINDOW(window), _("Module Browser"));
    gtk_window_set_wmclass(GTK_WINDOW(window), "browser_module",
                           g_get_application_name());
    paned = gtk_vpaned_new();
    gtk_container_add(GTK_CONTAINER(window), paned);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_paned_pack1(GTK_PANED(paned), scroll, TRUE, FALSE);
    browser = gwy_module_browser_construct(window);
    gtk_container_add(GTK_CONTAINER(scroll), browser);
    info = gwy_module_browser_info_table(window);
    gtk_paned_pack2(GTK_PANED(paned), info, FALSE, FALSE);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_widget_destroy), NULL);
    g_signal_connect_swapped(window, "destroy",
                             G_CALLBACK(g_nullify_pointer), &window);
    gtk_widget_show_all(window);
}

static GtkWidget*
gwy_module_browser_construct(GtkWidget *parent)
{
    static const struct {
        const gchar *title;
        const guint id;
    }
    columns[] = {
        { N_("Module"), MODULE_NAME },
        { N_("Version"), MODULE_VERSION },
        { N_("Author"), MODULE_AUTHOR },
    };

    GtkWidget *tree;
    GtkListStore *store;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GSList *l, *list = NULL;
    GtkTreeIter iter;
    gsize i;

    store = gtk_list_store_new(MODULE_LAST,
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_STRING);
    gwy_module_foreach(gwy_hash_table_to_slist_cb, &list);
    list = g_slist_sort(list, (GCompareFunc)module_name_compare_cb);
    for (l = list; l; l = g_slist_next(l)) {
        const GwyModuleInfo *mod_info = (const GwyModuleInfo*)l->data;

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           MODULE_NAME, mod_info->name,
                           MODULE_VERSION, mod_info->version,
                           MODULE_AUTHOR, mod_info->author,
                           -1);
    }
    g_slist_free(list);

    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
    g_object_unref(store);

    for (i = 0; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_(columns[i].title),
                                                          renderer,
                                                          "text",
                                                          columns[i].id,
                                                          NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    }

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

    g_signal_connect(G_OBJECT(tree), "cursor-changed",
                     G_CALLBACK(update_module_info_cb), parent);

    return tree;
}

static GtkWidget*
gwy_module_browser_info_table(GtkWidget *parent)
{
    GtkWidget *table, *align;
    gint i;

    table = gtk_table_new(7, 1, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 8);
    i = 0;
    attach_info_line(table, i++, _("Name-Version:"), parent, "name-version");
    attach_info_line(table, i++, _("File:"), parent, "file");
    attach_info_line(table, i++, _("Registered functions:"), parent, "funcs");
    attach_info_line(table, i++, _("Authors:"), parent, "author");
    attach_info_line(table, i++, _("Copyright:"), parent, "copy");
    attach_info_line(table, i++, _("Date:"), parent, "date");
    attach_info_line(table, i++, _("Description:"), parent, "desc");

    align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
    gtk_container_add(GTK_CONTAINER(align), table);

    return align;
}

static void
update_module_info_cb(GtkWidget *tree,
                      GtkWidget *parent)
{
    GtkLabel *label;
    GtkTreeModel *store;
    GtkTreeSelection *selection;
    const GwyModuleInfo *mod_info;
    GtkTreeIter iter;
    GSList *l;
    gchar *s, *name;
    gsize n;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    g_return_if_fail(selection);
    if (!gtk_tree_selection_get_selected(selection, &store, &iter))
        return;

    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, MODULE_NAME, &name, -1);
    mod_info = gwy_module_lookup(name);
    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "name-version"));
    s = g_strconcat(mod_info->name, "-", mod_info->version, NULL);
    gtk_label_set_text(label, s);
    g_free(s);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "file"));
    gtk_label_set_text(label, gwy_module_get_filename(name));

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "author"));
    gtk_label_set_text(label, mod_info->author);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "copy"));
    gtk_label_set_text(label, mod_info->copyright);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "date"));
    gtk_label_set_text(label, mod_info->date);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "desc"));
    gtk_label_set_text(label, _(mod_info->blurb));

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "funcs"));
    n = 0;
    for (l = gwy_module_get_functions(name); l; l = g_slist_next(l))
        n += strlen((gchar*)l->data) + 1;
    if (!n)
        gtk_label_set_text(label, "");
    else {
        gchar *p;

        p = s = g_new(gchar, n);
        for (l = gwy_module_get_functions(name); l; l = g_slist_next(l)) {
            p = g_stpcpy(p, (gchar*)l->data);
            *(p++) = '\n';
        }
        *(--p) = '\0';
        gtk_label_set_text(label, s);
        g_free(s);
    }

    g_free(name);
}

static void
attach_info_line(GtkWidget *table,
                 gint row,
                 const gchar *name,
                 GtkWidget *parent,
                 const gchar *key)
{
    GtkWidget *label;
    gboolean multiline;

    multiline = (strcmp(key, "desc") == 0) || (strcmp(key, "funcs") == 0);
    label = gtk_label_new(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, multiline ? 0.0 : 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, row, row+1);

    label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 2, row, row+1);
    if (multiline)
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

    g_object_set_data(G_OBJECT(parent), key, label);
}

static gint
module_name_compare_cb(const GwyModuleInfo *a,
                       const GwyModuleInfo *b)
{
    return strcmp(a->name, b->name);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
