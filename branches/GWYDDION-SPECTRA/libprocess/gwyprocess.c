/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwyddion.h>
#include "gwyprocess.h"
#include "gwyprocessinternal.h"

/**
 * gwy_process_type_init:
 *
 * Makes libgwyprocess types safe for deserialization and performs other
 * initialization.  You have to call this function before using objects
 * from libgwyprocess.
 *
 * Calls gwy_type_init() first to make sure libgwyddion is initialized.
 *
 * It is safe to call this function more than once, subsequent calls are no-op.
 **/
void
gwy_process_type_init(void)
{
    static gboolean types_initialized = FALSE;

    if (types_initialized)
        return;

    gwy_type_init();

    g_type_class_peek(GWY_TYPE_DATA_LINE);
    g_type_class_peek(GWY_TYPE_DATA_FIELD);
    g_type_class_peek(GWY_TYPE_CDLINE);
    g_type_class_peek(GWY_TYPE_SPECTRA);
    types_initialized = TRUE;

    _gwy_cdline_class_setup_presets();
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyprocess
 * @title: gwyprocess
 * @short_description: Base functions
 *
 * Gwyddion classes has to be initialized before they can be safely
 * deserialized. The function gwy_process_type_init() performs this
 * initialization.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

