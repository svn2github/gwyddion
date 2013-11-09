/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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

#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/object-utils.h"
#include "libgwyapp/file.h"

// FIXME: Duplicated with data-list.c.
#define GWY_DATA_NKINDS (GWY_DATA_SURFACE+1)
#define MAX_FILE_ID 0xffffff

enum {
    PROP_0,
    PROP_ID,
    N_PROPS
};

struct _GwyFilePrivate {
    GwyDataList *data_lists[GWY_DATA_NKINDS];
    guint id;
};

typedef struct _GwyFilePrivate File;

static void       gwy_file_finalize    (GObject *object);
static void       gwy_file_dispose     (GObject *object);
static void       gwy_file_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec);
static void       gwy_file_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec);
static void       assign_next_free_id  (GwyFile *file);
static GwyFile*   find_file_by_id      (guint file_id);
static GPtrArray* ensure_file_list     (void);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyFile, gwy_file, G_TYPE_OBJECT);

G_LOCK_DEFINE(file_list);

static void
gwy_file_class_init(GwyFileClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(File));

    gobject_class->dispose = gwy_file_dispose;
    gobject_class->finalize = gwy_file_finalize;
    gobject_class->get_property = gwy_file_get_property;
    gobject_class->set_property = gwy_file_set_property;

    properties[PROP_ID]
        = g_param_spec_uint("id",
                            "Id",
                            "Unique file identified within the running "
                            "program.",
                            1, MAX_FILE_ID, 1,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_file_init(GwyFile *file)
{
    file->priv = G_TYPE_INSTANCE_GET_PRIVATE(file, GWY_TYPE_FILE, File);
    assign_next_free_id(file);
}

static void
gwy_file_finalize(GObject *object)
{
    GwyFile *file = GWY_FILE(object);
    G_LOCK(file_list);
    GPtrArray *files = ensure_file_list();
    g_ptr_array_remove(files, file);
    G_UNLOCK(file_list);
    G_OBJECT_CLASS(gwy_file_parent_class)->finalize(object);
}

static void
gwy_file_dispose(GObject *object)
{
    GwyFile *file = GWY_FILE(object);
    File *priv = file->priv;
    for (guint i = 0; i < GWY_DATA_NKINDS; i++) {
        GWY_OBJECT_UNREF(priv->data_lists[i]);
    }
    G_OBJECT_CLASS(gwy_file_parent_class)->dispose(object);
}

static void
gwy_file_set_property(GObject *object,
                      guint prop_id,
                      G_GNUC_UNUSED const GValue *value,
                      GParamSpec *pspec)
{
    //GwyFile *file = GWY_FILE(object);

    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_file_get_property(GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
    GwyFile *file = GWY_FILE(object);
    File *priv = file->priv;

    switch (prop_id) {
        case PROP_ID:
        g_value_set_uint(value, priv->id);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_file_new:
 *
 * Creates a new data file representation.
 *
 * This method does not create any on-disk files as #GwyFile represents file
 * data loaded into memory.
 *
 * Returns: (transfer full):
 *          A new data file representation.
 **/
GwyFile*
gwy_file_new(void)
{
    return g_object_newv(GWY_TYPE_FILE, 0, NULL);
}

/**
 * gwy_file_get_id:
 * @file: A loaded data file.
 *
 * Gets the identifier of a loaded data file.
 *
 * Returns: The unique identifier.
 **/
guint
gwy_file_get_id(const GwyFile *file)
{
    g_return_val_if_fail(GWY_IS_FILE(file), 0);
    return file->priv->id;
}

/**
 * gwy_files_find_file:
 * @file_id: Unique identifier of a loaded file.
 *
 * Obtains a loaded file from its unique identifier.
 *
 * It is permitted to pass an identifier that does not correspond to any file,
 * the function returns %NULL then.
 *
 * Returns: (allow-none) (transfer none):
 *          The loaded file with identifier @file_id, or %NULL.
 **/
GwyFile*
gwy_files_find_file(guint file_id)
{
    G_LOCK(file_list);
    GwyFile *file = find_file_by_id(file_id);
    G_UNLOCK(file_list);
    return file;
}

/**
 * gwy_file_get_data_list:
 * @file: A loaded data file.
 * @kind: Data kind.
 *
 * Gets the list of data of given type in a loaded data file.
 *
 * Returns: (transfer none):
 *          The list of data of @kind kind.
 **/
GwyDataList*
gwy_file_get_data_list(const GwyFile *file,
                       GwyDataKind kind)
{
    g_return_val_if_fail(GWY_IS_FILE(file), NULL);
    g_return_val_if_fail(kind < GWY_DATA_NKINDS, NULL);
    return file->priv->data_lists[kind];
}

static void
assign_next_free_id(GwyFile *file)
{
    static guint file_id = 0;

    G_LOCK(file_list);
    GPtrArray *files = ensure_file_list();
    do {
        ++file_id;
        if (G_UNLIKELY(file_id == MAX_FILE_ID))
            file_id = 1;
    } while (G_UNLIKELY(find_file_by_id(file_id)));
    file->priv->id = file_id;
    g_ptr_array_add(files, file);
    G_UNLOCK(file_list);
}

// This function must be called with @file_list lock held.
static GwyFile*
find_file_by_id(guint file_id)
{
    GPtrArray *files = ensure_file_list();

    for (guint i = 0; i < files->len; i++) {
        GwyFile *file = (GwyFile*)files->pdata[i];
        if (file->priv->id == file_id)
            return file;
    }

    return NULL;
}

// This function must be called with @file_list lock held.
static GPtrArray*
ensure_file_list(void)
{
    static GPtrArray *files = NULL;

    if (!G_UNLIKELY(!files))
        files = g_ptr_array_new();

    return files;
}

/************************** Documentation ****************************/

/**
 * SECTION: file
 * @title: GwyFile
 * @short_description: Loaded data file
 *
 * #GwyFile represents one data file, loaded or created from whatever source
 * and present in the memory.
 **/

/**
 * GwyFile:
 *
 * Object representing a loaded data file.
 *
 * The #GwyFile struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyFileClass:
 *
 * Class of loaded data files.
 **/

/**
 * GwyDataId:
 * @gid: Unique identificator of any kind of data within the program.
 * @type: Integer type of the data.
 * @fileno: Identifier of a loaded file.
 * @datano: Identifier of the data within the file.
 *
 * Unique idenitfier of any data object type within one program instance.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
