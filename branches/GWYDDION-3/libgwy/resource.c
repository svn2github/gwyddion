/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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

/*
 * Resources are created by several means:
 * - instantiation
 * - loading
 * - deserialization
 * - copying using the inventory
 * - duplication
 * We create all as unmanaged.  This means we must catch when they enter the
 * inventory and make them managed, which is easy using the "item-inserted"
 * signal.
 *
 * As they become managed we must also ensure they are present on disk but we
 * do not want to save resources just loaded from disk.  So gwy_resource_load()
 * remembers in on_disk whether we the resource has an on-disk
 * representation.
 *
 * Then we only only have to save resources after modifications.  There are
 * several means of change notification:
 * - "item-updated" of the inventory
 * - "data-changed" of the resource
 * - "notify" of the resource
 * The first should catch all kinds of changes, however, only for managed
 * resources.  So to catch modifications of free-standing resources, that might
 * be loaded from disk but modified before being inserted to the inventory, we
 * watch all three.
 *
 * Removal is pretty simple.  A managed resource is removed from disk the
 * instant it is removed from the inventory and becomes free-standing; and its
 * on_disk flag is cleared.
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "libgwy/macros.h"
#include "libgwy/main.h"
#include "libgwy/strfuncs.h"
#include "libgwy/serialize.h"
#include "libgwy/resource.h"
#include "libgwy/object-internal.h"

#define MAGIC_HEADER2 "Gwyddion resource "
#define MAGIC_HEADER3 "Gwyddion3 resource "

enum { N_ITEMS = 1 };

enum {
    SGNL_DATA_CHANGED,
    N_SIGNALS
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_FILE_NAME,
    PROP_PREFERRED,
    PROP_MODIFIABLE,
    PROP_MODIFIED,
    PROP_MANAGED,
    N_PROPS
};

struct _GwyResourcePrivate {
    gchar *name;
    gchar *collation_key;
    GFile *file;

    gboolean modifiable : 1;
    gboolean modified : 1;
    gboolean preferred : 1;
    gboolean managed : 1;
    gboolean on_disk : 1;

    gdouble mtime;
};

struct _GwyResourceClassPrivate {
    GwyInventory *inventory;
    const gchar *name;
    const gchar *description;
    GFile *managed_directory;
    GwyInventoryItemType item_type;
    gulong item_inserted_id;

    gboolean managed;
};

typedef struct _GwyResourcePrivate      Resource;
typedef struct _GwyResourceClassPrivate ResourceClass;

static void               gwy_resource_class_init         (GwyResourceClass *klass);
static void               gwy_resource_class_intern_init  (gpointer klass);
static void               gwy_resource_class_base_init    (GwyResourceClass *klass);
static void               gwy_resource_class_base_finalize(GwyResourceClass *klass);
static void               gwy_resource_init               (GwyResource *resource);
static void               gwy_resource_dispose            (GObject *object);
static void               gwy_resource_finalize           (GObject *object);
static void               gwy_resource_serializable_init  (GwySerializableInterface *iface);
static gsize              gwy_resource_n_items            (GwySerializable *serializable);
static gsize              gwy_resource_itemize            (GwySerializable *serializable,
                                                           GwySerializableItems *items);
static gboolean           gwy_resource_construct          (GwySerializable *serializable,
                                                           GwySerializableItems *items,
                                                           GwyErrorList **error_list);
static void               gwy_resource_assign_impl        (GwySerializable *destination,
                                                           GwySerializable *source);
static void               gwy_resource_set_property       (GObject *object,
                                                           guint prop_id,
                                                           const GValue *value,
                                                           GParamSpec *pspec);
static void               gwy_resource_get_property       (GObject *object,
                                                           guint prop_id,
                                                           GValue *value,
                                                           GParamSpec *pspec);
static gboolean           gwy_resource_is_modifiable_impl (gconstpointer item);
static const gchar*       gwy_resource_get_item_name      (gconstpointer item);
static gboolean           gwy_resource_compare            (gconstpointer item1,
                                                           gconstpointer item2);
static void               gwy_resource_rename             (gpointer item,
                                                           const gchar *new_name);
static gpointer           gwy_resource_copy               (gconstpointer item);
static const GType*       gwy_resource_get_traits         (guint *ntraits);
static const gchar*       gwy_resource_get_trait_name     (guint i);
static void               gwy_resource_get_trait_value    (gconstpointer item,
                                                           guint i,
                                                           GValue *value);
static void               gwy_resource_delete             (gpointer item);
static void               set_is_managed                  (GwyResource *resource,
                                                           gboolean managed);
static void               inventory_item_inserted         (GwyInventory *inventory,
                                                           guint i,
                                                           GwyResourceClass *klass);
static GwyResourceClass*  get_resource_class              (const gchar *typename,
                                                           GType expected_type,
                                                           const gchar *filename_sys,
                                                           GError **error);

static void               err_filename                    (GError **error,
                                                           guint domain,
                                                           guint code,
                                                           gchar **filename_utf8,
                                                           const gchar *format,
                                                           ...) G_GNUC_PRINTF(5, 6);
static GwyResource*       parse                           (GwyStrLineIter *iter,
                                                           GType expected_type,
                                                           const gchar *filename,
                                                           GError **error);
static gboolean           name_is_unique                  (GwyResource *resource,
                                                           ResourceClass *klass,
                                                           GError **error);
static GFileOutputStream* output_stream_for_save          (GwyResource *resource,
                                                           GError **error);
static GString*           construct_filename              (const gchar *resource_name);
static void               gwy_resource_notify             (GObject *object,
                                                           GParamSpec *pspec);
static void               data_changed                    (GwyResource *resource);
static void               manage_create                   (GwyResource *resource);
static void               manage_delete                   (GwyResource *resource);
static void               manage_update                   (GwyResource *resource);
static void               manage_unqueue                  (GwyResource *resource);
static gdouble            get_timestamp                   (void);
static gboolean           manage_flush_timeout            (gpointer user_data);
static void               manage_flush                    (GType type,
                                                           gdouble older_than);
static gboolean           manage_flush_check_queue        (GType type,
                                                           gdouble older_than);

// Follow the convention of G_DEFINE_TYPE even though we could declare the
// parent class directly of the correct type.
static gpointer gwy_resource_parent_class = NULL;
static GObjectClass *parent_class = NULL;

/* Use a static propery -> trait map.  We could do it generically, too.
 * That g_param_spec_pool_list() is ugly and slow is the minor problem, the
 * major is that it does g_hash_table_foreach() so we would get different
 * orders on different invocations and have to sort it. */
static GType gwy_resource_trait_types[N_PROPS-1];
static const gchar *gwy_resource_trait_names[N_PROPS-1];
static guint gwy_resource_ntraits = 0;

static guint signals[N_SIGNALS];
static GParamSpec *properties[N_PROPS];

static GSList *resource_classes = NULL;
G_LOCK_DEFINE_STATIC(resource_classes);

static GwyResourceManagementType management_type = GWY_RESOURCE_MANAGEMENT_NONE;
static GList *update_queue = NULL;
G_LOCK_DEFINE_STATIC(update_queue);
static guint flush_source_id = 0;
G_LOCK_DEFINE_STATIC(flush);

static const GwyInventoryItemType resource_item_type = {
    0,
    "data-changed",
    &gwy_resource_get_item_name,
    &gwy_resource_is_modifiable_impl,
    &gwy_resource_compare,
    &gwy_resource_rename,
    &gwy_resource_delete,
    &gwy_resource_copy,
    &gwy_resource_get_traits,
    &gwy_resource_get_trait_name,
    &gwy_resource_get_trait_value,
};

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "name", .ctype = GWY_SERIALIZABLE_STRING, },
};

GType
gwy_resource_get_type(void)
{
    static volatile gsize type = 0;
    if (G_UNLIKELY(g_once_init_enter(&type))) {
        GTypeInfo info = {
            .class_size = sizeof(GwyResourceClass),
            .base_init = (GBaseInitFunc)&gwy_resource_class_base_init,
            .base_finalize = (GBaseFinalizeFunc)&gwy_resource_class_base_finalize,
            .class_init = (GClassInitFunc)gwy_resource_class_intern_init,
            .class_finalize = NULL,
            .class_data = NULL,
            .instance_size = sizeof(GwyResource),
            .n_preallocs = 0,
            .instance_init = (GInstanceInitFunc)gwy_resource_init,
            .value_table = NULL,
        };
        GType newtype = g_type_register_static(G_TYPE_OBJECT, "GwyResource",
                                               &info, G_TYPE_FLAG_ABSTRACT);
        GInterfaceInfo iface_info = {
            .interface_init = (GInterfaceInitFunc)gwy_resource_serializable_init,
            .interface_finalize = NULL,
            .interface_data = NULL,
        };
        g_type_add_interface_static(newtype, GWY_TYPE_SERIALIZABLE,
                                    &iface_info);
        g_type_add_class_private(newtype, sizeof(ResourceClass));
        g_once_init_leave(&type, newtype);
    }
    return type;
}

/**
 * gwy_resource_error_quark:
 *
 * Gets the error domain for resource operations.
 *
 * See and use %GWY_RESOURCE_ERROR.
 *
 * Returns: The error domain.
 **/
