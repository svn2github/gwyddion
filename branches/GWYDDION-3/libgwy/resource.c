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
#include <stdio.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "libgwy/macros.h"
#include "libgwy/main.h"
#include "libgwy/strfuncs.h"
#include "libgwy/resource.h"
#include "libgwy/libgwy-aliases.h"

#define MAGIC_HEADER2 "Gwyddion resource "
#define MAGIC_HEADER3 "Gwyddion3 resource "

#define STATIC \
    (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)

enum {
    DATA_CHANGED,
    N_SIGNALS
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_FILE_NAME,
    PROP_IS_PREFERRED,
    PROP_IS_MODIFIABLE,
    PROP_IS_MODIFIED,
    N_PROPS
};

static void              gwy_resource_finalize          (GObject *object);
static void              gwy_resource_set_property      (GObject *object,
                                                         guint prop_id,
                                                         const GValue *value,
                                                         GParamSpec *pspec);
static void              gwy_resource_get_property      (GObject *object,
                                                         guint prop_id,
                                                         GValue *value,
                                                         GParamSpec *pspec);
static gboolean          gwy_resource_is_modifiable_impl(gconstpointer item);
static const gchar*      gwy_resource_get_item_name     (gconstpointer item);
static gboolean          gwy_resource_compare           (gconstpointer item1,
                                                         gconstpointer item2);
static void              gwy_resource_rename            (gpointer item,
                                                         const gchar *new_name);
static const GType*      gwy_resource_get_traits        (guint *ntraits);
static const gchar*      gwy_resource_get_trait_name    (guint i);
static void              gwy_resource_get_trait_value   (gconstpointer item,
                                                         guint i,
                                                         GValue *value);
static GwyResourceClass* get_resource_class             (const gchar *typename,
                                                         GType expected_type,
                                                         const gchar *filename_sys,
                                                         GError **error);
static GwyResource*      parse                          (gchar *text,
                                                         GType expected_type,
                                                         const gchar *filename,
                                                         GError **error);
static gboolean          name_is_unique                 (GwyResource *resource,
                                                         GwyResourceClass *klass,
                                                         GError **error);
static gchar*            construct_filename             (const gchar *resource_name);
static void              gwy_resource_data_changed_impl (GwyResource *resource);

/* Use a static propery -> trait map.  We could do it generically, too.
 * g_param_spec_pool_list() is ugly and slow is the minor problem, the major
 * is that it does g_hash_table_foreach() so we would get different orders
 * on different invocations and have to sort it. */
static GType gwy_resource_trait_types[N_PROPS-1];
static const gchar *gwy_resource_trait_names[N_PROPS-1];
static guint gwy_resource_ntraits = 0;

static guint resource_signals[N_SIGNALS];

static GSList *all_resources = NULL;
G_LOCK_DEFINE(all_resources);

static const GwyInventoryItemType gwy_resource_item_type = {
    0,
    "data-changed",
    &gwy_resource_get_item_name,
    &gwy_resource_is_modifiable_impl,
    &gwy_resource_compare,
    &gwy_resource_rename,
    NULL,  /* FIXME: Should mark the resource as unmanaged. */
    NULL,  /* copy needs particular class */
    &gwy_resource_get_traits,
    &gwy_resource_get_trait_name,
    &gwy_resource_get_trait_value,
};

