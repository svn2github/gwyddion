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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwy/math.h"
#include "libgwy/grain-value.h"
#include "libgwy/field-level.h"
#include "libgwy/mask-field-grains.h"
#include "libgwy/field-internal.h"
#include "libgwy/mask-field-internal.h"
#include "libgwy/grain-value-builtin.h"

enum {
    SEDINF = 0x7fffffffu,
};

// Edges are used for inscribed discs.
typedef struct {
    gdouble xa;
    gdouble ya;
    gdouble xb;
    gdouble yb;
    gdouble r2;    // Used to remember squared distance from current centre.
} Edge;

typedef struct {
    guint size;
    guint len;
    Edge *edges;
} EdgeList;

// Inscribed/excrcribed disc/circle.
typedef struct {
    gdouble x;
    gdouble y;
    gdouble R2;
    guint size;   // For candidate sorting.
} FooscribedDisc;

// Iterative algorithms that try to moving some position use NDIRECTIONS
// directions in each quadrant; shift_directions[] lists the vectors.
enum { NDIRECTIONS = 12 };

static const gdouble shift_directions[NDIRECTIONS*2] = {
    1.0, 0.0,
    0.9914448613738104, 0.1305261922200516,
    0.9659258262890683, 0.2588190451025207,
    0.9238795325112867, 0.3826834323650898,
    0.8660254037844387, 0.5,
    0.7933533402912352, 0.6087614290087207,
    0.7071067811865475, 0.7071067811865475,
    0.6087614290087207, 0.7933533402912352,
    0.5,                0.8660254037844387,
    0.3826834323650898, 0.9238795325112867,
    0.2588190451025207, 0.9659258262890683,
    0.1305261922200517, 0.9914448613738104,
};

static gboolean
all_null(guint n, guint *ngrains, ...)
{
    va_list ap;
    va_start(ap, ngrains);
    for (guint i = 0; i < n; i++) {
        const GwyGrainValue *grainvalue = va_arg(ap, const GwyGrainValue*);
        if (grainvalue) {
            GWY_MAYBE_SET(ngrains, grainvalue->priv->ngrains);
            va_end(ap);
            return FALSE;
        }
    }
    va_end(ap);
    return TRUE;
}

static gboolean
check_target(GwyGrainValue *grainvalue,
             gdouble **values,
             BuiltinGrainValueId id)
{
    if (!grainvalue) {
        *values = NULL;
        return TRUE;
    }

    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_val_if_fail(builtin && builtin->id == id, FALSE);
    *values = priv->values;
    return TRUE;
}

static gboolean
check_dependence(const GwyGrainValue *grainvalue,
                 const gdouble **values,
                 BuiltinGrainValueId id)
{
    *values = NULL;
    g_return_val_if_fail(grainvalue, FALSE);
    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_val_if_fail(builtin && builtin->id == id, FALSE);
    *values = priv->values;
    return TRUE;
}

static inline void
edge_list_add(EdgeList *list,
              gdouble xa, gdouble ya,
              gdouble xb, gdouble yb)
{
    if (G_UNLIKELY(list->len == list->size)) {
        list->size = MAX(2*list->size, 16);
        list->edges = g_renew(Edge, list->edges, list->size);
    }

    list->edges[list->len].xa = xa;
    list->edges[list->len].ya = ya;
    list->edges[list->len].xb = xb;
    list->edges[list->len].yb = yb;
    list->len++;
}

static guint*
grain_maybe_realloc(guint *grain, guint w, guint h, guint *grainsize)
{
    if (w*h > *grainsize) {
        g_free(grain);
        *grainsize = w*h;
        grain = g_new(guint, *grainsize);
    }
    return grain;
}

/**
 * find_grain_convex_hull:
 * @xres: The number of columns in @grains.
 * @yres: The number of rows in @grains.
 * @grains: Grain numbers filled with gwy_data_field_number_grains().
 * @pos: Position of the top-left vertex of grain's convex hull.
 * @vertices: Array to fill with vertices.
 *
 * Finds vertices of a grain's convex hull.
 *
 * The grain is identified by @pos which must lie in a grain.
 *
 * The positions are returned as indices to vertex grid.  NB: The size of the
 * grid is (@xres + 1)*(@yres + 1), not @xres*@yres.
 *
 * The method is a bit naive, some atan2() calculations could be easily saved.
 **/
