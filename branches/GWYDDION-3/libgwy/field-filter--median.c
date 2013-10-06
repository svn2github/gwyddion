/*
 *  $Id$
 *  Copyright (C) 2010-2013 David Nečas (Yeti).
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
// Including before fftw3.h ensures fftw_complex is C99 ‘double complex’.
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/math.h"
#include "libgwy/master.h"
#include "libgwy/field-filter.h"
#include "libgwy/field-internal.h"
#include "libgwy/mask-field-internal.h"
#include "libgwy/object-internal.h"

enum {
    MEDIAN_FILTER_AUTO,
    MEDIAN_FILTER_DIRECT,
    MEDIAN_FILTER_BUCKET,
};

enum {
    UPPER = 0,
    LOWER = 1,
    LEFT = 2,
    RIGHT = 3,
};

// Offsets with respect to the top left corner of kernel pixels representing
// the lower, upper, left and right edges.  These are pixels we must add or
// remove when the kernel is moved.
typedef struct {
    IntList *upper;
    IntList *lower;
    IntList *left;
    IntList *right;
} KernelEdges;

typedef struct {
    const GwyField *field;
    guint col;
    guint row;
    guint width;
    guint height;
    GwyField *target;
    guint targetcol;
    guint targetrow;
    const GwyMaskField *kernel;
    KernelEdges *edges;
    guint kn;
    RectExtendFunc extend_rect;
    gdouble fill_value;
} MedianFilterData;

typedef void (*MedianFilterFunc)(const MedianFilterData *mfdata);

typedef struct {
    MedianFilterData mfdata;
    MedianFilterFunc medfunc;
    guint colwidth;
    guint col;
    guint targetcol;
} MedianFilterState;

typedef struct {
    MedianFilterData mfdata;
    MedianFilterFunc medfunc;
} MedianFilterTask;

static guint median_filter_method = MEDIAN_FILTER_AUTO;

void
_gwy_tune_median_filter_method(const gchar *method)
{
    g_return_if_fail(method);
    if (gwy_strequal(method, "auto"))
        median_filter_method = MEDIAN_FILTER_AUTO;
    else if (gwy_strequal(method, "direct"))
        median_filter_method = MEDIAN_FILTER_DIRECT;
    else if (gwy_strequal(method, "bucket"))
        median_filter_method = MEDIAN_FILTER_BUCKET;
    else {
        g_warning("Unknown median filter method %s.", method);
    }
}

static KernelEdges*
kernel_edges_new(guint xres, guint yres)
{
    KernelEdges *edges = g_slice_new(KernelEdges);

    edges->upper = int_list_new(xres);
    edges->lower = int_list_new(xres);
    edges->left = int_list_new(yres);
    edges->right = int_list_new(yres);

    return edges;
}

static void
kernel_edges_free(KernelEdges *edges)
{
    int_list_free(edges->upper);
    int_list_free(edges->lower);
    int_list_free(edges->left);
    int_list_free(edges->right);
    g_slice_free(KernelEdges, edges);
}

/* Note this function returns the indices assuming kernel xres.  For faster
 * lookup in the extended field we recalculate them for its xres later. */
static KernelEdges*
analyse_kernel_edges(const GwyMaskField *kernel)
{
    guint xres = kernel->xres, yres = kernel->yres;
    KernelEdges *edges = kernel_edges_new(xres, yres);

    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++) {
            if (!gwy_mask_field_get(kernel, j, i))
                continue;

            guint k = i*xres + j;
            if (!j || !gwy_mask_field_get(kernel, j-1, i))
                int_list_add(edges->left, k);
            if (j == xres-1 || !gwy_mask_field_get(kernel, j+1, i))
                int_list_add(edges->right, k);
            if (!i || !gwy_mask_field_get(kernel, j, i-1))
                int_list_add(edges->upper, k);
            if (i == yres-1 || !gwy_mask_field_get(kernel, j, i+1))
                int_list_add(edges->lower, k);
        }
    }

    g_assert(edges->upper->len == edges->lower->len);
    g_assert(edges->left->len == edges->right->len);

    return edges;
}

