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

#ifndef __GWY_RESOURCE_H__
#define __GWY_RESOURCE_H__

#include <glib-object.h>
#include <libgwy/inventory.h>
#include <libgwy/error-list.h>

G_BEGIN_DECLS

#define GWY_TYPE_RESOURCE \
    (gwy_resource_get_type())
#define GWY_RESOURCE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_RESOURCE, GwyResource))
#define GWY_RESOURCE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_RESOURCE, GwyResourceClass))
#define GWY_IS_RESOURCE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_RESOURCE))
#define GWY_IS_RESOURCE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_RESOURCE))
#define GWY_RESOURCE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_RESOURCE, GwyResourceClass))

typedef struct _GwyResource      GwyResource;
typedef struct _GwyResourceClass GwyResourceClass;

struct _GwyResource {
    GObject g_object;

    gint use_count;
    gchar *name;

    gboolean is_const : 1;
    gboolean is_modified : 1;
    gboolean is_preferred : 1;
    gboolean is_boolean1 : 1;
    gboolean is_boolean2 : 1;
    gint int1;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyResourceClass {
    /*< private >*/
    GObjectClass g_object_class;

    /*< public >*/
    GwyInventory *inventory;
    const gchar *name;

    /* Traits */
    GwyInventoryItemType item_type;

    /* Signals */
    void (*data_changed)(GwyResource *resource);

    /* Virtual table */
    void         (*use)    (GwyResource *resource);
    void         (*discard)(GwyResource *resource);
    void         (*dump)   (GwyResource *resource,
                            GString *string);
    GwyResource* (*parse)  (const gchar *text,
                            gboolean is_const);

    /*< private >*/
    void (*reserved1)(void);
    void (*reserved2)(void);
};

GType         gwy_resource_get_type           (void) G_GNUC_CONST;
const gchar*  gwy_resource_get_name           (GwyResource *resource);
gboolean      gwy_resource_is_modifiable      (GwyResource *resource);
gboolean      gwy_resource_get_is_preferred   (GwyResource *resource);
void          gwy_resource_set_is_preferred   (GwyResource *resource,
                                               gboolean is_preferred);
void          gwy_resource_use                (GwyResource *resource);
void          gwy_resource_discard            (GwyResource *resource);
gboolean      gwy_resource_is_used            (GwyResource *resource);
void          gwy_resource_data_changed       (GwyResource *resource);
void          gwy_resource_data_saved         (GwyResource *resource);
gchar*        gwy_resource_build_filename     (GwyResource *resource);
GString*      gwy_resource_dump               (GwyResource *resource);
GwyResource*  gwy_resource_parse              (const gchar *text,
                                               GType expected_type);
const gchar*  gwy_resource_class_get_name     (GwyResourceClass *klass);
const GwyInventoryItemType* gwy_resource_class_get_item_type(GwyResourceClass *klass);
GwyInventory* gwy_resource_class_get_inventory(GwyResourceClass *klass);
void          gwy_resource_class_load         (GwyResourceClass *klass);
void          gwy_resource_class_load_directory(GwyResourceClass *klass,
                                                const gchar *dirname,
                                                gboolean modifiable,
                                                GwyErrorList **error_list);
gboolean      gwy_resource_class_mkdir        (GwyResourceClass *klass);
void          gwy_resource_classes_finalize   (void);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
