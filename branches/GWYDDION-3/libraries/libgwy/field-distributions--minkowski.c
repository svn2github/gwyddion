/*
 *  $Id$
 *  Copyright (C) 2009-2013 David Nečas (Yeti).
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
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/line-distributions.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/field-statistics.h"
#include "libgwy/field-distributions.h"
#include "libgwy/field-internal.h"
#include "libgwy/mask-field-internal.h"

static void
sanitise_range(gdouble *min,
               gdouble *max)
{
    if (*max > *min)
        return;
    if (*max) {
        gdouble d = fabs(*max);
        *max += 0.1*d;
        *min -= 0.1*d;
        return;
    }
    *min = -1.0;
    *max = 1.0;
}

static inline void
add_to_dist(GwyLine *dist, gdouble z)
{
    gdouble x = (z - dist->off)/dist->real*dist->res - 0.5;
    if (x <= 0.0)
        dist->data[0] += 1.0;
    else if (x <= dist->res-1)
        dist->data[(guint)ceil(x)] += 1.0;
}

static GwyLine*
minkowski_volume(const GwyField *field,
                 guint col, guint row,
                 guint width, guint height,
                 const GwyMaskField *mask,
                 GwyMasking masking,
                 guint maskcol, guint maskrow,
                 guint npoints,
                 gdouble min, gdouble max)
{
    GwyFieldPart rect = { maskcol, maskrow, width, height };
    guint n = gwy_mask_field_part_count_masking(mask, &rect, masking);

    if (!n)
        return NULL;

    if (!(min < max))
        gwy_field_min_max(field, &(GwyFieldPart){ col, row, width, height },
                          mask, masking, &min, &max);
    sanitise_range(&min, &max);

    if (!npoints)
        npoints = dist_points_for_n_points(n);

    GwyLine *dist = gwy_line_new_sized(npoints, TRUE);
    dist->real = max - min;
    dist->off = min;

    if (masking != GWY_MASK_IGNORE) {
        gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            const gdouble *drow = field->data + (i + row)*field->xres + col;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, drow++) {
                if (!gwy_mask_iter_get(iter) == invert)
                    add_to_dist(dist, *drow);
                gwy_mask_iter_next(iter);
            }
        }
    }
    else {
        for (guint i = 0; i < height; i++) {
            const gdouble *drow = field->data + (i + row)*field->xres + col;
            for (guint j = width; j; j--, drow++)
                add_to_dist(dist, *drow);
        }
    }

    // The non-cumulative distributions are already prepared pixel-centered so
    // use plain summing here.
    gwy_line_accumulate(dist, FALSE);
    for (guint k = 0; k < dist->res; k++)
        dist->data[k] = 1.0 - dist->data[k]/n;

    return dist;
}

/**
 * count_edges:
 * @mask: A mask field.
 * @masking: Masking mode.
 * @col: Column index.
 * @row: Row index.
 * @width: Part width (number of column).
 * @height: Part height (number of rows).
 * @min: Location to store the minimum.
 * @max: Location to store the maximum.
 *
 * Counts the number of edges between two pixels.
 *
 * An edge is counted if both pixels are counted according to the masking mode
 * @masking.
 *
 * Since only edges that lie between two counted pixels contribute the minimum
 * and maximum is also calculated edge-wise.  This essentially means that
 * they are calculated ignoring single-pixel grains as all other masked values
 * have some neighbour.
 *
 * Returns: The number of edges.
 **/
