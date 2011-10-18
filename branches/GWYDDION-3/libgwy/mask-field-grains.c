/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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
#include "libgwy/mask-field-grains.h"
#include "libgwy/mask-field-arithmetic.h"
#include "libgwy/mask-field-internal.h"

static inline guint32*
ensure_map(guint max_no, guint *map, guint *mapsize)
{
    if (G_UNLIKELY(max_no == *mapsize)) {
        *mapsize *= 2;
        return g_renew(guint, map, *mapsize);
    }
    return map;
}

static void
number_grains(GwyMaskField *field)
{
    MaskField *priv = field->priv;
    g_return_if_fail(!priv->grains);

    guint xres = field->xres, yres = field->yres;
    priv->grains = g_new(guint, xres*yres);

    // A reasonable initial size of the grain map.
    guint msize = 4*(field->xres + field->yres);
    guint *m = g_new0(guint, msize);

    /* Number grains with simple unidirectional grain number propagation,
     * updating map m for later full grain join */
    guint max_id = 0;
    for (guint i = 0; i < yres; i++) {
        guint *g = priv->grains + i*xres;
        guint *gprev = g - xres;
        GwyMaskIter iter;
        gwy_mask_field_iter_init(field, iter, 0, i);
        guint grain_id = 0;
        for (guint j = xres; j; j--, g++, gprev++) {
            if (gwy_mask_iter_get(iter)) {
                /* Grain number is kept from the top or left neighbour
                 * unless it does not exist (a new number is assigned) or a
                 * join with top neighbour occurs (m is updated) */
                guint id;
                if (i && (id = *gprev)) {
                    if (!grain_id)
                        grain_id = id;
                    else if (id != grain_id) {
                        resolve_grain_map(m, id, grain_id);
                        grain_id = m[id];
                    }
                }
                if (!grain_id) {
                    grain_id = ++max_id;
                    m = ensure_map(grain_id, m, &msize);
                    m[grain_id] = grain_id;
                }
            }
            else
                grain_id = 0;
            *g = grain_id;
            gwy_mask_iter_next(iter);
        }
    }

    // Resolve remaining grain number links in the map.  This nicely works
    // because we resolve downwards and go from the lowest number.
    for (guint i = 1; i <= max_id; i++)
        m[i] = m[m[i]];

    /* Compactify grain numbers */
    guint *mm = g_new0(guint, max_id + 1);
    guint id = 0;
    for (guint i = 1; i <= max_id; i++) {
        if (!mm[m[i]]) {
            id++;
            mm[m[i]] = id;
        }
        m[i] = mm[m[i]];
    }
    g_free(mm);

    /* Renumber grains (we make use of the fact m[0] = 0) */
    guint *g = priv->grains;
    for (guint i = 0; i < xres*yres; i++)
        g[i] = m[g[i]];

    g_free(m);

    priv->ngrains = id;
}

/**
 * gwy_mask_field_n_grains:
 * @field: A two-dimensional mask field.
 *
 * Obtains the number of contiguous grains in a mask field.
 *
 * Returns: The number of grains in @field.
 **/
guint
gwy_mask_field_n_grains(GwyMaskField *field)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), 0);
    MaskField *priv = field->priv;
    if (!priv->grains)
        number_grains(field);

    return priv->ngrains;
}

/**
 * gwy_mask_field_grain_numbers:
 * @field: A two-dimensional mask field.
 *
 * Obtains grain numbers of a mask field.
 *
 * Returns: (transfer none):
 *          Array of integers of the same number of items as @field
 *          (without padding) filled with grain numbers of each pixel.  Empty
 *          space is set to 0, pixels inside a grain are set to the grain
 *          number.  Grains are numbered sequentially 1, 2, 3, ...
 *          The returned array is owned by @field and becomes invalid when
 *          the data change, gwy_mask_field_invalidate() is called or the
 *          mask field is finalized.
 **/
const guint*
gwy_mask_field_grain_numbers(GwyMaskField *field)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), NULL);
    MaskField *priv = field->priv;
    if (!priv->grains)
        number_grains(field);

    return priv->grains;
}

