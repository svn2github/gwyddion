/*
 *  $Id$
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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
#include "libgwy/field-statistics.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/field-render.h"

#define NOT_QUITE_1 0.999999999

#define COMPONENT_TO_PIXEL8(x) (guint)(256*CLAMP((x), 0.0, NOT_QUITE_1))

typedef struct {
    guint prev, next;
    gdouble wprev, wnext;
} InterpolationPoint;

static void autorange(const GwyField *field,
                      gdouble min,
                      gdouble max,
                      gdouble *from,
                      gdouble *to);

static inline void
gwy_rgba_to_rgb8_pixel(const GwyRGBA *rgba,
                       guchar *pixel)
{
    *(pixel++) = COMPONENT_TO_PIXEL8(rgba->r);
    *(pixel++) = COMPONENT_TO_PIXEL8(rgba->g);
    *pixel = COMPONENT_TO_PIXEL8(rgba->b);
}

static inline void
gwy_rgba_to_rgba8_pixel(const GwyRGBA *rgba,
                        guchar *pixel)
{
    *(pixel++) = COMPONENT_TO_PIXEL8(rgba->r);
    *(pixel++) = COMPONENT_TO_PIXEL8(rgba->g);
    *(pixel++) = COMPONENT_TO_PIXEL8(rgba->b);
    *pixel = COMPONENT_TO_PIXEL8(rgba->a);
}

static inline guint32
gwy_rgba_to_rgb8_color(const GwyRGBA *rgba)
{
    guint red = COMPONENT_TO_PIXEL8(rgba->r),
          green = COMPONENT_TO_PIXEL8(rgba->g),
          blue = COMPONENT_TO_PIXEL8(rgba->b);
    return 0xffu | (blue << 8) | (green << 16) | (red << 24);
}

static inline guint32
gwy_rgba_to_rgba8_color(const GwyRGBA *rgba)
{
    guint red = COMPONENT_TO_PIXEL8(rgba->r),
          green = COMPONENT_TO_PIXEL8(rgba->g),
          blue = COMPONENT_TO_PIXEL8(rgba->b),
          alpha = COMPONENT_TO_PIXEL8(rgba->a);
    return alpha | (blue << 8) | (green << 16) | (red << 24);
}

static inline guint32
gwy_rgba_to_rgb8_cairo(const GwyRGBA *rgba)
{
    guint red = COMPONENT_TO_PIXEL8(rgba->r),
          green = COMPONENT_TO_PIXEL8(rgba->g),
          blue = COMPONENT_TO_PIXEL8(rgba->b);
    return (red << 16) | (green << 8) | blue;
}

static inline guint32
gwy_rgba_to_rgba8_cairo(const GwyRGBA *rgba)
{
    guint red = COMPONENT_TO_PIXEL8(rgba->r),
          green = COMPONENT_TO_PIXEL8(rgba->g),
          blue = COMPONENT_TO_PIXEL8(rgba->b),
          alpha = COMPONENT_TO_PIXEL8(rgba->a);
    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

static inline void
interpolate_color(const GwyGradientPoint *pts,
                  guint len,
                  gdouble x,
                  GwyRGBA *color)
{
    const GwyGradientPoint *pt = pts;
    guint i;

    /* find the right subinterval */
    for (i = 0; i < len; i++) {
        pt = pts + i;
        if (pt->x == x) {
            *color = pt->color;
            return;
        }
        if (pt->x > x)
            break;
    }
    const GwyGradientPoint *pt2 = pts + i - 1;

    gdouble t = (x - pt2->x)/(pt->x - pt2->x), mt = 1.0 - t;
    color->r = t*pt->color.r + mt*pt2->color.r;
    color->g = t*pt->color.g + mt*pt2->color.g;
    color->b = t*pt->color.b + mt*pt2->color.b;
    // Alpha is ignored.  We would have to use gwy_rgba_interpolate().
}