static guint
count_edges(const GwyField *field,
            guint col, guint row,
            guint width, guint height,
            const GwyMaskField *mask,
            GwyMasking masking,
            guint maskcol, guint maskrow,
            gdouble *min, gdouble *max)
{
    if (masking == GWY_MASK_IGNORE) {
        gwy_field_min_max(field, &(GwyFieldPart){ col, row, width, height },
                          NULL, GWY_MASK_IGNORE, min, max);
        return 2*width*height - width - height;
    }

    g_assert(mask);

    gboolean invert = (masking == GWY_MASK_EXCLUDE);
    guint xres = field->xres;
    guint nedges = 0;

    gdouble min1 = G_MAXDOUBLE, max1 = -G_MAXDOUBLE;
    const gdouble *base = field->data + row*xres + col;
    for (guint i = 0; i < height-1; i++) {
        GwyMaskIter iter1, iter2;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + i);
        gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow + i+1);

        gboolean curr = !gwy_mask_iter_get(iter1);
        gboolean lower = !gwy_mask_iter_get(iter2);
        if (curr == invert && lower == invert) {
            gdouble z1 = base[i*xres], z2 = base[(i + 1)*xres];
            GWY_ORDER(gdouble, z1, z2);
            if (z1 < min1)
                min1 = z1;
            if (z2 > max1)
                max1 = z2;
            nedges++;
        }

        for (guint j = 1; j < width; j++) {
            gboolean right = curr;
            gwy_mask_iter_next(iter1);
            gwy_mask_iter_next(iter2);
            curr = !gwy_mask_iter_get(iter1);
            lower = !gwy_mask_iter_get(iter2);
            if (curr == invert && right == invert) {
                gdouble z1 = base[i*xres + j-1], z2 = base[i*xres + j];
                GWY_ORDER(gdouble, z1, z2);
                if (z1 < min1)
                    min1 = z1;
                if (z2 > max1)
                    max1 = z2;
                nedges++;
            }
            if (curr == invert && lower == invert) {
                gdouble z1 = base[i*xres + j], z2 = base[(i + 1)*xres + j];
                GWY_ORDER(gdouble, z1, z2);
                if (z1 < min1)
                    min1 = z1;
                if (z2 > max1)
                    max1 = z2;
                nedges++;
            }
        }
    }

    GwyMaskIter iter;
    gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + height-1);
    gboolean curr = !gwy_mask_iter_get(iter);
    for (guint j = 1; j < width; j++) {
        gboolean right = curr;
        gwy_mask_iter_next(iter);
        curr = !gwy_mask_iter_get(iter);
        if (curr == invert && right == invert) {
            gdouble z1 = base[(height-1)*xres + j-1],
                    z2 = base[(height-1)*xres + j];
            GWY_ORDER(gdouble, z1, z2);
            if (z1 < min1)
                min1 = z1;
            if (z2 > max1)
                max1 = z2;
            nedges++;
        }
    }

    *min = min1;
    *max = max1;

    return nedges;
}

static inline void
add_to_min_max_dist(GwyLine *mindist, GwyLine *maxdist,
                    gdouble z1, gdouble z2)
{
    GWY_ORDER(gdouble, z1, z2);
    add_to_dist(mindist, z1);
    add_to_dist(maxdist, z2);
}

static void
calculate_min_max_dist(const GwyField *field,
                       guint col, guint row,
                       guint width, guint height,
                       const GwyMaskField *mask,
                       GwyMasking masking,
                       guint maskcol, guint maskrow,
                       GwyLine *mindist, GwyLine *maxdist)
{
    guint xres = field->xres;

    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height-1; i++) {
            const gdouble *frow = field->data + (row + i)*xres + col;
            add_to_min_max_dist(mindist, maxdist, *frow, *(frow + xres));
            for (guint j = width-1; j; j--) {
                add_to_min_max_dist(mindist, maxdist, *frow, *(frow + 1));
                frow++;
                add_to_min_max_dist(mindist, maxdist, *frow, *(frow + xres));
            }
        }
        const gdouble *frow = field->data + (row + height-1)*xres + col;
        for (guint j = width-1; j; j--, frow++)
            add_to_min_max_dist(mindist, maxdist, *frow, *(frow + 1));

        return;
    }

    g_assert(mask);

    gboolean invert = (masking == GWY_MASK_EXCLUDE);

    for (guint i = 0; i < height-1; i++) {
        const gdouble *frow = field->data + (row + i)*xres + col;
        GwyMaskIter iter1, iter2;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + i);
        gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow + i+1);

        gboolean curr = !gwy_mask_iter_get(iter1);
        gboolean lower = !gwy_mask_iter_get(iter2);
        if (curr == invert && lower == invert)
            add_to_min_max_dist(mindist, maxdist, *frow, *(frow + xres));

        for (guint j = width-1; j; j--) {
            gboolean right = curr;
            gwy_mask_iter_next(iter1);
            gwy_mask_iter_next(iter2);
            curr = !gwy_mask_iter_get(iter1);
            lower = !gwy_mask_iter_get(iter2);
            if (curr == invert && right == invert)
                add_to_min_max_dist(mindist, maxdist, *frow, *(frow + 1));
            frow++;
            if (curr == invert && lower == invert)
                add_to_min_max_dist(mindist, maxdist, *frow, *(frow + xres));
        }
    }

    const gdouble *frow = field->data + (row + height-1)*xres + col;
    GwyMaskIter iter;
    gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + height-1);
    gboolean curr = !gwy_mask_iter_get(iter);
    for (guint j = width-1; j; j--, frow++) {
        gboolean right = curr;
        gwy_mask_iter_next(iter);
        curr = !gwy_mask_iter_get(iter);
        if (curr == invert && right == invert)
            add_to_min_max_dist(mindist, maxdist, *frow, *(frow + 1));
    }
}