static IntList*
adapt_int_list(const IntList *klist,
               guint kxres, guint xres)
{
    guint len = klist->len;
    IntList *list = int_list_new(len);
    list->len = len;

    for (guint k = 0; k < len; k++) {
        guint pos = klist->data[k];
        list->data[k] = (pos/kxres)*xres + (pos % kxres);
    }

    return list;
}

static KernelEdges*
adapt_kernel_edges(const KernelEdges *kedges,
                   guint kxres, guint xres)
{
    KernelEdges *edges = g_slice_new(KernelEdges);

    edges->upper = adapt_int_list(kedges->upper, kxres, xres);
    edges->lower = adapt_int_list(kedges->lower, kxres, xres);
    edges->left = adapt_int_list(kedges->left, kxres, xres);
    edges->right = adapt_int_list(kedges->right, kxres, xres);

    return edges;
}

static const IntList*
kernel_edge_list(const KernelEdges *edges,
                 guint edge)
{
    if (edge == UPPER)
        return edges->upper;
    if (edge == LOWER)
        return edges->lower;
    if (edge == LEFT)
        return edges->left;
    if (edge == RIGHT)
        return edges->right;

    g_return_val_if_reached(NULL);
}

/* Find the median of an array of pointers to doubles, shuffling the pointers
 * but leaving the double values intact. */
static gdouble
median_from_pointers(const gdouble **array, gsize n)
{
    gsize lo, hi;
    gsize median;
    gsize middle, ll, hh;

    g_return_val_if_fail(n, NAN);

    lo = 0;
    hi = n - 1;
    median = n/2;
    while (TRUE) {
        if (hi <= lo)        /* One element only */
            return *array[median];

        if (hi == lo + 1) {  /* Two elements only */
            if (*array[lo] > *array[hi])
                GWY_SWAP(const gdouble*, array[lo], array[hi]);
            return *array[median];
        }

        /* Find median of lo, middle and hi items; swap into position lo */
        middle = (lo + hi)/2;
        if (*array[middle] > *array[hi])
            GWY_SWAP(const gdouble*, array[middle], array[hi]);
        if (*array[lo] > *array[hi])
            GWY_SWAP(const gdouble*, array[lo], array[hi]);
        if (*array[middle] > *array[lo])
            GWY_SWAP(const gdouble*, array[middle], array[lo]);

        /* Swap low item (now in position middle) into position (lo+1) */
        GWY_SWAP(const gdouble*, array[middle], array[lo + 1]);

        /* Nibble from each end towards middle, swapping items when stuck */
        ll = lo + 1;
        hh = hi;
        while (TRUE) {
            do {
                ll++;
            } while (*array[lo] > *array[ll]);
            do {
                hh--;
            } while (*array[hh] > *array[lo]);

            if (hh < ll)
                break;

            GWY_SWAP(const gdouble*, array[ll], array[hh]);
        }

        /* Swap middle item (in position lo) back into correct position */
        GWY_SWAP(const gdouble*, array[lo], array[hh]);

        /* Re-set active partition */
        if (hh <= median)
            lo = ll;
        if (hh >= median)
            hi = hh - 1;
    }
}

static guint
extract_masked_data_double_with_revmap(const gdouble *data,
                                       guint xres,
                                       const GwyMaskField *kernel,
                                       gdouble *values,
                                       guint *revmap)
{
    guint kxres = kernel->xres, kyres = kernel->yres;
    guint k = 0;

    for (guint ki = 0; ki < kyres; ki++) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(kernel, iter, 0, ki);
        for (guint kj = 0; kj < kxres; kj++) {
            if (gwy_mask_iter_get(iter)) {
                revmap[ki*xres + kj] = k;
                values[k++] = data[ki*xres + kj];
            }
            gwy_mask_iter_next(iter);
        }
    }

    return k;
}