static InterpolationPoint*
build_interpolation(guint n, gdouble from, gdouble to, guint res)
{
    InterpolationPoint *intpoints = g_new(InterpolationPoint, n);
    gdouble q = (to - from)/n;
    gdouble r = 0.5*NOT_QUITE_1*MIN(q, 1.0);
    gdouble limit = NOT_QUITE_1*res;

    for (guint i = 0; i < n; i++) {
        InterpolationPoint *ip = intpoints + i;
        gdouble centre = q*(i + 0.5) + from;
        gdouble left = centre - r, right = centre + r;

        left = CLAMP(left, 0.0, limit);
        right = CLAMP(right, 0.0, limit);

        ip->prev = (guint)left;
        ip->next = (guint)right;
        g_assert(ip->prev < res);
        g_assert(ip->next < res);

        if (ip->next == ip->prev)
            ip->wprev = ip->wnext = 0.5;
        else {
            g_assert(ip->prev + 1 == ip->next);
            ip->wnext = left - ip->prev;
            ip->wprev = 1.0 - ip->wnext;
            g_assert(ip->wprev >= 0.0);
            g_assert(ip->wnext >= 0.0);
        }
    }

    return intpoints;
}

static void
field_render_empty_range_pixbuf(GdkPixbuf *pixbuf,
                                const GwyGradient *gradient)
{
    GwyRGBA rgba;
    gwy_gradient_color(gradient, 0.5, &rgba);
    guint32 color = gwy_rgba_to_rgb8_color(&rgba);
    gdk_pixbuf_fill(pixbuf, color);
}

static void
field_render_empty_range_surface(cairo_surface_t *surface,
                                 const GwyGradient *gradient)
{
    GwyRGBA rgba;
    gwy_gradient_color(gradient, 0.5, &rgba);
    cairo_t *cr = cairo_create(surface);
    gwy_cairo_set_source_rgba(cr, &rgba);
    cairo_paint(cr);
    cairo_destroy(cr);
}

static gboolean
check_field_rectangle(const cairo_rectangle_t *rectangle,
                      guint xres, guint yres,
                      gdouble *xfrom, gdouble *yfrom,
                      gdouble *xto, gdouble *yto)
{
    if (rectangle) {
        g_return_val_if_fail(rectangle->x >= 0.0 && rectangle->x < xres, FALSE);
        g_return_val_if_fail(rectangle->width > 0.0, FALSE);
        g_return_val_if_fail(rectangle->x + rectangle->width <= xres, FALSE);
        g_return_val_if_fail(rectangle->y >= 0.0 && rectangle->y < yres, FALSE);
        g_return_val_if_fail(rectangle->height > 0.0, FALSE);
        g_return_val_if_fail(rectangle->y + rectangle->height <= yres, FALSE);
        *xfrom = rectangle->x;
        *xto = rectangle->x + rectangle->width;
        *yfrom = rectangle->y;
        *yto = rectangle->y + rectangle->height;
    }
    else {
        *xfrom = *yfrom = 0.0;
        *xto = xres;
        *yto = yres;
    }
    return TRUE;
}

/**
 * gwy_field_render_pixbuf:
 * @field: A two-dimensional data field.
 * @pixbuf: A pixbuf in %GDK_COLORSPACE_RGB colorspace without alpha channel.
 * @gradient: A false colour gradient.
 * @rectangle: (allow-none):
 *             Area in @field to render, %NULL for entire field.
 * @range: (allow-none):
 *         False colour mapping range corresponding to full @gradient range.
 *         As a shorthand, you can pass %NULL to map the full data range.
 *
 * Renders a field to pixbuf using false colour gradient.
 *
 * Parameters defining the area to render are measured in pixels; the entire
 * area must line within @field.
 *
 * The false colour mapping range can be arbitrary with respect to the data
 * range.  Passing @range.to smaller than @range.from just renders the gradient
 * inversely.
 **/
