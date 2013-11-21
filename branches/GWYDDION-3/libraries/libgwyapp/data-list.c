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
#include "libgwyapp/channel-data.h"
#include "libgwyapp/data-list.h"
#include "libgwyapp/data-list-internal.h"

enum {
    PROP_0,
    PROP_DATA_KIND,
    PROP_DATA_TYPE,
    PROP_FILE,
    N_PROPS
};

struct _GwyDataListPrivate {
    GwyFile *parent_file;
    guint item_id;
    GwyDataKind kind;
    GType type;
};

typedef struct _GwyDataListPrivate DataList;

static void                gwy_data_list_finalize    (GObject *object);
static void                gwy_data_list_dispose     (GObject *object);
static void                gwy_data_list_set_property(GObject *object,
                                                      guint prop_id,
                                                      const GValue *value,
                                                      GParamSpec *pspec);
static void                gwy_data_list_get_property(GObject *object,
                                                      guint prop_id,
                                                      GValue *value,
                                                      GParamSpec *pspec);
static gboolean            set_data_type             (GwyDataList *datalist,
                                                      GType type);
static gboolean            set_file                  (GwyDataList *datalist,
                                                      GwyFile *file);
static void                init_kind_to_type_map     (void);
static guint               find_item_index_by_id     (const GwyDataList *datalist,
                                                      guint id);
static guint               advance_to_next_free_id   (GwyDataList *datalist);
static const GwyDataItem** get_item_list             (const GwyDataList *datalist,
                                                      guint *pn);

static GParamSpec *properties[N_PROPS];
static GType kind_to_type_map[GWY_DATA_NKINDS];

G_DEFINE_TYPE_WITH_CODE(GwyDataList, gwy_data_list, GWY_TYPE_ARRAY,
                        init_kind_to_type_map(););

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
                             GWY_TYPE_DATA_ITEM,
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

    gwy_array_set_item_type(GWY_ARRAY(datalist),
                            sizeof(gpointer), g_object_unref);
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
 * gwy_data_list_new_for_file().
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
 * @type: Type of data this list will hold.  It must be a concrete type derived
 *        from #GwyDataItem.
 *
 * Creates a new data list for data of specified type and file.
 *
 * Returns: (transfer full):
 *          A new data list.
 **/