static void
find_grain_convex_hull(gint xres, gint yres,
                       const guint *grains,
                       gint pos,
                       GridPointList *vertices)
{
    enum { RIGHT = 0, DOWN, LEFT, UP } newdir = RIGHT, dir;
    g_return_if_fail(grains[pos]);
    gint initpos = pos;
    guint gno = grains[pos];
    GridPoint v = { .i = pos/xres, .j = pos % xres };
    vertices->len = 0;
    grid_point_list_add(vertices, v.j, v.i);

    do {
        dir = newdir;
        switch (dir) {
            case RIGHT:
            v.j++;
            if (v.i > 0 && v.j < xres && grains[(v.i-1)*xres + v.j] == gno)
                newdir = UP;
            else if (v.j < xres && grains[v.i*xres + v.j] == gno)
                newdir = RIGHT;
            else
                newdir = DOWN;
            break;

            case DOWN:
            v.i++;
            if (v.j < xres && v.i < yres && grains[v.i*xres + v.j] == gno)
                newdir = RIGHT;
            else if (v.i < yres && grains[v.i*xres + v.j-1] == gno)
                newdir = DOWN;
            else
                newdir = LEFT;
            break;

            case LEFT:
            v.j--;
            if (v.i < yres && v.j > 0 && grains[v.i*xres + v.j-1] == gno)
                newdir = DOWN;
            else if (v.j > 0 && grains[(v.i-1)*xres + v.j-1] == gno)
                newdir = LEFT;
            else
                newdir = UP;
            break;

            case UP:
            v.i--;
            if (v.j > 0 && v.i > 0 && grains[(v.i-1)*xres + v.j-1] == gno)
                newdir = LEFT;
            else if (v.i > 0 && grains[(v.i-1)*xres + v.j] == gno)
                newdir = UP;
            else
                newdir = RIGHT;
            break;

            default:
            g_assert_not_reached();
            break;
        }

        /* When we turn right, the previous point is a potential vertex, and
         * it can also supersed previous vertices. */
        if (newdir == (dir + 1) % 4) {
            grid_point_list_add(vertices, v.j, v.i);
            guint len = vertices->len;
            while (len > 2) {
                GridPoint *cur = vertices->points + (len-1);
                GridPoint *mid = vertices->points + (len-2);
                GridPoint *prev = vertices->points + (len-3);
                gdouble phi = atan2(cur->i - mid->i, cur->j - mid->j);
                gdouble phim = atan2(mid->i - prev->i, mid->j - prev->j);
                phi = fmod(phi - phim + 4.0*G_PI, 2.0*G_PI);
                /* This should be fairly safe as (a) not real harm is done
                 * when we have an occasional extra vertex (b) the greatest
                 * possible angle is G_PI/2.0 */
                if (phi > 1e-12 && phi < G_PI)
                    break;

                // Get rid of mid, it is in a locally concave part.
                vertices->points[len-2] = *cur;
                vertices->len--;
                len = vertices->len;
            }
        }
    } while (v.i*xres + v.j != initpos);

    // The last point is duplicated first point.
    vertices->len--;
}

/**
 * grain_maximum_bound:
 * @vertices: Convex hull vertex list.
 * @qx: Scale (pixel size) in x-direction.
 * @qy: Scale (pixel size) in y-direction.
 * @vx: Location to store vector x component to.
 * @vy: Location to store vector y component to.
 *
 * Given a list of integer convex hull vertices, return the vector between
 * the two most distance vertices.
 **/
static void
grain_maximum_bound(const GridPointList *vertices,
                    gdouble qx, gdouble qy,
                    gdouble *vx, gdouble *vy)
{
    gdouble vm = -G_MAXDOUBLE;
    for (guint g1 = 0; g1 < vertices->len; g1++) {
        const GridPoint *a = vertices->points + g1;
        for (guint g2 = g1 + 1; g2 < vertices->len; g2++) {
            const GridPoint *x = vertices->points + g2;
            gdouble dx = qx*(x->j - a->j);
            gdouble dy = qy*(x->i - a->i);
            gdouble v = dx*dx + dy*dy;
            if (v > vm) {
                vm = v;
                *vx = dx;
                *vy = dy;
            }
        }
    }
}

/**
 * grain_minimum_bound:
 * @vertices: Convex hull vertex list.
 * @qx: Scale (pixel size) in x-direction.
 * @qy: Scale (pixel size) in y-direction.
 * @vx: Location to store vector x component to.
 * @vy: Location to store vector y component to.
 *
 * Given a list of integer convex hull vertices, return the vector
 * corresponding to the minimum linear projection.
 **/
static void
grain_minimum_bound(const GridPointList *vertices,
                    gdouble qx, gdouble qy,
                    gdouble *vx, gdouble *vy)
{
    g_return_if_fail(vertices->len >= 3);
    gdouble vm = G_MAXDOUBLE;
    for (guint g1 = 0; g1 < vertices->len; g1++) {
        const GridPoint *a = vertices->points + g1;
        guint g1p = (g1 + 1) % vertices->len;
        const GridPoint *b = vertices->points + g1p;
        gdouble bx = qx*(b->j - a->j);
        gdouble by = qy*(b->i - a->i);
        gdouble b2 = bx*bx + by*by;
        gdouble vm1 = -G_MAXDOUBLE, vx1 = -G_MAXDOUBLE, vy1 = -G_MAXDOUBLE;
        for (guint g2 = 0; g2 < vertices->len; g2++) {
            const GridPoint *x = vertices->points + g2;
            gdouble dx = qx*(x->j - a->j);
            gdouble dy = qy*(x->i - a->i);
            gdouble s = (dx*bx + dy*by)/b2;
            dx -= s*bx;
            dy -= s*by;
            gdouble v = dx*dx + dy*dy;
            if (v > vm1) {
                vm1 = v;
                vx1 = dx;
                vy1 = dy;
            }
        }
        if (vm1 < vm) {
            vm = vm1;
            *vx = vx1;
            *vy = vy1;
        }
    }
}