void
gwy_field_render_pixbuf(const GwyField *field,
                        GdkPixbuf *pixbuf,
                        GwyGradient *gradient,
                        const cairo_rectangle_t *rectangle,
                        const GwyRange *range)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(GDK_IS_PIXBUF(pixbuf));
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gdk_pixbuf_get_colorspace(pixbuf) == GDK_COLORSPACE_RGB);
    g_return_if_fail(gdk_pixbuf_get_n_channels(pixbuf) == 3);

    gdouble xfrom, xto, yfrom, yto;
    if (!check_field_rectangle(rectangle, field->xres, field->yres,
                               &xfrom, &yfrom, &xto, &yto))
        return;

    gdouble from, to;
    if (range) {
        from = range->from;
        to = range->to;
    }
    else
        gwy_field_min_max_full(field, &from, &to);

    if (from == to || isnan(from) || isnan(to)) {
        field_render_empty_range_pixbuf(pixbuf, gradient);
        return;
    }

    guint width = gdk_pixbuf_get_width(pixbuf);
    guint height = gdk_pixbuf_get_height(pixbuf);
    guint rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    guint xres = field->xres, yres = field->yres;
    const gdouble *data = field->data;

    gdouble qc = 1.0/(to - from);
    guint gradlen;
    const GwyGradientPoint *gradpts = gwy_gradient_get_data(gradient, &gradlen);

    // TODO: If width = xto - xfrom and height = yto - yfrom do not interpolate.
    InterpolationPoint *interpx = build_interpolation(width, xfrom, xto, xres),
                       *interpy = build_interpolation(height, yfrom, yto, yres);

    InterpolationPoint *ipy = interpy;
    for (guint i = 0; i < height; i++, ipy++) {
        guchar *pixrow = pixels + i*rowstride;
        const gdouble *rowprev = data + xres*ipy->prev,
                      *rownext = data + xres*ipy->next;
        gdouble wyp = ipy->wprev, wyn = ipy->wnext;
        InterpolationPoint *ipx = interpx;

        for (guint j = 0; j < width; j++, ipx++) {
            guint iprev = ipx->prev, inext = ipx->next;
            gdouble wxp = ipx->wprev, wxn = ipx->wnext;
            gdouble z = ((rowprev[iprev]*wxp + rowprev[inext]*wxn)*wyp
                         + (rownext[iprev]*wxp + rownext[inext]*wxn)*wyn);

            z = qc*(z - from);
            // TODO: Support pixmaps with alpha
            GwyRGBA rgba;
            interpolate_color(gradpts, gradlen, CLAMP(z, 0.0, 1.0), &rgba);
            gwy_rgba_to_rgb8_pixel(&rgba, pixrow);
            pixrow += 3;
        }
    }

    g_free(interpy);
    g_free(interpx);
}

/**
 * gwy_field_render_cairo:
 * @field: A two-dimensional data field.
 * @surface: A Cairo image surface of format %CAIRO_FORMAT_RGB24.
 * @gradient: A false colour gradient.
 * @rectangle: (allow-none):
 *             Area in @field to render, %NULL for entire field.
 * @range: (allow-none):
 *         False colour mapping range corresponding to full @gradient range.
 *         As a shorthand, you can pass %NULL to map the full data range.
 *
 * Renders a field to Cairo image surface using false colour gradient.
 *
 * Parameters defining the area to render are measured in pixels; the entire
 * area must line within @field.
 *
 * The false colour mapping range can be arbitrary with respect to the data
 * range.  Passing @range.to smaller than @range.from just renders the gradient
 * inversely.
 **/
void
gwy_field_render_cairo(const GwyField *field,
                       cairo_surface_t *surface,
                       GwyGradient *gradient,
                       const cairo_rectangle_t *rectangle,
                       const GwyRange *range)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(surface);
    g_return_if_fail(cairo_surface_get_type(surface)
                     == CAIRO_SURFACE_TYPE_IMAGE);
    g_return_if_fail(cairo_image_surface_get_format(surface)
                     == CAIRO_FORMAT_RGB24);
    g_return_if_fail(GWY_IS_GRADIENT(gradient));

    gdouble xfrom, xto, yfrom, yto;
    if (!check_field_rectangle(rectangle, field->xres, field->yres,
                               &xfrom, &yfrom, &xto, &yto))
        return;

    gdouble from, to;
    if (range) {
        from = range->from;
        to = range->to;
    }
    else
        gwy_field_min_max_full(field, &from, &to);

    if (from == to || isnan(from) || isnan(to)) {
        field_render_empty_range_surface(surface, gradient);
        return;
    }

    guint width = cairo_image_surface_get_width(surface);
    guint height = cairo_image_surface_get_height(surface);
    guint stride = cairo_image_surface_get_stride(surface);
    g_assert(stride % 4 == 0);
    guint32 *pixels = (guint32*)cairo_image_surface_get_data(surface);

    guint xres = field->xres, yres = field->yres;
    const gdouble *data = field->data;

    gdouble qc = 1.0/(to - from);
    guint gradlen;
    const GwyGradientPoint *gradpts = gwy_gradient_get_data(gradient, &gradlen);

    // TODO: If width = xto - xfrom and height = yto - yfrom do not interpolate.
    InterpolationPoint *interpx = build_interpolation(width, xfrom, xto, xres),
                       *interpy = build_interpolation(height, yfrom, yto, yres);

    InterpolationPoint *ipy = interpy;
    for (guint i = 0; i < height; i++, ipy++) {
        guint32 *pixrow = pixels + i*(stride/4);
        const gdouble *rowprev = data + xres*ipy->prev,
                      *rownext = data + xres*ipy->next;
        gdouble wyp = ipy->wprev, wyn = ipy->wnext;
        InterpolationPoint *ipx = interpx;

        for (guint j = 0; j < width; j++, ipx++, pixrow++) {
            guint iprev = ipx->prev, inext = ipx->next;
            gdouble wxp = ipx->wprev, wxn = ipx->wnext;
            gdouble z = ((rowprev[iprev]*wxp + rowprev[inext]*wxn)*wyp
                         + (rownext[iprev]*wxp + rownext[inext]*wxn)*wyn);

            z = qc*(z - from);
            // TODO: Support pixmaps with alpha
            GwyRGBA rgba;
            interpolate_color(gradpts, gradlen, CLAMP(z, 0.0, 1.0), &rgba);
            *pixrow = gwy_rgba_to_rgb8_cairo(&rgba);
        }
    }

    g_free(interpy);
    g_free(interpx);
}