GQuark
gwy_resource_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain = g_quark_from_static_string("gwy-resource-error-quark");

    return error_domain;
}

static void
gwy_resource_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_resource_n_items;
    iface->itemize   = gwy_resource_itemize;
    iface->construct = gwy_resource_construct;
    iface->assign    = gwy_resource_assign_impl;
}

static void
gwy_resource_class_intern_init(gpointer klass)
{
    gwy_resource_parent_class = g_type_class_peek_parent(klass);
    gwy_resource_class_init((GwyResourceClass*)klass);
}

static void
gwy_resource_class_base_init(GwyResourceClass *klass)
{
    klass->priv = G_TYPE_CLASS_GET_PRIVATE(klass, GWY_TYPE_RESOURCE,
                                           ResourceClass);
    klass->priv->managed = TRUE;
    klass->priv->item_type = resource_item_type;
    klass->priv->item_type.type = G_TYPE_FROM_CLASS(klass);
}

static void
gwy_resource_class_base_finalize(GwyResourceClass *klass)
{
    g_printerr("BASE FINALIZE %s\n", g_type_name(G_TYPE_FROM_CLASS(klass)));
    GWY_OBJECT_UNREF(klass->priv->inventory);
    g_free(klass->priv);
}

static void
gwy_resource_class_init(GwyResourceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GParamSpec *pspec;

    g_type_class_add_private(klass, sizeof(Resource));

    parent_class = G_OBJECT_CLASS(gwy_resource_parent_class);

    gobject_class->dispose = gwy_resource_dispose;
    gobject_class->finalize = gwy_resource_finalize;
    gobject_class->get_property = gwy_resource_get_property;
    gobject_class->set_property = gwy_resource_set_property;
    gobject_class->notify = gwy_resource_notify;

    pspec = properties[PROP_NAME]
        = g_param_spec_string("name",
                              "Name",
                              "Resource name",
                              NULL, /* What is the default value good for? */
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
                              | G_PARAM_STATIC_STRINGS);
    gwy_resource_trait_types[gwy_resource_ntraits] = pspec->value_type;
    gwy_resource_trait_names[gwy_resource_ntraits] = pspec->name;
    gwy_resource_ntraits++;

    pspec = properties[PROP_FILE_NAME]
        = g_param_spec_string("file-name",
                              "File name",
                              "Name of file corresponding to the resource "
                              "in GLib encoding, it may be NULL for "
                              "built-in or newly created resourced.",
                              NULL, /* What is the default value good for? */
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    gwy_resource_trait_types[gwy_resource_ntraits] = pspec->value_type;
    gwy_resource_trait_names[gwy_resource_ntraits] = pspec->name;
    gwy_resource_ntraits++;

    pspec = properties[PROP_PREFERRED]
        = g_param_spec_boolean("preferred",
                               "Preferred",
                               "Whether a resource is preferred",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    gwy_resource_trait_types[gwy_resource_ntraits] = pspec->value_type;
    gwy_resource_trait_names[gwy_resource_ntraits] = pspec->name;
    gwy_resource_ntraits++;

    pspec = properties[PROP_MODIFIABLE]
        = g_param_spec_boolean("modifiable",
                               "Modifiable",
                               "Whether a resource is modifiable",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
                               | G_PARAM_STATIC_STRINGS);
    gwy_resource_trait_types[gwy_resource_ntraits] = pspec->value_type;
    gwy_resource_trait_names[gwy_resource_ntraits] = pspec->name;
    gwy_resource_ntraits++;

    properties[PROP_MODIFIED]
        = g_param_spec_boolean("modified",
                               "Modified",
                               "Whether a resource was modified, this is "
                               "set when data-changed signal is emitted",
                               FALSE,
                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_MANAGED]
        = g_param_spec_boolean("managed",
                               "Managed",
                               "Whether a resource is managed by the class "
                               "inventory.  False for free-standing "
                               "resources.",
                               FALSE,
                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    /**
     * GwyResource::data-changed:
     * @gwyresource: The #GwyResource which received the signal.
     *
     * The ::data-changed signal is emitted when resource data changes.
     */
    signals[SGNL_DATA_CHANGED]
        = g_signal_new_class_handler("data-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     G_CALLBACK(data_changed),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_resource_init(GwyResource *resource)
{
    resource->priv = G_TYPE_INSTANCE_GET_PRIVATE(resource, GWY_TYPE_RESOURCE,
                                                 Resource);
}

static void
gwy_resource_dispose(GObject *object)
{
    GwyResource *resource = (GwyResource*)object;
    Resource *priv = resource->priv;

    GWY_OBJECT_UNREF(priv->file);

    if (parent_class->dispose)
        parent_class->dispose(object);
}

static void
gwy_resource_finalize(GObject *object)
{
    GwyResource *resource = (GwyResource*)object;
    Resource *priv = resource->priv;

    GWY_FREE(priv->name);
    GWY_FREE(priv->collation_key);

    if (parent_class->finalize)
        parent_class->finalize(object);
}

static gsize
gwy_resource_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return N_ITEMS;
}

static gsize
gwy_resource_itemize(GwySerializable *serializable,
                     GwySerializableItems *items)
{
    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    GwyResource *resource = GWY_RESOURCE(serializable);
    Resource *priv = resource->priv;
    GwySerializableItem *it = items->items + items->n;

    *it = serialize_items[0];
    it->value.v_string = priv->name;
    it++, items->n++;

    return N_ITEMS;
}

static gboolean
gwy_resource_construct(GwySerializable *serializable,
                       GwySerializableItems *items,
                       GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyResource", error_list);
    GwyResource *resource = GWY_RESOURCE(serializable);
    Resource *priv = resource->priv;

    if (its[0].value.v_string)
        GWY_TAKE_STRING(priv->name, its[0].value.v_string);

    return TRUE;
}

static void
gwy_resource_assign_impl(GwySerializable *destination,
                         GwySerializable *source)
{
    GwyResource *dest = GWY_RESOURCE(destination);
    GwyResource *src = GWY_RESOURCE(source);

    g_return_if_fail(dest->priv->modifiable);
    if (!dest->priv->managed)
        gwy_resource_rename(dest, src->priv->name);

    // XXX: The rest are management properties, not value.  Do not assign them.
}

static void
gwy_resource_set_property(GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    GwyResource *resource = GWY_RESOURCE(object);
    Resource *priv = resource->priv;

    switch (prop_id) {
        case PROP_NAME:
        // This can be set as property only upon construction.
        g_assert(!priv->name);
        priv->name = g_value_dup_string(value);
        break;

        case PROP_MODIFIABLE:
        // FIXME: This is a bit weird for managed resources.  Should it really
        // create them on-disk and remove them?
        priv->modifiable = g_value_get_boolean(value);
        break;

        case PROP_PREFERRED:
        gwy_resource_set_preferred(resource, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_resource_get_property(GObject *object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    GwyResource *resource = GWY_RESOURCE(object);
    Resource *priv = resource->priv;

    switch (prop_id) {
        case PROP_NAME:
        g_value_set_string(value, priv->name);
        break;

        case PROP_FILE_NAME:
        if (priv->file)
            g_value_take_string(value, g_file_get_path(priv->file));
        else
            g_value_set_string(value, NULL);
        break;

        case PROP_PREFERRED:
        g_value_set_boolean(value, priv->preferred);
        break;

        case PROP_MODIFIABLE:
        g_value_set_boolean(value, priv->modifiable);
        break;

        case PROP_MODIFIED:
        g_value_set_boolean(value, priv->modified);
        break;

        case PROP_MANAGED:
        g_value_set_boolean(value, priv->managed);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static const gchar*
gwy_resource_get_item_name(gconstpointer item)
{
    GwyResource *resource = (GwyResource*)item;
    return resource->priv->name;
}

static gboolean
gwy_resource_is_modifiable_impl(gconstpointer item)
{
    GwyResource *resource = (GwyResource*)item;
    return resource->priv->modifiable;
}

static gboolean
gwy_resource_compare(gconstpointer item1,
                     gconstpointer item2)
{
    Resource *priv1 = ((GwyResource*)item1)->priv,
             *priv2 = ((GwyResource*)item2)->priv;

    if (!priv1->collation_key)
        priv1->collation_key = g_utf8_collate_key(priv1->name, -1);
    if (!priv2->collation_key)
        priv2->collation_key = g_utf8_collate_key(priv2->name, -1);

    return strcmp(priv1->collation_key, priv2->collation_key);
}

static void
gwy_resource_rename(gpointer item,
                    const gchar *new_name)
{
    GwyResource *resource = (GwyResource*)item;
    Resource *priv = resource->priv;

    g_return_if_fail(priv->modifiable);
    g_return_if_fail(new_name);
    if (_gwy_assign_string(&priv->name, new_name))
        g_object_notify_by_pspec(G_OBJECT(item), properties[PROP_NAME]);
}

static gpointer
gwy_resource_copy(gconstpointer item)
{
    GwyResourceClass *klass = GWY_RESOURCE_GET_CLASS(item);
    g_return_val_if_fail(klass && klass->copy, NULL);
    return klass->copy(GWY_RESOURCE(item));
}

static const GType*
gwy_resource_get_traits(guint *ntraits)
{
    if (ntraits)
        *ntraits = gwy_resource_ntraits;

    return gwy_resource_trait_types;
}

static const gchar*
gwy_resource_get_trait_name(guint i)
{
    g_return_val_if_fail(i < gwy_resource_ntraits, NULL);
    return gwy_resource_trait_names[i];
}

static void
gwy_resource_get_trait_value(gconstpointer item,
                             guint i,
                             GValue *value)
{
    g_return_if_fail(i < gwy_resource_ntraits);
    g_value_init(value, gwy_resource_trait_types[i]);
    g_object_get_property(G_OBJECT(item), gwy_resource_trait_names[i], value);
}

static void
gwy_resource_delete(gpointer item)
{
    GwyResource *resource = GWY_RESOURCE(item);
    manage_delete(resource);
    set_is_managed(resource, FALSE);
}

static void
set_is_managed(GwyResource *resource,
               gboolean managed)
{
    g_return_if_fail(!resource->priv->managed != !managed);
    resource->priv->managed = managed;
    g_object_notify_by_pspec(G_OBJECT(resource), properties[PROP_MANAGED]);
}

static void
inventory_item_inserted(GwyInventory *inventory,
                        guint i,
                        GwyResourceClass *klass)
{
    g_return_if_fail(klass->priv->inventory == inventory);
    GwyResource *resource = gwy_inventory_get_nth(inventory, i);
    Resource *priv = resource->priv;
    GObject *object = G_OBJECT(resource);
    g_object_freeze_notify(object);
    set_is_managed(resource, TRUE);
    if (priv->modifiable)
        manage_create(resource);
    g_object_thaw_notify(object);
}

/**
 * gwy_resource_get_name:
 * @resource: A resource.
 *
 * Gets the name of a resource.
 *
 * Returns: Name of @resource.  The string is owned by @resource and must not
 *          be modfied or freed.
 **/
const gchar*
gwy_resource_get_name(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), NULL);
    return resource->priv->name;
}

/**
 * gwy_resource_set_name:
 * @resource: A free-standing resource.
 * @name: New name.
 *
 * Sets the name of a free-standing resource.
 *
 * Only free-standing resources can be renamed using this method.  Resources
 * managed by the class inventory can be only renamed with
 * gwy_inventory_rename().
 **/
void
gwy_resource_set_name(GwyResource *resource,
                      const gchar *name)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    g_return_if_fail(!resource->priv->managed);
    gwy_resource_rename(resource, name);
}

/**
 * gwy_resource_get_filename:
 * @resource: A resource.
 *
 * Gets the file name of a resource.
 *
 * Generally, only modifiable managed resource and resources that have been
 * loaded from a file have a file name assigned (these two groups may overlap).
 * You can also set the file name of free-standing resources using
 * gwy_resource_set_filename().
 *
 * Returns: File name of @resource in GLib encoding, possibly %NULL.  The
 *          string is owned by @resource and must not be modfied or freed.
 **/
const gchar*
gwy_resource_get_filename(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), NULL);
    Resource *priv = resource->priv;
    return priv->file ? g_file_get_path(priv->file) : NULL;
}

/**
 * gwy_resource_set_filename:
 * @resource: A free-standing resource.
 * @filename: New file name in GLib encoding, possibly %NULL to unset.
 *
 * Sets the file name of a free-standing resource.
 *
 * Only free-standing resources can have their file name set using this method.
 * Resources managed by the class inventory have their file names determined
 * automatically.
 **/
void
gwy_resource_set_filename(GwyResource *resource,
                          const gchar *filename)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    Resource *priv = resource->priv;
    g_return_if_fail(!priv->managed);
    // FIXME: Maybe we should absolutize and canonicalize the file names.
    gboolean emit_filename_changed = FALSE;
    if (filename) {
        GFile *newfile = g_file_new_for_path(filename);
        if (priv->file) {
            if (!g_file_equal(priv->file, newfile)) {
                GWY_SWAP(GFile*, priv->file, newfile);
                emit_filename_changed = TRUE;
            }
            g_object_unref(newfile);
        }
        else {
            priv->file = newfile;
            emit_filename_changed = TRUE;
        }
    }
    else {
        if (priv->file) {
            GWY_OBJECT_UNREF(priv->file);
            emit_filename_changed = TRUE;
        }
    }

    if (emit_filename_changed)
        g_object_notify_by_pspec(G_OBJECT(resource),
                                 properties[PROP_FILE_NAME]);
}

/**
 * gwy_resource_is_managed:
 * @resource: A resource.
 *
 * Determines whether a resource is managed.
 *
 * Returns: %TRUE if resource is managed by the class inventory, %FALSE if
 *          it is a free-standing resource.
 **/
gboolean
gwy_resource_is_managed(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), FALSE);
    return resource->priv->managed;
}

/**
 * gwy_resource_is_modifiable:
 * @resource: A resource.
 *
 * Determines whether a resource is modifiable.
 *
 * Returns: %TRUE if resource is modifiable, %FALSE if it is fixed (system)
 *          resource.
 **/
gboolean
gwy_resource_is_modifiable(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), FALSE);
    return resource->priv->modifiable;
}