static gdouble
grain_convex_hull_area(const GridPointList *vertices, gdouble dx, gdouble dy)
{
    g_return_val_if_fail(vertices->len >= 4, 0.0);

    const GridPoint *a = vertices->points,
                    *b = vertices->points + 1,
                    *c = vertices->points + 2;
    gdouble s = 0.0;

    for (guint i = 2; i < vertices->len; i++) {
        gdouble bx = b->j - a->j, by = b->i - a->i,
                cx = c->j - a->j, cy = c->i - a->i;
        s += 0.5*(bx*cy - by*cx);
        b = c;
        c++;
    }

    return dx*dy*s;
}

// By centre we mean centre of mass of its area.
static void
grain_convex_hull_centre(const GridPointList *vertices,
                         gdouble dx, gdouble dy,
                         gdouble *centrex, gdouble *centrey)
{
    g_return_if_fail(vertices->len >= 4);

    const GridPoint *a = vertices->points,
                    *b = vertices->points + 1,
                    *c = vertices->points + 2;
    gdouble s = 0.0, xc = 0.0, yc = 0.0;

    for (guint i = 2; i < vertices->len; i++) {
        gdouble bx = b->j - a->j, by = b->i - a->i,
                cx = c->j - a->j, cy = c->i - a->i;
        gdouble s1 = bx*cy - by*cx;
        xc += s1*(a->j + b->j + c->j);
        yc += s1*(a->i + b->i + c->i);
        s += s1;
        b = c;
        c++;
    }
    *centrex = xc*dx/(3.0*s);
    *centrey = yc*dy/(3.0*s);
}

static gdouble
minimize_circle_radius(FooscribedDisc *circle,
                       const GridPointList *vertices,
                       gdouble dx, gdouble dy)
{
    const GridPoint *v = vertices->points;
    gdouble x = circle->x, y = circle->y, r2best = 0.0;
    guint n = vertices->len;

    while (n--) {
        gdouble deltax = dx*v->j - x, deltay = dy*v->i - y;
        gdouble r2 = deltax*deltax + deltay*deltay;

        if (r2 > r2best)
            r2best = r2;

        v++;
    }

    return r2best;
}

static void
improve_circumscribed_circle(FooscribedDisc *circle,
                             const GridPointList *vertices,
                             gdouble dx, gdouble dy)
{
    gdouble eps = 1.0, improvement, qgeom = sqrt(dx*dy);

    do {
        FooscribedDisc best = *circle;

        improvement = 0.0;
        for (guint i = 0; i < NDIRECTIONS; i++) {
            FooscribedDisc cand;
            gdouble sx = eps*qgeom*shift_directions[2*i],
                    sy = eps*qgeom*shift_directions[2*i + 1];

            cand.size = circle->size;

            cand.x = circle->x + sx;
            cand.y = circle->y + sy;
            if ((cand.R2 = minimize_circle_radius(&cand, vertices, dx, dy))
                < best.R2)
                best = cand;

            cand.x = circle->x - sy;
            cand.y = circle->y + sx;
            if ((cand.R2 = minimize_circle_radius(&cand, vertices, dx, dy))
                < best.R2)
                best = cand;

            cand.x = circle->x - sx;
            cand.y = circle->y - sy;
            if ((cand.R2 = minimize_circle_radius(&cand, vertices, dx, dy))
                < best.R2)
                best = cand;

            cand.x = circle->x + sy;
            cand.y = circle->y - sx;
            if ((cand.R2 = minimize_circle_radius(&cand, vertices, dx, dy))
                < best.R2)
                best = cand;
        }
        if (best.R2 < circle->R2) {
            improvement = (best.R2 - circle->R2)/(dx*dy);
            *circle = best;
        }
        else {
            eps *= 0.5;
        }
    } while (eps > 1e-3 || improvement > 1e-3);
}