static void
remove_edge_data_with_revmap(const KernelEdges *edges,
                             guint basepos,
                             guint edge,
                             guint *revmap,
                             guint *freelist)
{
    const IntList *list = kernel_edge_list(edges, edge);
    g_return_if_fail(list);

    for (guint k = 0; k < list->len; k++) {
        guint pos = basepos + list->data[k];
        guint wsppos = revmap[pos];
        *(freelist++) = wsppos;
    }
}

static void
add_edge_data_with_revmap(const KernelEdges *edges,
                          const gdouble *data,
                          guint basepos,
                          guint edge,
                          gdouble *workspace,
                          guint *revmap,
                          guint *freelist)
{
    const IntList *list = kernel_edge_list(edges, edge);
    g_return_if_fail(list);

    for (guint k = 0; k < list->len; k++) {
        guint pos = basepos + list->data[k];
        guint wsppos = *(freelist++);
        workspace[wsppos] = data[pos];
        revmap[pos] = wsppos;
    }
}

static void
filter_median_direct(const MedianFilterData *mfdata)
{
    const GwyField *field = mfdata->field;
    guint width = mfdata->width;
    guint height = mfdata->height;
    GwyField *target = mfdata->target;
    guint targetcol = mfdata->targetcol;
    guint targetrow = mfdata->targetrow;
    const GwyMaskField *kernel = mfdata->kernel;

    guint xres = field->xres, yres = field->yres,
          kxres = kernel->xres, kyres = kernel->yres;
    guint kn = mfdata->kn;
    guint xsize = width + kxres - 1, ysize = height + kyres - 1;
    guint extend_left, extend_right, extend_up, extend_down;
    _gwy_make_symmetrical_extension(width, xsize, &extend_left, &extend_right);
    _gwy_make_symmetrical_extension(height, ysize, &extend_up, &extend_down);

    guint n = xsize*ysize;
    g_assert(n >= kn);
    gdouble *extdata = g_new(gdouble, n);
    gdouble *targetbase = target->data + targetrow*target->xres + targetcol;
    KernelEdges *edges = adapt_kernel_edges(mfdata->edges, kxres, xsize);

    mfdata->extend_rect(field->data, xres, extdata, xsize,
                        mfdata->col, mfdata->row, width, height, xres, yres,
                        extend_left, extend_right, extend_up, extend_down,
                        mfdata->fill_value);

    gdouble *workspace = g_new(gdouble, kn);
    guint *revmap = g_new(guint, n);
    guint *freelist = g_new(guint, MAX(edges->upper->len, edges->left->len));
    // Declare @pointers with const to get an error if something tries to
    // modify @workspace through them.
    const gdouble **pointers = g_new(const gdouble*, kn);
    for (guint k = 0; k < kn; k++)
        pointers[k] = workspace + k;

    // Fill the workspace with the contents of the initial kernel.
    guint k = extract_masked_data_double_with_revmap(extdata, xsize, kernel,
                                                     workspace, revmap);
    g_assert(k == kn);
    targetbase[0] = median_from_pointers(pointers, kn);

    // Scan.  A bit counterintiutively, we scan by column because this means
    // the samples added/removed to the workspace in each step form a
    // contiguous block in a single row.
    guint i = 0, j = 0;
    while (TRUE) {
        // Downward pass of the zig-zag pattern.
        while (i < height-1) {
            remove_edge_data_with_revmap(edges, i*xsize + j, UPPER,
                                         revmap, freelist);
            i++;
            add_edge_data_with_revmap(edges, extdata, i*xsize + j, LOWER,
                                      workspace, revmap, freelist);
            targetbase[i*target->xres + j] = median_from_pointers(pointers, kn);
        }
        if (j == width-1)
            break;

        // Move right (at the bottom)
        remove_edge_data_with_revmap(edges, i*xsize + j, LEFT,
                                     revmap, freelist);
        j++;
        add_edge_data_with_revmap(edges, extdata, i*xsize + j, RIGHT,
                                  workspace, revmap, freelist);
        targetbase[i*target->xres + j] = median_from_pointers(pointers, kn);

        // Upward pass of the zig-zag pattern.
        while (i) {
            remove_edge_data_with_revmap(edges, i*xsize + j, LOWER,
                                         revmap, freelist);
            i--;
            add_edge_data_with_revmap(edges, extdata, i*xsize + j, UPPER,
                                      workspace, revmap, freelist);
            targetbase[i*target->xres + j] = median_from_pointers(pointers, kn);
        }
        if (j == width-1)
            break;

        // Move right (at the top)
        remove_edge_data_with_revmap(edges, i*xsize + j, LEFT,
                                     revmap, freelist);
        j++;
        add_edge_data_with_revmap(edges, extdata, i*xsize + j, RIGHT,
                                  workspace, revmap, freelist);
        targetbase[i*target->xres + j] = median_from_pointers(pointers, kn);
    }

    kernel_edges_free(edges);
    g_free(revmap);
    g_free(freelist);
    g_free(pointers);
    g_free(workspace);
    g_free(extdata);
}

