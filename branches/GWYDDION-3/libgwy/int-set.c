/*
 *  $Id$
 *  Copyright (C) 2012 David Nečas (Yeti).
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
#include "libgwy/int-set.h"

enum {
    N_ITEMS = 1,
};

enum {
    ADDED,
    REMOVED,
    ASSIGNED,
    N_SIGNALS
};

typedef struct {
    gint32 from;
    gint32 to;
} IntRange;

struct _GwyIntSetPrivate {
    GArray *ranges;
};

typedef struct _GwyIntSetPrivate IntSet;

static void     gwy_int_set_finalize         (GObject *object);
static void     gwy_int_set_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_int_set_n_items          (GwySerializable *serializable);
static gsize    gwy_int_set_itemize          (GwySerializable *serializable,
                                              GwySerializableItems *items);
static gboolean gwy_int_set_construct        (GwySerializable *serializable,
                                              GwySerializableItems *items,
                                              GwyErrorList **error_list);
static GObject* gwy_int_set_duplicate_impl   (GwySerializable *serializable);
static void     gwy_int_set_assign_impl      (GwySerializable *destination,
                                              GwySerializable *source);
static void     set_data_silent              (GwyIntSet *intset,
                                              const gint *values,
                                              guint n);
static int      int_compare                  (const void *pa,
                                              const void *pb);
static guint    uniq                         (gint *values,
                                              guint n);
static guint    ranges_size                  (const GArray *ranges);
static gboolean is_strictly_ascending        (const gint *values,
                                              guint n);
static gboolean ranges_are_canonical         (const GArray *ranges);
static gboolean find_range                   (const GArray *ranges,
                                              gint i,
                                              guint *rid);
static gboolean is_present                   (const GArray *ranges,
                                              gint value);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "ranges",  .ctype = GWY_SERIALIZABLE_INT32_ARRAY, },
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED
    (GwyIntSet, gwy_int_set, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_int_set_serializable_init));

static void
gwy_int_set_class_init(GwyIntSetClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(IntSet));

    gobject_class->finalize = gwy_int_set_finalize;

    /**
     * GwyIntSet::added:
     * @gwyintset: The #GwyIntSet which received the signal.
     * @arg1: Integer added to the set.
     *
     * The ::item-inserted signal is emitted when an integer is added into
     * the integer set.
     **/
    signals[ADDED]
        = g_signal_new_class_handler("added",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__INT,
                                     G_TYPE_NONE, 1, G_TYPE_INT);

    /**
     * GwyIntSet::removed:
     * @gwyintset: The #GwyIntSet which received the signal.
     * @arg1: Integer removed from the set.
     *
     * The ::item-deleted signal is emitted when an integer is removed from
     * the integer set.
     **/
    signals[REMOVED]
        = g_signal_new_class_handler("removed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__INT,
                                     G_TYPE_NONE, 1, G_TYPE_INT);
    /**
     * GwyIntSet::assigned:
     * @gwyintset: The #GwyIntSet which received the signal.
     *
     * The ::assigned signal is emitted when the entire contents of the set
     * changes, e.g. by assignment.
     **/
    signals[ASSIGNED]
        = g_signal_new_class_handler("assigned",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_int_set_init(GwyIntSet *intset)
{
    intset->priv = G_TYPE_INSTANCE_GET_PRIVATE(intset, GWY_TYPE_INT_SET,
                                               IntSet);
    IntSet *priv = intset->priv;
    priv->ranges = g_array_new(FALSE, FALSE, sizeof(IntRange));
}

static void
gwy_int_set_finalize(GObject *object)
{
    IntSet *priv = GWY_INT_SET(object)->priv;
    g_array_free(priv->ranges, TRUE);
    G_OBJECT_CLASS(gwy_int_set_parent_class)->finalize(object);
}

static void
gwy_int_set_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_int_set_n_items;
    iface->itemize   = gwy_int_set_itemize;
    iface->construct = gwy_int_set_construct;
    iface->duplicate = gwy_int_set_duplicate_impl;
    iface->assign    = gwy_int_set_assign_impl;
}

static gsize
gwy_int_set_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return N_ITEMS;
}

static gsize
gwy_int_set_itemize(GwySerializable *serializable,
                    GwySerializableItems *items)
{
    GwyIntSet *intset = GWY_INT_SET(serializable);
    GArray *ranges = intset->priv->ranges;

    if (!ranges->len)
        return 0;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    GwySerializableItem *it = items->items + items->n;

    *it = serialize_items[0];
    it->value.v_int32_array = (gint32*)ranges->data;
    it->array_size = 2*ranges->len;
    it++, items->n++;

    return N_ITEMS;
}

