/*
 *  $Id$
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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
#include "libgwy/strfuncs.h"
#include "libgwy/serialize.h"
#include "libgwy/object-internal.h"

void
_gwy_assign_unit(GwyUnit **dest,
                 const GwyUnit *source)
{
    if (*dest && source)
        gwy_unit_assign(*dest, source);
    else if (*dest)
        gwy_unit_clear(*dest);
    else if (source)
        *dest = gwy_unit_duplicate(source);
}

void
_gwy_serialize_unit(GwyUnit *unit,
                    const GwySerializableItem *template,
                    GwySerializableItems *items,
                    guint *n)
{
    if (gwy_unit_is_empty(unit))
        return;

    g_return_if_fail(items->len - items->n > 0);
    g_warn_if_fail(template->ctype == GWY_SERIALIZABLE_OBJECT);
    GwySerializableItem it = *template;
    it.value.v_object = (GObject*)unit;
    items->items[items->n++] = it;
    gwy_serializable_itemize(GWY_SERIALIZABLE(unit), items);
    (*n)++;
}

void
_gwy_serialize_string(gchar *string,
                      const GwySerializableItem *template,
                      GwySerializableItems *items,
                      guint *n)
{
    if (!string || !*string)
        return;

    g_return_if_fail(items->len - items->n > 0);
    g_warn_if_fail(template->ctype == GWY_SERIALIZABLE_STRING);
    GwySerializableItem it = *template;
    it.value.v_string = string;
    items->items[items->n++] = it;
    (*n)++;
}

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

void
_gwy_notify_properties_by_pspec(GObject *object,
                                GParamSpec **pspecs,
                                guint npspecs)
{
    if (!npspecs || !pspecs[0])
        return;
    if (npspecs == 1) {
        g_object_notify_by_pspec(object, pspecs[0]);
        return;
    }
    g_object_freeze_notify(object);
    for (guint i = 0; i < npspecs && pspecs[i]; i++)
        g_object_notify_by_pspec(object, pspecs[i]);
    g_object_thaw_notify(object);
}

guint
_gwy_itemize_chain_to_parent(GwySerializable *serializable,
                             GType parent_type,
                             GwySerializableInterface *parent_iface,
                             GwySerializableItems *items,
                             guint child_items)
{
    g_return_val_if_fail(items->len - items->n, 0);

    GwySerializableItem *it = items->items + items->n;
    it->ctype = GWY_SERIALIZABLE_PARENT;
    it->name = g_type_name(parent_type);
    it->array_size = 0;
    it->value.v_type = parent_type;
    it++, items->n++;

    return child_items + 1 + parent_iface->itemize(serializable, items);
}

// Pass the object instead of the type to avoid lookup of the object GType we
// need only in the case of error.
gboolean
_gwy_check_object_component(const GwySerializableItem *item,
                            gpointer object,
                            GType component_type,
                            GwyErrorList **error_list)
{
    if (item->ctype == GWY_SERIALIZABLE_OBJECT) {
        if (G_UNLIKELY(item->value.v_object
                       && !G_TYPE_CHECK_INSTANCE_TYPE(item->value.v_object,
                                                      component_type))) {
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_INVALID,
                               // TRANSLATORS: Error message.
                               _("Component ‘%s’ of object %s is of type %s "
                                 "instead of %s."),
                               item->name,
                               G_OBJECT_TYPE_NAME(object),
                               G_OBJECT_TYPE_NAME(item->value.v_object),
                               g_type_name(component_type));
            return FALSE;
        }
        return TRUE;
    }
    if (item->ctype == GWY_SERIALIZABLE_OBJECT_ARRAY) {
        for (gsize i = 0; i < item->array_size; i++) {
            GObject *iobject = item->value.v_object_array[i];
            if (G_UNLIKELY(!G_TYPE_CHECK_INSTANCE_TYPE(iobject,
                                                       component_type))) {
                gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                                   GWY_DESERIALIZE_ERROR_INVALID,
                                   // TRANSLATORS: Error message.
                                   _("Object %lu in component ‘%s’ of object "
                                     "%s is of type %s instead of %s."),
                                   (gulong)i,
                                   item->name,
                                   G_OBJECT_TYPE_NAME(object),
                                   G_OBJECT_TYPE_NAME(iobject),
                                   g_type_name(component_type));
                return FALSE;
            }
        }
        return TRUE;
    }
    g_return_val_if_reached(FALSE);
}

gpointer
_gwy_hash_table_keys(GHashTable *table)
{
    gpointer *keys = g_new(gpointer, g_hash_table_size(table)+1), *key = keys;
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, table);
    while (g_hash_table_iter_next(&iter, key, NULL))
        key++;
    *key = NULL;
    return keys;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