static void
order_transform(gdouble *data,
                guint *orderfield,
                guint *workspace,
                guint xres, guint yres)
{
    guint n = xres*yres;
    for (guint i = 0; i < n; i++)
        workspace[i] = i;
    gwy_math_sort(data, workspace, n);
    for (guint i = 0; i < n; i++)
        orderfield[workspace[i]] = i;
}

/*
 * @buckets: Array of size of entire image, split into buckets of size
 *           1 << bshift, each currently holds bucket_len[bi] items and if it
 *           is sorted then bucket_sorted[i] == TRUE.
 * @revindex: Array of size of entire image, indexed by integral values from
 *            order transform and keeping positions of the values in buckets.
 * @values: Array of the size of entire image, and organised the same way as
 *          the image and holding the integral values from order transform.
 */
static void
sort_bucket(guint *buckets,
            gboolean *bucket_sorted,
            const guint *bucket_len,
            guint bshift,
            guint bi,
            guint *revindex)
{
    if (bucket_sorted[bi])
        return;

    guint b0 = bi << bshift;
    guint *bbase = buckets + b0;
    guint blen = bucket_len[bi];
    gwy_sort_uint(bbase, blen);
    // Update the reverse index in one pass once all the sorting is done.
    for (guint i = 0; i < blen; i++)
        revindex[bbase[i]] = b0 + i;
    bucket_sorted[bi] = TRUE;
}

static void
bucket_remove_values(guint *buckets,
                     guint *bucket_len,
                     gboolean *bucket_sorted,
                     guint bshift,
                     const guint *values,
                     guint nvalues,
                     guint *revindex,
                     guint median,
                     gint *addedbelow,
                     gint *addedabove)
{
    guint bmask = (1U << bshift) - 1U;
    while (nvalues--) {
        const guint v = *values;
        const guint absbpos = revindex[v];
        const guint bi = absbpos >> bshift;
        const guint bpos = absbpos & bmask;
        const guint blen = bucket_len[bi];
        if (v < median)
            (*addedbelow)--;
        if (v > median)
            (*addedabove)--;
        if (bpos < blen-1) {
            // Move the last item to position of v (which is being removed).
            guint p = (bi << bshift) + blen-1;
            guint w = buckets[p];
            revindex[w] = absbpos;
            buckets[absbpos] = w;
            bucket_sorted[bi] = FALSE;
        }
        bucket_len[bi]--;
        values++;
    }
}