void
_gwy_grain_value_builtin_convex_hull(GwyGrainValue *minsizegrainvalue,
                                     GwyGrainValue *minanglegrainvalue,
                                     GwyGrainValue *maxsizegrainvalue,
                                     GwyGrainValue *maxanglegrainvalue,
                                     GwyGrainValue *chullareagrainvalue,
                                     GwyGrainValue *excircrgrainvalue,
                                     GwyGrainValue *excircxgrainvalue,
                                     GwyGrainValue *excircygrainvalue,
                                     const guint *grains,
                                     const guint *anyboundpos,
                                     const GwyField *field)
{
    guint ngrains;
    gdouble *minsizevalues, *maxsizevalues, *minanglevalues, *maxanglevalues,
            *chullareavalues, *excircrvalues, *excircxvalues, *excircyvalues;
    if (all_null(8, &ngrains, minsizegrainvalue, maxsizegrainvalue,
                 minanglegrainvalue, maxanglegrainvalue, chullareagrainvalue,
                 excircrgrainvalue, excircxgrainvalue, excircygrainvalue)
        || !check_target(minsizegrainvalue, &minsizevalues,
                         GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE)
        || !check_target(maxsizegrainvalue, &maxsizevalues,
                         GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE)
        || !check_target(minanglegrainvalue, &minanglevalues,
                         GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE)
        || !check_target(maxanglegrainvalue, &maxanglevalues,
                         GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE)
        || !check_target(chullareagrainvalue, &chullareavalues,
                         GWY_GRAIN_VALUE_CONVEX_HULL_AREA)
        || !check_target(excircrgrainvalue, &excircrvalues,
                         GWY_GRAIN_VALUE_CIRCUMCIRCLE_R)
        || !check_target(excircxgrainvalue, &excircxvalues,
                         GWY_GRAIN_VALUE_CIRCUMCIRCLE_X)
        || !check_target(excircygrainvalue, &excircyvalues,
                         GWY_GRAIN_VALUE_CIRCUMCIRCLE_Y))
        return;

    guint xres = field->xres, yres = field->yres;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);

    // Find the complete convex hulls.
    GridPointList *vertices = grid_point_list_new(0);
    for (guint gno = 1; gno <= ngrains; gno++) {
        gdouble vx = dx, vy = dy;

        find_grain_convex_hull(xres, yres, grains, anyboundpos[gno], vertices);
        if (minsizevalues || minanglevalues) {
            grain_minimum_bound(vertices, dx, dy, &vx, &vy);
            if (minsizevalues)
                minsizevalues[gno] = hypot(vx, vy);
            if (minanglevalues)
                minanglevalues[gno] = gwy_standardize_direction(atan2(-vy, vx));
        }
        if (maxsizevalues || maxanglevalues) {
            grain_maximum_bound(vertices, dx, dy, &vx, &vy);
            if (maxsizevalues)
                maxsizevalues[gno] = hypot(vx, vy);
            if (maxanglevalues)
                maxanglevalues[gno] = gwy_standardize_direction(atan2(-vy, vx));
        }
        if (chullareavalues)
            chullareavalues[gno] = grain_convex_hull_area(vertices, dx, dy);
        if (excircrvalues || excircxvalues || excircyvalues) {
            FooscribedDisc circle = { 0.0, 0.0, 0.0, 0 };

            grain_convex_hull_centre(vertices, dx, dy, &circle.x, &circle.y);
            circle.R2 = minimize_circle_radius(&circle, vertices, dx, dy);
            improve_circumscribed_circle(&circle, vertices, dx, dy);

            if (excircrvalues)
                excircrvalues[gno] = sqrt(circle.R2);
            if (excircxvalues)
                excircxvalues[gno] = circle.x + field->xoff;
            if (excircyvalues)
                excircyvalues[gno] = circle.y + field->yoff;
        }
    }

    grid_point_list_free(vertices);
}

static guint*
extract_upsampled_square_pixel_grain(const guint *grains, guint xres,
                                     guint gno,
                                     const GwyFieldPart *bbox,
                                     guint *grain, guint *grainsize,
                                     guint *widthup, guint *heightup,
                                     gdouble dx, gdouble dy)
{
    guint col = bbox->col, row = bbox->row, w = bbox->width, h = bbox->height;
    guint w2 = 2*w, h2 = 2*h;

    /* Do not bother with nearly square pixels and upsample also 2×2. */
    if (fabs(log(dy/dx)) < 0.05
        || (dy < dx && gwy_round(dx/dy*w2) == w2)
        || (dy > dx && gwy_round(dy/dx*h2) == h2)) {
        grain = grain_maybe_realloc(grain, w2, h2, grainsize);
        for (guint i = 0; i < h; i++) {
            guint k2 = w2*(2*i);
            guint k = (i + row)*xres + col;
            for (guint j = 0; j < w; j++, k++, k2 += 2) {
                guint v = (grains[k] == gno) ? SEDINF : 0;
                grain[k2] = v;
                grain[k2+1] = v;
                grain[k2 + w2] = v;
                grain[k2 + w2+1] = v;
            }
        }
    }
    else if (dy < dx) {
        /* Horizontal upsampling, precalculate index map to use in each row. */
        w2 = gwy_round(dx/dy*w2);
        grain = grain_maybe_realloc(grain, w2, h2, grainsize);
        guint memsize = w2*sizeof(guint);
        guint *indices = (guint*)g_slice_alloc(memsize);
        for (guint j = 0; j < w2; j++) {
            gint jj = (gint)floor((j + 0.5)*w/w2);
            indices[j] = CLAMP(jj, 0, (gint)w-1);
        }
        for (guint i = 0; i < h; i++) {
            guint k = (i + row)*xres + col;
            guint k2 = w2*(2*i);
            for (guint j = 0; j < w2; j++) {
                guint v = (grains[k + indices[j]] == gno) ? SEDINF : 0;
                grain[k2 + j] = v;
                grain[k2 + w2 + j] = v;
            }
        }
        g_slice_free1(memsize, indices);
    }
    else {
        /* Vertical upsampling, rows are 2× scaled copies but uneven. */
        h2 = gwy_round(dy/dx*h2);
        grain = grain_maybe_realloc(grain, w2, h2, grainsize);
        for (guint i = 0; i < h2; i++) {
            guint k, k2 = i*w2;
            gint ii = (gint)floor((i + 0.5)*h/h2);
            ii = CLAMP(ii, 0, (gint)h-1);
            k = (ii + row)*xres + col;
            for (guint j = 0; j < w; j++) {
                guint v = (grains[k + j] == gno) ? SEDINF : 0;
                grain[k2 + 2*j] = v;
                grain[k2 + 2*j + 1] = v;
            }
        }
    }

    *widthup = w2;
    *heightup = h2;
    return grain;
}

