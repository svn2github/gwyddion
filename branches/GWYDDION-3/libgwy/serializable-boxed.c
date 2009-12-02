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

#include <string.h>
#include "libgwy/serializable-boxed.h"
#include "libgwy/libgwy-aliases.h"

#define serializable_boxed_index(a, i) \
    g_array_index((a), GwySerializableBoxedData, (i))

typedef struct {
    GType type;
    const GwySerializableBoxedInfo *info;
} GwySerializableBoxedData;

static GStaticMutex serializable_boxed_mutex = G_STATIC_MUTEX_INIT;
static GArray *serializable_boxed_data = NULL;

static gpointer
init_serializable_boxed(G_GNUC_UNUSED gpointer arg)
{
    g_type_init();
    serializable_boxed_data = g_array_new(FALSE, FALSE,
                                          sizeof(GwySerializableBoxedData));
    return NULL;
}

static const GwySerializableBoxedInfo*
find_serializable_boxed_info(GType type)
{
    g_return_val_if_fail(serializable_boxed_data, NULL);
    g_static_mutex_lock(&serializable_boxed_mutex);
    GArray *data = serializable_boxed_data;
    for (guint i = 0; i < data->len; i++) {
        if (serializable_boxed_index(data, i).type == type) {
            GwySerializableBoxedData retval = serializable_boxed_index(data, i);
            if (i) {
                /* The just looked-up type is likely to be looked-up again
                 * soon. */
                serializable_boxed_index(data, i) 
                    = serializable_boxed_index(data, 0);
                serializable_boxed_index(data, 0) = retval;
            }
            g_static_mutex_unlock(&serializable_boxed_mutex);
            return retval.info;
        }
    }
    return NULL;
}

gboolean
gwy_boxed_type_is_serializable(GType type)
{
    return find_serializable_boxed_info(type) != NULL;
}

void
gwy_serializable_boxed_register_static(GType type,
                                       const GwySerializableBoxedInfo *info)
{
    static GOnce serializable_boxed_initialized = G_ONCE_INIT;
    g_once(&serializable_boxed_initialized, init_serializable_boxed, NULL);

    g_return_if_fail(G_TYPE_IS_BOXED(type));
    g_return_if_fail(!G_TYPE_IS_ABSTRACT(type));    // That's GBoxed for you
    g_return_if_fail(info);
    g_return_if_fail(info->n_items);
    g_return_if_fail(info->itemize);
    g_return_if_fail(info->construct);
    g_return_if_fail(info->assign);

    g_static_mutex_lock(&serializable_boxed_mutex);
    GArray *data = serializable_boxed_data;
    for (guint i = 0; i < data->len; i++) {
        if (G_UNLIKELY(serializable_boxed_index(data, i).type == type)) {
            g_static_mutex_unlock(&serializable_boxed_mutex);
            g_warning("Type %s has been already registered as serializable "
                      "boxed.", g_type_name(type));
            return;
        }
    }
    GwySerializableBoxedData newitem = { type, info };
    g_array_append_val(serializable_boxed_data, newitem);
    g_static_mutex_unlock(&serializable_boxed_mutex);
}

void
gwy_serializable_boxed_assign(GType type,
                              gpointer destination,
                              gconstpointer source)
{
    const GwySerializableBoxedInfo *info = find_serializable_boxed_info(type);
    g_return_if_fail(info);
    info->assign(destination, source);
}

gsize
gwy_serializable_boxed_n_items(GType type)
{
    const GwySerializableBoxedInfo *info = find_serializable_boxed_info(type);
    g_return_val_if_fail(info, 0);
    return info->n_items + 1;
}

void
gwy_serializable_boxed_itemize(GType type,
                               gpointer boxed,
                               GwySerializableItems *items)
{
    g_return_if_fail(items->n_items < items->len);
    const GwySerializableBoxedInfo *info = find_serializable_boxed_info(type);
    g_return_if_fail(info);
    GwySerializableItem *item = items->items + items->n_items;
    items->n_items++;
    item->name = g_type_name(type);
    item->value.v_size = 0;
    item->ctype = GWY_SERIALIZABLE_HEADER;
    item->array_size = info->itemize(boxed, items);
}

gpointer
gwy_serializable_boxed_construct(GType type,
                                 GwySerializableItems *items,
                                 GwyErrorList **error_list)
{
    const GwySerializableBoxedInfo *info = find_serializable_boxed_info(type);
    /* This is a hard error, the caller must check the type beforehand. */
    g_return_val_if_fail(info, NULL);
    return info->construct(items, error_list);
}

#define __LIBGWY_SERIALIZABLE_BOXED_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: serializable-boxed
 * @title: serializable-boxed
 * @short_description: Making boxed types serializable
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