static void
bucket_add_values(guint *buckets,
                  guint *bucket_len,
                  gboolean *bucket_sorted,
                  guint bshift,
                  const guint *values,
                  guint nvalues,
                  guint *revindex,
                  guint median,
                  gint *addedbelow,
                  gint *addedabove)
{
    while (nvalues--) {
        const guint v = *values;
        const guint bi = v >> bshift;
        const guint blen = bucket_len[bi];
        const guint absbpos = (bi << bshift) + blen;
        if (v < median)
            (*addedbelow)++;
        if (v > median)
            (*addedabove)++;
        // Append item to the end.
        revindex[v] = absbpos;
        buckets[absbpos] = v;
        bucket_sorted[bi] = !blen;
        bucket_len[bi]++;
        values++;
    }
}

static guint
bucket_median(guint *buckets,
              const guint *bucket_len,
              gboolean *bucket_sorted,
              guint bshift,
              guint oldmedian,
              gint addedbelow,
              gint addedabove,
              guint *revindex)
{
    // If the same number of elements was added above and below then we don't
    // care how it happened: the median did not change.  Return and don't
    // complicate the logic below with zeroes.
    if (!addedabove && !addedbelow)
        return oldmedian;

    guint bi = oldmedian >> bshift;
    sort_bucket(buckets, bucket_sorted, bucket_len, bshift, bi, revindex);

    guint *bbase = buckets + (bi << bshift);
    guint blen = bucket_len[bi];
    gint medpos;
    gboolean medremoved = TRUE;
    for (medpos = 0; medpos < (gint)blen; medpos++) {
        if (bbase[medpos] == oldmedian) {
            medremoved = FALSE;
            break;
        }
        if (bbase[medpos] > oldmedian)
            break;
    }
    // Position in this bucket where the new median should be.  It can be huge
    // if we need to go to following buckets and negative if we need to go to
    // preceding buckets.
    if (medremoved) {
        if (addedabove > 0)
            medpos += addedabove-1;
        else
            medpos -= addedbelow;
    }
    else
        medpos += addedabove;

    while (medpos >= (gint)bucket_len[bi]) {
        medpos -= (gint)bucket_len[bi];
        bi++;
    }
    while (medpos < 0) {
        bi--;
        medpos += (gint)bucket_len[bi];
    }

    sort_bucket(buckets, bucket_sorted, bucket_len, bshift, bi, revindex);

    return buckets[(bi << bshift) + medpos];
}

static guint
gather_edge_intvalues(const KernelEdges *edges,
                      const guint *base,
                      guint edge,
                      guint *target)
{
    const IntList *list = kernel_edge_list(edges, edge);
    g_return_val_if_fail(list, 0);

    for (guint k = 0; k < list->len; k++)
        *(target++) = base[list->data[k]];

    return list->len;
}

static guint
extract_masked_data_uint(const guint *data,
                         guint xres,
                         const GwyMaskField *kernel,
                         guint *values)
{
    guint kxres = kernel->xres, kyres = kernel->yres;
    guint k = 0;

    for (guint ki = 0; ki < kyres; ki++) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(kernel, iter, 0, ki);
        for (guint kj = 0; kj < kxres; kj++) {
            if (gwy_mask_iter_get(iter))
                values[k++] = data[ki*xres + kj];
            gwy_mask_iter_next(iter);
        }
    }

    return k;
}

