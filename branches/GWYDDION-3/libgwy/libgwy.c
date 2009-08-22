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

#include "libgwy/libgwy.h"
#include "libgwy/libgwy-aliases.h"

static gpointer init_types(G_GNUC_UNUSED gpointer arg);

/**
 * gwy_type_init:
 *
 * Forces registration of all serializable libgwy types.
 *
 * It calls g_type_init() first to make sure GLib object system is initialized.
 *
 * Calling this function is not necessary to use libgwy, except perhaps if you
 * implement custom deserializers.  It is safe to call this function more than
 * once, subsequent calls are no-op.  It is safe to call this function from
 * more threads if GLib thread support is initialized.
 **/
void
gwy_type_init(void)
{
    static GOnce types_initialized = G_ONCE_INIT;
    g_once(&types_initialized, init_types, NULL);
}

static gpointer
init_types(G_GNUC_UNUSED gpointer arg)
{
    g_type_init();

    g_type_class_peek(GWY_TYPE_SERIALIZABLE);
    g_type_class_peek(GWY_TYPE_UNIT);
    g_type_class_peek(GWY_TYPE_CONTAINER);
    g_type_class_peek(GWY_TYPE_INVENTORY);

    return NULL;
}

#define __GWY_LIBGWY_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: libgwy
 * @title: libgwy
 * @short_description: Library-level functions of libgwy
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
