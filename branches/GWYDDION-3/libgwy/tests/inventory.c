/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

#include "testlibgwy.h"

/***************************************************************************
 *
 * Inventory
 *
 ***************************************************************************/

typedef struct {
    gchar *name;
    gint value;
} GwyItemTest;

static int item_destroy_count;

static GwyItemTest*
item_new(const gchar *name, gint value)
{
    GwyItemTest *itemtest = g_slice_new(GwyItemTest);
    itemtest->name = g_strdup(name);
    itemtest->value = value;
    return itemtest;
}

static const gchar*
item_get_name(gconstpointer item)
{
    const GwyItemTest *itemtest = (const GwyItemTest*)item;
    return itemtest->name;
}

static gboolean
item_is_modifiable(gconstpointer item)
{
    const GwyItemTest *itemtest = (const GwyItemTest*)item;
    return itemtest->value >= 0;
}

static gint
item_compare(gconstpointer a,
             gconstpointer b)
{
    const GwyItemTest *itemtesta = (const GwyItemTest*)a;
    const GwyItemTest *itemtestb = (const GwyItemTest*)b;
    return strcmp(itemtesta->name, itemtestb->name);
}

static void
item_rename(gpointer item,
            const gchar *newname)
{
    GwyItemTest *itemtest = (GwyItemTest*)item;
    g_free(itemtest->name);
    itemtest->name = g_strdup(newname);
}

static void
item_destroy(gpointer item)
{
    GwyItemTest *itemtest = (GwyItemTest*)item;
    g_free(itemtest->name);
    g_slice_free(GwyItemTest, item);
    item_destroy_count++;
}

static gpointer
item_copy(gconstpointer item)
{
    const GwyItemTest *itemtest = (const GwyItemTest*)item;
    GwyItemTest *copy = g_slice_dup(GwyItemTest, item);
    copy->name = g_strdup(itemtest->name);
    return copy;
}

void
test_inventory_data(void)
{
    GwyInventoryItemType item_type = {
        0,
        NULL,
        item_get_name,
        item_is_modifiable,
        item_compare,
        item_rename,
        item_destroy,
        item_copy,
        NULL,
        NULL,
        NULL,
    };

    GwyInventory *inventory = gwy_inventory_new();
    g_assert(GWY_IS_INVENTORY(inventory));
    gwy_inventory_set_item_type(inventory, &item_type);
    item_destroy_count = 0;

    guint64 insert_log = 0, update_log = 0, delete_log = 0;
    g_signal_connect(inventory, "item-inserted",
                     G_CALLBACK(record_item_change), &insert_log);
    g_signal_connect(inventory, "item-updated",
                     G_CALLBACK(record_item_change), &update_log);
    g_signal_connect(inventory, "item-deleted",
                     G_CALLBACK(record_item_change), &delete_log);

    gwy_inventory_insert(inventory, item_new("Fixme", -1));
    gwy_inventory_insert(inventory, item_new("Second", 2));
    gwy_inventory_insert(inventory, item_new("First", 1));
    g_assert_cmpuint(gwy_inventory_size(inventory), ==, 3);
    g_assert_cmphex(insert_log, ==, 0x121);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = 0;

    GwyItemTest *itemtest;
    g_assert((itemtest = gwy_inventory_get(inventory, "Fixme")));
    g_assert_cmpint(itemtest->value, ==, -1);
    g_assert((itemtest = gwy_inventory_get(inventory, "Second")));
    g_assert_cmpint(itemtest->value, ==, 2);
    g_assert((itemtest = gwy_inventory_get(inventory, "First")));
    g_assert_cmpint(itemtest->value, ==, 1);

    g_assert((itemtest = gwy_inventory_get_nth(inventory, 0)));
    g_assert_cmpstr(itemtest->name, ==, "First");
    g_assert((itemtest = gwy_inventory_get_nth(inventory, 1)));
    g_assert_cmpstr(itemtest->name, ==, "Fixme");
    g_assert((itemtest = gwy_inventory_get_nth(inventory, 2)));
    g_assert_cmpstr(itemtest->name, ==, "Second");

    gwy_inventory_forget_order(inventory);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);

    gwy_inventory_insert(inventory, item_new("Abel", 0));
    gwy_inventory_insert(inventory, item_new("Kain", 1));
    g_assert_cmphex(insert_log, ==, 0x45);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = 0;
    gwy_inventory_restore_order(inventory);
    g_assert_cmpuint(gwy_inventory_size(inventory), ==, 5);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);

    g_assert((itemtest = gwy_inventory_get_nth(inventory, 0)));
    g_assert_cmpstr(itemtest->name, ==, "Abel");
    g_assert((itemtest = gwy_inventory_get_nth(inventory, 3)));
    g_assert_cmpstr(itemtest->name, ==, "Kain");

    g_assert((itemtest = gwy_inventory_get(inventory, "Fixme")));
    itemtest->value = 3;
    gwy_inventory_rename(inventory, "Fixme", "Third");
    g_assert_cmphex(insert_log, ==, 0x0);
    g_assert_cmphex(update_log, ==, 0x5);
    g_assert_cmphex(delete_log, ==, 0x0);
    insert_log = update_log = delete_log = 0;

    g_assert((itemtest = gwy_inventory_get_nth(inventory, 4)));
    g_assert_cmpstr(itemtest->name, ==, "Third");

    gwy_inventory_delete_nth(inventory, 0);
    gwy_inventory_delete(inventory, "Kain");
    g_assert_cmpuint(gwy_inventory_size(inventory), ==, 3);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0x12);
    delete_log = 0;

    g_object_unref(inventory);
    g_assert_cmpuint(item_destroy_count, ==, 5);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