static void
filter_median_bucket(const MedianFilterData *mfdata)
{
    const GwyField *field = mfdata->field;
    guint width = mfdata->width;
    guint height = mfdata->height;
    GwyField *target = mfdata->target;
    guint targetcol = mfdata->targetcol;
    guint targetrow = mfdata->targetrow;
    const GwyMaskField *kernel = mfdata->kernel;

    guint xres = field->xres, yres = field->yres,
          kxres = kernel->xres, kyres = kernel->yres;
    guint kn = mfdata->kn;
    guint xsize = width + kxres - 1, ysize = height + kyres - 1;
    guint extend_left, extend_right, extend_up, extend_down;
    _gwy_make_symmetrical_extension(width, xsize, &extend_left, &extend_right);
    _gwy_make_symmetrical_extension(height, ysize, &extend_up, &extend_down);

    guint n = xsize*ysize;
    g_assert(n >= kn);
    gdouble *extdata = g_new(gdouble, n);
    gdouble *targetbase = target->data + targetrow*target->xres + targetcol;
    KernelEdges *edges = adapt_kernel_edges(mfdata->edges, kxres, xsize);

    mfdata->extend_rect(field->data, xres, extdata, xsize,
                        mfdata->col, mfdata->row, width, height, xres, yres,
                        extend_left, extend_right, extend_up, extend_down,
                        mfdata->fill_value);

    gdouble bsizemin = n/(kn*(log(kn) + 1.0));  // Avoid too many buckets.
    gdouble bsizemax = 2.0*sqrt(kn);            // Avoid too large buckets.
    guint bshift;
    if (bsizemin <= bsizemax)
        bshift = gwy_round(0.5*log2(bsizemin*bsizemax));
    else
        bshift = gwy_round(log2(bsizemin));
    bshift = MAX(bshift, 1);

    guint bsize = 1 << bshift;
    guint nbuckets = (n + bsize)/bsize;
    guint *intvalues = g_new(guint, n);
    guint *buckets = g_new(guint, n);
    guint *revindex = g_new(guint, n);
    guint *bucket_len = g_new0(guint, nbuckets);
    guint *transferbuf = g_new(guint, MAX(edges->upper->len, edges->left->len));
    gboolean *bucket_sorted = g_new0(gboolean, nbuckets);

    // Use buckets temporarily as scratch buffer.
    order_transform(extdata, intvalues, buckets, xsize, ysize);

    // Find the median for top left corner separately.
    guint k = extract_masked_data_uint(intvalues, xsize, kernel, buckets);
    g_assert(k == kn);
    gwy_sort_uint(buckets, kn);
    guint median = buckets[kn/2];
    targetbase[0] = extdata[median];

    // Fill the workspace with the contents of the initial kernel.
    for (guint ki = 0; ki < kyres; ki++) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(kernel, iter, 0, ki);
        for (guint kj = 0; kj < kxres; kj++) {
            if (gwy_mask_iter_get(iter)) {
                gint dumb = 0;
                bucket_add_values(buckets, bucket_len, bucket_sorted, bshift,
                                  intvalues + ki*xsize + kj, 1, revindex,
                                  G_MAXUINT, &dumb, &dumb);
            }
            gwy_mask_iter_next(iter);
        }
    }

    // Scan.  A bit counterintiutively, we scan by column because this means
    // the samples added/removed to the workspace in each step form a
    // contiguous block in a single row.
    guint i = 0, j = 0;
    gint addedabove, addedbelow, ne;
    const guint *base = intvalues;
    while (TRUE) {
        // Downward pass of the zig-zag pattern.
        while (i < height-1) {
            addedabove = addedbelow = 0;
            ne = gather_edge_intvalues(edges, base, UPPER, transferbuf);
            bucket_remove_values(buckets, bucket_len, bucket_sorted, bshift,
                                 transferbuf, ne, revindex,
                                 median, &addedbelow, &addedabove);
            i++;
            base += xsize;
            ne = gather_edge_intvalues(edges, base, LOWER, transferbuf);
            bucket_add_values(buckets, bucket_len, bucket_sorted, bshift,
                              transferbuf, ne, revindex,
                              median, &addedbelow, &addedabove);
            median = bucket_median(buckets, bucket_len, bucket_sorted, bshift,
                                   median, addedbelow, addedabove, revindex);
            targetbase[i*target->xres + j] = extdata[median];
        }
        if (j == width-1)
            break;

        // Move right (at the bottom)
        addedabove = addedbelow = 0;
        ne = gather_edge_intvalues(edges, base, LEFT, transferbuf);
        bucket_remove_values(buckets, bucket_len, bucket_sorted, bshift,
                             transferbuf, ne, revindex,
                             median, &addedbelow, &addedabove);
        j++;
        base++;
        ne = gather_edge_intvalues(edges, base, RIGHT, transferbuf);
        bucket_add_values(buckets, bucket_len, bucket_sorted, bshift,
                          transferbuf, ne, revindex,
                          median, &addedbelow, &addedabove);
        median = bucket_median(buckets, bucket_len, bucket_sorted, bshift,
                               median, addedbelow, addedabove, revindex);
        targetbase[i*target->xres + j] = extdata[median];

        // Upward pass of the zig-zag pattern.
        while (i) {
            addedabove = addedbelow = 0;
            ne = gather_edge_intvalues(edges, base, LOWER, transferbuf);
            bucket_remove_values(buckets, bucket_len, bucket_sorted, bshift,
                                 transferbuf, ne, revindex,
                                 median, &addedbelow, &addedabove);
            i--;
            base -= xsize;
            ne = gather_edge_intvalues(edges, base, UPPER, transferbuf);
            bucket_add_values(buckets, bucket_len, bucket_sorted, bshift,
                              transferbuf, ne, revindex,
                              median, &addedbelow, &addedabove);
            median = bucket_median(buckets, bucket_len, bucket_sorted, bshift,
                                   median, addedbelow, addedabove, revindex);
            targetbase[i*target->xres + j] = extdata[median];
        }
        if (j == width-1)
            break;

        // Move right (at the top)
        addedabove = addedbelow = 0;
        ne = gather_edge_intvalues(edges, base, LEFT, transferbuf);
        bucket_remove_values(buckets, bucket_len, bucket_sorted, bshift,
                             transferbuf, ne, revindex,
                             median, &addedbelow, &addedabove);
        j++;
        base++;
        ne = gather_edge_intvalues(edges, base, RIGHT, transferbuf);
        bucket_add_values(buckets, bucket_len, bucket_sorted, bshift,
                          transferbuf, ne, revindex,
                          median, &addedbelow, &addedabove);
        median = bucket_median(buckets, bucket_len, bucket_sorted, bshift,
                               median, addedbelow, addedabove, revindex);
        targetbase[i*target->xres + j] = extdata[median];
    }

    kernel_edges_free(edges);
    g_free(transferbuf);
    g_free(extdata);
    g_free(intvalues);
    g_free(revindex);
    g_free(buckets);
    g_free(bucket_len);
    g_free(bucket_sorted);
}