static gboolean
gwy_int_set_construct(GwySerializable *serializable,
                      GwySerializableItems *items,
                      GwyErrorList **error_list)
{
    GwyIntSet *intset = GWY_INT_SET(serializable);
    GArray *ranges = intset->priv->ranges;

    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyIntSet", error_list);

    guint len = its[0].array_size;
    if (len && its[0].value.v_int32_array) {
        if (len % 2 != 0) {
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_INVALID,
                               _("IntSet data length is %lu which is not "
                                 "a multiple of 2."),
                               (gulong)its[0].array_size);
            goto fail;
        }
        g_array_set_size(ranges, 0);
        g_array_append_vals(ranges, its[0].value.v_int32_array, len/2);
        if (!ranges_are_canonical(ranges)) {
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_INVALID,
                               _("IntSet ranges are not in canonical form."));
            goto fail;
        }

        GWY_FREE(its[0].value.v_double_array);
        its[0].array_size = 0;
    }

    return TRUE;

fail:
    GWY_FREE(its[0].value.v_int32_array);
    return FALSE;
}

static GObject*
gwy_int_set_duplicate_impl(GwySerializable *serializable)
{
    GwyIntSet *intset = GWY_INT_SET(serializable);
    GArray *ranges = intset->priv->ranges;
    GwyIntSet *duplicate = g_object_newv(GWY_TYPE_INT_SET, 0, NULL);
    GArray *dupranges = duplicate->priv->ranges;
    g_array_append_vals(dupranges, ranges->data, ranges->len);

    return G_OBJECT(duplicate);
}

static void
gwy_int_set_assign_impl(GwySerializable *destination,
                        GwySerializable *source)
{
    GwyIntSet *dest = GWY_INT_SET(destination);
    GwyIntSet *src = GWY_INT_SET(source);
    GArray *sranges = src->priv->ranges;
    GArray *dranges = dest->priv->ranges;

    g_array_set_size(dranges, 0);
    g_array_append_vals(dranges, sranges->data, sranges->len);
    // TODO: g_signal_emit(dest, signals[???], 0);
}

/**
 * gwy_int_set_new:
 *
 * Creates a new integer set.
 *
 * Returns: A new integer set.
 **/
GwyIntSet*
gwy_int_set_new(void)
{
    return g_object_newv(GWY_TYPE_INT_SET, 0, NULL);
}

/**
 * gwy_int_set_new_with_values:
 * @values: Integers to fill the integer set with.  They may repeat and may be
 *          in non-ascending order, however, if they do the construction is
 *          somewhat more efficient.
 * @n: Number of items in @values.
 *
 * Creates a new integer set fills it with values.
 *
 * This is the preferred method to create an integer set filled with values as
 * it does not emit GwyIntSet::added for each item.
 *
 * Returns: A new integer set.
 **/
GwyIntSet*
gwy_int_set_new_with_values(const gint *values,
                            guint n)
{
    GwyIntSet *intset = g_object_newv(GWY_TYPE_INT_SET, 0, NULL);
    set_data_silent(intset, values, n);
    return intset;
}

/* Set the data without emitting any signals.  Use with care, intended only
 * for deserialization. */
static void
set_data_silent(GwyIntSet *intset,
                const gint *values,
                guint n)
{
    GArray *ranges = intset->priv->ranges;
    g_array_set_size(ranges, 0);

    if (!n)
        return;

    gint *myvalues = NULL;

    if (!is_strictly_ascending(values, n)) {
        myvalues = g_memdup(values, n*sizeof(gint));
        qsort(myvalues, n, sizeof(gint), &int_compare);
        n = uniq(myvalues, n);
        values = myvalues;
    }
    IntRange r = { .from = values[0] };
    guint ifrom = 0;

    for (guint i = 1; i < n; i++) {
        if ((guint)values[i] - (guint)r.from != i - ifrom) {
            r.to = values[i-1];
            g_array_append_val(ranges, r);
            r.from = values[i];
            ifrom = i;
        }
    }
    r.to = values[n-1];
    g_array_append_val(ranges, r);

    GWY_FREE(myvalues);
}

/**
 * gwy_int_set_contains:
 * @intset: A set of integers.
 * @value: Value to test for presence in the set.
 *
 * Tests whether a value is present in an integer set.
 *
 * Returns: %TRUE if @value is present in @intset, %FALSE if it is not present.
 **/
gboolean
gwy_int_set_contains(const GwyIntSet *intset,
                     gint value)
{
    g_return_val_if_fail(GWY_IS_INT_SET(intset), FALSE);
    return is_present(intset->priv->ranges, value);
}