/**
 * gwy_resource_get_preferred:
 * @resource: A resource.
 *
 * Determines whether a resource is preferred.
 *
 * Returns: %TRUE if resource is preferred, %FALSE otherwise.
 **/
gboolean
gwy_resource_get_preferred(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), FALSE);
    return resource->priv->preferred;
}

/**
 * gwy_resource_set_preferred:
 * @resource: A resource.
 * @preferred: %TRUE to make @resource preferred, %FALSE to make it not
 *             preferred.
 *
 * Sets the preferability of a resource.
 **/
void
gwy_resource_set_preferred(GwyResource *resource,
                           gboolean preferred)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    preferred = !!preferred;
    if (preferred == resource->priv->preferred)
        return;
    resource->priv->preferred = preferred;
    g_object_notify_by_pspec(G_OBJECT(resource), properties[PROP_PREFERRED]);
}

/**
 * gwy_resource_type_get_name:
 * @type: A resource type.
 *
 * Gets the name of a resource type.
 *
 * This is an simple identifier usable for example as directory name.  See
 * gwy_resource_type_get_description() for a descriptive and translatable name
 * intended for humans.
 *
 * Returns: Resource class name, as a constant string that must not be modified
 *          nor freed.
 **/
const gchar*
gwy_resource_type_get_name(GType type)
{
    GwyResourceClass *klass = g_type_class_peek(type);
    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), NULL);
    return klass->priv->name;
}

/**
 * gwy_resource_type_get_description:
 * @type: A resource type.
 *
 * Gets the description of a resource type.
 *
 * See gwy_resource_type_get_name() for a name that is better suited as an
 * identifier.
 *
 * Returns: Resource class description, as a constant string that must not be
 *          modified nor freed.  It is not translated; translation is up to
 *          the caller.
 **/
const gchar*
gwy_resource_type_get_description(GType type)
{
    GwyResourceClass *klass = g_type_class_peek(type);
    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), NULL);
    return klass->priv->description;
}

static ResourceClass*
ensure_class_inventory(GType type)
{
    // This possibly causes gwy_resource_class_register() to be called,
    // creating the eternal reference so the subsequent unref is safe.
    // Check that gwy_resource_class_register() has been indeed called by
    // asserting that priv->name exists.
    GwyResourceClass *klass = g_type_class_ref(type);
    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), NULL);
    ResourceClass *cpriv = klass->priv;
    g_return_val_if_fail(cpriv->name, NULL);
    if (!cpriv->inventory) {
        cpriv->inventory = gwy_inventory_new_with_type(&cpriv->item_type);
        cpriv->item_inserted_id
            = g_signal_connect(cpriv->inventory, "item-inserted",
                               G_CALLBACK(inventory_item_inserted), klass);
        if (klass->setup_inventory)
            klass->setup_inventory(cpriv->inventory);
    }
    g_type_class_unref(klass);
    return cpriv;
}

/**
 * gwy_resource_type_get_inventory:
 * @type: A resource type.
 *
 * Gets the inventory which holds resources of a resource type.
 *
 * Returns: (transfer none):
 *          Resource class inventory.
 **/
GwyInventory*
gwy_resource_type_get_inventory(GType type)
{
    ResourceClass *cpriv = ensure_class_inventory(type);
    g_return_val_if_fail(cpriv, NULL);
    return cpriv->inventory;
}

/**
 * gwy_resource_type_get_item_type:
 * @type: A resource type.
 *
 * Gets the inventory item type for a resource type.
 *
 * Returns: Inventory item type.
 **/
const GwyInventoryItemType*
gwy_resource_type_get_item_type(GType type)
{
    ResourceClass *cpriv = ensure_class_inventory(type);
    g_return_val_if_fail(cpriv, NULL);
    return &cpriv->item_type;
}