static gpointer
filter_median_task(gpointer user_data)
{
    MedianFilterState *state = (MedianFilterState*)user_data;
    if (state->col >= state->mfdata.width)
        return NULL;

    MedianFilterTask *task = g_slice_new(MedianFilterTask);
    task->mfdata = state->mfdata;
    task->medfunc = state->medfunc;

    guint colend = MIN(state->col + state->colwidth, state->mfdata.width);
    task->mfdata.col = state->mfdata.col + state->col;
    task->mfdata.width = colend - state->col;
    task->mfdata.targetcol = state->targetcol + state->col;
    state->col = colend;

    return task;
}

static gpointer
filter_median_worker(gpointer taskp,
                     G_GNUC_UNUSED gpointer data)
{
    MedianFilterTask *task = (MedianFilterTask*)taskp;
    task->medfunc(&task->mfdata);
    return taskp;
}

static void
filter_median_result(gpointer result,
                     G_GNUC_UNUSED gpointer user_data)
{
    MedianFilterTask *task = (MedianFilterTask*)result;
    g_slice_free(MedianFilterTask, task);
}

static void
filter_median_split(const MedianFilterData *mfdata,
                    MedianFilterFunc medfunc,
                    guint split_threshold)
{
    guint width = mfdata->width, height = mfdata->height;
    guint targetcol = mfdata->targetcol, targetrow = mfdata->targetrow;
    guint ncols = MIN(width*height/split_threshold, width);
    if (ncols < 2) {
        medfunc(mfdata);
        return;
    }

    guint colwidth = MAX(width/ncols, mfdata->kernel->xres);
    if (colwidth >= width) {
        medfunc(mfdata);
        return;
    }

    GwyField *tmptarget = NULL;
    guint tmpcol = 0, tmprow = 0;
    if (mfdata->target == mfdata->field)
        tmptarget = gwy_field_new_sized(width, height, FALSE);
    else {
        tmptarget = mfdata->target;
        tmpcol = targetcol;
        tmprow = targetrow;
    }

    MedianFilterData mfdatacol = *mfdata;
    mfdatacol.target = tmptarget;
    mfdatacol.targetrow = tmprow;

    GwyMaster *master = gwy_master_acquire_default(TRUE);
    MedianFilterState state = {
        .mfdata = mfdatacol, .medfunc = medfunc,
        .col = 0, .colwidth = colwidth, .targetcol = tmpcol,
    };
    gwy_master_manage_tasks(master, 0,
                            &filter_median_worker,
                            &filter_median_task,
                            &filter_median_result,
                            &state,
                            NULL);
    gwy_master_release_default(master);

    if (tmptarget != mfdata->target) {
        gwy_field_copy(tmptarget, NULL, mfdata->target, targetcol, targetrow);
        g_object_unref(tmptarget);
    }
}