/* The calculation is based on expressing the functional as the difference of
 * cumulative distributions of min(z1, z2) and max(z1, z2) where (z1, z2) are
 * couples of pixels with a common edge. */
static GwyLine*
minkowski_boundary(const GwyField *field,
                   guint col, guint row,
                   guint width, guint height,
                   const GwyMaskField *mask,
                   GwyMasking masking,
                   guint maskcol, guint maskrow,
                   guint npoints,
                   gdouble min, gdouble max)
{
    gdouble min1, max1;
    guint nedges = count_edges(field, col, row, width, height,
                               mask, masking, maskcol, maskrow,
                               &min1, &max1);

    if (!nedges)
        return NULL;

    // FIXME: For npoints, it would be more useful to count only edges in
    // range, not all edges.  However, the total number if needed for
    // normalization.
    if (!npoints)
        npoints = dist_points_for_n_points(nedges);

    if (!(min < max)) {
        min = min1;
        max = max1;
    }
    sanitise_range(&min, &max);

    GwyLine *line = gwy_line_new_sized(npoints, TRUE);
    line->real = max - min;
    line->off = min;
    GwyLine *mindist = line, *maxdist = gwy_line_duplicate(mindist);
    calculate_min_max_dist(field, col, row, width, height,
                           mask, masking, maskcol, maskrow,
                           mindist, maxdist);
    // The non-cumulative distributions are already prepared pixel-centered so
    // use plain summing here.
    gwy_line_accumulate(mindist, FALSE);
    gwy_line_accumulate(maxdist, FALSE);
    gwy_line_add_line(maxdist, NULL, mindist, 0, -1.0);
    g_object_unref(maxdist);
    gwy_line_multiply_full(mindist, 1.0/nedges);

    return line;
}

/* Calculate discrete heights.
 *
 * There are @npoints+1 buckes, the 0th collects everything below 1/2, the
 * rest is pixel-sized, the last collects evrything above @npoints-1/2 . This
 * makes the distribution pixel-centered and symmetrical for white and black
 * cases, as necessary.
 */
static guint*
discretise_heights(const GwyField *field,
                   guint col, guint row,
                   guint width, guint height,
                   const GwyMaskField *mask,
                   GwyMasking masking,
                   guint maskcol, guint maskrow,
                   guint npoints,
                   gdouble min, gdouble max,
                   gboolean white)
{
    guint *heights = g_new(guint, width*height);
    gdouble q = npoints/(max - min);
    gboolean invert = (masking == GWY_MASK_EXCLUDE);

    for (guint i = 0; i < height; i++) {
        const gdouble *drow = field->data + (i + row)*field->xres + col;
        guint *hrow = heights + i*width;

        if (masking == GWY_MASK_IGNORE) {
            for (guint j = width; j; j--, drow++, hrow++) {
                gdouble x = white ? (max - *drow) : (*drow - min);
                x = ceil(x*q - 0.5);
                x = CLAMP(x, 0.0, npoints);
                *hrow = (guint)x;
            }
        }
        else {
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, drow++, hrow++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    gdouble x = white ? (max - *drow) : (*drow - min);
                    x = ceil(x*q - 0.5);
                    x = CLAMP(x, 0.0, npoints);
                    *hrow = (guint)x;
                }
                else {
                    *hrow = G_MAXUINT;
                }
                gwy_mask_iter_next(iter);
            }
        }
    }

    return heights;
}