static gint
compare_candidates(const FooscribedDisc *a,
                   const FooscribedDisc *b)
{
    if (a->size > b->size)
        return -1;
    if (a->size < b->size)
        return 1;

    if (a->R2 < b->R2)
        return -1;
    if (a->R2 > b->R2)
        return 1;

    return 0;
}

/* Rectangular grains are handled directly by the caller.  So the minimum
 * dimension this function can get is 4 (2 upscaled twice) and it does not need
 * to care about edge pixels, maxima cannot lie there. */
static gdouble
find_disc_centre_candidates(GArray *candidates, guint ncandmax,
                            const guint *grain,
                            guint width, guint height,
                            gdouble dx, gdouble dy,
                            gdouble centrex, gdouble centrey)
{
    g_return_val_if_fail(width >= 4 && height >= 4, 0.0);

    g_array_set_size(candidates, 0);
    guint bestsize = 0, worstgoodsize = 0, maxd2 = 0;
    for (guint i = 1; i < height-1; i++) {
        for (guint j = 1, k = i*width + 1; j < width-1; j++, k++) {
            guint size = grain[k];
            /* Boundary pixels cannot be candidates and might fall out of
             * original pixels due to rounding errors when dx ≠ dy */
            if (size <= 1)
                continue;

            size = 4*size + (grain[k - width-1] + 2*grain[k - width]
                             + grain[k - width+1]
                             + 2*grain[k-1] + 2*grain[k+1]
                             + grain[k + width-1] + 2*grain[k + width]
                             + grain[k + width+1]);
            if (candidates->len == ncandmax && size < worstgoodsize)
                continue;
            if (size < bestsize/2)
                continue;

            FooscribedDisc cand = {
                .x = (j + 0.5)*dx, .y = (i + 0.5)*dy, .size = size
            };
            /* Use R2 temporarily for distance from the entire grain centre;
             * this is only for sorting below. */
            cand.R2 = ((cand.x - centrex)*(cand.x - centrex)
                       + (cand.y - centrey)*(cand.y - centrey));

            guint insertpos = candidates->len;
            while (insertpos) {
                const FooscribedDisc *candi = &g_array_index(candidates,
                                                             FooscribedDisc,
                                                             insertpos-1);
                if (compare_candidates(&cand, candi) >= 0)
                    break;
                insertpos--;
            }

            g_array_insert_val(candidates, insertpos, cand);
            if (!insertpos) {
                maxd2 = grain[k];
                bestsize = size;
                for (guint m = 1; m < candidates->len; m++) {
                    const FooscribedDisc *candi = &g_array_index(candidates,
                                                                 FooscribedDisc,
                                                                 m);
                    if (candi->size < bestsize/2) {
                        g_array_set_size(candidates, m);
                        break;
                    }
                }
            }
            if (candidates->len > ncandmax)
                g_array_set_size(candidates, ncandmax);

            worstgoodsize = g_array_index(candidates, FooscribedDisc,
                                          candidates->len-1).size;
        }
    }

    g_assert(candidates->len);
    return sqrt(maxd2);
}

