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
 * Array
 *
 ***************************************************************************/

void
record_item_change(G_GNUC_UNUSED GObject *object,
                   guint pos,
                   guint64 *counter)
{
    *counter <<= 4;
    *counter |= pos + 1;
}

void
test_array_data(void)
{
    GwyRGBA init_colors[] = { { 1.0, 1.0, 1.0, 1.0 }, { 0.0, 0.0, 0.0, 0.0 } };
    GwyArray *array = gwy_array_new_with_data(sizeof(GwyRGBA), NULL,
                                              init_colors,
                                              G_N_ELEMENTS(init_colors));
    g_assert(GWY_IS_ARRAY(array));
    g_assert_cmpuint(gwy_array_size(array), ==, 2);
    guint64 insert_log = 0, update_log = 0, delete_log = 0;
    g_signal_connect(array, "item-inserted",
                     G_CALLBACK(record_item_change), &insert_log);
    g_signal_connect(array, "item-updated",
                     G_CALLBACK(record_item_change), &update_log);
    g_signal_connect(array, "item-deleted",
                     G_CALLBACK(record_item_change), &delete_log);

    // Single-item operations.
    GwyRGBA color1 = { 0.1, 0.1, 0.1, 0.1 };
    GwyRGBA color2 = { 0.2, 0.2, 0.2, 0.2 };
    GwyRGBA color3 = { 0.3, 0.3, 0.3, 0.3 };
    gwy_array_insert1(array, 1, &color1);
    gwy_array_insert1(array, 3, &color2);
    gwy_array_insert1(array, 0, &color3);
    g_assert_cmpuint(gwy_array_size(array), ==, 5);
    g_assert_cmphex(insert_log, ==, 0x241);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = 0;

    // Manually emitted signals.
    gwy_array_updated(array, 1);
    gwy_array_updated(array, 0);
    gwy_array_updated(array, 2);
    g_assert_cmphex(update_log, ==, 0x213);
    update_log = 0;

    // More single-item operations.
    GwyRGBA *color;
    color = gwy_array_get(array, 0);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color3, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 1);
    g_assert(color);
    g_assert_cmpint(memcmp(color, init_colors + 0, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 2);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color1, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 3);
    g_assert(color);
    g_assert_cmpint(memcmp(color, init_colors + 1, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 4);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color2, sizeof(GwyRGBA)), ==, 0);

    gwy_array_delete1(array, 2);
    gwy_array_delete1(array, 1);
    gwy_array_delete1(array, 1);
    g_assert_cmpuint(gwy_array_size(array), ==, 2);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0x322);
    delete_log = 0;

    color = gwy_array_get(array, 0);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color3, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 1);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color2, sizeof(GwyRGBA)), ==, 0);

    gwy_array_append1(array, &color1);
    gwy_array_replace1(array, 0, &color1);
    g_assert_cmpuint(gwy_array_size(array), ==, 3);
    g_assert_cmphex(insert_log, ==, 0x3);
    g_assert_cmphex(update_log, ==, 0x1);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = update_log = 0;

    color = gwy_array_get(array, 0);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color1, sizeof(GwyRGBA)), ==, 0);

    // Multi-item operations.
    GwyRGBA colors[] = { color1, color2, color3 };
    gwy_array_insert(array, 1, init_colors, 2);
    g_assert_cmpuint(gwy_array_size(array), ==, 5);
    g_assert_cmphex(insert_log, ==, 0x23);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = 0;

    color = gwy_array_get(array, 1);
    g_assert(color);
    g_assert_cmpint(memcmp(color, init_colors + 0, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 2);
    g_assert(color);
    g_assert_cmpint(memcmp(color, init_colors + 1, sizeof(GwyRGBA)), ==, 0);

    gwy_array_replace(array, 2, colors, 3);
    g_assert_cmpuint(gwy_array_size(array), ==, 5);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0x345);
    g_assert_cmphex(delete_log, ==, 0);
    update_log = 0;

    color = gwy_array_get(array, 2);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color1, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 3);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color2, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 4);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color3, sizeof(GwyRGBA)), ==, 0);

    gwy_array_delete(array, 1, 3);
    g_assert_cmpuint(gwy_array_size(array), ==, 2);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0x432);
    delete_log = 0;

    color = gwy_array_get(array, 1);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color3, sizeof(GwyRGBA)), ==, 0);

    gwy_array_set_data(array, colors, 3);
    g_assert_cmphex(insert_log, ==, 0x3);
    g_assert_cmphex(update_log, ==, 0x12);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = update_log = 0;

    color = gwy_array_get_data(array);
    g_assert(color);
    g_assert_cmpint(memcmp(color, colors, 3*sizeof(GwyRGBA)), ==, 0);

    gwy_array_set_data(array, init_colors, 2);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0x12);
    g_assert_cmphex(delete_log, ==, 0x3);
    update_log = delete_log = 0;

    color = gwy_array_get_data(array);
    g_assert(color);
    g_assert_cmpint(memcmp(color, init_colors, 2*sizeof(GwyRGBA)), ==, 0);

    g_object_unref(array);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