/*
 * Group pixels of the same discrete height.  nh then holds indices in
 * hindex where each height starts.
 */
static void
group_by_height(const guint *heights,
                guint npoints, guint size,
                guint *nh, guint *hindex)
{
    // Make nh[i] the start of the block of discrete height i in hindex[].
    for (guint i = 0; i < size; i++) {
        guint h = heights[i];
        if (h != G_MAXUINT)
            nh[h]++;
    }
    for (guint i = 1; i <= npoints; i++)
        nh[i] += nh[i-1];
    for (guint i = npoints; i; i--)
        nh[i] = nh[i-1];
    nh[0] = 0;

    // Fill the blocks in hindex[] with indices of points with the
    // corresponding discrete height.
    for (guint i = 0; i < size; i++) {
        guint h = heights[i];
        if (h != G_MAXUINT)
            hindex[nh[h]++] = i;
    }
    for (guint i = npoints+1; i; i--)
        nh[i] = nh[i-1];
    nh[0] = 0;
}

static inline guint
uniq_array(guint *x, guint n)
{
    guint i = 1;
    while (i < n) {
        guint j;
        for (j = 0; j < i; j++) {
            if (x[i] == x[j])
                break;
        }

        if (j < i) {
            x[i] = x[--n];
        }
        else
            i++;
    }
    return n;
}

static guint
compress_grain_numbers(guint *grains, guint n, guint *m, guint ngrains)
{
    // Build a fully resolved renumbering.
    guint count = 1;
    for (guint i = 1; i < ngrains; i++) {
        if (m[i] == i)
            m[i] = count++;
        else
            m[i] = m[m[i]];
    }

    // Apply the renumbering to grains.
    for (guint k = n; k; k--, grains++)
        *grains = m[*grains];

    // Apply the renumbering to the map.  This is trivial because the result
    // must be the sequence 1..count-1.
    for (guint i = 1; i < count; i++)
        m[i] = i;

    return count;
}

/**
 * grain_number_dist:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to the requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @min: Minimum threshold value.
 * @max: Maximum threshold value.
 * @white: If %TRUE, hills are marked, otherwise valleys are marked.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates threshold grain number distribution in given height range.
 *
 * This is the number of grains for each of @nstats equidistant height
 * threshold levels.  For large @nstats this function is much faster than the
 * equivalent number of gwy_data_field_grains_mark_height() calls.
 **/
