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

#include "config.h"
#include <string.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtkcellrenderertext.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libgwydgets/gwycombobox.h>

static void     cell_translate_func              (GtkCellLayout *cell_layout,
                                                  GtkCellRenderer *renderer,
                                                  GtkTreeModel *tree_model,
                                                  GtkTreeIter *iter,
                                                  gpointer data);
static gboolean gwy_enum_combo_box_try_set_active(GtkComboBox *combo,
                                                  gint active);
static gboolean gwy_enum_combo_box_find_value    (gpointer key,
                                                  const GwyEnum *item,
                                                  gint *i);

/**
 * gwy_enum_combo_box_new:
 * @entries: An enum with choices.
 * @nentries: The number of items in @entries, may be -1 when @entries is
 *            terminated with %NULL enum name.
 * @callback: A callback called when a new choice is selected (may be %NULL).
 *            If you want to just update an integer, you can use
 *            gwy_enum_combo_box_update_int() here.
 * @cbdata: User data passed to the callback.
 * @active: The enum value to show as currently selected.  If it isn't equal to
 *          any @entries value, first item is selected.
 * @translate: Whether to apply translation function (gwy_sgettext()) to item
 *             names.
 *
 * Creates a combo box with choices from a enum.
 *
 * Returns: A newly created combo box as #GtkWidget.
 **/
GtkWidget*
gwy_enum_combo_box_new(const GwyEnum *entries,
                       gint nentries,
                       GCallback callback,
                       gpointer cbdata,
                       gint active,
                       gboolean translate)
{
    GtkWidget *combo;
    GwyInventory *inventory;
    GwyInventoryStore *store;
    GtkCellRenderer *renderer;
    GtkCellLayout *layout;
    GtkTreeModel *model;

    inventory = gwy_enum_inventory_new(entries, nentries);
    store = gwy_inventory_store_new(inventory);
    g_object_unref(inventory);
    model = GTK_TREE_MODEL(store);
    combo = gtk_combo_box_new_with_model(model);
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 1);
    g_object_unref(store);
    layout = GTK_CELL_LAYOUT(combo);

    g_assert(gwy_inventory_store_get_column_by_name(store, "name") == 1);
    g_assert(gwy_inventory_store_get_column_by_name(store, "value") == 2);

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(layout, renderer, FALSE);
    if (translate)
        gtk_cell_layout_set_cell_data_func(layout, renderer,
                                           cell_translate_func, &gwy_sgettext,
                                           NULL);
    else
        gtk_cell_layout_add_attribute(layout, renderer, "markup", 1);

    if (!gwy_enum_combo_box_try_set_active(GTK_COMBO_BOX(combo), active))
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    if (callback)
        g_signal_connect(combo, "changed", callback, cbdata);

    return combo;
}

/**
 * gwy_enum_combo_box_set_active:
 * @combo: A combo box which was created with gwy_enum_combo_box_new().
 * @active: The enum value to show as currently selected.
 *
 * Sets the active combo box item by corresponding enum value.
 **/
void
gwy_enum_combo_box_set_active(GtkComboBox *combo,
                              gint active)
{
    if (!gwy_enum_combo_box_try_set_active(combo, active))
        g_warning("Enum value not between inventory enums");
}

/**
 * gwy_enum_combo_box_get_active:
 * @combo: A combo box which was created with gwy_enum_combo_box_new().
 *
 * Gets the enum value corresponding to currently active combo box item.
 *
 * Returns: The selected enum value.
 **/
gint
gwy_enum_combo_box_get_active(GtkComboBox *combo)
{
    GwyInventoryStore *store;
    const GwyEnum *item;
    gint i;

    i = gtk_combo_box_get_active(combo);
    store = GWY_INVENTORY_STORE(gtk_combo_box_get_model(combo));
    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(store), -1);
    item = gwy_inventory_get_nth_item(gwy_inventory_store_get_inventory(store),
                                      i);
    g_return_val_if_fail(item, -1);

    return item->value;
}

/**
 * gwy_enum_combo_box_update_int:
 * @combo: A combo box which was created with gwy_enum_combo_box_new().
 * @integer: Pointer to an integer to update to selected enum value.
 *
 * Convenience callback keeping an integer synchronized with selected enum
 * combo box value.
 **/
void
gwy_enum_combo_box_update_int(GtkComboBox *combo,
                              gint *integer)
{
    GwyInventoryStore *store;
    const GwyEnum *item;
    gint i;

    i = gtk_combo_box_get_active(combo);
    store = GWY_INVENTORY_STORE(gtk_combo_box_get_model(combo));
    g_return_if_fail(GWY_IS_INVENTORY_STORE(store));
    item = gwy_inventory_get_nth_item(gwy_inventory_store_get_inventory(store),
                                      i);
    g_return_if_fail(item);
    *integer = item->value;
}

static gboolean
gwy_enum_combo_box_try_set_active(GtkComboBox *combo,
                                  gint active)
{
    GwyInventoryStore *store;

    store = GWY_INVENTORY_STORE(gtk_combo_box_get_model(combo));
    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(store), FALSE);
    if (!gwy_inventory_find(gwy_inventory_store_get_inventory(store),
                            (GHRFunc)&gwy_enum_combo_box_find_value,
                            &active))
        return FALSE;

    gtk_combo_box_set_active(combo, active);
    return TRUE;
}

