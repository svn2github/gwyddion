/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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

#ifndef __LIBGWY_RESOURCE_H__
#define __LIBGWY_RESOURCE_H__

#include <glib-object.h>
#include <libgwy/inventory.h>
#include <libgwy/error-list.h>

G_BEGIN_DECLS

#define GWY_RESOURCE_ERROR gwy_resource_error_quark()

typedef enum {
    GWY_RESOURCE_ERROR_HEADER = 1,
    GWY_RESOURCE_ERROR_TYPE,
    GWY_RESOURCE_ERROR_NAME,
    GWY_RESOURCE_ERROR_DUPLICIT,
    GWY_RESOURCE_ERROR_DATA,
} GwyResourceError;

typedef enum {
    GWY_RESOURCE_LINE_OK = 0,
    GWY_RESOURCE_LINE_EMPTY,
    GWY_RESOURCE_LINE_BAD_KEY,
    GWY_RESOURCE_LINE_BAD_UTF8,
    GWY_RESOURCE_LINE_BAD_NUMBER,
} GwyResourceLineType;

typedef enum {
    GWY_RESOURCE_MANAGEMENT_NONE = 0,
    GWY_RESOURCE_MANAGEMENT_MANUAL,
    GWY_RESOURCE_MANAGEMENT_MAIN,
} GwyResourceManagementType;

GQuark gwy_resource_error_quark(void);

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
    struct _GwyResourcePrivate *priv;
};

struct _GwyResourceClass {
    /*<private>*/
    GObjectClass g_object_class;
    struct _GwyResourceClassPrivate *priv;

    /*<public>*/  /* XXX: protected, in fact, but gtk-doc omits protected */
    /* Signals */
    void (*data_changed)(GwyResource *resource);

    /* Virtual table */
    void         (*setup_inventory)(GwyInventory *inventory);
    GwyResource* (*copy)   (GwyResource *resource);
    gchar*       (*dump)   (GwyResource *resource);
    gboolean     (*parse)  (GwyResource *resource,
                            gchar *text,
                            GError **error);

    /*< private >*/
    void (*reserved1)(void);
    void (*reserved2)(void);
};

GType                       gwy_resource_get_type                  (void)                                   G_GNUC_CONST;
const gchar*                gwy_resource_get_name                  (GwyResource *resource)                  G_GNUC_PURE;
void                        gwy_resource_set_name                  (GwyResource *resource,
                                                                    const gchar *name);
const gchar*                gwy_resource_get_filename              (GwyResource *resource)                  G_GNUC_PURE;
void                        gwy_resource_set_filename              (GwyResource *resource,
                                                                    const gchar *filename);
gboolean                    gwy_resource_is_modifiable             (GwyResource *resource)                  G_GNUC_PURE;
gboolean                    gwy_resource_is_managed                (GwyResource *resource)                  G_GNUC_PURE;
gboolean                    gwy_resource_get_is_preferred          (GwyResource *resource)                  G_GNUC_PURE;
void                        gwy_resource_set_is_preferred          (GwyResource *resource,
                                                                    gboolean is_preferred);
void                        gwy_resource_data_changed              (GwyResource *resource);
GwyResource*                gwy_resource_load                      (const gchar *filename,
                                                                    GType expected_type,
                                                                    gboolean modifiable,
                                                                    GError **error)                         G_GNUC_MALLOC;
gboolean                    gwy_resource_save                      (GwyResource *resource,
                                                                    GError **error);
void                        gwy_resource_class_register            (GwyResourceClass *klass,
                                                                    const gchar *name,
                                                                    const GwyInventoryItemType *item_type);
const gchar*                gwy_resource_type_get_name             (GType type)                             G_GNUC_PURE;
const GwyInventoryItemType* gwy_resource_type_get_item_type        (GType type)                             G_GNUC_PURE;
GwyInventory*               gwy_resource_type_get_inventory        (GType type)                             G_GNUC_PURE;
void                        gwy_resource_type_load                 (GType type);
void                        gwy_resource_type_load_directory       (GType type,
                                                                    const gchar *dirname,
                                                                    gboolean modifiable,
                                                                    GwyErrorList **error_list);
void                        gwy_resource_type_set_managed          (GType type,
                                                                    gboolean managed);
gchar*                      gwy_resource_type_get_managed_directory(GType type);
void                        gwy_resource_type_set_managed_directory(GType type,
                                                                    const gchar *dirname);
GwyResourceManagementType   gwy_resources_get_management_type      (void)                                   G_GNUC_PURE;
void                        gwy_resources_set_management_type      (GwyResourceManagementType type);
void                        gwy_resource_type_flush                (GType type);
void                        gwy_resources_flush                    (void);
void                        gwy_resources_lock                     (void);
void                        gwy_resources_unlock                   (void);
void                        gwy_resources_finalize                 (void);
GwyResourceLineType         gwy_resource_parse_param_line          (gchar *line,
                                                                    gchar **key,
                                                                    gchar **value);
GwyResourceLineType         gwy_resource_parse_data_line           (const gchar *line,
                                                                    guint ncolumns,
                                                                    gdouble *data);
gchar*                      gwy_resource_dump_data_line            (const gdouble *data,
                                                                    guint ncolumns)                         G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
