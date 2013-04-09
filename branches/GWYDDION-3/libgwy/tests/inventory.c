/*
 *  $Id$
 *  Copyright (C) 2009-2013 David Nečas (Yeti).
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
static gboolean sort_by_name;

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

static gint
item_compare_tricky(gconstpointer a,
                    gconstpointer b)
{
    if (sort_by_name)
        return item_compare(a, b);

    const GwyItemTest *itemtesta = (const GwyItemTest*)a;
    const GwyItemTest *itemtestb = (const GwyItemTest*)b;
    return itemtesta->value - itemtestb->value;
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

    gwy_inventory_updated(inventory, "Second");
    gwy_inventory_updated(inventory, "First");
    gwy_inventory_updated(inventory, "Second");
    g_assert_cmpuint(gwy_inventory_size(inventory), ==, 3);
    g_assert_cmphex(insert_log, ==, 0);
    // Inventory is sorted so "Second" comes after "Fixme".
    g_assert_cmphex(update_log, ==, 0x313);
    g_assert_cmphex(delete_log, ==, 0);
    update_log = 0;

    gwy_inventory_nth_updated(inventory, 0);
    gwy_inventory_nth_updated(inventory, 1);
    gwy_inventory_nth_updated(inventory, 2);
    g_assert_cmpuint(gwy_inventory_size(inventory), ==, 3);
    g_assert_cmpuint(gwy_listable_size(GWY_LISTABLE(inventory)), ==, 3);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0x123);
    g_assert_cmphex(delete_log, ==, 0);
    update_log = 0;

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

    for (guint i = 0; i < 3; i++) {
        g_assert(gwy_inventory_get_nth(inventory, i)
                 == gwy_listable_get(GWY_LISTABLE(inventory), i));
    }

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

static void
check_reordering(G_GNUC_UNUSED GwyInventory *inventory,
                 guint *new_order,
                 gboolean *expect_reorder)
{
    g_assert(*expect_reorder);
    // The mapping is new_order[newpos] = oldpos.
    g_assert_cmpuint(new_order[0], ==, 2);
    g_assert_cmpuint(new_order[1], ==, 3);
    g_assert_cmpuint(new_order[2], ==, 1);
    g_assert_cmpuint(new_order[3], ==, 5);
    g_assert_cmpuint(new_order[4], ==, 4);
    g_assert_cmpuint(new_order[5], ==, 0);
}

void
test_inventory_sorting(void)
{
    GwyInventoryItemType item_type = {
        0,
        NULL,
        item_get_name,
        item_is_modifiable,
        item_compare_tricky,
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
    gboolean expect_reorder = FALSE;
    item_destroy_count = 0;
    sort_by_name = TRUE;

    guint64 insert_log = 0, update_log = 0, delete_log = 0;
    g_signal_connect(inventory, "item-inserted",
                     G_CALLBACK(record_item_change), &insert_log);
    g_signal_connect(inventory, "item-updated",
                     G_CALLBACK(record_item_change), &update_log);
    g_signal_connect(inventory, "item-deleted",
                     G_CALLBACK(record_item_change), &delete_log);
    g_signal_connect_swapped(inventory, "items-reordered",
                             G_CALLBACK(check_reordering), &expect_reorder);

    gwy_inventory_insert(inventory, item_new("Bananna", 2));
    gwy_inventory_insert(inventory, item_new("Apple", 8));
    gwy_inventory_insert(inventory, item_new("Date", 4));
    gwy_inventory_insert(inventory, item_new("Citrus", 1));
    g_assert_cmpuint(gwy_inventory_size(inventory), ==, 4);
    g_assert_cmphex(insert_log, ==, 0x1133);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = 0;

    // Break the sorting order so that followig items are just appended.
    gwy_inventory_insert_nth(inventory, item_new("Turd", 0), 2);
    g_assert_cmpuint(gwy_inventory_size(inventory), ==, 5);
    g_assert_cmphex(insert_log, ==, 0x3);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = 0;

    gwy_inventory_insert(inventory, item_new("Bogus", 3));
    g_assert_cmpuint(gwy_inventory_size(inventory), ==, 6);
    g_assert_cmphex(insert_log, ==, 0x6);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = 0;

    g_assert_cmpuint(gwy_inventory_position(inventory, "Apple"), ==, 0);
    g_assert_cmpuint(gwy_inventory_position(inventory, "Bananna"), ==, 1);
    g_assert_cmpuint(gwy_inventory_position(inventory, "Turd"), ==, 2);
    g_assert_cmpuint(gwy_inventory_position(inventory, "Citrus"), ==, 3);
    g_assert_cmpuint(gwy_inventory_position(inventory, "Date"), ==, 4);
    g_assert_cmpuint(gwy_inventory_position(inventory, "Bogus"), ==, 5);

    sort_by_name = FALSE;
    expect_reorder = TRUE;
    gwy_inventory_restore_order(inventory);
    g_assert_cmpuint(gwy_inventory_size(inventory), ==, 6);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);

    g_assert_cmpuint(gwy_inventory_position(inventory, "Turd"), ==, 0);
    g_assert_cmpuint(gwy_inventory_position(inventory, "Citrus"), ==, 1);
    g_assert_cmpuint(gwy_inventory_position(inventory, "Bananna"), ==, 2);
    g_assert_cmpuint(gwy_inventory_position(inventory, "Bogus"), ==, 3);
    g_assert_cmpuint(gwy_inventory_position(inventory, "Date"), ==, 4);
    g_assert_cmpuint(gwy_inventory_position(inventory, "Apple"), ==, 5);

    expect_reorder = FALSE;
    g_object_unref(inventory);
    g_assert_cmpuint(item_destroy_count, ==, 6);
}

static gboolean
predicate1(G_GNUC_UNUSED guint n,
           G_GNUC_UNUSED gpointer item,
           G_GNUC_UNUSED gpointer user_data)
{
    return FALSE;
}

static gboolean
predicate2(G_GNUC_UNUSED guint n,
           G_GNUC_UNUSED gpointer item,
           G_GNUC_UNUSED gpointer user_data)
{
    return TRUE;
}

static gboolean
predicate3(guint n,
           G_GNUC_UNUSED gpointer item,
           G_GNUC_UNUSED gpointer user_data)
{
    return n % 2;
}

static gboolean
predicate4(G_GNUC_UNUSED guint n,
           gpointer item,
           G_GNUC_UNUSED gpointer user_data)
{
    GwyItemTest *itemtest = (GwyItemTest*)item;
    return strlen(itemtest->name) == (guint)itemtest->value;
}

static void
sum_values(G_GNUC_UNUSED guint n,
           gpointer item,
           gpointer user_data)
{
    GwyItemTest *itemtest = (GwyItemTest*)item;
    gint *psum = (gint*)user_data;
    *psum += itemtest->value;
}

void
test_inventory_functional(void)
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
    sort_by_name = TRUE;

    guint64 insert_log = 0, update_log = 0, delete_log = 0;
    g_signal_connect(inventory, "item-inserted",
                     G_CALLBACK(record_item_change), &insert_log);
    g_signal_connect(inventory, "item-updated",
                     G_CALLBACK(record_item_change), &update_log);
    g_signal_connect(inventory, "item-deleted",
                     G_CALLBACK(record_item_change), &delete_log);

    gwy_inventory_insert(inventory, item_new("Bananna", 2));
    gwy_inventory_insert(inventory, item_new("Apple", 8));
    gwy_inventory_insert(inventory, item_new("Date", 4));
    gwy_inventory_insert(inventory, item_new("Citrus", 1));
    g_assert_cmpuint(gwy_inventory_size(inventory), ==, 4);
    g_assert_cmphex(insert_log, ==, 0x1133);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = 0;

    GwyItemTest *itemtest = gwy_inventory_find(inventory, predicate1, NULL);
    g_assert(!itemtest);

    itemtest = gwy_inventory_find(inventory, predicate2, NULL);
    g_assert(itemtest);
    g_assert_cmpstr(itemtest->name, ==, "Apple");
    g_assert_cmpint(itemtest->value, ==, 8);

    itemtest = gwy_inventory_find(inventory, predicate3, NULL);
    g_assert(itemtest);
    g_assert_cmpstr(itemtest->name, ==, "Bananna");
    g_assert_cmpint(itemtest->value, ==, 2);

    itemtest = gwy_inventory_find(inventory, predicate4, NULL);
    g_assert(itemtest);
    g_assert_cmpstr(itemtest->name, ==, "Date");
    g_assert_cmpint(itemtest->value, ==, 4);

    gint sum = 0;
    gwy_inventory_foreach(inventory, sum_values, &sum);
    g_assert_cmpint(sum, ==, 15);

    g_object_unref(inventory);
    g_assert_cmpuint(item_destroy_count, ==, 4);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