G_DEFINE_ABSTRACT_TYPE(GwyResource, gwy_resource, G_TYPE_OBJECT)

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
gwy_resource_class_init(GwyResourceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GParamSpec *pspec;

    gobject_class->finalize = gwy_resource_finalize;
    gobject_class->get_property = gwy_resource_get_property;
    gobject_class->set_property = gwy_resource_set_property;

    klass->item_type = gwy_resource_item_type;
    klass->item_type.type = G_TYPE_FROM_CLASS(klass);
    klass->data_changed = gwy_resource_data_changed_impl;

    pspec = g_param_spec_string("name",
                                "Name",
                                "Resource name",
                                NULL, /* What is the default value good for? */
                                G_PARAM_READABLE | STATIC);
    g_object_class_install_property(gobject_class, PROP_NAME, pspec);
    gwy_resource_trait_types[gwy_resource_ntraits] = pspec->value_type;
    gwy_resource_trait_names[gwy_resource_ntraits] = pspec->name;
    gwy_resource_ntraits++;

    pspec = g_param_spec_string("file-name",
                                "File name",
                                "Name of file corresponding to the resource "
                                "in GLib encoding, it may be NULL for "
                                "built-in or newly created resourced.",
                                NULL, /* What is the default value good for? */
                                G_PARAM_READABLE | STATIC);
    g_object_class_install_property(gobject_class, PROP_FILE_NAME, pspec);
    gwy_resource_trait_types[gwy_resource_ntraits] = pspec->value_type;
    gwy_resource_trait_names[gwy_resource_ntraits] = pspec->name;
    gwy_resource_ntraits++;

    pspec = g_param_spec_boolean("is-preferred",
                                 "Is preferred",
                                 "Whether a resource is preferred",
                                 FALSE,
                                 G_PARAM_READWRITE | STATIC);
    g_object_class_install_property(gobject_class, PROP_IS_PREFERRED, pspec);
    gwy_resource_trait_types[gwy_resource_ntraits] = pspec->value_type;
    gwy_resource_trait_names[gwy_resource_ntraits] = pspec->name;
    gwy_resource_ntraits++;

    pspec = g_param_spec_boolean("is-modifiable",
                                 "Is modifiable",
                                 "Whether a resource is modifiable",
                                 FALSE,
                                 G_PARAM_READWRITE
                                 | G_PARAM_CONSTRUCT_ONLY | STATIC);
    g_object_class_install_property(gobject_class, PROP_IS_MODIFIABLE, pspec);
    gwy_resource_trait_types[gwy_resource_ntraits] = pspec->value_type;
    gwy_resource_trait_names[gwy_resource_ntraits] = pspec->name;
    gwy_resource_ntraits++;

    pspec = g_param_spec_boolean("is-modified",
                                 "Is modified",
                                 "Whether a resource was modified, this is "
                                 "set when data-changed signal is emitted",
                                 FALSE,
                                 G_PARAM_READABLE | STATIC);
    g_object_class_install_property(gobject_class, PROP_IS_MODIFIED, pspec);

    /**
     * GwyResource::data-changed:
     * @gwyresource: The #GwyResource which received the signal.
     *
     * The ::data-changed signal is emitted when resource data changes.
     */
    resource_signals[DATA_CHANGED]
        = g_signal_new("data-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyResourceClass, data_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_resource_init(G_GNUC_UNUSED GwyResource *resource)
{
}

static void
gwy_resource_finalize(GObject *object)
{
    GwyResource *resource = (GwyResource*)object;

    if (resource->use_count)
        g_critical("Resource %p with nonzero use_count is finalized.", object);
    GWY_FREE(resource->name);
    GWY_FREE(resource->filename);

    G_OBJECT_CLASS(gwy_resource_parent_class)->finalize(object);
}

static void
gwy_resource_set_property(GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    GwyResource *resource = GWY_RESOURCE(object);

    switch (prop_id) {
        case PROP_IS_MODIFIABLE:
        resource->is_modifiable = g_value_get_boolean(value);
        break;

        case PROP_IS_PREFERRED:
        gwy_resource_set_is_preferred(resource, g_value_get_boolean(value));
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

    switch (prop_id) {
        case PROP_NAME:
        g_value_set_string(value, resource->name);
        break;

        case PROP_FILE_NAME:
        g_value_set_string(value, resource->filename);
        break;

        case PROP_IS_PREFERRED:
        g_value_set_boolean(value, resource->is_preferred);
        break;

        case PROP_IS_MODIFIABLE:
        g_value_set_boolean(value, resource->is_modifiable);
        break;

        case PROP_IS_MODIFIED:
        g_value_set_boolean(value, resource->is_modified);
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
    return resource->name;
}

static gboolean
gwy_resource_is_modifiable_impl(gconstpointer item)
{
    GwyResource *resource = (GwyResource*)item;
    return resource->is_modifiable;
}

static gboolean
gwy_resource_compare(gconstpointer item1,
                     gconstpointer item2)
{
    GwyResource *resource1 = (GwyResource*)item1;
    GwyResource *resource2 = (GwyResource*)item2;

    return g_utf8_collate(resource1->name, resource2->name);
}

static void
gwy_resource_rename(gpointer item,
                    const gchar *new_name)
{
    GwyResource *resource = (GwyResource*)item;

    g_return_if_fail(resource->is_modifiable);

    GWY_FREE(resource->name);
    resource->name = g_strdup(new_name);
    g_object_notify(G_OBJECT(item), "name");
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

/**
 * gwy_resource_get_name:
 * @resource: A resource.
 *
 * Returns the resource name.
 *
 * Returns: Name of @resource.  The string is owned by @resource and must not
 *          be modfied or freed.
 **/
const gchar*
gwy_resource_get_name(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), NULL);
    return resource->name;
}

/**
 * gwy_resource_is_modifiable:
 * @resource: A resource.
 *
 * Returns whether a resource is modifiable.
 *
 * Returns: %TRUE if resource is modifiable, %FALSE if it's fixed (system)
 *          resource.
 **/
gboolean
gwy_resource_is_modifiable(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), FALSE);
    return resource->is_modifiable;
}

/**
 * gwy_resource_get_is_preferred:
 * @resource: A resource.
 *
 * Returns whether a resource is preferred.
 *
 * Returns: %TRUE if resource is preferred, %FALSE otherwise.
 **/
gboolean
gwy_resource_get_is_preferred(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), FALSE);
    return resource->is_preferred;
}