/**
 * gwy_resource_class_register:
 * @klass: A resource class.
 * @name: Resource class name, usable as resource directory name for on-disk
 *        resources.  It must be a valid identifier.  Normally, a lower-case
 *        plain name in plural is used, e.g. "gradients".
 * @description: Descriptive and translatable (but untranslated) name intended
 *               to be displayed to humans, e.g. "Fitting function".
 * @item_type: Inventory item type.  Usually pass %NULL, to use the parent's
 *             item type.  Modification might be useful for instance if you
 *             want to add traits, in this case acquire parent's item type with
 *             gwy_resource_type_get_item_type() and modify it accordingly
 *             with chaining.
 *
 * Registers a resource class.
 *
 * Calling this class method is necessary to set up the class inventory.  This
 * is normally done in the class init function of a resource class.
 *
 * The strings are assumed to be static, no copies are made.
 **/
void
gwy_resource_class_register(GwyResourceClass *klass,
                            const gchar *name,
                            const gchar *description,
                            const GwyInventoryItemType *item_type)
{
    g_return_if_fail(GWY_IS_RESOURCE_CLASS(klass));
    g_return_if_fail(name);

    ResourceClass *priv = klass->priv;
    g_return_if_fail(!priv->name);    // Do not permit repeated registration
    if (item_type) {
        g_return_if_fail(item_type->type == G_TYPE_FROM_CLASS(klass));
        klass->priv->item_type = *item_type;
    }
    if (!gwy_ascii_strisident(name, "-_", NULL))
        g_warning("Resource class name %s is not a valid identifier.", name);
    priv->name = name;
    priv->description = description;

    // This reference is released only in gwy_resources_finalize().
    GType type = G_TYPE_FROM_CLASS(klass);
    g_type_class_ref(type);

    gpointer ptype = GSIZE_TO_POINTER(type);
    G_LOCK(resource_classes);
    if (!g_slist_find(resource_classes, ptype))
        resource_classes = g_slist_prepend(resource_classes, ptype);
    G_UNLOCK(resource_classes);
}

/**
 * gwy_resource_save:
 * @resource: A resource.
 * @error: Location to store the error occuring, %NULL to ignore.  Errors from
 *         #GFileError domains can occur.
 *
 * Saves a resource.
 *
 * The resource is actually saved only if it has been modified.  If the saving
 * succeeds the modification flag is cleared, otherwise it is kept set.
 *
 * It is an error to try to save resources with no file assigned and
 * non-modifiable resources.
 *
 * Both free-standing and managed resources can be saved with this method
 * although you should rarely need to explicitly save managed resources.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE if it failed.  Not saving
 *          the resource because it has not been modified counts as success.
 **/
gboolean
gwy_resource_save(GwyResource *resource,
                  GError **error)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), FALSE);
    Resource *priv = resource->priv;
    g_return_val_if_fail(priv->name, FALSE);
    g_return_val_if_fail(priv->modifiable, FALSE);
    g_return_val_if_fail(priv->managed || priv->file, FALSE);

    priv->mtime = 0;
    if (!priv->modified)
        return TRUE;

    GwyResourceClass *klass = GWY_RESOURCE_GET_CLASS(resource);
    g_return_val_if_fail(klass && klass->dump, FALSE);

    g_object_freeze_notify(G_OBJECT(resource));
    // This may set the file name for managed resources.
    GFileOutputStream *ostream = output_stream_for_save(resource, error);
    if (!ostream) {
        g_object_thaw_notify(G_OBJECT(resource));
        return FALSE;
    }

    gchar *body = klass->dump(resource);
    gchar *buffer = g_strconcat(MAGIC_HEADER3,
                                G_OBJECT_TYPE_NAME(resource),
                                "\n",
                                "name ",
                                priv->name,
                                "\n",
                                body,
                                NULL);
    g_free(body);

    gboolean ok;
    ok = g_output_stream_write_all(G_OUTPUT_STREAM(ostream),
                                   buffer, strlen(buffer), NULL, NULL, error);
    if (!g_output_stream_close(G_OUTPUT_STREAM(ostream), NULL, error))
        ok = FALSE;
    g_free(buffer);
    g_object_unref(ostream);

    priv->modified = FALSE;
    if (priv->managed)
        priv->on_disk = TRUE;
    g_object_notify_by_pspec(G_OBJECT(resource), properties[PROP_MODIFIED]);
    g_object_thaw_notify(G_OBJECT(resource));

    return ok;
}

static GFileOutputStream*
output_stream_for_save(GwyResource *resource,
                       GError **error)
{
    Resource *priv = resource->priv;
    GError *err = NULL;

    if (priv->file)
        return g_file_replace(priv->file, NULL, FALSE, 0, NULL, error);

    ResourceClass *cpriv = GWY_RESOURCE_GET_CLASS(resource)->priv;
    if (!priv->managed || !cpriv->managed_directory) {
        g_warning("Cannot save non-managed resource %s with no file name.",
                  priv->name);
        return NULL;
    }

    GString *basename = construct_filename(priv->name);
    GFileOutputStream *ostream = NULL;
    guint i = 1, len = basename->len;

    while (!ostream) {
        GFile *file = g_file_resolve_relative_path(cpriv->managed_directory,
                                                   basename->str);
        ostream = g_file_create(file, 0, NULL, &err);
        if (ostream)
            priv->file = file;
        else {
            g_assert(err->domain == G_IO_ERROR);
            if (err->code == G_IO_ERROR_FILENAME_TOO_LONG && len) {
                len--;
                i--;
            }
            else if (err->code != G_IO_ERROR_EXISTS
                     && err->code != G_IO_ERROR_IS_DIRECTORY
                     && err->code != G_IO_ERROR_NOT_REGULAR_FILE) {
                g_propagate_error(error, err);
                break;
            }
            g_object_unref(file);
            g_string_truncate(basename, len);
            g_string_append_printf(basename, "-%u", i);
            i++;
        }
    }
    g_string_free(basename, TRUE);
    g_object_notify_by_pspec(G_OBJECT(resource), properties[PROP_FILE_NAME]);

    return ostream;
}

static GString*
construct_filename(const gchar *resource_name)
{
    /*
     * The name must be representable on any file system and in any code page.
     * Therefore, it must consists of ASCII letters only!
     */
    static const gchar fallback_name[] = "Untitled";

    g_return_val_if_fail(resource_name, g_string_new(fallback_name));

    /* First get rid of bad ASCII characers because we know the encoding now.
     * We might have troubles filtering them after the conversion.   Be very
     * conservative, the name does not matter. */
    gsize len = strlen(resource_name);
    gchar *resname = g_slice_alloc(len + 1);

    gboolean garbage = FALSE;
    gsize j = 0;
    for (gsize i = 0; i < len; i++) {
        if ((guchar)resource_name[i] >= 128
            || g_ascii_isalnum(resource_name[i])
            || resource_name[i] == '-'
            || resource_name[i] == '.'
            || resource_name[i] == '_') {
            resname[j++] = resource_name[i];
            garbage = FALSE;
        }
        else {
            if (!garbage)
                resname[j++] = '-';
            garbage = TRUE;
        }
    }
    resname[j] = '\0';

    /* Then convert to filename encoding, piece by piece. */
    GString *str = g_string_new(NULL);
    GError *error = NULL;
    do {
        g_clear_error(&error);
        gsize bytes_read;
        gchar *filename_sys = g_filename_from_utf8(resname, -1,
                                                   &bytes_read, NULL, &error);
        if (filename_sys) {
            g_string_append(str, filename_sys);
            g_free(filename_sys);
        }
        resource_name += bytes_read;
        if (*resource_name)
            resource_name++;
    } while (error && *resource_name);

    g_clear_error(&error);
    g_slice_free1(len + 1, resname);

    if (!str->len || (str->len == 1 && garbage))
        g_string_assign(str, fallback_name);

    return str;
}

/**
 * gwy_resource_load:
 * @filename: Name of resource file to load, in GLib encoding.
 * @expected_type: Expected resource type.  This must be a valid type, however,
 *                 it is possible to pass %GWY_TYPE_RESOURCE which effectively
 *                 accepts all possible resources.
 * @modifiable: %TRUE to create the resource as modifiable if it appears
 *              modifiable also on disk
 * @error: Location to store the error occuring, %NULL to ignore.  Errors from
 *         #GwyResourceError and #GFileError domains can occur.
 *
 * Loads a resource from a file.
 *
 * The resource is created as free-standing.
 *
 * Returns: (transfer full):
 *          A newly created resource object.
 **/