/**
 * gwy_int_set_add:
 * @intset: A set of integers.
 * @value: Value to add to the set.
 *
 * Adds a value to an integer set.
 *
 * The value may be already present; you can test this by examining the return
 * value.  Signal GwyIntSet::added is emitted only if the value was newly
 * added.
 *
 * Returns: %TRUE if @value was newly added, %FALSE if it was already present.
 **/
gboolean
gwy_int_set_add(GwyIntSet *intset,
                gint value)
{
    g_return_val_if_fail(GWY_IS_INT_SET(intset), FALSE);
    guint rid;
    GArray *ranges = intset->priv->ranges;
    if (find_range(ranges, value, &rid))
        return FALSE;

    IntRange *r = (IntRange*)ranges->data;
    guint n = ranges->len;

    if (rid < n && r[rid].from == value+1) {
        // Attach to begining, possibly merge into previous.  Since
        // find_range() prefers the first range if a value is adjacent to two
        // we do not need to check for merging here.
        r[rid].from--;
    }
    else if (rid < n && r[rid].to+1 == value) {
        // Attach to end, possibly merge the next into this one.
        r[rid].to++;
        if (rid+1 < n && r[rid+1].from-1 == r[rid].to) {
            r[rid].to = r[rid+1].to;
            g_array_remove_index(ranges, rid+1);
        }
    }
    else {
        // Cannot attach, insert a new one-item range.
        IntRange range = { .from = value, .to = value };
        g_array_insert_val(ranges, rid, range);
    }

    g_signal_emit(intset, signals[ADDED], 0, value);

    return TRUE;
}

/**
 * gwy_int_set_remove:
 * @intset: A set of integers.
 * @value: Value to remove from the set.
 *
 * Removes a value from an integer set.
 *
 * The value may not be present in the set; you can test this by examining the
 * return value.  Signal GwyIntSet::removed is emitted only if the value was
 * actually removed.
 *
 * Returns: %TRUE if @value was removed, %FALSE if it was not in the set.
 **/
gboolean
gwy_int_set_remove(GwyIntSet *intset,
                   gint value)
{
    g_return_val_if_fail(GWY_IS_INT_SET(intset), FALSE);
    guint rid;
    GArray *ranges = intset->priv->ranges;
    if (!find_range(ranges, value, &rid))
        return FALSE;

    IntRange *r = (IntRange*)ranges->data;

    if (r[rid].from == value) {
        // Remove from the begining, possibly killing the range.
        r[rid].from++;
        if (r[rid].from > r[rid].to)
            g_array_remove_index(ranges, rid);
    }
    else if (r[rid].to == value) {
        // Remove from the end; since single-value ranges were already handled
        // above we do not need to check for the removal of entire range.
        r[rid].to--;
    }
    else {
        // Must split.
        IntRange range = { .from = r[rid].from, .to = value-1 };
        r[rid].from = value+1;
        g_array_insert_val(ranges, rid, range);
    }

    g_signal_emit(intset, signals[REMOVED], 0, value);

    return TRUE;
}

// Find the range containing @value and return TRUE or, failing that, the range
// that @value may be attached to or, failing that, the position after which
// @value could be added as a new one-item range.
static gboolean
find_range(const GArray *ranges,
           gint value,
           guint *rid)
{
    const IntRange *r = (const IntRange*)ranges->data;
    guint n = ranges->len;

    if (!n) {
        *rid = 0;
        return FALSE;
    }

    for (guint j = 0; j < n; j++) {
        if (value < r[j].from) {
            *rid = (j && r[j-1].to + 1 == value) ? j-1 : j;
            return FALSE;
        }
        if (value <= r[j].to) {
            *rid = j;
            return TRUE;
        }
    }

    *rid = (r[n-1].to + 1 == value) ? n-1 : n;
    return FALSE;
}

static gboolean
is_present(const GArray *ranges,
           gint value)
{
    const IntRange *r = (const IntRange*)ranges->data;
    guint n = ranges->len;

    for (guint j = 0; j < n; j++) {
        if (value < r[j].from)
            return FALSE;
        if (value <= r[j].to)
            return TRUE;
    }
    return FALSE;
}

static int
int_compare(const void *pa, const void *pb)
{
    const gint *ia = (const gint*)pa, *ib = (const gint*)pb;
    if (*ia < *ib)
        return -1;
    if (*ia > *ib)
        return 1;
    return 0.0;
}

static guint
uniq(gint *values, guint n)
{
    guint j = 0;
    for (guint i = 1; i < n; i++) {
        if (values[i] != values[j]) {
            j++;
            values[j] = values[i];
        }
    }
    return j+1;
}