// FIXME: This duplicates mask-field.c.
static void
scale_source_row(GwyMaskIter srciter, GwyMaskScalingSegment *seg,
                 gdouble *target, guint res, gdouble step, gdouble weight)
{
    if (step > 1.0) {
        // seg->move is always nonzero.
        for (guint i = res; i; i--, seg++) {
            gdouble s = seg->w0 * !!gwy_mask_iter_get(srciter);
            guint c = 0;
            for (guint k = seg->move-1; k; k--) {
                gwy_mask_iter_next(srciter);
                c += !!gwy_mask_iter_get(srciter);
            }
            gwy_mask_iter_next(srciter);
            s += c/step + seg->w1 * !!gwy_mask_iter_get(srciter);
            *(target++) += weight*s;
        }
    }
    else {
        // seg->move is at most 1.
        for (guint i = res; i; i--, seg++) {
            gdouble s = seg->w0 * !!gwy_mask_iter_get(srciter);
            if (seg->move) {
                gwy_mask_iter_next(srciter);
                s += seg->w1 * !!gwy_mask_iter_get(srciter);
            }
            *(target++) += weight*s;
        }
    }
}

/**
 * gwy_mask_field_render_cairo:
 * @field: A two-dimensional data field.
 * @surface: A Cairo image surface of format %CAIRO_FORMAT_A8.
 * @rectangle: (allow-none):
 *             Area in @field to render, %NULL for entire field.
 *
 * Renders a mask field to Cairo image surface.
 *
 * Parameters defining the area to render are measured in pixels; the entire
 * area must line within @field.
 **/
void
gwy_mask_field_render_cairo(const GwyMaskField *field,
                            cairo_surface_t *surface,
                            const cairo_rectangle_t *rectangle)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    g_return_if_fail(surface);
    g_return_if_fail(cairo_surface_get_type(surface)
                     == CAIRO_SURFACE_TYPE_IMAGE);
    g_return_if_fail(cairo_image_surface_get_format(surface)
                     == CAIRO_FORMAT_A8);

    gdouble xfrom, xto, yfrom, yto;
    if (!check_field_rectangle(rectangle, field->xres, field->yres,
                               &xfrom, &yfrom, &xto, &yto))
        return;

    guint width = cairo_image_surface_get_width(surface);
    guint height = cairo_image_surface_get_height(surface);
    guint stride = cairo_image_surface_get_stride(surface);
    guint8 *pixels = (guint8*)cairo_image_surface_get_data(surface);

    guint xreq_bits, yreq_bits;
    gdouble xstep = (xto - xfrom)/width, ystep = (yto - yfrom)/height;
    GwyMaskScalingSegment *xsegments = gwy_mask_prepare_scaling(0.0, xstep,
                                                                width,
                                                                &xreq_bits),
                          *ysegments = gwy_mask_prepare_scaling(0.0, ystep,
                                                                height,
                                                                &yreq_bits);
    gdouble *row = g_new(gdouble, width);
    guint jfrom = floor(xfrom), ifrom = floor(yfrom);
    g_assert(jfrom + xreq_bits <= field->xres);
    g_assert(ifrom + yreq_bits <= field->yres);

    GwyMaskScalingSegment *yseg = ysegments;
    for (guint i = 0, isrc = 0; i < height; i++, yseg++) {
        guint8 *pixrow = pixels + i*stride;
        GwyMaskIter iter;
        gwy_clear(row, width);
        gwy_mask_field_iter_init(field, iter, jfrom, ifrom + isrc);
        scale_source_row(iter, xsegments, row, width, xstep, yseg->w0);
        if (yseg->move) {
            for (guint k = yseg->move-1; k; k--) {
                isrc++;
                gwy_mask_field_iter_init(field, iter, jfrom, ifrom + isrc);
                scale_source_row(iter, xsegments, row, width, xstep, 1.0/ystep);
            }
            isrc++;
            gwy_mask_field_iter_init(field, iter, jfrom, ifrom + isrc);
            scale_source_row(iter, xsegments, row, width, xstep, yseg->w1);
        }
        for (guint j = 0; j < width; j++)
            pixrow[j] = COMPONENT_TO_PIXEL8(row[j]);
    }

    g_free(row);
    g_free(ysegments);
    g_free(xsegments);
}