static void
find_all_edges(EdgeList *edges, GArray *verticesarray,
               const guint *grains, guint xres,
               guint gno, const GwyFieldPart *bbox,
               gdouble dx, gdouble dy)
{
    guint col = bbox->col, row = bbox->row, w = bbox->width, h = bbox->height;

    g_array_set_size(verticesarray, w+1);
    guint *vertices = (guint*)verticesarray->data;
    memset(vertices, 0xff, (w+1)*sizeof(guint));

    edges->len = 0;
    for (guint i = 0; i <= h; i++) {
        guint k = (i + row)*xres + col;
        guint vertex = G_MAXUINT;

        for (guint j = 0; j <= w; j++, k++) {
            /*
             * 1 2
             * 3 4
             */
            guint g0 = i && j && grains[k - xres - 1] == gno;
            guint g1 = i && j < w && grains[k - xres] == gno;
            guint g2 = i < h && j && grains[k - 1] == gno;
            guint g3 = i < h && j < w && grains[k] == gno;
            guint g = g0 | (g1 << 1) | (g2 << 2) | (g3 << 3);

            if (g == 8 || g == 7) {
                vertex = j;
                vertices[j] = i;
            }
            else if (g == 2 || g == 13) {
                edge_list_add(edges, dx*j, dy*vertices[j], dx*j, dy*i);
                vertex = j;
                vertices[j] = G_MAXUINT;
            }
            else if (g == 4 || g == 11) {
                edge_list_add(edges, dx*vertex, dy*i, dx*j, dy*i);
                vertex = G_MAXUINT;
                vertices[j] = i;
            }
            else if (g == 1 || g == 14) {
                edge_list_add(edges, dx*vertex, dy*i, dx*j, dy*i);
                edge_list_add(edges, dx*j, dy*vertices[j], dx*j, dy*i);
                vertex = G_MAXUINT;
                vertices[j] = G_MAXUINT;
            }
            else if (g == 6 || g == 9) {
                edge_list_add(edges, dx*vertex, dy*i, dx*j, dy*i);
                edge_list_add(edges, dx*j, dy*vertices[j], dx*j, dy*i);
                vertex = j;
                vertices[j] = i;
            }
        }
    }
}

static gdouble
maximize_disc_radius(FooscribedDisc *disc, Edge *edges, guint n)
{
    gdouble x = disc->x, y = disc->y, r2best = HUGE_VAL;

    while (n--) {
        gdouble rax = edges->xa - x, ray = edges->ya - y,
                rbx = edges->xb - x, rby = edges->yb - y,
                deltax = edges->xb - edges->xa, deltay = edges->yb - edges->ya;
        gdouble ca = -(deltax*rax + deltay*ray),
                cb = deltax*rbx + deltay*rby;

        if (ca <= 0.0)
            edges->r2 = rax*rax + ray*ray;
        else if (cb <= 0.0)
            edges->r2 = rbx*rbx + rby*rby;
        else {
            gdouble tx = cb*rax + ca*rbx, ty = cb*ray + ca*rby, D = ca + cb;
            edges->r2 = (tx*tx + ty*ty)/(D*D);
        }

        if (edges->r2 < r2best)
            r2best = edges->r2;
        edges++;
    }

    return r2best;
}

static guint
filter_relevant_edges(EdgeList *edges, gdouble r2, gdouble eps)
{
    Edge *edge = edges->edges, *enear = edges->edges;
    gdouble limitr = sqrt(r2) + 4.0*eps + 0.5, limit = limitr*limitr;

    for (guint i = edges->len; i; i--, edge++) {
        if (edge->r2 <= limit) {
            if (edge != enear)
                GWY_SWAP(Edge, *edge, *enear);
            enear++;
        }
    }

    return enear - edges->edges;
}

static void
improve_inscribed_disc(FooscribedDisc *disc, EdgeList *edges, double dist)
{
    gdouble eps = 0.5 + 0.25*(dist >= 4.0) + 0.25*(dist >= 16.0), improvement;
    guint nsuccessiveimprovements = 0;

    do {
        disc->R2 = maximize_disc_radius(disc, edges->edges, edges->len);
        eps = fmin(eps, 0.5*sqrt(disc->R2));
        FooscribedDisc best = *disc;
        guint nr = filter_relevant_edges(edges, best.R2, eps);

        improvement = 0.0;
        for (guint i = 0; i < NDIRECTIONS; i++) {
            FooscribedDisc cand = { .size = disc->size };
            gdouble sx = eps*shift_directions[2*i],
                    sy = eps*shift_directions[2*i + 1];

            cand.x = disc->x + sx;
            cand.y = disc->y + sy;
            if ((cand.R2 = maximize_disc_radius(&cand, edges->edges, nr))
                > best.R2)
                best = cand;

            cand.x = disc->x - sy;
            cand.y = disc->y + sx;
            if ((cand.R2 = maximize_disc_radius(&cand, edges->edges, nr))
                > best.R2)
                best = cand;

            cand.x = disc->x - sx;
            cand.y = disc->y - sy;
            if ((cand.R2 = maximize_disc_radius(&cand, edges->edges, nr))
                > best.R2)
                best = cand;

            cand.x = disc->x + sy;
            cand.y = disc->y - sx;
            if ((cand.R2 = maximize_disc_radius(&cand, edges->edges, nr))
                > best.R2)
                best = cand;
        }

        if (best.R2 > disc->R2) {
            improvement = sqrt(best.R2) - sqrt(disc->R2);
            *disc = best;
            // This scales up *each* successive improvement after 3 so eps can
            // grow very quickly.
            if (nsuccessiveimprovements++ > 2)
                eps *= 1.5;
        }
        else {
            eps *= 0.5;
            nsuccessiveimprovements = 0;
        }
    } while (eps > 1e-3 || improvement > 1e-3);
}

