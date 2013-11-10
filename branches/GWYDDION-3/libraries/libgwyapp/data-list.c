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
#include "libgwyapp/types.h"
#include "libgwyapp/file.h"
#include "libgwyapp/data.h"
#include "libgwyapp/data-list.h"

// FIXME: Duplicated with file.c.
#define GWY_DATA_NKINDS (GWY_DATA_SURFACE+1)

enum {
    PROP_0,
    PROP_DATA_KIND,
    PROP_DATA_TYPE,
    PROP_FILE,
    N_PROPS
};

struct _GwyDataListPrivate {
    GwyFile *parent_file;
    GwyDataKind kind;
    GType type;
};

typedef struct _GwyDataListPrivate DataList;

static void     gwy_data_list_finalize    (GObject *object);
static void     gwy_data_list_dispose     (GObject *object);
static void     gwy_data_list_set_property(GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec);
static void     gwy_data_list_get_property(GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec);
static gboolean set_data_type             (GwyDataList *datalist,
                                           GType type);
static gboolean set_file                  (GwyDataList *datalist,
                                           GwyFile *file);

static GParamSpec *properties[N_PROPS];

// FIXME: Must implement GwyListable
G_DEFINE_TYPE(GwyDataList, gwy_data_list, GWY_TYPE_ARRAY);

static void
gwy_data_list_class_init(GwyDataListClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(DataList));

    gobject_class->dispose = gwy_data_list_dispose;
    gobject_class->finalize = gwy_data_list_finalize;
    gobject_class->get_property = gwy_data_list_get_property;
    gobject_class->set_property = gwy_data_list_set_property;

    properties[PROP_DATA_TYPE]
        = g_param_spec_gtype("data-type",
                             "Data type",
                             "Type of data in the list.",
                             GWY_TYPE_DATA,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
                             | G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_DATA_KIND]
        = g_param_spec_enum("data-kind",
                            "Data kind",
                            "Kind of data in the list.",
                            GWY_TYPE_DATA_KIND,
                            GWY_DATA_UNKNOWN,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_FILE]
        = g_param_spec_object("file",
                              "File",
                              "The file the list belongs to.",
                              GWY_TYPE_FILE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
                              | G_PARAM_CONSTRUCT_ONLY);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_data_list_init(GwyDataList *datalist)
{
    datalist->priv = G_TYPE_INSTANCE_GET_PRIVATE(datalist,
                                                 GWY_TYPE_DATA_LIST, DataList);
}

static void
gwy_data_list_finalize(GObject *object)
{
    //GwyDataList *datalist = GWY_DATA_LIST(object);
    G_OBJECT_CLASS(gwy_data_list_parent_class)->finalize(object);
}

static void
gwy_data_list_dispose(GObject *object)
{
    //GwyDataList *datalist = GWY_DATA_LIST(object);
    G_OBJECT_CLASS(gwy_data_list_parent_class)->dispose(object);
}