/**
 * gwy_field_filter_median:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @target: A two-dimensional data field where the result will be placed.
 *          It may be @field for an in-place modification.  Its dimensions may
 *          match either @field or @fpart.  In the former case the
 *          placement of result is determined by @fpart; in the latter case
 *          the result fills the entire @target.
 * @kernel: Kernel mask defining the shape of the area of which the median
 *          is taken.  It must have at least one pixel set.  It is recommended
 *          not to have any empty borders in the kernel; make the field fit
 *          closely the non-zero region.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 *
 * Processes a field with median filter.
 **/
void
gwy_field_filter_median(const GwyField *field,
                        const GwyFieldPart *fpart,
                        GwyField *target,
                        const GwyMaskField *kernel,
                        GwyExteriorType exterior,
                        gdouble fill_value)
{
    guint col, row, width, height, targetcol, targetrow;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height)
        || !gwy_field_check_target(field, target,
                                   &(GwyFieldPart){ col, row, width, height },
                                   &targetcol, &targetrow))
        return;

    g_return_if_fail(GWY_IS_MASK_FIELD(kernel));
    RectExtendFunc extend_rect = _gwy_get_rect_extend_func(exterior);
    if (!extend_rect)
        return;

    guint kn = gwy_mask_field_count(kernel, NULL, TRUE);
    if (!kn) {
        g_warning("Median filter requires a non-empty kernel.");
        // Do something basically sane.
        if (target != field || targetcol != col || targetrow != row)
            gwy_field_copy(field, fpart, target, targetcol, targetrow);
        return;
    }

    KernelEdges *edges = analyse_kernel_edges(kernel);

    MedianFilterData mfdata = {
        .field = field,
        .col = col, .row = row, .width = width, .height = height,
        .target = target,
        .targetcol = targetcol, .targetrow = targetrow,
        .kernel = kernel, .edges = edges, .kn = kn,
        .extend_rect = extend_rect, .fill_value = fill_value,
    };

    // FIXME: Using kn, edges and kxres*kyres we can assess how scattered the
    // kernel is and prefer the direct filter for sparse kernels.
    if (median_filter_method == MEDIAN_FILTER_BUCKET
        || (median_filter_method == MEDIAN_FILTER_AUTO
            && kernel->xres*kernel->yres >= 100))
        filter_median_split(&mfdata, &filter_median_bucket, 500*500);
    else
        filter_median_split(&mfdata, &filter_median_direct, 500*500);

    kernel_edges_free(edges);

    if (target != field) {
        _gwy_assign_unit(&target->priv->xunit, field->priv->xunit);
        _gwy_assign_unit(&target->priv->yunit, field->priv->yunit);
        _gwy_assign_unit(&target->priv->zunit, field->priv->zunit);
    }

    gwy_field_invalidate(target);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