/**
 * gwy_resource_set_is_preferred:
 * @resource: A resource.
 * @is_preferred: %TRUE to make @resource preferred, %FALSE to make it not
 *                preferred.
 *
 * Sets the preferability of a resource.
 **/
void
gwy_resource_set_is_preferred(GwyResource *resource,
                              gboolean is_preferred)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    resource->is_preferred = !!is_preferred;
    g_object_notify(G_OBJECT(resource), "is-preferred");
}

/**
 * gwy_resource_class_get_name:
 * @klass: A resource class.
 *
 * Gets the name of a resource class.
 *
 * This is an simple identifier usable for example as directory name.
 *
 * Returns: Resource class name, as a constant string that must not be modified
 *          nor freed.
 **/
const gchar*
gwy_resource_class_get_name(GwyResourceClass *klass)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), NULL);
    return klass->name;
}

/**
 * gwy_resource_class_get_inventory:
 * @klass: A resource class.
 *
 * Gets the inventory which holds resources of a resource class.
 *
 * Returns: Resource class inventory.
 **/
GwyInventory*
gwy_resource_class_get_inventory(GwyResourceClass *klass)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), NULL);
    return klass->inventory;
}

/**
 * gwy_resource_class_get_item_type:
 * @klass: A resource class.
 *
 * Gets the inventory item type for a resource class.
 *
 * Returns: Inventory item type.
 **/
const GwyInventoryItemType*
gwy_resource_class_get_item_type(GwyResourceClass *klass)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), NULL);
    return &klass->item_type;
}

/**
 * gwy_resource_use:
 * @resource: A resource.
 *
 * Starts using a resource.
 *
 * Calling this function is necessary to use a resource properly.
 * It tells the resource to create any auxiliary structures that consume
 * considerable amount of memory and perform other initialization to
 * ready-to-use form.
 *
 * In addition, it calls g_object_ref() on the resource.
 *
 * When a resource is no longer used, it should be discarded with
 * gwy_resource_discard() which releases the auxilary data.
 *
 * Although resources often exist through almost whole program lifetime from
 * #GObject perspective, from the viewpoint of their use this method plays the
 * role of constructor and gwy_resource_discard() is the destructor.
 **/