static void
calculate_grain_properties(GwyMaskField *field)
{
    MaskField *priv = field->priv;

    gwy_mask_field_grain_numbers(field);

    GWY_FREE(priv->grain_sizes);
    GWY_FREE(priv->grain_bounding_boxes);

    // Put max_col to width, max_row to height first, then subtract col, row.
    guint ngrains = priv->ngrains;
    priv->grain_sizes = g_new0(guint, ngrains+1);
    priv->grain_bounding_boxes = g_new(GwyFieldPart, ngrains+1);
    GwyFieldPart *fpart;

    fpart = priv->grain_bounding_boxes;
    for (guint id = 0; id <= ngrains; id++, fpart++) {
        *fpart = (GwyFieldPart){ G_MAXUINT, G_MAXUINT, 0, 0 };
    }

    guint xres = field->xres, yres = field->yres;
    guint *g = priv->grains;
    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, g++) {
            fpart = priv->grain_bounding_boxes + *g;

            if (j < fpart->col)
                fpart->col = j;
            if (j > fpart->width)
                fpart->width = j;
            if (i < fpart->row)
                fpart->row = i;
            if (i > fpart->height)
                fpart->height = i;

            priv->grain_sizes[*g]++;
        }
    }

    fpart = priv->grain_bounding_boxes;
    // The zeroth grain (empty space) can be empty.
    if (fpart->col < G_MAXUINT) {
        fpart->width = fpart->width+1 - fpart->col;
        fpart->height = fpart->height+1 - fpart->row;
    }
    else {
        gwy_clear(fpart, 1);
    }
    fpart++;

    for (guint id = 1; id <= ngrains; id++, fpart++) {
        fpart->width = fpart->width+1 - fpart->col;
        fpart->height = fpart->height+1 - fpart->row;
    }
}

/**
 * gwy_mask_field_grain_sizes:
 * @field: A two-dimensional mask field.
 *
 * Obtains the number of pixels of each grain in a mask field.
 *
 * Items 1 to @ngrains correspond to grains while the 0th item corresponds to
 * the empty space between.
 *
 * Returns: (transfer none):
 *          Array of @ngrains+1 grain sizes. The returned array is owned by
 *          @field and becomes invalid when the data change,
 *          gwy_mask_field_invalidate() is called or the mask field is
 *          finalized.
 **/
const guint*
gwy_mask_field_grain_sizes(GwyMaskField *field)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), NULL);
    if (!field->priv->grain_sizes)
        calculate_grain_properties(field);

    return field->priv->grain_sizes;
}

/**
 * gwy_mask_field_grain_bounding_boxes:
 * @field: A two-dimensional mask field.
 *
 * Obtains the bounding box of each grain in a mask field.
 *
 * Items 1 to @ngrains correspond to grains while the 0th item corresponds to
 * the empty space between.  The dimensions of the 0th item can be zero in case
 * the field is fully filled with ones.
 *
 * Returns: (transfer none):
 *          Array of @ngrains+1 grain bounding boxes.  The returned array is
 *          owned by @field and becomes invalid when the data change,
 *          gwy_mask_field_invalidate() is called or the mask field is
 *          finalized.
 **/
const GwyFieldPart*
gwy_mask_field_grain_bounding_boxes(GwyMaskField *field)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), NULL);
    if (!field->priv->grain_bounding_boxes)
        calculate_grain_properties(field);

    return field->priv->grain_bounding_boxes;
}

static GwyXY*
find_grain_positions(GwyMaskField *field)
{
    guint xres = field->xres, yres = field->yres;
    guint ngrains = gwy_mask_field_n_grains(field);
    const guint *grains = gwy_mask_field_grain_numbers(field);
    const guint *sizes = gwy_mask_field_grain_sizes(field);
    GwyXY *centres = g_new0(GwyXY, ngrains+1);
    const guint *g = grains;

    //if (xres == 1 || yres == 1) {
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++, g++) {
                guint grain_id = *g;
                centres[grain_id].x += j;
                centres[grain_id].y += i;
            }
        }
        centres[0].x = centres[0].y = NAN;
        for (guint k = 1; k <= ngrains; k++) {
            centres[k].x /= sizes[k];
            centres[k].y /= sizes[k];
        }
        return centres;
    //}

    // FIXME: This needs some rethinking.  All proper methods seem terribly
    // slow.