GwyResource*
gwy_resource_load(const gchar *filename_sys,
                  GType expected_type,
                  gboolean modifiable,
                  GError **error)
{
    GwyResource *resource = NULL;
    gchar *text = NULL;

    if (g_file_get_contents(filename_sys, &text, NULL, error)) {
        GwyStrLineIter *iter = gwy_str_line_iter_new_take(text);
        resource = parse(iter, expected_type, filename_sys, error);
        if (resource) {
            Resource *priv = resource->priv;
            priv->file = g_file_new_for_path(filename_sys);
            GFileInfo *info = g_file_query_info(priv->file,
                                                G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
                                                0, NULL, NULL);
            gboolean mod_on_disk = FALSE;
            if (info && g_file_info_get_attribute_boolean(info,
                                                          G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
                mod_on_disk = TRUE;
            g_object_unref(info);
            priv->modifiable = !!modifiable && mod_on_disk;
            // Do not set on_disk as we really care only about managed
            // resources so it's set in gwy_resource_type_load_directory().
        }
        gwy_str_line_iter_free(iter);
    }

    return resource;
}

static void
err_filename(GError **error,
             guint domain,
             guint code,
             gchar **filename_disp,
             const gchar *format,
             ...)
{
    va_list ap;
    va_start(ap, format);

    gchar *message = g_strdup_vprintf(format, ap);

    g_set_error_literal(error, domain, code, message);

    g_free(message);
    g_free(*filename_disp);

    va_end(ap);
}

static GwyResource*
parse(GwyStrLineIter *iter,
      GType expected_type,
      const gchar *filename_sys,
      GError **error)
{
    gchar *line = gwy_str_line_iter_next(iter);
    gchar *filename_disp = NULL;
    GError *err = NULL;

    if (!line) {
        err_filename(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_HEADER,
                     &filename_disp,
                     // TRANSLATORS: Error message.
                     _("Wrong or missing resource magic header in file ‘%s’."),
                     (filename_disp = g_filename_display_name(filename_sys)));
        return NULL;
    }

    g_strstrip(line);

    /* Magic header */
    guint version;
    if (g_str_has_prefix(line, MAGIC_HEADER3)) {
        line += sizeof(MAGIC_HEADER3) - 1;
        version = 3;
    }
    else if (g_str_has_prefix(line, MAGIC_HEADER2)) {
        line += sizeof(MAGIC_HEADER2) - 1;
        version = 2;
    }
    else {
        err_filename(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_HEADER,
                     &filename_disp,
                     _("Wrong or missing resource magic header in file ‘%s’."),
                     (filename_disp = g_filename_display_name(filename_sys)));
        return NULL;
    }

    /* Resource type */
    line += strspn(line, " \t");
    const gchar *typename = line;
    if (G_UNLIKELY(!line)) {
        err_filename(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_HEADER,
                     &filename_disp,
                     // TRANSLATORS: Error message.
                     _("Resource header of file ‘%s’ is truncated."),
                     (filename_disp = g_filename_display_name(filename_sys)));
        return NULL;
    }

    /* Parse the name in v3 resources. */
    gchar *key, *name = NULL;
    if (version == 3) {
        GwyResourceLineType type = gwy_resource_parse_param_line(iter,
                                                                 &key, &name,
                                                                 &err);
        if (type != GWY_RESOURCE_LINE_OK) {
            err_filename(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME,
                         &filename_disp,
                         // TRANSLATORS: Error message.
                         // TRANSLATORS: %s after colon is a detailed error.
                         _("Resource name line in file ‘%s’ is malformed: %s"),
                         (filename_disp = g_filename_display_name(filename_sys)),
                         err->message);
            g_clear_error(&err);
            return NULL;
        }
        if (!gwy_strequal(key, "name")) {
            err_filename(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME,
                         &filename_disp,
                         // TRANSLATORS: Error message.
                        _("First line in version 3 resource ‘%s’ in file ‘%s’ "
                          "does not contain the name."),
                        typename,
                        (filename_disp = g_filename_display_name(filename_sys)));
            return NULL;
        }
        if (!strlen(name)) {
            err_filename(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME,
                         &filename_disp,
                         // TRANSLATORS: Error message.
                        _("Resource name is empty in "
                          "version 3 resource ‘%s’ in file ‘%s’."),
                        typename,
                        (filename_disp = g_filename_display_name(filename_sys)));
            return NULL;
        }
    }

    /* Resource class */
    GwyResourceClass *klass = get_resource_class(typename, expected_type,
                                                 filename_sys, error);
    if (!klass)
        return NULL;

    /* Parse. */
    GwyResource *resource = g_object_newv(expected_type, 0, NULL);
    if ((klass->parse(resource, iter, &err))) {
        if (name) {
            resource->priv->name = g_strdup(name);
        }
        else {
            name = g_path_get_basename(filename_sys);
            resource->priv->name = g_filename_to_utf8(name, -1,
                                                      NULL, NULL, NULL);
            g_free(name);
        }
    }
    else {
        err_filename(error, GWY_RESOURCE_ERROR, err->code,
                     &filename_disp,
                     // TRANSLATORS: Error message.
                     // TRANSLATORS: %s after colon is a detailed error.
                     _("Resource ‘%s’ in file ‘%s’ is malformed: %s"),
                     typename,
                     (filename_disp = g_filename_display_name(filename_sys)),
                     err->message);
        g_clear_error(&err);
        GWY_OBJECT_UNREF(resource);
    }
    g_type_class_unref(klass);

    return resource;
}

static GwyResourceClass*
get_resource_class(const gchar *typename,
                   GType expected_type,
                   const gchar *filename_sys,
                   GError **error)
{
    GType type = g_type_from_name(typename);
    gchar *filename_disp = NULL;

    if (G_UNLIKELY(!type)) {
        err_filename(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_TYPE,
                     &filename_disp,
                     // TRANSLATORS: Error message.
                     _("Resource type ‘%s’ of file ‘%s’ is invalid."),
                     typename,
                     (filename_disp = g_filename_display_name(filename_sys)));
        return NULL;
    }

    /* Does it make sense to accept subclasses? */
    if (G_UNLIKELY(!g_type_is_a(type, expected_type))) {
        err_filename(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_TYPE,
                     &filename_disp,
                     // TRANSLATORS: Error message.
                     _("Resource type ‘%s’ of file ‘%s’ does not match "
                       "the expected type ‘%s’."),
                     typename,
                     (filename_disp = g_filename_display_name(filename_sys)),
                     g_type_name(expected_type));
        return NULL;
    }

    /* If the resource type matches the requested type yet the type is invalid
     * then this is a programmer's error.  Fail ungracefully.  */
    g_return_val_if_fail(g_type_is_a(type, GWY_TYPE_RESOURCE)
                         && G_TYPE_IS_INSTANTIATABLE(type)
                         && !G_TYPE_IS_ABSTRACT(type),
                         NULL);

    GwyResourceClass *klass = GWY_RESOURCE_CLASS(g_type_class_ref(type));
    g_return_val_if_fail(klass, NULL);

    if (!klass->parse) {
        g_critical("%s class does not implement parse()", typename);
        g_type_class_unref(klass);
        return NULL;
    }

    return klass;
}

/**
 * gwy_resource_data_changed:
 * @resource: A resource.
 *
 * Emits signal "data-changed" on a resource.
 *
 * It can be called only on non-constant resources.  The default class handler
 * marks the resource as modified.
 *
 * This function is primarily intended for resource implementation.
 **/
void
gwy_resource_data_changed(GwyResource *resource)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    g_signal_emit(resource, signals[SGNL_DATA_CHANGED], 0);
}

static void
data_changed(GwyResource *resource)
{
    Resource *priv = resource->priv;
    if (!priv->modifiable)
        g_warning("Constant resource ‘%s’ of type %s was modified",
                  priv->name, G_OBJECT_TYPE_NAME(resource));
    if (!priv->modified) {
        priv->modified = TRUE;
        g_object_notify_by_pspec(G_OBJECT(resource), properties[PROP_MODIFIED]);
    }
    manage_update(resource);
}

static void
gwy_resource_notify(GObject *object,
                    GParamSpec *pspec)
{
    if (pspec->owner_type == GWY_TYPE_RESOURCE
        && !gwy_strequal(pspec->name, "name"))
        goto chain;

    GwyResource *resource = GWY_RESOURCE(object);
    Resource *priv = resource->priv;
    // FIXME: Recursion.
    if (!priv->modified) {
        priv->modified = TRUE;
        g_object_notify_by_pspec(G_OBJECT(resource), properties[PROP_MODIFIED]);
    }
    manage_update(resource);

chain:
    if (parent_class->notify)
        parent_class->notify(object, pspec);
}

static gdouble
get_timestamp(void)
{
    static glong offset = 0;

    GTimeVal tv;
    g_get_current_time(&tv);
    if (!offset)
        offset = tv.tv_sec;

    return (tv.tv_sec - offset) + (gdouble)tv.tv_usec/G_USEC_PER_SEC;
}

static void
manage_create(GwyResource *resource)
{
    if (management_type == GWY_RESOURCE_MANAGEMENT_NONE
        || !resource->priv->managed)
        return;

    GwyResourceClass *klass = GWY_RESOURCE_GET_CLASS(resource);
    ResourceClass *cpriv = klass->priv;
    if (!cpriv->managed)
        return;

    Resource *priv = resource->priv;

    if (!cpriv->managed_directory) {
        // XXX: Remove in production version
        g_warning("Cannot create resource %s of type %s on disk: "
                  "No managed directory has been set up.",
                  priv->name, G_OBJECT_TYPE_NAME(resource));
        return;
    }

    GError *err = NULL;
    if (!gwy_resource_save(resource, &err)) {
        g_warning("Cannot create resource on disk: %s", err->message);
        g_clear_error(&err);
    }
}

static void
manage_delete(GwyResource *resource)
{
    if (management_type == GWY_RESOURCE_MANAGEMENT_NONE
        || !resource->priv->managed)
        return;

    GwyResourceClass *klass = GWY_RESOURCE_GET_CLASS(resource);
    ResourceClass *cpriv = klass->priv;
    if (!cpriv->managed)
        return;

    Resource *priv = resource->priv;

    if (!priv->file) {
        // XXX: Keep in production version?  How it could happen?
        g_warning("Cannot remove resource %s of type %s from disk: "
                  "It has not associated file name.",
                  resource->priv->name, G_OBJECT_TYPE_NAME(resource));
        return;
    }

    manage_unqueue(resource);
    priv->on_disk = FALSE;
    GError *err = NULL;
    if (!g_file_delete(priv->file, NULL, &err)) {
        g_warning("Cannot remove resource from disk: %s", err->message);
        g_clear_error(&err);
    }
}

static void
manage_update(GwyResource *resource)
{
    if (management_type == GWY_RESOURCE_MANAGEMENT_NONE
        || !resource->priv->managed)
        return;

    GwyResourceClass *klass = GWY_RESOURCE_GET_CLASS(resource);
    ResourceClass *cpriv = klass->priv;
    if (!cpriv->managed)
        return;

    Resource *priv = resource->priv;
    if (priv->mtime) {
        G_LOCK(update_queue);
        update_queue = g_list_prepend(update_queue, resource);
        G_UNLOCK(update_queue);
    }
    priv->mtime = get_timestamp();
    if (management_type == GWY_RESOURCE_MANAGEMENT_MANUAL)
        return;

    if (!flush_source_id)
        flush_source_id = g_timeout_add_seconds_full(G_PRIORITY_LOW, 1,
                                                     manage_flush_timeout,
                                                     NULL, NULL);
}

static void
manage_unqueue(GwyResource *resource)
{
    G_LOCK(update_queue);
    update_queue = g_list_remove(update_queue, resource);
    G_UNLOCK(update_queue);
    resource->priv->mtime = 0;
}

static gboolean
manage_flush_timeout(G_GNUC_UNUSED gpointer user_data)
{
    // Destroy the timeout source if the queue becomes empty.
    G_LOCK(flush);
    gboolean retval = manage_flush_check_queue(0, get_timestamp() - 2.0);
    if (!retval)
        flush_source_id = 0;
    G_UNLOCK(flush);
    return retval;
}

static void
manage_flush(GType type,
             gdouble older_than)
{
    if (!manage_flush_check_queue(type, older_than) && flush_source_id) {
        g_source_remove(flush_source_id);
        flush_source_id = 0;
    }
}

static gboolean
manage_flush_check_queue(GType type,
                         gdouble older_than)
{
    GList *to_save = NULL;

    // Gather the resources to save and remove them from the queue.
    // Obviously we must hold the queue lock but also the manage lock because
    // someone could e.g. remove the resource meanwhile.
    G_LOCK(update_queue);
    for (GList *l = update_queue; l; ) {
        GwyResource *resource = (GwyResource*)l->data;
        if (resource->priv->mtime > older_than
            || (type && G_OBJECT_TYPE(resource) != type)) {
            l = g_list_next(l);
        }
        else {
            GList *next = g_list_next(l);
            update_queue = g_list_remove_link(update_queue, l);
            to_save = g_list_concat(l, to_save);
            l = next;
        }
    }
    gboolean retval = (update_queue != NULL);
    G_UNLOCK(update_queue);

    for (GList *l = to_save; l; l = g_list_next(l))
        gwy_resource_save((GwyResource*)l->data, NULL);
    g_list_free(to_save);

    return retval;
}

/**
 * gwy_resource_type_load:
 * @type: A resource type.
 *
 * Loads resources of a given type from disk.
 *
 * Resources are loaded from the library itself and from system directory (both
 * marked constant), then from the user directory (marked modifiable).
 **/
void
gwy_resource_type_load(GType type)
{
    g_return_if_fail(g_type_is_a(type, GWY_TYPE_RESOURCE));
    ResourceClass *cpriv = ensure_class_inventory(type);
    g_return_if_fail(cpriv);

    gwy_resource_type_load_builtins(type, NULL);

    gchar *userdir = gwy_user_directory(cpriv->name);
    gchar **dirs = gwy_data_search_path(cpriv->name);
    for (gchar **d = dirs; *d; d++) {
        gboolean writable = userdir && gwy_strequal(*d, userdir);
        gwy_resource_type_load_directory(type, *d, writable, NULL);
    }
    if (!g_file_test(userdir, G_FILE_TEST_IS_DIR))
        g_mkdir_with_parents(userdir, 0700);
    GWY_FREE(userdir);
    g_strfreev(dirs);
}

static void
set_managed_directory(ResourceClass *cpriv,
                      GFile *dir,
                      const gchar *message)
{
    if (cpriv->managed_directory) {
        if (!g_file_equal(cpriv->managed_directory, dir)) {
            gchar *classdir = g_file_get_path(cpriv->managed_directory);
            gchar *thisdir = g_file_get_path(dir);
            g_warning(message, g_type_name(cpriv->item_type.type),
                      classdir, thisdir);
            g_free(classdir);
            g_free(thisdir);
        }
    }
    else
        cpriv->managed_directory = g_object_ref(dir);
}

/**
 * gwy_resource_type_load_directory:
 * @type: Resource type of the resources in the directory.
 * @dirname: Directory to load resources from (in system encoding).
 * @modifiable: %TRUE to create resources as modifiable, %FALSE to create them
 *              as fixed.  At most one directory can be loaded with @modifiable
 *              as %TRUE.  Newly created and modified resources will be saved
 *              to this directory.
 * @error_list: Location to store the errors occuring, %NULL to ignore.
 *
 * Loads all resources of given class from a directory.
 *
 * All files in the directory (except hidden and backup files as defined by
 * GIO %G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN and
 * %G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP) are assumed to be resources of class
 * @klass.  Subdirectories are ignored.
 *
 * The loaded resources are created as managed.
 **/
void
gwy_resource_type_load_directory(GType type,
                                 const gchar *dirname_sys,
                                 gboolean modifiable,
                                 GwyErrorList **error_list)
{
    g_return_if_fail(g_type_is_a(type, GWY_TYPE_RESOURCE));
    ResourceClass *cpriv = ensure_class_inventory(type);
    g_return_if_fail(cpriv);

    GFile *gfile = g_file_new_for_path(dirname_sys);
    GFileEnumerator *dir
        = g_file_enumerate_children(gfile,
                                    G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                    G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                    G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
                                    G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP,
                                    0, NULL, NULL);
    if (modifiable) {
        set_managed_directory(cpriv, gfile,
                              "Modifiable resources of type %s were already "
                              "loaded from another directory ‘%s’ different "
                              "from ‘%s’.  Resource management will be "
                              "confused.");
    }
    g_object_unref(gfile);
    if (!dir)
        return;

    GFileInfo *fileinfo;
    while ((fileinfo = g_file_enumerator_next_file(dir, NULL, NULL))) {
        if (g_file_info_get_file_type(fileinfo) != G_FILE_TYPE_REGULAR
            || g_file_info_get_is_hidden(fileinfo)
            || g_file_info_get_is_backup(fileinfo)) {
            g_object_unref(fileinfo);
            continue;
        }

        gchar *filename_sys = g_build_filename(dirname_sys,
                                               g_file_info_get_name(fileinfo),
                                               NULL);
        GError *error = NULL;
        GwyResource *resource = gwy_resource_load(filename_sys, type,
                                                  modifiable, &error);

        if (G_LIKELY(resource) && name_is_unique(resource, cpriv, &error)) {
            Resource *priv = resource->priv;
            priv->on_disk = TRUE;
            gwy_inventory_insert(cpriv->inventory, resource);
            g_object_unref(resource);
        }
        else {
            gwy_error_list_propagate(error_list, error);
        }
        g_free(filename_sys);
        g_object_unref(fileinfo);
    }
    g_object_unref(dir);
}

/**
 * gwy_resource_type_load_builtins:
 * @type: Resource type of the resources.
 * @error_list: Location to store the errors occuring, %NULL to ignore.
 *
 * Loads all built-in resources of given class.
 *
 * Built-in resources are those compiles to the library; using #GResource or
 * some specific mechanism.  The loaded resources are created as managed and
 * fixed.
 **/
void
gwy_resource_type_load_builtins(GType type,
                                GwyErrorList **error_list)
{
    g_return_if_fail(g_type_is_a(type, GWY_TYPE_RESOURCE));
    ResourceClass *cpriv = ensure_class_inventory(type);
    g_return_if_fail(cpriv);

    GwyResourceClass *klass = g_type_class_ref(type);
    g_return_if_fail(GWY_IS_RESOURCE_CLASS(klass));
    if (klass->load_builtins)
        klass->load_builtins(error_list);

    g_type_class_unref(klass);
}

/* Check if the name does not conflict with an existing resource. */
static gboolean
name_is_unique(GwyResource *resource,
               ResourceClass *klass,
               GError **error)
{
    Resource *priv = resource->priv;
    GwyResource *obstacle = gwy_inventory_get(klass->inventory, priv->name);
    if (!obstacle)
        return TRUE;

    /* Do not bother with message formatting when errors are ingored. */
    if (!error)
        return FALSE;

    gchar *filename = g_file_get_parse_name(priv->file);
    // TRANSLATORS: Error message.
    g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DUPLICIT,
                // TRANSLATORS: Error message.
                _("Resource ‘%s’ named ‘%s’ from file ‘%s’ "
                  "conflicts with an existing resource."),
                G_OBJECT_TYPE_NAME(resource), priv->name, filename);
    g_free(filename);

    return FALSE;
}