void
gwy_resource_use(GwyResource *resource)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));

    g_object_ref(resource);
    if (!resource->use_count++) {
        void (*method)(GwyResource*);

        method = GWY_RESOURCE_GET_CLASS(resource)->use;
        if (method)
            method(resource);
    }
}

/**
 * gwy_resource_discard:
 * @resource: A resource.
 *
 * Releases a resource.
 *
 * When the number of resource uses drops to zero, it frees all auxiliary data
 * and returns back to `latent' form.  In addition, it calls g_object_unref()
 * on it.  See gwy_resource_use() for more.
 **/
void
gwy_resource_discard(GwyResource *resource)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    g_return_if_fail(resource->use_count);

    if (!--resource->use_count) {
        void (*method)(GwyResource*);

        method = GWY_RESOURCE_GET_CLASS(resource)->discard;
        if (method)
            method(resource);
    }
    g_object_unref(resource);
}

/**
 * gwy_resource_is_used:
 * @resource: A resource.
 *
 * Tells whether a resource is currently in use.
 *
 * See gwy_resource_use() for details.
 *
 * Returns: %TRUE if resource is in use, %FALSE otherwise.
 **/
gboolean
gwy_resource_is_used(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), FALSE);
    return resource->use_count > 0;
}

/**
 * gwy_resource_save:
 * @resource: A resource.
 * @error: Location to store the error occuring, %NULL to ignore.  Errors from
 *         #GFileError domains can occur.
 *
 * Saves a resource.
 *
 * The resource is actually save only if the @is_modified flag it set.
 * If it fails to save self, the flag is kept set, otherwise it is cleared.
 * Only modifiable resources can be saved and the file name is determined
 * automatically (if the resources was loaded from a file originally, it will
 * be saved to the same file).
 *
 * Returns: %TRUE if the operation succeeded, %FALSE if it failed.  Not saving
 *          the resource because it was not modified counts as success.
 **/
gboolean
gwy_resource_save(GwyResource *resource,
                  GError **error)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), FALSE);
    g_return_val_if_fail(resource->is_modifiable, FALSE);

    if (!resource->is_modified)
        return TRUE;

    GwyResourceClass *klass = GWY_RESOURCE_GET_CLASS(resource);
    g_return_val_if_fail(klass && klass->dump, FALSE);

    gchar *body = klass->dump(resource);
    gchar *buffer = g_strconcat(MAGIC_HEADER3,
                                G_OBJECT_TYPE_NAME(resource),
                                "\n",
                                "name ",
                                resource->name,
                                "\n",
                                body,
                                NULL);
    g_free(body);

    if (!resource->filename) {
        gchar *dirname = gwy_user_directory(klass->name);
        gchar *basename_sys = construct_filename(resource->name);
        gchar *filename_sys = g_build_filename(dirname, basename_sys, NULL);
        g_free(dirname);
        g_free(basename_sys);

        /* Avoid the numbering loop in the simple case. */
        if (g_file_test(filename_sys, G_FILE_TEST_EXISTS)) {
            GString *str = g_string_new(filename_sys);
            gsize len = str->len;

            for (guint i = 1; ; i++) {
                g_string_truncate(str, len);
                g_string_append_printf(str, "-%u", i);

                if (!g_file_test(filename_sys, G_FILE_TEST_EXISTS))
                    break;
            }

            g_free(filename_sys);
            filename_sys = g_string_free(str, FALSE);
        }
        resource->filename = filename_sys;
    }

    gboolean ok = g_file_set_contents(resource->filename, buffer, -1, error);
    g_free(buffer);

    return ok;
}