#if 0
    guint *levels = g_new0(guint, 2*xres*yres),
          *todo = levels + xres*yres,
          *new_todo = todo + xres*yres/2;

    guint *l = levels;
    guint todopos = 0, todofrom = 0, k = 0;
    guint grain_id;

    // Locate the boundary.
    // The trick here is to avoid ever adding boundary points to todo.  Then
    // the subsequent iterations never touch the boundaries and we do not have
    // to check for it at all.

    // Top row.
    if ((grain_id = *g)) {
        centres[grain_id].x += 0;
        centres[grain_id].y += 0;
        *l = 1;
        guint right_id = *(g + 1), lower_id = *(g + xres);
        if (right_id && !*(l + 1))
            *(l + 1) = 2;
        if (lower_id && !*(l + xres))
            *(l + xres) = 2;
    }
    g++, l++, k++;
    for (guint j = 1; j < xres-1; j++, g++, l++, k++) {
        if ((grain_id = *g)) {
            centres[grain_id].x += j;
            centres[grain_id].y += 0;
            *l = 1;
            guint left_id = *(g - 1), right_id = *(g + 1),
                  lower_id = *(g + xres);
            if (left_id && !*(l - 1))
                *(l - 1) = 2;
            if (right_id && !*(l + 1))
                *(l + 1) = 2;
            if (lower_id && !*(l + xres)) {
                *(l + xres) = 2;
                if (yres > 2)
                    todo[todopos++] = k + xres;
            }
        }
    }
    if ((grain_id = *g)) {
        centres[grain_id].x += xres-1;
        centres[grain_id].y += 0;
        *l = 1;
        guint left_id = *(g - 1), lower_id = *(g + xres);
        if (left_id && !*(l - 1))
            *(l - 1) = 2;
        if (lower_id && !*(l + xres))
            *(l + xres) = 2;
    }
    g++, l++, k++;

    // Middle rows.
    for (guint i = 1; i < yres-1; i++) {
        if ((grain_id = *g)) {
            centres[grain_id].x += 0;
            centres[grain_id].y += i;
            *l = 1;
            guint upper_id = *(g - xres), right_id = *(g + 1),
                  lower_id = *(g + xres);
            if (upper_id && !*(l - xres))
                *(l - xres) = 2;
            if (right_id && !*(l + 1)) {
                *(l + 1) = 2;
                if (xres > 2)
                    todo[todopos++] = k + 1;
            }
            if (lower_id && !*(l + xres))
                *(l + xres) = 2;
        }
        g++, l++, k++;

        for (guint j = 1; j < xres-1; j++, g++, l++, k++) {
            if ((grain_id = *g)) {
                centres[grain_id].x += j;
                centres[grain_id].y += i;
                guint upper_id = *(g - xres), left_id = *(g - 1),
                      right_id = *(g + 1), lower_id = *(g + xres);
                if (!(upper_id | left_id | right_id | lower_id)) {
                    *l = 1;
                    if (upper_id && !*(l - xres)) {
                        *(l - xres) = 2;
                        if (i > 1)
                            todo[todopos++] = k - xres;
                    }
                    if (left_id && !*(l - 1)) {
                        *(l - 1) = 2;
                        if (j > 1)
                            todo[todopos++] = k - 1;
                    }
                    if (right_id && !*(l + 1)) {
                        *(l + 1) = 2;
                        if (j < xres-2)
                            todo[todopos++] = k + 1;
                    }
                    if (lower_id && !*(l + xres)) {
                        *(l + xres) = 2;
                        if (i < yres-2)
                            todo[todopos++] = k + xres;
                    }
                }
            }
        }

        if ((grain_id = *g)) {
            centres[grain_id].x += xres-1;
            centres[grain_id].y += i;
            *l = 1;
            guint upper_id = *(g - xres), left_id = *(g - 1),
                  lower_id = *(g + xres);
                if (upper_id && !*(l - xres))
                    *(l - xres) = 2;
                if (left_id && !*(l - 1)) {
                    *(l - 1) = 2;
                    if (xres > 2)
                        todo[todopos++] = k - 1;
                }
                if (lower_id && !*(l + xres))
                    *(l + xres) = 2;
        }
        g++, l++, k++;
    }

    // Bottom row.
    if ((grain_id = *g)) {
        centres[grain_id].x += 0;
        centres[grain_id].y += yres-1;
        *l = 1;
        guint right_id = *(g + 1), upper_id = *(g - xres);
        if (upper_id && !*(l - xres))
            *(l - xres) = 2;
        if (right_id && !*(l + 1))
            *(l + 1) = 2;
    }
    g++, l++, k++;
    for (guint j = 1; j < xres-1; j++, g++, l++, k++) {
        if ((grain_id = *g)) {
            centres[grain_id].x += j;
            centres[grain_id].y += yres-1;
            *l = 1;
            guint left_id = *(g - 1), right_id = *(g + 1),
                  upper_id = *(g - xres);
            if (upper_id && !*(l - xres)) {
                *(l - xres) = 2;
                if (yres > 2)
                    todo[todopos++] = k - xres;
            }
            if (left_id && !*(l - 1))
                *(l - 1) = 2;
            if (right_id && !*(l + 1))
                *(l + 1) = 2;
        }
    }
    if ((grain_id = *g)) {
        centres[grain_id].x += xres-1;
        centres[grain_id].y += yres-1;
        *l = 1;
        guint left_id = *(g - 1), upper_id = *(g - xres);
        if (upper_id && !*(l - xres))
            *(l - xres) = 2;
        if (left_id && !*(l - 1))
            *(l - 1) = 2;
    }
    g++, l++, k++;
    g_assert(todopos <= xres*yres/2);

    for (guint k = 0; k <= ngrains; k++) {
        centres[k].x /= sizes[k];
        centres[k].y /= sizes[k];
    }

    // Fill the insides by stages.
    guint level = 3;
    while (todopos) {
        guint n = todopos;
        todopos = 0;
        for (guint m = 0; m < n; m++) {
            k = todo[m];
            g = grains + k;
            l = levels + k;
            if (*(g - xres) && !*(l - xres)) {
                *(l - xres) = level;
                new_todo[todopos++] = k - xres;
            }
            if (*(g - 1) && !*(l - 1)) {
                *(l - 1) = level;
                new_todo[todopos++] = k - 1;
            }
            if (*(g + 1) && !*(l + 1)) {
                *(l + 1) = level;
                new_todo[todopos++] = k + 1;
            }
            if (*(g + xres) && !*(l + xres)) {
                *(l + xres) = level;
                new_todo[todopos++] = k + xres;
            }
        }
        level++;
        GWY_SWAP(guint*, todo, new_todo);
        g_assert(todopos <= xres*yres/2);
    }

    // For each grain, find the max-level pixel that is closest to the centre.
    GwyXY *visual_centres = g_new0(GwyXY, ngrains+1);

    g_free(levels);
    g_free(centres);

    return visual_centres;