/**
 * gwy_resource_type_get_managed_directory:
 * @type: A resource type.
 *
 * Obtains the name of the managed directory for a resource type.
 *
 * This is the directory where newly created or moved resources are stored.
 *
 * Returns: The name of the managed directory in system encoding as a newly
 *          allocated string.  %NULL can be returned if the directory has not
 *          been set yet.
 **/
gchar*
gwy_resource_type_get_managed_directory(GType type)
{
    g_return_val_if_fail(g_type_is_a(type, GWY_TYPE_RESOURCE), NULL);
    ResourceClass *cpriv = ensure_class_inventory(type);
    g_return_val_if_fail(cpriv, NULL);
    return cpriv->managed_directory
           ? g_file_get_path(cpriv->managed_directory) : NULL;
}

/**
 * gwy_resource_type_set_managed_directory:
 * @type: A resource type.
 * @dirname: Managed directory name (in system encoding).
 *
 * Sets the name of the managed directory for a resource type.
 *
 * The directory must not change once set.  It is permitted to call this
 * function multiple times with the same directory though.
 *
 * Note the managed directory can be set implicitly by
 * gwy_resource_type_load_directory() with @modifiable argument %TRUE, and
 * consequently also by gwy_resource_type_load() as it marks the user resource
 * directory as modifiable.
 **/