static gchar*
construct_filename(const gchar *resource_name)
{
    /*
     * The name must be representable on any file system and in any code page.
     * Therefore, it must consists of ASCII letters only!
     */
    static const gchar fallback_name[] = "Untitled";

    g_return_val_if_fail(resource_name, g_strdup(fallback_name));

    /* First get rid of bad ASCII characers because we know the encoding now.
     * We might have troubles filtering them after the conversion.   Be very
     * conservative, the name does not matter. */
    gsize len = strlen(resource_name);
    gchar *resname = g_slice_alloc(sizeof(gchar)*(len + 1));

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
    g_free(resname);

    if (!str->len || (str->len == 1 && garbage))
        g_string_assign(str, fallback_name);

    return g_string_free(str, FALSE);;
}

/**
 * gwy_resource_load:
 * @filename_sys: Name of resource file to load, in GLib encoding.
 * @expected_type: Expected resource type.  This must be a valid type, however,
 *                 it is possible to pass %GWY_TYPE_RESOURCE which effectively
 *                 accepts all possible resources.
 * @error: Location to store the error occuring, %NULL to ignore.  Errors from
 *         #GwyResourceError and #GFileError domains can occur.
 *
 * Loads a resource from a file.
 *
 * The resource is not added to the inventory neither is its name checked
 * for uniqueness.
 *
 * Returns: A newly created resource object.
 **/
GwyResource*
gwy_resource_load(const gchar *filename_sys,
                  GType expected_type,
                  GError **error)
{
    GwyResource *resource = NULL;
    gchar *text = NULL;

    if (g_file_get_contents(filename_sys, &text, NULL, error))
        resource = parse(text, expected_type, filename_sys, error);

    GWY_FREE(text);
    return resource;
}

static GwyResource*
parse(gchar *text,
      GType expected_type,
      const gchar *filename_sys,
      GError **error)
{
    gchar *line = gwy_str_next_line(&text);

    if (!line) {
        gchar *filename_utf8 = g_filename_to_utf8(filename_sys, -1,
                                                  NULL, NULL, NULL);
        g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_HEADER,
                    _("Wrong or missing resource magic header in file ‘%s’."),
                    filename_utf8);
        g_free(filename_utf8);
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
        gchar *filename_utf8 = g_filename_to_utf8(filename_sys, -1,
                                                  NULL, NULL, NULL);
        g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_HEADER,
                    _("Wrong or missing resource magic header in file ‘%s’."),
                    filename_utf8);
        g_free(filename_utf8);
        return NULL;
    }

    /* Resource type */
    line += strspn(line, " \t");
    const gchar *typename = line;
    line = gwy_str_next_line(&text);
    if (G_UNLIKELY(!line)) {
        gchar *filename_utf8 = g_filename_to_utf8(filename_sys, -1,
                                                  NULL, NULL, NULL);
        g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_HEADER,
                    _("Resource header of file ‘%s’ is truncated."),
                    filename_utf8);
        g_free(filename_utf8);
        return NULL;
    }

    /* Parse the name in v3 resources. */
    gchar *name = NULL;
    if (version == 3) {
        guint len = sizeof("name")-1;
        line = gwy_str_next_line(&text);
        if (!line
            || !g_strstrip(line)
            || !strncmp(line, "name", len)
            || !g_ascii_isspace(line[len])) {
            gchar *filename_utf8 = g_filename_to_utf8(filename_sys, -1,
                                                      NULL, NULL, NULL);
            g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME,
                        _("Resource name is missing for "
                          "version 3 resource ‘%s’ in file ‘%s’."),
                        typename, filename_utf8);
            return NULL;
        }

        line += len;
        line += strspn(line, " \t");
        name = line;
        if (!strlen(name)) {
            gchar *filename_utf8 = g_filename_to_utf8(filename_sys, -1,
                                                      NULL, NULL, NULL);
            g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME,
                        _("Resource name is missing for "
                          "version 3 resource ‘%s’ in file ‘%s’."),
                        typename, filename_utf8);
            return NULL;
        }

        if (!g_utf8_validate(name, len, NULL)) {
            gchar *filename_utf8 = g_filename_to_utf8(filename_sys, -1,
                                                      NULL, NULL, NULL);
            g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME,
                        _("Resource name is not valid UTF-8 for "
                          "version 3 resource ‘%s’ in file ‘%s’."),
                        typename, filename_utf8);
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
    if ((klass->parse(resource, text, error))) {
        if (name) {
            resource->name = g_strdup(name);
        }
        else {
            name = g_path_get_basename(filename_sys);
            resource->name = g_filename_to_utf8(name, -1, NULL, NULL, NULL);
            g_free(name);
        }
    }
    else {
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

    if (G_UNLIKELY(!type)) {
        gchar *filename_utf8 = g_filename_to_utf8(filename_sys, -1,
                                                  NULL, NULL, NULL);
        g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_TYPE,
                    _("Resource type ‘%s’ of file ‘%s’ is invalid."),
                   filename_utf8, typename);
        g_free(filename_utf8);
        return NULL;
    }

    /* Does it make sense to accept subclasses? */
    if (G_UNLIKELY(g_type_is_a(!type, expected_type))) {
        gchar *filename_utf8 = g_filename_to_utf8(filename_sys, -1,
                                                  NULL, NULL, NULL);
        g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_TYPE,
                    _("Resource type ‘%s’ of file ‘%s’ does not match "
                      "the expected type ‘%s’."),
                    filename_utf8, typename, g_type_name(expected_type));
        g_free(filename_utf8);
        return NULL;
    }

    /* If the resource type matches the requested type yet the type is invalid
     * this is a programmer's error.  Fail ungracefully.  */
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
 * sets @is_modified flag on the resource.
 *
 * This function is primarily intended for resource implementation.
 **/