/**
 * gwy_field_find_color_range:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Part of @field that is visible.  It is used only if any requested
 *         range is %GWY_COLOR_RANGE_VISIBLE.
 * @mask: (allow-none):
 *        Mask representing the value values to take into account/exclude.
 *        It is used only if any range is %GWY_COLOR_RANGE_MASKED or
 *        %GWY_COLOR_RANGE_UNMASKED.
 * @from_method: Method to use for the start of the false colour map.
 * @to_method: Method to use for the end of the false colour map.
 * @range: (out):
 *         Location where the range is to be stored.
 *
 * Finds appropriate start and end value for false colour mapping.
 *
 * The stard and end values can determined using different methods though you
 * should rarely need this.  If a method is %GWY_COLOR_RANGE_USER the
 * corresponding field in @range is left untouched.  So combining this function
 * with user-set ranges is easiest by initialising the range to the user range
 * beforehand:
 * |[
 * GwyRange range = user_set_range;
 * gwy_field_find_color_range(field, fpart, mask, from_method, to_method,
 *                            &range);
 * ]|
 **/
void
gwy_field_find_color_range(const GwyField *field,
                           const GwyFieldPart *fpart,
                           const GwyMaskField *mask,
                           GwyColorRangeType from,
                           GwyColorRangeType to,
                           GwyRange *range)
{
    g_return_if_fail(range);

    GwyMaskingType masking = GWY_MASK_IGNORE;
    // This is not a real setting because @from and @to can require different
    // masking.  Just request something if masking should be checked.
    if (from == GWY_COLOR_RANGE_MASKED || to == GWY_COLOR_RANGE_MASKED)
        masking = GWY_MASK_INCLUDE;
    else if (from == GWY_COLOR_RANGE_UNMASKED || to == GWY_COLOR_RANGE_UNMASKED)
        masking = GWY_MASK_EXCLUDE;

    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height,
                              &maskcol, &maskrow)) {
        range->from = range->to = 0.0;
        return;
    }

    if (masking == GWY_MASK_IGNORE) {
        if (from == GWY_COLOR_RANGE_MASKED || from == GWY_COLOR_RANGE_UNMASKED)
            from = GWY_COLOR_RANGE_FULL;
        if (to == GWY_COLOR_RANGE_MASKED || to == GWY_COLOR_RANGE_UNMASKED)
            to = GWY_COLOR_RANGE_FULL;
    }
    if (!fpart) {
        if (from == GWY_COLOR_RANGE_VISIBLE)
            from = GWY_COLOR_RANGE_FULL;
        if (to == GWY_COLOR_RANGE_VISIBLE)
            to = GWY_COLOR_RANGE_FULL;
    }

    gdouble fullmin, fullmax;
    gwy_field_min_max_full(field, &fullmin, &fullmax);

    if (from == GWY_COLOR_RANGE_VISIBLE || to == GWY_COLOR_RANGE_VISIBLE) {
        gdouble min, max;
        gwy_field_min_max(field, &(GwyFieldPart){ col, row, width, height },
                          NULL, GWY_MASK_IGNORE, &min, &max);
        if (from == GWY_COLOR_RANGE_VISIBLE)
            range->from = min;
        if (to == GWY_COLOR_RANGE_VISIBLE)
            range->to = max;
    }

    if (from == GWY_COLOR_RANGE_MASKED || to == GWY_COLOR_RANGE_MASKED) {
        gdouble min, max;
        gwy_field_min_max(field, NULL, mask, GWY_MASK_INCLUDE, &min, &max);
        if (max < min) {
            min = fullmin;
            max = fullmax;
        }
        if (from == GWY_COLOR_RANGE_MASKED)
            range->from = min;
        if (to == GWY_COLOR_RANGE_MASKED)
            range->to = max;
    }

    if (from == GWY_COLOR_RANGE_UNMASKED || to == GWY_COLOR_RANGE_UNMASKED) {
        gdouble min, max;
        gwy_field_min_max(field, NULL, mask, GWY_MASK_EXCLUDE, &min, &max);
        if (max < min) {
            min = fullmin;
            max = fullmax;
        }
        if (from == GWY_COLOR_RANGE_UNMASKED)
            range->from = min;
        if (to == GWY_COLOR_RANGE_UNMASKED)
            range->to = max;
    }

    if (from == GWY_COLOR_RANGE_AUTO || to == GWY_COLOR_RANGE_AUTO) {
        gdouble min, max;
        autorange(field, fullmin, fullmax, &min, &max);
        if (from == GWY_COLOR_RANGE_AUTO)
            range->from = min;
        if (to == GWY_COLOR_RANGE_AUTO)
            range->to = max;
    }

    if (from == GWY_COLOR_RANGE_FULL || to == GWY_COLOR_RANGE_FULL) {
        if (from == GWY_COLOR_RANGE_FULL)
            range->from = fullmin;
        if (to == GWY_COLOR_RANGE_FULL)
            range->to = fullmax;
    }
}