void
gwy_resource_type_set_managed_directory(GType type,
                                        const gchar *dirname_sys)
{
    g_return_if_fail(g_type_is_a(type, GWY_TYPE_RESOURCE));
    ResourceClass *cpriv = ensure_class_inventory(type);
    g_return_if_fail(cpriv);
    GFile *gfile = g_file_new_for_path(dirname_sys);
    set_managed_directory(cpriv, gfile,
                          "Managed directory of resource ‘%s’ has been set to "
                          "‘%s’ and it cannot be changed to ‘%s’.");
}

/**
 * gwy_resource_type_set_managed:
 * @type: A resource type.
 * @managed: %TRUE to enable automated synchronization of resources to disk,
 *           %FALSE to disable it.
 *
 * Enabled/disables on-disk resource management for a resource type.
 *
 * Changes that have occurred while the resource type was unmanaged will not
 * be reflected on disk when you set it managed again.  You have to synchronize
 * the resources manually should you ever need such thing.
 **/
void
gwy_resource_type_set_managed(GType type,
                              gboolean managed)
{
    g_return_if_fail(g_type_is_a(type, GWY_TYPE_RESOURCE));
    GwyResourceClass *klass = g_type_class_peek(type);
    g_return_if_fail(klass);
    klass->priv->managed = managed;
}

/**
 * gwy_resources_get_management_type:
 *
 * Reports the current on-disk resource management type.
 *
 * Returns: The current management type.  Note management can be still off for
 *          individual classes.
 **/
GwyResourceManagementType
gwy_resources_get_management_type(void)
{
    return management_type;
}

/**
 * gwy_resources_set_management_type:
 * @type: Requested management type.
 *
 * Sets if and how are resource managed on disk.
 *
 * Resources that have been queued for saving before the type is changed will
 * be still saved even if the management type is set to
 * %GWY_RESOURCE_MANAGEMENT_NONE.
 *
 * If the current type is %GWY_RESOURCE_MANAGEMENT_MANUAL and the management is
 * switched off the modified resources simply remain in the queue and can be
 * saved with gwy_resource_type_flush() or gwy_resources_flush().  If the
 * management is chaned to %GWY_RESOURCE_MANAGEMENT_MAIN they will be saved
 * during normal operation.
 *
 * This function should be called from the thread running the application main
 * loop.
 **/
void
gwy_resources_set_management_type(GwyResourceManagementType type)
{
    g_return_if_fail(type == GWY_RESOURCE_MANAGEMENT_NONE
                     || type == GWY_RESOURCE_MANAGEMENT_MANUAL
                     || type == GWY_RESOURCE_MANAGEMENT_MAIN);

    if (management_type == type)
        return;

    // MAIN promises the resources will be saved automatically so, fullfill it.
    if (management_type == GWY_RESOURCE_MANAGEMENT_MAIN)
        gwy_resources_flush();

    management_type = type;
}

/**
 * gwy_resource_type_flush:
 * @type: A resource type.
 *
 * Immediately saves all modified resources of a type to disk.
 *
 * This function can be used with all management methods, it does anything only
 * if any modifications have been queued for saving.
 **/
void
gwy_resource_type_flush(GType type)
{
    g_return_if_fail(g_type_is_a(type, GWY_TYPE_RESOURCE));
    manage_flush(type, 0.0);
}

/**
 * gwy_resources_flush:
 *
 * Immediately saves all modified resources of all types to disk.
 *
 * It is a good idea to call this function before your application terminates
 * to save any resources that might be in the queue.
 **/
void
gwy_resources_flush(void)
{
    manage_flush(0, 0.0);
}

/**
 * gwy_resources_lock:
 *
 * Enters a critical section in which managed resources can be modified.
 *
 * This avoids collisions with the automated asynchronous saving of modified
 * resources.
 *
 * Locking resources is necessary only in threaded applications that modify
 * or explicitly flush resources in other threads than main (i.e. the thread
 * running the Gtk+ main loop) and use the automated resource management.
 *
 * Usually you modify resources only in the GUI thread and then no locking
 * is necessary.
 **/
void
gwy_resources_lock(void)
{
    G_LOCK(flush);
}

/**
 * gwy_resources_unlock:
 *
 * Leaves a critical section in which managed resources can be modified.
 *
 * See gwy_resources_lock() for details.
 **/
void
gwy_resources_unlock(void)
{
    G_UNLOCK(flush);
}

/**
 * gwy_resources_finalize:
 *
 * Destroys the inventories of all resource classes.
 *
 * This function makes the affected resource classes unusable.  Its purpose is
 * to facilitate reference leak debugging by destroying a large number of
 * objects that normally live forever.  No attempt to flush changes to disk
 * is made, you should do that beforehand.
 *
 * It would quite defeat the purpose of this function if it was called while
 * the program was busy loading more resource classes in other threads.  So, no
 * locking is used and just do not expect it to work in such case.
 **/
void
gwy_resources_finalize(void)
{
    GSList *l;

    for (l = resource_classes; l; l = g_slist_next(l)) {
        GType type = (GType)GPOINTER_TO_SIZE(l->data);
        GwyResourceClass *klass = g_type_class_peek(type);
        ResourceClass *priv = klass->priv;
        // Disconnect in case someone else holds a reference.
        GWY_SIGNAL_HANDLER_DISCONNECT(priv->inventory, priv->item_inserted_id);
        GWY_OBJECT_UNREF(priv->inventory);
        // The eternal reference goes poof too.
        g_type_class_unref(klass);
    }

    g_slist_free(resource_classes);
    resource_classes = NULL;
}

static GwyResourceLineType
err_identifier(GError **error,
               GwyStrLineIter *iter)
{
    g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA,
                // TRANSLATORS: Error message.
                _("Key at line %u is not a valid identifier."),
                gwy_str_line_iter_lineno(iter));
    return GWY_RESOURCE_LINE_INVALID;
}

static GwyResourceLineType
err_utf8(GError **error,
         GwyStrLineIter *iter)
{
    g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA,
                // TRANSLATORS: Error message.
                _("Value at line %u is not valid UTF-8."),
                gwy_str_line_iter_lineno(iter));
    return GWY_RESOURCE_LINE_INVALID;
}

static GwyResourceLineType
err_too_few_values(GError **error,
                   GwyStrLineIter *iter,
                   guint nvals,
                   guint expected)
{
    g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA,
                // TRANSLATORS: Error message.
                _("Data at line %u consists of too few values "
                  "(%u instead of %u)."),
                gwy_str_line_iter_lineno(iter), nvals, expected);
    return GWY_RESOURCE_LINE_INVALID;
}

static GwyResourceLineType
err_invalid_value(GError **error,
                  GwyStrLineIter *iter,
                  guint nvals)
{
    g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA,
                // TRANSLATORS: Error message.
                _("Data at line %u contain invalid/unexpected text after "
                  "value %u."),
                gwy_str_line_iter_lineno(iter), nvals);
    return GWY_RESOURCE_LINE_INVALID;
}

/**
 * gwy_resource_parse_param_line:
 * @iter: A text line iterator.
 * @key: Location to store the key to.
 * @value: Location to store the value to.
 * @error: Location to store the error occuring, %NULL to ignore.  Errors from
 *         #GwyResourceError can occur.
 *
 * Extracts one key-value pair from a resource file.
 *
 * This is a helper function for resource file parsing.
 *
 * If it returns %GWY_RESOURCE_LINE_OK, @key and @value are pointed to
 * locations in the buffer where the key and value on the next resource line
 * starts.  Whitespace is stripped from both the key and value.
 *
 * In all other cases @key and @value are not set and @line is left intact.
 * Note a line containing no value (just the key) is valid, @value will be set
 * to the empty string.
 *
 * Returns: The line type parsing result.
 **/