/**
 * gwy_int_set_update:
 * @intset: A set of integers.
 * @values: Integers to fill the integer set with.
 * @n: Number of items in @values.
 *
 * Updates an integer set to contain the given list of values.
 *
 * Values present in @intset but missing in @values are removed then values
 * present in @values but missing in @intset are added.  Signals are emitted
 * for each addition or removal.  So this method is useful namely when it can
 * be expected that the set does not change much by the update.  Otherwise it
 * could be just rebuilt completely.
 **/
void
gwy_int_set_update(GwyIntSet *intset,
                   const gint *values,
                   guint n_)
{
    g_return_if_fail(GWY_IS_INT_SET(intset));

    GwyIntSet *tmp = gwy_int_set_new_with_values(values, n_);
    GArray *tmpranges = tmp->priv->ranges;
    GArray *ranges = intset->priv->ranges;
    guint n = ranges->len;

    /* Best not to try to iterate over @ranges while we change it by
     * gwy_int_set_remove().  Use a temporary copy to see what values were
     * there originally. */
    IntRange *r = (IntRange*)g_slice_copy(n*sizeof(IntRange), ranges->data);
    for (guint i = 0; i < n; i++) {
        for (gint value = r[i].from; value <= r[i].to; value++) {
            if (!is_present(tmpranges, value))
                gwy_int_set_remove(intset, value);
        }
    }
    g_slice_free1(n*sizeof(IntRange), r);

    r = (IntRange*)tmpranges->data;
    n = tmpranges->len;
    for (guint i = 0; i < n; i++) {
        for (gint value = r[i].from; value <= r[i].to; value++)
            gwy_int_set_add(intset, value);
    }

    g_object_unref(tmp);
}

/**
 * gwy_int_set_size:
 * @intset: A set of integers.
 *
 * Calculates the number of values in an integer set.
 *
 * Returns: The number of values in @intset.
 **/
guint
gwy_int_set_size(const GwyIntSet *intset)
{
    g_return_val_if_fail(GWY_IS_INT_SET(intset), 0);
    return ranges_size(intset->priv->ranges);
}

/**
 * gwy_int_set_values:
 * @intset: A set of integers.
 * @len: Location where to store the number of values in @intset and,
 *       consequently, in the returned array.
 *
 * Creates a plain array with the values from an integer set.
 *
 * Returns: (allow-none):
 *          Newly created array with all the values in @intset.  %NULL is
 *          returned if the set is empty.
 **/
gint*
gwy_int_set_values(const GwyIntSet *intset,
                   guint *len)
{
    g_return_val_if_fail(GWY_IS_INT_SET(intset), NULL);
    const GArray *ranges = intset->priv->ranges;
    const IntRange *r = (const IntRange*)ranges->data;
    guint n = ranges->len;

    if (!n) {
        GWY_MAYBE_SET(len, 0);
        return NULL;
    }

    guint s = ranges_size(ranges);
    gint *values = g_new(gint, s), *v = values;

    for (guint i = 0; i < n; i++) {
        for (gint j = r[i].from; j <= r[i].to; j++, v++)
            *v = j;
    }
    GWY_MAYBE_SET(len, s);

    return values;
}

static guint
ranges_size(const GArray *ranges)
{
    const IntRange *r = (const IntRange*)ranges->data;
    guint n = ranges->len;
    guint s = 0;

    for (guint i = 0; i < n; i++)
        s += r[i].to - r[i].from + 1;

    return s;
}

static gboolean
is_strictly_ascending(const gint *values,
                      guint n)
{
    while (n > 1) {
        if (*values >= *(values + 1))
            return FALSE;
        n--;
        values++;
    }
    return TRUE;
}

static gboolean
ranges_are_canonical(const GArray *ranges)
{
    const IntRange *r = (const IntRange*)ranges->data;
    guint n = ranges->len;
    if (!n)
        return TRUE;

    if (r[0].from > r[0].to)
        return FALSE;
    for (guint i = 1; i < n; i++) {
        if (r[i].from > r[i].to)
            return FALSE;
        if (r[i].from - 1 < r[i-1].to + 1)
            return FALSE;
    }
    return TRUE;
}

/**
 * SECTION: int-set
 * @title: GwyIntSet
 * @short_description: Set of integers
 *
 * #GwyIntSet represents a set of integers, internaly represented as a list
 * of intervals they belong to.  So it is quite efficient for representing
 * even large sets as long as they are mostly contiguous.  It may be used as
 * the backend for selections of defined lists.
 *
 * Note integers %G_MAXINT and %G_MININT may not be inserted.
 **/

/**
 * GwyIntSet:
 *
 * Object representing an integer set.
 *
 * The #GwyIntSet struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyIntSetClass:
 *
 * Class of integer sets.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