void
gwy_resource_data_changed(GwyResource *resource)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    g_signal_emit(resource, resource_signals[DATA_CHANGED], 0);
}

static void
gwy_resource_data_changed_impl(GwyResource *resource)
{
    if (!resource->is_modifiable)
        g_warning("Constant resource ‘%s’ of type %s was modified",
                  resource->name, G_OBJECT_TYPE_NAME(resource));
    resource->is_modified = TRUE;
}

/**
 * gwy_resource_class_load:
 * @klass: A resource class.
 *
 * Loads resources of a resources class from disk.
 *
 * Resources are loaded from system directory (and marked constant) and from
 * user directory (marked modifiable).
 **/
void
gwy_resource_class_load(GwyResourceClass *klass)
{
    g_return_if_fail(GWY_IS_RESOURCE_CLASS(klass));
    g_return_if_fail(klass->inventory);

    gpointer type = GSIZE_TO_POINTER(G_TYPE_FROM_CLASS(klass));
    G_LOCK(all_resources);
    if (!g_slist_find(all_resources, type))
        all_resources = g_slist_prepend(all_resources, type);
    G_UNLOCK(all_resources);

    gchar *userdir = gwy_user_directory(klass->name);
    gchar **dirs = gwy_data_search_path(klass->name);
    for (gchar **d = dirs; *d; d++) {
        gboolean writable = userdir && gwy_strequal(*d, userdir);
        gwy_resource_class_load_directory(klass, *d, writable, NULL);
    }
    if (!g_file_test(userdir, G_FILE_TEST_IS_DIR))
        g_mkdir_with_parents(userdir, 0700);
    GWY_FREE(userdir);
    g_strfreev(dirs);
}

/**
 * gwy_resource_class_load_directory:
 * @klass: Resource klass of the resources in the directory.
 * @dirname: Directory to load resources from.
 * @modifiable: %TRUE to create resources as modifiable, %FALSE to create them
 *              as fixed.
 * @error_list: Location to store the errors occuring, %NULL to ignore.
 *
 * Loads all resources of given class from a directory.
 *
 * All files in the directory (except hidden and backup files as defined by
 * GIO %G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN and
 * %G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP) are assumed to be resources of class
 * @klass.  Subdirectories are ignored.
 **/