GwyDataList*
gwy_data_list_new_for_file(const GwyFile *file,
                           GType type)
{
    g_assert(g_type_is_a(type, GWY_TYPE_DATA_ITEM));
    g_assert(!G_TYPE_IS_ABSTRACT(type));
    return g_object_new(GWY_TYPE_DATA_LIST,
                        "file", file,
                        "data-type", type,
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
    guint n;
    const GwyDataItem **items = get_item_list(datalist, &n);
    *nids = n;
    if (!n)
        return NULL;

    guint *ids = g_new(guint, n);
    for (guint i = 0; i < n; i++)
        ids[i] = gwy_data_item_get_id(items[i]);
    return ids;
}

/**
 * gwy_data_list_add:
 * @datalist: A list of one kind of data.
 * @dataitem: A high-level data item of the corresponding kind.
 *
 * Adds an item to a list of one kind of data.
 *
 * Adding the item to the list sinks any floating reference.
 *
 * Returns: The identifier of the added item.
 **/
guint
gwy_data_list_add(GwyDataList *datalist,
                  GwyDataItem *dataitem)
{
    g_return_val_if_fail(GWY_IS_DATA_LIST(datalist), MAX_ITEM_ID);
    DataList *priv = datalist->priv;
    g_return_val_if_fail(priv->type
                         && G_TYPE_CHECK_INSTANCE_TYPE(dataitem, priv->type),
                         MAX_ITEM_ID);
    g_return_val_if_fail(gwy_data_item_get_data_list(dataitem) == NULL,
                         MAX_ITEM_ID);

    guint id = advance_to_next_free_id(datalist);
    g_object_ref_sink(dataitem);
    g_object_freeze_notify(G_OBJECT(dataitem));
    _gwy_data_item_set_list_and_id(dataitem, datalist, id);
    gwy_array_append(GWY_ARRAY(datalist), &dataitem, 1);
    g_object_thaw_notify(G_OBJECT(dataitem));

    return id;
}

/**
 * gwy_data_list_add_with_id:
 * @datalist: A list of one kind of data.
 * @dataitem: A high-level data item of the corresponding kind.
 * @id: Requested identifier.
 *
 * Adds an item to a list of one kind of data, requesting a speficic
 * identifier.
 *
 * This method may or may not succeed, depending on whether @id is free or
 * occupied.  While it can be useful for (de)serialisation and some dirty
 * trick, you should rarely need it. The usual method to add a data item is
 * gwy_data_list_add().
 *
 * Adding the item to the list sinks any floating reference.  Of course, if the
 * item is not added no reference is sunk.
 *
 * Returns: %TRUE if the item was added with the requested identifier, %FALSE
 *          if that identifier is already occupied.
 **/
gboolean
gwy_data_list_add_with_id(GwyDataList *datalist,
                          GwyDataItem *dataitem,
                          guint id)
{
    g_return_val_if_fail(GWY_IS_DATA_LIST(datalist), FALSE);
    g_return_val_if_fail(id <= MAX_ITEM_ID, FALSE);
    DataList *priv = datalist->priv;
    g_return_val_if_fail(priv->type
                         && G_TYPE_CHECK_INSTANCE_TYPE(dataitem, priv->type),
                         FALSE);
    g_return_val_if_fail(gwy_data_item_get_data_list(dataitem) == NULL,
                         FALSE);

    if (find_item_index_by_id(datalist, id) != G_MAXUINT)
        return FALSE;

    g_object_ref_sink(dataitem);
    g_object_freeze_notify(G_OBJECT(dataitem));
    _gwy_data_item_set_list_and_id(dataitem, datalist, id);
    gwy_array_append(GWY_ARRAY(datalist), &dataitem, 1);
    g_object_thaw_notify(G_OBJECT(dataitem));

    return TRUE;
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

/**
 * gwy_data_kind_to_type:
 * @kind: Data kind.
 *
 * Transforms a data kind identifier to the type of the corresponding data
 * item.
 *
 * Returns: The #GType corresponding to @kind.
 **/
GType
gwy_data_kind_to_type(GwyDataKind kind)
{
    g_return_val_if_fail(kind > GWY_DATA_UNKNOWN && kind < GWY_DATA_NKINDS, 0);
    // This invokes init_kind_to_type_map() if necessary.
    if (G_LIKELY(GWY_TYPE_DATA_LIST))
        return kind_to_type_map[kind];
    g_assert_not_reached();
    return 0;
}

/**
 * gwy_data_type_to_kind:
 * @type: Type of data.
 *
 * Transforms a data item type to the corresponding data kind identifier.
 *
 * Returns: The data kind corresponding to @type.
 **/
GwyDataKind
gwy_data_type_to_kind(GType type)
{
    // This invokes init_kind_to_type_map() if necessary.
    for (guint kind = 0; kind < GWY_DATA_NKINDS; kind++) {
        if (kind_to_type_map[kind] == type)
            return kind;
    }
    // Can we get here?
    for (guint kind = 0; kind < GWY_DATA_NKINDS; kind++) {
        if (g_type_is_a(type, kind_to_type_map[kind]))
            return kind;
    }
    g_critical("Type 0x%lx is not a data item type.", (gulong)type);
    return GWY_DATA_UNKNOWN;
}

static gboolean
set_data_type(GwyDataList *datalist, GType type)
{
    DataList *priv = datalist->priv;
    // This also makes passing zero type OK.
    if (type == priv->type)
        return FALSE;
    g_return_val_if_fail(!priv->type, FALSE);
    g_return_val_if_fail(g_type_is_a(type, GWY_TYPE_DATA_ITEM), FALSE);
    g_return_val_if_fail(!G_TYPE_IS_ABSTRACT(type), FALSE);

    priv->type = type;
    priv->kind = gwy_data_type_to_kind(type);
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

static void
init_kind_to_type_map(void)
{
    kind_to_type_map[GWY_DATA_CHANNEL] = GWY_TYPE_CHANNEL_DATA;
}

static guint
find_item_index_by_id(const GwyDataList *datalist,
                      guint id)
{
    guint n;
    const GwyDataItem **items = get_item_list(datalist, &n);
    for (guint i = 0; i < n; i++) {
        if (gwy_data_item_get_id(items[i]) == id)
            return i;
    }
    return G_MAXUINT;
}

static guint
advance_to_next_free_id(GwyDataList *datalist)
{
    DataList *priv = datalist->priv;
    while (find_item_index_by_id(datalist, priv->item_id) != G_MAXUINT) {
        ++priv->item_id;
        if (G_UNLIKELY(priv->item_id == MAX_ITEM_ID))
            priv->item_id = 0;
    }

    guint retval = priv->item_id;
    // Avoid id recycling if item is created and immediately destroyed.
    ++priv->item_id;
    if (G_UNLIKELY(priv->item_id == MAX_ITEM_ID))
        priv->item_id = 0;

    return retval;
}

static const GwyDataItem**
get_item_list(const GwyDataList *datalist,
              guint *pn)
{
    const GwyArray *array = GWY_ARRAY(datalist);
    if (pn)
        *pn = gwy_array_size(array);

    return (const GwyDataItem**)gwy_array_get_data(array);
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