static gdouble
mean_euclidean_distance(const guint *sedt,
                        guint n,
                        gdouble dx, gdouble dy)
{
    guint i = n, np = 0;
    gdouble dmean = 0.0;
    while (i--) {
        if (*sedt) {
            dmean += sqrt(*sedt);
            np++;
        }
        sedt++;
    }
    return (dmean/np - 0.5) * 0.5*(dx + dy);
}

static void
inscribed_discs_and_friends(gdouble *inscrdrvalues,
                            gdouble *inscrdxvalues,
                            gdouble *inscrdyvalues,
                            gdouble *edmeanvalues,
                            const gdouble *xvalues,
                            const gdouble *yvalues,
                            const guint *grains,
                            const guint *sizes,
                            guint ngrains,
                            const GwyMaskField *mask,
                            gdouble dx, gdouble dy)
{
    enum { NCAND_MAX = 15 };

    g_return_if_fail(grains);
    g_return_if_fail(sizes);
    g_return_if_fail(xvalues);
    g_return_if_fail(yvalues);
    g_return_if_fail(GWY_IS_MASK_FIELD(mask));
    g_return_if_fail(inscrdrvalues || inscrdxvalues || inscrdyvalues
                     || edmeanvalues);

    const GwyFieldPart *bbox = gwy_mask_field_grain_bounding_boxes(mask);
    guint xres = mask->xres;
    gdouble qgeom = sqrt(dx*dy);
    gboolean nodiscs = !inscrdrvalues && !inscrdxvalues && !inscrdyvalues;

    guint *grain = NULL, *workspace = NULL;
    guint grainsize = 0;
    IntList *inqueue = int_list_new(0);
    IntList *outqueue = int_list_new(0);
    GArray *candidates = g_array_new(FALSE, FALSE, sizeof(FooscribedDisc));
    GArray *vertices = g_array_new(FALSE, FALSE, sizeof(guint));
    EdgeList edges = { 0, 0, NULL };

    /*
     * For each grain:
     *    Extract it, find all boundary pixels.
     *    Use (octagnoal) erosion to find disc centre candidate(s).
     *    For each candidate:
     *       Find maximum disc that fits with this centre.
     *       By expanding/moving try to find a larger disc until we cannot
     *       improve it.
     */
    for (guint gno = 1; gno <= ngrains; gno++) {
        guint w = bbox[gno].width, h = bbox[gno].height;
        gdouble xoff = dx*bbox[gno].col, yoff = dy*bbox[gno].row;

        /* If the grain is rectangular, calculate the properties directly.
         * Large rectangular grains are rare but the point is to catch
         * grains with width or height of 1 here. */
        if (sizes[gno] == w*h) {
            gdouble sdx = 0.5*w*dx, sdy = 0.5*h*dy;
            gdouble Lmax = fmax(sdx, sdy), Lmin = fmin(sdx, sdy);
            if (inscrdrvalues)
                inscrdrvalues[gno] = 0.999999*Lmin;
            if (inscrdxvalues)
                inscrdxvalues[gno] = sdx + xoff;
            if (inscrdyvalues)
                inscrdyvalues[gno] = sdy + yoff;
            if (edmeanvalues)
                edmeanvalues[gno] = Lmin/6.0*(3.0 - Lmin/Lmax);
            continue;
        }

        /* Upsampling twice combined with octagonal erosion has the nice
         * property that we get candidate pixels in places such as corners
         * or junctions of one-pixel thin lines. */
        guint width, height, wspsize = grainsize;
        grain = extract_upsampled_square_pixel_grain(grains, xres, gno,
                                                     bbox + gno,
                                                     grain, &grainsize,
                                                     &width, &height,
                                                     dx, dy);
        workspace = grain_maybe_realloc(workspace, width, height, &wspsize);
        g_assert(wspsize == grainsize);
        /* Size of upsampled pixel in original squeezed pixel coordinates.
         * Normally equal to 1/2 and always approximately 1:1. */
        gdouble sdx = w*(dx/qgeom)/width;
        gdouble sdy = h*(dy/qgeom)/height;
        /* Grain centre in squeezed pixel coordinates within the bbox. */
        gdouble centrex = (xvalues[gno] + 0.5)*(dx/qgeom);
        gdouble centrey = (yvalues[gno] + 0.5)*(dy/qgeom);

        _gwy_distance_transform_raw(grain, workspace, width, height, TRUE,
                                    inqueue, outqueue);

        if (edmeanvalues) {
            edmeanvalues[gno] = mean_euclidean_distance(grain, width*height,
                                                        w*dx/width,
                                                        h*dy/height);
        }
        if (nodiscs)
            continue;

#if 0
        g_printerr("Grain #%u, orig %ux%u, resampled to %ux%u\n",
                   gno, bbox[gno].width, bbox[gno].height, width, height);
        for (guint i = 0; i < height; i++) {
            for (guint j = 0; j < width; j++) {
                if (!grain[i*width + j])
                    g_printerr("..");
                else
                    g_printerr("%02u", grain[i*width + j]);
                g_printerr("%c", j == width-1 ? '\n' : ' ');
            }
        }
#endif
        double dist = find_disc_centre_candidates(candidates, NCAND_MAX,
                                                  grain, width, height,
                                                  sdx, sdy,
                                                  centrex, centrey);
        find_all_edges(&edges, vertices, grains, xres, gno, bbox + gno,
                       dx/qgeom, dy/qgeom);

        /* Try a few first candidates for the inscribed disc centre. */
        FooscribedDisc *cand;
        for (guint i = 0; i < candidates->len; i++) {
            cand = &g_array_index(candidates, FooscribedDisc, i);
            improve_inscribed_disc(cand, &edges, dist);
        }

        cand = &g_array_index(candidates, FooscribedDisc, 0);
        for (guint i = 1; i < candidates->len; i++) {
            if (g_array_index(candidates, FooscribedDisc, i).R2 > cand->R2)
                cand = &g_array_index(candidates, FooscribedDisc, i);
        }

        if (inscrdrvalues)
            inscrdrvalues[gno] = sqrt(cand->R2)*qgeom;
        if (inscrdxvalues)
            inscrdxvalues[gno] = cand->x*qgeom + xoff;
        if (inscrdyvalues)
            inscrdyvalues[gno] = cand->y*qgeom + yoff;

        //g_printerr("[%u] %g %g %g\n", gno, sqrt(cand->R2)*qgeom, cand->x*qgeom + xoff, cand->y*qgeom + yoff);
    }

    g_free(workspace);
    g_free(grain);
    int_list_free(inqueue);
    int_list_free(outqueue);
    g_free(edges.edges);
    g_array_free(vertices, TRUE);
    g_array_free(candidates, TRUE);
}