static void
gwy_data_list_set_property(GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
    GwyDataList *datalist = GWY_DATA_LIST(object);

    switch (prop_id) {
        case PROP_DATA_TYPE:
        set_data_type(datalist, g_value_get_gtype(value));
        break;

        case PROP_FILE:
        set_file(datalist, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_data_list_get_property(GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
    GwyDataList *datalist = GWY_DATA_LIST(object);
    DataList *priv = datalist->priv;

    switch (prop_id) {
        case PROP_DATA_KIND:
        g_value_set_enum(value, priv->kind);
        break;

        case PROP_DATA_TYPE:
        g_value_set_gtype(value, priv->type);
        break;

        case PROP_FILE:
        g_value_set_object(value, priv->parent_file);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_data_list_new: (constructor)
 *
 * Creates a new data list for one kind of data.
 *
 * This constructor exists namely for bindings.  Usually, it is better to
 * create the data list directly for a specific type of data using
 * gwy_data_list_new_for_type().
 *
 * Returns: (transfer full):
 *          A new data list.
 **/
GwyDataList*
gwy_data_list_new(void)
{
    return g_object_newv(GWY_TYPE_DATA_LIST, 0, NULL);
}

/**
 * gwy_data_list_new_for_file: (constructor)
 * @file: The file the data list will belong to.
 * @type: Type of data this list will hold.  It must be derived from #GwyData.
 *
 * Creates a new data list for data of specified type and file.
 *
 * Returns: (transfer full):
 *          A new data list.
 **/
GwyDataList*
gwy_data_list_new_for_file(GwyFile *file,
                           GType type)
{
    return g_object_new(GWY_TYPE_DATA_LIST,
                        "file", file,
                        "type", type,
                        NULL);
}

/**
 * gwy_data_list_set_data_type:
 * @datalist: A list of one kind of data.
 * @type: Type of data this list will hold.  It must be derived from #GwyData.
 *
 * Sets the type a data list with yet undefined type of data will hold.
 *
 * Once the type of data is set, either upon construction or using this method,
 * it cannot be changed.  It is possible to call this method repeatedly with
 * the same type tough.
 **/
void
gwy_data_list_set_data_type(GwyDataList *datalist,
                            GType type)
{
    g_return_if_fail(GWY_IS_DATA_LIST(datalist));
    if (!set_data_type(datalist, type))
        return;

    g_object_notify_by_pspec(G_OBJECT(datalist), properties[PROP_DATA_TYPE]);
}

/**
 * gwy_data_list_get_data_type:
 * @datalist: A list of one kind of data.
 *
 * Gets the type of data in a list of one kind of data.
 *
 * Returns: The type of data stored in this list.
 **/
GType
gwy_data_list_get_data_type(const GwyDataList *datalist)
{
    g_return_val_if_fail(GWY_IS_DATA_LIST(datalist), 0);
    return datalist->priv->type;
}

/**
 * gwy_data_list_get_data_kind:
 * @datalist: A list of one kind of data.
 *
 * Gets the kind of data in a list of one kind of data.
 *
 * Returns: The kind of data stored in this list.
 **/
GwyDataKind
gwy_data_list_get_data_kind(const GwyDataList *datalist)
{
    g_return_val_if_fail(GWY_IS_DATA_LIST(datalist), GWY_DATA_UNKNOWN);
    return datalist->priv->kind;
}

/**
 * gwy_data_list_get_ids:
 * @datalist: A list of one kind of data.
 * @nids: (out):
 *        Location to store the number of returned items.
 *
 * Gets the identifiers of all data objects in the list.
 *
 * Returns: (array length=nids) (transfer full):
 *          Newly allocated array containing object identifiers.
 **/
guint*
gwy_data_list_get_ids(const GwyDataList *datalist,
                      guint *nids)
{
    g_return_val_if_fail(GWY_IS_DATA_LIST(datalist), NULL);
    *nids = 0;
    return NULL;
}

/**
 * gwy_data_kinds_n_kinds:
 *
 * Obtains the number of possible data kinds.
 *
 * The data kinds are enumerated in #GwyDataKind enum.  This function may be
 * useful to generic data enumeration that works even with data kinds added
 * later.
 *
 * Returns: The number of possible data kinds.
 **/
guint
gwy_data_kinds_n_kinds(void)
{
    return GWY_DATA_NKINDS;
}

static gboolean
set_data_type(GwyDataList *datalist, GType type)
{
    DataList *priv = datalist->priv;
    // This also makes passing zero type OK.
    if (type == priv->type)
        return FALSE;
    g_return_val_if_fail(!priv->type, FALSE);
    g_return_val_if_fail(g_type_is_a(type, GWY_TYPE_DATA), FALSE);
    g_return_val_if_fail(!G_TYPE_IS_ABSTRACT(type), FALSE);

    priv->type = type;
    // TODO
    priv->kind = 12345;
    g_object_notify_by_pspec(G_OBJECT(datalist), properties[PROP_DATA_KIND]);
    return TRUE;
}

static gboolean
set_file(GwyDataList *datalist,
         GwyFile *file)
{
    DataList *priv = datalist->priv;
    if (file == priv->parent_file)
        return FALSE;

    g_return_val_if_fail(!priv->parent_file, FALSE);
    priv->parent_file = file;
    // FIXME: We should hold a weak ref/pointer to avoid file becoming
    // invalid.
    return TRUE;
}

/************************** Documentation ****************************/

/**
 * SECTION: data-list
 * @title: GwyDataList
 * @short_description: List of one kind of data
 **/

/**
 * GwyDataList:
 *
 * Object representing the list of one kind of data.
 *
 * The #GwyDataList struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyDataListClass:
 *
 * Class of lists of one kind of data.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