#endif
}

/**
 * gwy_mask_field_grain_positions:
 * @field: A two-dimensional mask field.
 *
 * Obtains the representative visual positions of each grain in a mask field.
 *
 * Items 1 to @ngrains correspond to grains while the 0th item is set to NaN.
 *
 * The returned positions should be suitable for drawing a marker, for instance
 * the grain number, in a visual representation of the field.  They may not
 * have any direct relation to grain mass centres and similar quantities.
 *
 * Returns: (transfer none):
 *          Array of @ngrains+1 grain bounding boxes.  The returned array is
 *          owned by @field and becomes invalid when the data change,
 *          gwy_mask_field_invalidate() is called or the mask field is
 *          finalized.
 **/
const GwyXY*
gwy_mask_field_grain_positions(GwyMaskField *field)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), NULL);
    if (!field->priv->grain_positions)
        field->priv->grain_positions = find_grain_positions(field);

    return field->priv->grain_positions;
}

/**
 * gwy_mask_field_remove_grain:
 * @field: A two-dimensional mask field.
 * @grain_id: Grain number (from 1 to @ngrains).
 *
 * Removes the grain of given number from a mask field.
 *
 * The grain number is the number used e.g. in gwy_mask_field_grain_numbers().
 *
 * The remaining grains are renumbered and the sizes and bounding boxes of
 * empty space updated so that you can continue to use the arrays returned
 * from e.g. gwy_mask_field_grain_sizes(), just with the number of grains one
 * smaller.
 **/