// FIXME: This is Gwyddion2 algorithm and leaves something to be desired.
// Also, make it public once it is improved.
static void
autorange(const GwyField *field,
          gdouble min, gdouble max,
          gdouble *from, gdouble *to)
{
    if (min == max) {
        GWY_MAYBE_SET(from, min);
        GWY_MAYBE_SET(to, max);
        return;
    }

    max += 1e-6*(max - min);

    enum { AR_NDH = 512 };
    guint dh[AR_NDH];
    gdouble q = AR_NDH/(max - min);
    const gdouble *p;
    guint i, j;

    guint n = field->xres*field->yres;
    gwy_clear(dh, AR_NDH);
    for (i = n, p = field->data; i; i--, p++) {
        j = (*p - min)*q;
        dh[MIN(j, AR_NDH-1)]++;
    }

    for (i = 0, j = 0; dh[i] < 5e-2*n/AR_NDH && j < 2e-2*n; i++)
        j += dh[i];
    GWY_MAYBE_SET(from, min + i/q);

    for (i = AR_NDH-1, j = 0; dh[i] < 5e-2*n/AR_NDH && j < 2e-2*n; i--)
        j += dh[i];

    GWY_MAYBE_SET(to, min + (i + 1)/q);
}

/**
 * SECTION: field-render
 * @section_id: GwyField-render
 * @title: GwyField rendering
 * @short_description: Rendering of fields to raster images
 *
 * Data fields are usually displayed using false colour mapping in
 * #GwyRasterArea widgets, often embedded in #GwyRasterView.  Occassionally, a
 * straightoward rendering of a field directly to a Cairo surface or a pixbuf
 * can be also useful.  The functions described here are the same that are used
 * by #GwyRasterArea internally.
 **/

/**
 * GwyColorRangeType:
 * @GWY_COLOR_RANGE_FULL: Total minimum or maximum value within the entire
 *                        data.
 * @GWY_COLOR_RANGE_MASKED: Minimum or maximum value within the data
 *                          under mask.
 *                          It reduces to %GWY_COLOR_RANGE_FULL if no mask is
 *                          present.
 * @GWY_COLOR_RANGE_UNMASKED: Minimum or maximum within the data
 *                            outside the mask.
 *                            It reduces to %GWY_COLOR_RANGE_FULL if no mask is
 *                            present.
 * @GWY_COLOR_RANGE_VISIBLE: Minimum or maximum value within the visible
 *                           part.
 *                           It reduces to %GWY_COLOR_RANGE_FULL if no part is
 *                           specified.
 * @GWY_COLOR_RANGE_AUTO: Minimum or maximum of entire data with outliers
 *                        removed.
 * @GWY_COLOR_RANGE_USER: User-set value.  Obviously, it can be used by
 *                        visualisation widgets but cannot be calculated
 *                        automatically.
 *
 * Type of color range boundary determination method.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
