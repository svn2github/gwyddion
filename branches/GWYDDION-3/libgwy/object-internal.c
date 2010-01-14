/*
 *  $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
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
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/serialize.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/object-internal.h"

void
_gwy_notify_properties(GObject *object,
                       const gchar **properties,
                       guint nproperties)
{
    if (!nproperties || !properties[0])
        return;
    if (nproperties == 1) {
        g_object_notify(object, properties[0]);
        return;
    }
    g_object_freeze_notify(object);
    for (guint i = 0; i < nproperties && properties[i]; i++)
        g_object_notify(object, properties[i]);
    g_object_thaw_notify(object);
}

// Pass the object instead of the type to avoid lookup of the object GType we
// need only in the case of error.
gboolean
_gwy_check_object_component(const GwySerializableItem *item,
                            gpointer object,
                            GType component_type,
                            GwyErrorList **error_list)
{
    if (G_UNLIKELY(item->value.v_object
                   && !G_TYPE_CHECK_INSTANCE_TYPE(item->value.v_object,
                                                  component_type))) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Component ‘%s’ of %s is of type %s "
                             "instead of %s."),
                           item->name,
                           G_OBJECT_TYPE_NAME(object),
                           G_OBJECT_TYPE_NAME(item->value.v_object),
                           g_type_name(component_type));
        return FALSE;
    }
    return TRUE;
}

#define __LIBGWY_OBJECT_C__
#include "libgwy/libgwy-aliases.c"

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