/* This protected function is for in-library users that do not require all the
 * fancy quantities.  Implementation is in inscribed_discs_and_friends() which
 * possibly calculates mean edge distances, shape numbers and other stuff. */
void
_gwy_mask_field_grain_inscribed_discs(gdouble *inscrdrvalues,
                                      gdouble *inscrdxvalues,
                                      gdouble *inscrdyvalues,
                                      const gdouble *xvalues,
                                      const gdouble *yvalues,
                                      const guint *grains,
                                      const guint *sizes,
                                      guint ngrains,
                                      const GwyMaskField *mask,
                                      gdouble dx, gdouble dy)
{
    inscribed_discs_and_friends(inscrdrvalues, inscrdxvalues, inscrdyvalues,
                                NULL,
                                xvalues, yvalues,
                                grains, sizes, ngrains, mask,
                                dx, dy);
}

void
_gwy_grain_value_builtin_inscribed_disc(GwyGrainValue *inscrdrgrainvalue,
                                        GwyGrainValue *inscrdxgrainvalue,
                                        GwyGrainValue *inscrdygrainvalue,
                                        GwyGrainValue *edmeangrainvalue,
                                        const GwyGrainValue *xgrainvalue,
                                        const GwyGrainValue *ygrainvalue,
                                        const guint *grains,
                                        const guint *sizes,
                                        const GwyMaskField *mask,
                                        const GwyField *field)
{
    guint ngrains;
    const gdouble *xvalues, *yvalues;
    gdouble *inscrdrvalues, *inscrdxvalues, *inscrdyvalues, *edmeanvalues;
    if (all_null(4, &ngrains,
                 inscrdrgrainvalue, inscrdxgrainvalue, inscrdygrainvalue,
                 edmeangrainvalue)
        || !check_target(inscrdrgrainvalue, &inscrdrvalues,
                         GWY_GRAIN_VALUE_INSCRIBED_DISC_R)
        || !check_target(inscrdxgrainvalue, &inscrdxvalues,
                         GWY_GRAIN_VALUE_INSCRIBED_DISC_X)
        || !check_target(inscrdygrainvalue, &inscrdyvalues,
                         GWY_GRAIN_VALUE_INSCRIBED_DISC_Y)
        || !check_target(edmeangrainvalue, &edmeanvalues,
                         GWY_GRAIN_VALUE_MEAN_EDGE_DISTANCE)
        || !check_dependence(xgrainvalue, &xvalues, GWY_GRAIN_VALUE_CENTER_X)
        || !check_dependence(ygrainvalue, &yvalues, GWY_GRAIN_VALUE_CENTER_Y))
        return;

    inscribed_discs_and_friends(inscrdrvalues, inscrdxvalues, inscrdyvalues,
                                edmeanvalues,
                                xvalues, yvalues,
                                grains, sizes, ngrains, mask,
                                gwy_field_dx(field), gwy_field_dy(field));

    if (inscrdxvalues) {
        gdouble off = field->xoff;
        for (guint i = 1; i <= ngrains; i++)
            inscrdxvalues[i] += off;
    }
    if (inscrdyvalues) {
        gdouble off = field->yoff;
        for (guint i = 1; i <= ngrains; i++)
            inscrdyvalues[i] += off;
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