GwyResourceLineType
gwy_resource_parse_param_line(GwyStrLineIter *iter,
                              gchar **key,
                              gchar **value,
                              GError **error)
{
    g_return_val_if_fail(iter, GWY_RESOURCE_LINE_NONE);

    // Next non-empty line.
    gchar *line;
    do {
        line = gwy_str_line_iter_next(iter);
        if (!line)
            return GWY_RESOURCE_LINE_NONE;

        while (g_ascii_isspace(*line))
            line++;
    } while (!*line);

    // Key.
    if (!g_ascii_isalpha(*line))
        return err_identifier(error, iter);

    gchar *s = line;
    while (g_ascii_isalnum(*s) || *s == '_')
        s++;

    if (!*s) {
        *key = line;
        *value = s;
        return GWY_RESOURCE_LINE_OK;
    }

    if (!g_ascii_isspace(*s))
        return err_identifier(error, iter);

    gchar *t = s;
    do {
        t++;
    } while (g_ascii_isspace(*t));

    // Value.
    if (!g_utf8_validate(t, -1, NULL))
        return err_utf8(error, iter);

    *s = '\0';
    g_strchomp(t);
    *key = line;
    *value = t;
    return GWY_RESOURCE_LINE_OK;
}

/**
 * gwy_resource_parse_data_line:
 * @iter: A text line iterator.
 * @ncolumns: Expected number of columns.
 * @data: Array of length @number to store the read values to.
 * @error: Location to store the error occuring, %NULL to ignore.  Errors from
 *         #GwyResourceError can occur.
 *
 * Extracts one row of floating point values from a resource file.
 *
 * This is a helper function for resource file parsing.
 *
 * If it returns %GWY_RESOURCE_LINE_OK, @data is filled with the parsed values.
 * Otherwise the content of @data is undefined.  The numbers are parsed using
 * g_ascii_strtod(), i.e. the numbers are expected to be stored in POSIX
 * format.
 *
 * Returns: The line type parsing result.
 **/
GwyResourceLineType
gwy_resource_parse_data_line(GwyStrLineIter *iter,
                             guint ncolumns,
                             gdouble *data,
                             GError **error)
{
    g_return_val_if_fail(iter, GWY_RESOURCE_LINE_NONE);

    // Next non-empty line.
    gchar *line;
    do {
        line = gwy_str_line_iter_next(iter);
        if (!line)
            return GWY_RESOURCE_LINE_NONE;

        while (g_ascii_isspace(*line))
            line++;
    } while (!*line);

    // Read the data.
    gchar *end = line;
    for (guint i = 0; i < ncolumns; i++) {
        data[i] = g_ascii_strtod(line, &end);
        if (end == line) {
            if (*end)
                return err_invalid_value(error, iter, i+1);
            else
                return err_too_few_values(error, iter, i+1, ncolumns);
        }
        line = end;
    }

    while (g_ascii_isspace(*end))
        end++;
    if (*end)
        return err_invalid_value(error, iter, ncolumns);

    return GWY_RESOURCE_LINE_OK;
}

/**
 * gwy_resource_dump_data_line:
 * @data: Array with @ncolumns double values to dump in text.
 * @ncolumns: Number of items in @data.
 *
 * Dumps one row of floating point values to a resource file.
 *
 * Returns: A newly allocated string with @data dumped to text using decimal
 *          point.
 **/
gchar*
gwy_resource_dump_data_line(const gdouble *data,
                            guint ncolumns)
{
    guint buflen = ncolumns*(G_ASCII_DTOSTR_BUF_SIZE + 1);
    gchar *buffer = g_new0(gchar, buflen);
    gchar *p = buffer;
    for (guint i = 0; i < ncolumns; i++) {
        g_ascii_dtostr(p, buflen - (p - buffer), data[i]);
        if (i + 1 < ncolumns) {
            p += strlen(p);
            *p = ' ';
            p++;
        }
    }
    return buffer;
}


/**
 * SECTION: resource
 * @title: GwyResource
 * @short_description: Built-in and/or user supplied application resources
 * @see_also: #GwyInventory
 *
 * #GwyResource is a base class for various application resources.  It defines
 * common interface: questioning resource name using gwy_resource_get_name(),
 * modifiability using gwy_resource_is_modifiable(), expressing user's
 * favorites, loading resources from files and saving them.
 *
 * All resources of given kind are usually contained in the class inventory,
 * available through gwy_resource_type_get_inventory(), although they may also
 * exist as private objects the inventory knows nothing about.  In the first
 * case they are called managed resources while in the second they are called
 * free-standing.
 *
 * Managed resources come from gwy_resource_type_load() or they may be
 * built-in.  They can also be created using the inventory methods such as
 * gwy_inventory_copy().  They cannot be freely renamed, only renaming using
 * gwy_inventory_rename() is possible.  If a resource is removed from the
 * inventory but still referenced by other users, it becomes free-standing.
 *
 * Free-standing resources are created using gwy_resource_load(),
 * g_object_new(), deserialization and other direct methods.  They do not have
 * a resource file associated and they are not automatically saved.  If you
 * insert a free-standing resource to the class inventory, it becomes managed.
 * Before inserting, you must check whether its name its unique though.
 *
 * FIXME: The automated synchronization of managed resources to disk must be
 * described here.
 **/

/**
 * GwyResource:
 *
 * The #GwyResource struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyResourceClass:
 * @load_builtins: Loads built-in resources, i.e. those compiled into the
 *                 library either using #GResource or some specific means.
 *                 The class inventory must be set up beforehand.  Also note
 *                 creation of some default item usually occurs during the
 *                 inventory setup; this method loads additional built-in
 *                 resources.
 * @setup_inventory: Sets up the class inventory after its creation.  This
 *                   may involve for example setting the default item name and
 *                   inserting the built-in system items to the inventory.
 *                   The inventory passed to this function is always the class
 *                   inventory, given for convenience.
 * @copy: Creates copy of a resource.  For serializable resources, this should
 *        be the same as duplication.
 * @dump: Dumps resource data to text, the envelope is added by #GwyResource.
 * @parse: Parses back the text dump (provided as a line iterator), it parses
 *         only the actual data of the resource subclass, the envelope is
 *         handled by #GwyResource.
 *
 * Resource class.
 **/

/**
 * GWY_RESOURCE_ERROR:
 *
 * Error domain for resource operations.
 *
 * Errors in this domain will be from the #GwyResourceError enumeration.
 * See #GError for information on error domains.
 **/

/**
 * GwyResourceError:
 * @GWY_RESOURCE_ERROR_HEADER: Resource file has wrong or missing magic header.
 * @GWY_RESOURCE_ERROR_TYPE: Resource type is invalid.
 * @GWY_RESOURCE_ERROR_NAME: Resource name is invalid or missing.
 * @GWY_RESOURCE_ERROR_DUPLICIT: Resource conflicts with an already existing
 *                               resource of the same name.  Note
 *                               gwy_resource_load() does not return this
 *                               error because it does not put the resource to
 *                               the inventory.
 * @GWY_RESOURCE_ERROR_DATA: Resource data is invalid.  This error code is
 *                           only set by resource line parsers and otherwise it
 *                           is intended to be used for specific resource data
 *                           errors.
 *
 * Error codes returned by resource operations.
 **/

/**
 * GwyResourceLineType:
 * @GWY_RESOURCE_LINE_OK: Line is in the expected format.  This also means
 *                        the requested values have been filled.
 * @GWY_RESOURCE_LINE_NONE: There are no more lines.
 * @GWY_RESOURCE_LINE_INVALID: Line is invalid and the error was set.
 *
 * The type of resource file line parsing result.
 **/

/**
 * GwyResourceManagementType:
 * @GWY_RESOURCE_MANAGEMENT_NONE: Changed resources are not tracked apart from
 *                                setting GwyResource:modified and no
 *                                changes are written to disk automatically.
 *                                No queue of resources to save is maintained.
 * @GWY_RESOURCE_MANAGEMENT_MANUAL: A queue of resources to update on disk is
 *                                  maintained; creation and deletion of
 *                                  resources is immediately reflected on disk.
 *                                  However, resource data modification saving
 *                                  must be explicitly requested with
 *                                  gwy_resources_flush().
 * @GWY_RESOURCE_MANAGEMENT_MAIN: A queue of resources to update on disk is
 *                                maintained; creation and deletion of
 *                                resources is immediately reflected on disk.
 *                                Modified resources are automatically saved to
 *                                disk in periods of inactivity using
 *                                <link linkend='glib-The-Main-Event-Loop'>the
 *                                main event loop</link>.
 *
 * The type of resource management.
 *
 * The initial state is %GWY_RESOURCE_MANAGEMENT_NONE, i.e. you have to
 * excplitily enable resource management with
 * gwy_resources_set_management_type() if you want automated or semiautomated
 * management.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