static GwyLine*
grain_number_dist(const GwyField *field,
                  guint col, guint row,
                  guint width, guint height,
                  const GwyMaskField *mask,
                  GwyMasking masking,
                  guint maskcol, guint maskrow,
                  gboolean white,
                  guint npoints,
                  gdouble min, gdouble max)
{
    GwyFieldPart rect = { maskcol, maskrow, width, height };
    guint n = gwy_mask_field_part_count_masking(mask, &rect, masking);

    if (!n)
        return NULL;

    if (!npoints)
        npoints = dist_points_for_n_points(n);

    GwyLine *line = gwy_line_new_sized(npoints, FALSE);
    line->real = max - min;
    line->off = min;

    guint *heights = discretise_heights(field, col, row, width, height,
                                        mask, masking, maskcol, maskrow,
                                        npoints, min, max, white);
    guint *nh = g_new0(guint, npoints+2);
    guint *hindex = g_new(guint, n);
    group_by_height(heights, npoints, width*height, nh, hindex);

    guint *grains = heights;     // No longer needed.
    gwy_clear(grains, width*height);
    guint *m = g_new(guint, n+1);
    m[0] = 0;

    // Main iteration
    guint ngrains = 1;
    for (guint h = 0; h < npoints; h++) {
        if (h && nh[h] == nh[h+1]) {
            line->data[h] = line->data[h-1];
            continue;
        }

        for (guint l = nh[h]; l < nh[h+1]; l++) {
            guint k = hindex[l], i = k/width, j = k % width;
            g_assert(!grains[k]);

            // Find grain numbers of neighbours, if any.
            guint neigh[4], nn = 0;
            if (i && grains[k-width])
                neigh[nn++] = grains[k-width];
            if (j && grains[k-1])
                neigh[nn++] = grains[k-1];
            if (j < width-1 && grains[k+1])
                neigh[nn++] = grains[k+1];
            if (i < height-1 && grains[k+width])
                neigh[nn++] = grains[k+width];

            if (nn) {
                // Merge all grains that touch this pixel to one.
                nn = uniq_array(neigh, nn);
                for (guint p = 1; p < nn; p++)
                    resolve_grain_map(m, neigh[p-1], neigh[p]);
                guint ming = m[neigh[0]];
                for (guint p = 1; p < nn; p++) {
                    if (m[neigh[p]] < ming)
                        ming = m[neigh[p]];
                }
                // And this is also the number the new pixel gets.
                grains[k] = ming;
            }
            else {
                // A new grain not touching anything gets a new number.
                g_assert(ngrains <= n);
                m[ngrains] = ngrains;
                grains[k] = ngrains++;
                continue;
            }

        }

        // Resolve remaining grain number links in the map.  This nicely works
        // because we resolve downwards and go from the lowest number.
        guint count = 0;
        for (guint i = 1; i < ngrains; i++) {
            m[i] = m[m[i]];
            if (m[i] == i)
                count++;
        }

        line->data[h] = (gdouble)count/n;

        if (7*count/5 + 4 < ngrains)
            ngrains = compress_grain_numbers(grains, width*height, m, ngrains);

#if 0
        g_printerr("GRAINS %u :: %u\n", h, count);
        for (guint i = 0; i < height; i++) {
            for (guint j = 0; j < width; j++) {
                if (grains[i*width + j])
                    g_printerr("%02u", grains[i*width + j]);
                else
                    g_printerr("..");
                g_printerr("%c", j == width-1 ? '\n' : ' ');
            }
        }
        g_printerr("MAPPED %u\n", h);
        for (guint i = 0; i < height; i++) {
            for (guint j = 0; j < width; j++) {
                if (grains[i*width + j])
                    g_printerr("%02u", m[grains[i*width + j]]);
                else
                    g_printerr("..");
                g_printerr("%c", j == width-1 ? '\n' : ' ');
            }
        }
#endif
    }

    if (white) {
        for (guint j = 0; j < npoints/2; j++)
            GWY_SWAP(gdouble, line->data[j], line->data[npoints-1 - j]);
    }

    g_free(m);
    g_free(hindex);
    g_free(nh);
    g_free(heights);

    return line;
}

static GwyLine*
minkowski_ngrains(const GwyField *field,
                  guint col, guint row,
                  guint width, guint height,
                  const GwyMaskField *mask,
                  GwyMasking masking,
                  guint maskcol, guint maskrow,
                  gboolean white,
                  guint npoints,
                  gdouble min, gdouble max)
{
    if (!(min < max))
        gwy_field_min_max(field, &(GwyFieldPart){ col, row, width, height },
                          mask, masking, &min, &max);
    sanitise_range(&min, &max);

    return grain_number_dist(field, col, row, width, height,
                             mask, masking, maskcol, maskrow,
                             white, npoints, min, max);
}