/* Find an enum and translate its enum value to inventory position */
static gboolean
gwy_enum_combo_box_find_value(gpointer key,
                              const GwyEnum *item,
                              gint *i)
{
    if (item->value == *i) {
        *i = GPOINTER_TO_UINT(key);
        return TRUE;
    }
    return FALSE;
}

static void
cell_translate_func(G_GNUC_UNUSED GtkCellLayout *cell_layout,
                    GtkCellRenderer *renderer,
                    GtkTreeModel *tree_model,
                    GtkTreeIter *iter,
                    gpointer data)
{
    const gchar* (*method)(const gchar*) = data;
    const GwyEnum *enum_item;

    gtk_tree_model_get(tree_model, iter, 0, &enum_item, -1);
    g_object_set(renderer, "text", method(enum_item->name), NULL);
}

/**
 * gwy_combo_box_metric_unit_new:
 * @callback: A callback called when a menu item is activated (or %NULL for
 * @cbdata: User data passed to the callback.
 * @from: The exponent of 10 the menu should start at (a multiple of 3, will
 *        be rounded towards zero if isn't).
 * @to: The exponent of 10 the menu should end at (a multiple of 3, will be
 *      rounded towards zero if isn't).
 * @unit: The unit to be prefixed.
 * @active: The power of 10 to show as currently selected (a multiple of 3).
 *
 * Creates an enum combo box with SI power of 10 multiplies.
 *
 * The integer value is the power of 10.
 *
 * Returns: The newly created combo box as #GtkWidget.
 **/
GtkWidget*
gwy_combo_box_metric_unit_new(GCallback callback,
                              gpointer cbdata,
                              gint from,
                              gint to,
                              GwySIUnit *unit,
                              gint active)
{
    enum { min = -18, max = 18 };
    GtkWidget *combo;
    GwyEnum *entries;
    GwySIValueFormat *format = NULL;
    gint i, n;

    from = CLAMP(from/3, min, max);
    to = CLAMP(to/3, min, max);
    if (to < from)
        GWY_SWAP(gint, from, to);

    n = (to - from) + 1;
    entries = g_new(GwyEnum, n + 1);
    for (i = from; i <= to; i++) {
        format = gwy_si_unit_get_format_for_power10(unit,
                                                    GWY_SI_UNIT_FORMAT_MARKUP,
                                                    3*i, format);
        if (*format->units)
            entries[i - from].name = g_strdup(format->units);
        else
            entries[i - from].name = g_strdup("1");
        entries[i - from].value = 3*i;
    }
    entries[n].name = NULL;
    gwy_si_unit_value_format_free(format);

    combo = gwy_enum_combo_box_new(entries, n, callback, cbdata, active, FALSE);
    g_signal_connect_swapped(combo, "destroy",
                             G_CALLBACK(gwy_enum_freev), entries);

    return combo;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwycombobox
 * @title: gwycombobox
 * @short_description: Combo box constructors
 * @see_also: <link linkend="libgwydget-gwyradiobuttons">gwyradiobuttons</link>
 *            -- radio button constructors
 *
 * Combo boxes can be easily constructed from #GwyEnum's with
 * gwy_enum_combo_box_new().   Here's an example of construction of a combo
 * box with three items:
 * <informalexample><programlisting>
 * typedef enum {
 *     MY_ENUM_FOO, MY_ENUM_BAR, MY_ENUM_BAZ
 * } MyEnum;
 * <!-- Hello, gtk-doc! -->
 * static GwyEnum my_enum_fields[] = {
 *     { N_("Foo"), MY_ENUM_FOO },
 *     { N_("Bar"), MY_ENUM_BAR },
 *     { N_("Baz"), MY_ENUM_BAZ },
 * };
 * <!-- Hello, gtk-doc! -->
 * static void
 * menu_callback(GtkWidget *combo, gpointer cbdata)
 * {
 *     MyEnum value;
 *     <!-- Hello, gtk-doc! -->
 *     value = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
 *     ...
 * }
 * <!-- Hello, gtk-doc! -->
 * static void
 * function(void) {
 *     GtkWidget *combo;
 *     ...
 *     <!-- Hello, gtk-doc! -->
 *     combo = gwy_enum_combo_box_new(fields, G_N_ELEMENTS(fields),
 *                                    G_CALLBACK(menu_callback), NULL,
 *                                    MY_ENUM_FOO, TRUE);
 *     ...
 * }
 * </programlisting></informalexample>
 *
 * Many common Gwyddion enumerations have companion function returning
 * corresponding #GwyEnum, see for example
 * <link linkend="libgwyprocess-gwyprocessenums">gwyprocessenums</link>,
 * making combo box construction even easier.
 *
 * For example, a combo box with possible interpolation types can be
 * constructed:
 * <informalexample><programlisting>
 * combo = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
 *                                G_CALLBACK(menu_callback), NULL,
 *                                GWY_INTERPOLATION_BILINEAR, TRUE);
 * </programlisting></informalexample>
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