void
gwy_resource_class_load_directory(GwyResourceClass *klass,
                                  const gchar *dirname_sys,
                                  gboolean modifiable,
                                  GwyErrorList **error_list)
{
    GFile *gfile = g_file_new_for_path(dirname_sys);
    GFileEnumerator *dir
        = g_file_enumerate_children(gfile,
                                    G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                    G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                    G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
                                    G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP,
                                    0, NULL, NULL);
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
        GwyResource *resource
            = gwy_resource_load(filename_sys, G_TYPE_FROM_CLASS(klass), &error);

        if (G_LIKELY(resource) && name_is_unique(resource, klass, &error)) {
            resource->is_modifiable = modifiable;
            resource->filename = filename_sys;
            resource->is_modified = FALSE;
            gwy_inventory_insert(klass->inventory, resource);
            g_object_unref(resource);
        }
        else {
            gwy_error_list_propagate(error_list, error);
            g_free(filename_sys);
        }
        g_object_unref(fileinfo);
    }
    g_object_unref(dir);
}

/* Check if the name does not conflict with an existing resource. */
static gboolean
name_is_unique(GwyResource *resource,
               GwyResourceClass *klass,
               GError **error)
{
    GwyResource *obstacle = gwy_inventory_get(klass->inventory, resource->name);
    if (!obstacle)
        return TRUE;

    /* Do not bother with message formatting when errors are ingored. */
    if (!error)
        return FALSE;

    gchar *filename_utf8 = g_filename_to_utf8(resource->filename, -1,
                                              NULL, NULL, NULL);
    gchar *ofilename_utf8 = (obstacle->filename
                             ? g_filename_to_utf8(obstacle->filename, -1,
                                                  NULL, NULL, NULL)
                             : "<internal>");
    g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DUPLICIT,
                _("Resource named ‘%s’ loaded from file ‘%s’ "
                  "conflicts with already loaded resource from ‘%s’."),
                G_OBJECT_TYPE_NAME(resource), filename_utf8, ofilename_utf8);
    g_free(filename_utf8);
    if (obstacle->filename)
        g_free(ofilename_utf8);

    return FALSE;
}

/**
 * gwy_resource_classes_finalize:
 *
 * Destroys the inventories of all resource classes.
 *
 * This function makes the affected resource classes unusable.  Its purpose is
 * to faciliate reference leak debugging by destroying a large number of
 * objects that normally live forever.
 *
 * Note static resource classes that never called gwy_resource_class_load()
 * are excluded.
 *
 * It quite quite defeat the purpose of this function if the program was busy
 * loading more resource classes in other threads when it is executed.  So, no
 * locking is used and just do not do this.
 **/
void
gwy_resource_classes_finalize(void)
{
    GSList *l;

    for (l = all_resources; l; l = g_slist_next(l)) {
        GwyResourceClass *klass;

        klass = g_type_class_ref((GType)GPOINTER_TO_SIZE(all_resources->data));
        GWY_OBJECT_UNREF(klass->inventory);
    }

    g_slist_free(all_resources);
    all_resources = NULL;
}

/**
 * gwy_resource_parse_param_line:
 * @line: Text buffer containing a resource file line (writable).
 * @key: Location to store the key to.
 * @value: Location to store the value to.
 *
 * Extracts one key-value pair from a resource file.
 *
 * This is a helper function for resource file parsing.
 *
 * If it returns %GWY_RESOURCE_LINE_OK, @key and @value are pointed to
 * locations in @line where the key and value on the next resource line
 * starts.  The contents of @line is modified to make @key and @value stripped
 * NUL-terminated strings.
 *
 * In all other cases @key and @value are not set and @line is left intact.
 * Note a line containing no value is valid, @value will be set to the empty
 * string.
 *
 * Returns: The line type, any type can be returned except
 *          %GWY_RESOURCE_LINE_BAD_NUMBER.
 **/