void
gwy_mask_field_remove_grain(GwyMaskField *field,
                            guint grain_id)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    g_return_if_fail(grain_id > 0);
    // Normally the caller must have obtained the grain id somewhere so the
    // grains are numbered.  But just in case...
    gwy_mask_field_grain_numbers(field);

    MaskField *priv = field->priv;
    guint ngrains = priv->ngrains;
    g_return_if_fail(grain_id <= ngrains);

    guint xres = field->xres, yres = field->yres;
    GwyFieldPart *bboxes = priv->grain_bounding_boxes;
    guint *sizes = priv->grain_sizes;

    // A silly case we do not want to handle below as it is more convenient to
    // assume grain 0 (empty space) has some pixels.  We promise to keep the
    // cache usable so do not simply call gwy_mask_field_fill() as it would
    // destroy the cache.
    if (ngrains == 1) {
        gwy_clear(field->data, field->stride*yres);
        gwy_clear(priv->grains, xres*yres);
        priv->ngrains = 0;
        if (sizes)
            sizes[0] = xres*yres;
        if (bboxes)
            bboxes[0] = (GwyFieldPart){ 0, 0, xres, yres };
        return;
    }

    // Remove the grain, renumber the field.
    if (bboxes) {
        // If we have the bbox it is not necessary to process the entire field.
        GwyFieldPart *fpart = bboxes + grain_id;
        for (guint i = fpart->row; i < fpart->row + fpart->height; i++) {
            guint *g = priv->grains + i*xres + fpart->col;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(field, iter, fpart->col, i);
            for (guint j = fpart->width; j; j--, g++) {
                if (*g == grain_id)
                    gwy_mask_iter_set(iter, FALSE);
                gwy_mask_iter_next(iter);
            }
        }

        guint *g = priv->grains;
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++, g++) {
                if (*g == grain_id)
                    *g = 0;
                else if (*g > grain_id)
                    (*g)--;
            }
        }

        // Update the bbox of empty space.
        gwy_field_part_union(bboxes, fpart);

        // Update the other changed bboxes.
        memmove(bboxes + grain_id, bboxes + (grain_id + 1),
                (ngrains - grain_id)*sizeof(GwyFieldPart));
    }
    else {
        guint *g = priv->grains;
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++, g++) {
                if (*g == grain_id) {
                    gwy_mask_field_set(field, j, i, FALSE);
                    *g = 0;
                }
                else if (*g > grain_id)
                    (*g)--;
            }
        }
    }

    // If we have grain data update them too.
    if (sizes) {
        sizes[0] += sizes[grain_id];
        memmove(sizes + grain_id, sizes + (grain_id + 1),
                (ngrains - grain_id)*sizeof(guint));
    }

    priv->ngrains--;
}

/**
 * SECTION: mask-field-grains
 * @section_id: GwyMaskField-grains
 * @title: GwyMaskField grains
 * @short_description: Using mask fields to mark grains
 *
 * Several mask field methods deal with grains.  In this context, grain
 * simply means a contiguous part of the mask, not touching other parts of the
 * mask (two pixels with just a common corner are considered separate).  The
 * term grain has the origin in the common use of these methods on the result
 * of a grain marking function.
 *
 * Grains are numbered sequentially from 1 to the maximum grain number denoted
 * @ngrains below.  The numbering is stable, i.e. it is always the same for
 * the same mask field.  Specifically, grains are numbered by the position of
 * the first pixel belonging to the grain.
 *
 * Grain functions often return or work with arrays where each item corresponds
 * to one grain.  The dimension of such arrays is always @ngrains+1,
 * <emphasis>not</emphasis> @ngrains.  The zeroth element may either correspond
 * to the empty space between grains or have no meaning, but it is always
 * present.  Elements 1 to @ngrains correspond to grains.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