static GwyLine*
minkowski_connectivity(const GwyField *field,
                       guint col, guint row,
                       guint width, guint height,
                       const GwyMaskField *mask,
                       GwyMasking masking,
                       guint maskcol, guint maskrow,
                       guint npoints,
                       gdouble min, gdouble max)
{
    if (!(min < max))
        gwy_field_min_max(field, &(GwyFieldPart){ col, row, width, height },
                          mask, masking, &min, &max);
    sanitise_range(&min, &max);

    GwyLine *whitedist = grain_number_dist(field, col, row, width, height,
                                           mask, masking, maskcol, maskrow,
                                           TRUE, npoints, min, max);
    if (!whitedist)
        return NULL;

    GwyLine *blackdist = grain_number_dist(field, col, row, width, height,
                                           mask, masking, maskcol, maskrow,
                                           FALSE, npoints, min, max);
    gwy_line_add_line(blackdist, NULL, whitedist, 0, -1.0);
    g_object_unref(blackdist);

    return whitedist;
}

/**
 * gwy_field_minkowski:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @type: Type of functional to calculate.
 * @npoints: Resolution, i.e. the number of returned line points.
 *           Pass zero to choose a suitable resolution automatically.
 * @min: Minimum value of the range to calculate the distribution in.
 * @max: Maximum value of the range to calculate the distribution in.
 *
 * Calculates given Minkowski functional of values in a field.
 *
 * Pass @max <= @min to calculate the functional in the full data range
 * (with masking still considered).
 *
 * Note at present masking is implemented only for the volume and boundary
 * functionals %GWY_MINKOWSKI_VOLUME and %GWY_MINKOWSKI_BOUNDARY.
 *
 * Returns: (transfer full):
 *          A new one-dimensional data line with the requested functional.
 **/
GwyLine*
gwy_field_minkowski(const GwyField *field,
                    const GwyFieldPart *fpart,
                    const GwyMaskField *mask,
                    GwyMasking masking,
                    GwyMinkowskiFunctional type,
                    guint npoints,
                    gdouble min, gdouble max)
{
    GwyLine *line = NULL;

    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    // Cannot determine npoints here, it depends on the functional.
    if (type == GWY_MINKOWSKI_VOLUME) {
        line = minkowski_volume(field, col, row, width, height,
                                mask, masking, maskcol, maskrow,
                                npoints, min, max);
    }
    else if (type == GWY_MINKOWSKI_BOUNDARY) {
        line = minkowski_boundary(field, col, row, width, height,
                                  mask, masking, maskcol, maskrow,
                                  npoints, min, max);
    }
    else if (type == GWY_MINKOWSKI_BLACK) {
        line = minkowski_ngrains(field, col, row, width, height,
                                 mask, masking, maskcol, maskrow,
                                 FALSE, npoints, min, max);
    }
    else if (type == GWY_MINKOWSKI_WHITE) {
        line = minkowski_ngrains(field, col, row, width, height,
                                 mask, masking, maskcol, maskrow,
                                 TRUE, npoints, min, max);
    }
    else if (type == GWY_MINKOWSKI_CONNECTIVITY) {
        line = minkowski_connectivity(field, col, row, width, height,
                                      mask, masking, maskcol, maskrow,
                                      npoints, min, max);
    }
    else {
        g_critical("Unknown Minkowski functional type %u.", type);
    }

fail:
    if (!line)
        line = gwy_line_new();

    gwy_unit_assign(gwy_line_get_xunit(line), gwy_field_get_zunit(field));

    return line;
}

/**
 * GwyMinkowskiFunctional:
 * @GWY_MINKOWSKI_VOLUME: Fraction of ‘white’ pixels from the total
 *                        number of pixels.
 * @GWY_MINKOWSKI_BOUNDARY: Fraction of ‘black–white’ pixel edges from the
 *                          total number of edges.
 * @GWY_MINKOWSKI_BLACK: The number of ‘black’ connected areas (grains) divided
 *                       by the total number of pixels.
 * @GWY_MINKOWSKI_WHITE: The number of ‘white’ connected areas (grains) divided
 *                       by the total number of pixels.
 * @GWY_MINKOWSKI_CONNECTIVITY: Difference between the numbers of ‘white’ and
 *                              ‘black’ connected areas (grains) divided by
 *                              the total number of pixels.
 *
 * Types of Minkowski functionals and related quantities.
 *
 * Each quantity is a function of threshold; pixels above this threshold are
 * considered ‘white’, pixels below ‘black’.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