GwyResourceLineType
gwy_resource_parse_param_line(gchar *line,
                              gchar **key,
                              gchar **value)
{
    // Empty?
    while (g_ascii_isspace(*line))
        line++;

    if (!*line)
        return GWY_RESOURCE_LINE_EMPTY;

    // Key.
    if (!g_ascii_isalpha(*line))
        return GWY_RESOURCE_LINE_BAD_KEY;

    gchar *s = line;
    while (g_ascii_isalnum(*s) || *s == '_')
        s++;

    if (!*s) {
        *key = line;
        *value = s;
        return GWY_RESOURCE_LINE_OK;
    }

    if (!g_ascii_isspace(*s))
        return GWY_RESOURCE_LINE_BAD_KEY;

    gchar *t = s;
    do {
        t++;
    } while (g_ascii_isspace(*t));

    // Value.
    if (!g_utf8_validate(t, -1, NULL))
        return GWY_RESOURCE_LINE_BAD_UTF8;

    *s = '\0';
    g_strchomp(t);
    *key = line;
    *value = t;
    return GWY_RESOURCE_LINE_OK;
}

/**
 * gwy_resource_parse_data_line:
 * @line: Text buffer containing a resource file line.
 * @ncolumns: Expected number of columns.
 * @data: Array of length @number to store the read values to.
 *
 * Extracts one row of floating point values from a resource file.
 *
 * This is a helper function for resource file parsing.
 *
 * If it returns %GWY_RESOURCE_LINE_OK, @data is filled with the parsed values.
 * They are parsed using g_ascii_strtod(), i.e. the numbers are expected to
 * be stored in POSIX format.
 *
 * In all other cases @key and @value are left untouched.
 *
 * Returns: The line type, it can be one of %GWY_RESOURCE_LINE_OK,
 *          %GWY_RESOURCE_LINE_EMPTY and %GWY_RESOURCE_LINE_BAD_NUMBER.
 **/
GwyResourceLineType
gwy_resource_parse_data_line(const gchar *line,
                             guint ncolumns,
                             gdouble *data)
{
    // Empty?
    while (g_ascii_isspace(*line))
        line++;

    if (!*line)
        return GWY_RESOURCE_LINE_EMPTY;

    // Read the data.
    for (guint i = 0; i < ncolumns; i++) {
        gchar *end;
        data[i] = g_ascii_strtod(line, &end);
        if (end == line)
            return GWY_RESOURCE_LINE_BAD_NUMBER;
        line = end;
    }
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

#define __LIBGWY_RESOURCE_C__
#include "libgwy/libgwy-aliases.c"

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
 **/

/**
 * GwyResource:
 *
 * The #GwyResource struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyResourceClass:
 * @inventory: Inventory with resources.
 * @name: Resource class name, usable as resource directory name for on-disk
 *        resources.
 * @item_type: Inventory item type.  Most fields are pre-filled, but namely
 *             @type and @copy must be filled by particular resource type.
 * @data_changed: "data-changed" class signal handler.
 * @use: gwy_resource_use() virtual method.
 * @discard: gwy_resource_discard() virtual method.
 * @dump: Dumps resource data to text, the envelope is added by #GwyResource.
 * @parse: Parses back the text dump, in parses only the
 *         actual resource data, the envelope is handled by #GwyResource.
 *         The method is permitted to modify the contents of @text.
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
 *                           not used by #GwyResource, it is intended for
 *                           resource implementations.
 *
 * Error codes returned by resource operations.
 **/

/**
 * GwyResourceLineType:
 * @GWY_RESOURCE_LINE_OK: Line is in the expected format.
 * @GWY_RESOURCE_LINE_EMPTY: Line contains only whitespace.
 * @GWY_RESOURCE_LINE_BAD_KEY: Line starts with characters that do not form
 *                             an identifier.
 * @GWY_RESOURCE_LINE_BAD_UTF8: The value part of line is not valid UTF-8 (the
 *                              key part is always ASCII).
 * @GWY_RESOURCE_LINE_BAD_NUMBER: It is not possible to parse the line into
 *                                the specified number of floating point
 *                                values.
 *
 * The type of resource file line parsing result.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
