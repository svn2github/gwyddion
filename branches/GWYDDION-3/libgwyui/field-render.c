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

#include "libgwyui/field-render.h"

#define NOT_QUITE_1 0.999999999999999

#define COMPONENT_TO_PIXEL8(x) (guint)(256*CLAMP((x), 0.0, NOT_QUITE_1))

typedef struct {
    guint prev, next;
    gdouble wprev, wnext;
} InterpolationPoint;

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
    return (blue << 16) | (green << 8) | red;
}

static inline guint32
gwy_rgba_to_rgba8_cairo(const GwyRGBA *rgba)
{
    guint red = COMPONENT_TO_PIXEL8(rgba->r),
          green = COMPONENT_TO_PIXEL8(rgba->g),
          blue = COMPONENT_TO_PIXEL8(rgba->b),
          alpha = COMPONENT_TO_PIXEL8(rgba->a);
    return (alpha << 24) | (blue << 16) | (green << 8) | red;
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
    gdouble r = 0.5*MIN(q, NOT_QUITE_1);
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
field_render_empty_range(GdkPixbuf *pixbuf,
                         GwyGradient *gradient)
{
    GwyRGBA rgba;
    gwy_gradient_color(gradient, 0.5, &rgba);
    guint32 color = gwy_rgba_to_rgb8_color(&rgba);
    gdk_pixbuf_fill(pixbuf, color);
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
 * @pixbuf: A pixbuf.
 * @gradient: A false colour gradient.
 * @rectangle: (allow-none):
 *             Area in @field to render, %NULL for entire field.
 * @min: Value to map to @gradient begining.
 * @max: Value to map to @gradient end.
 *
 * Renders a field to pixbuf using false colour gradient.
 *
 * Parameters defining the area to render are measured in pixels; the entire
 * area must line within @field.
 *
 * The value range, determined by @min and @max, can be arbitrary.  Passing
 * @max smaller than @min renders the gradient inversely.
 **/
void
gwy_field_render_pixbuf(const GwyField *field,
                        GdkPixbuf *pixbuf,
                        GwyGradient *gradient,
                        cairo_rectangle_t *rectangle,
                        gdouble min, gdouble max)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(GDK_IS_PIXBUF(pixbuf));
    g_return_if_fail(GWY_IS_GRADIENT(gradient));

    gdouble xfrom, xto, yfrom, yto;
    if (!check_field_rectangle(rectangle, field->xres, field->yres,
                               &xfrom, &yfrom, &xto, &yto))
        return;

    if (min == max) {
        field_render_empty_range(pixbuf, gradient);
        return;
    }

    guint width = gdk_pixbuf_get_width(pixbuf);
    guint height = gdk_pixbuf_get_height(pixbuf);
    guint rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    guint xres = field->xres, yres = field->yres;
    const gdouble *data = field->data;

    gdouble qc = 1.0/(max - min);
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

            z = qc*(z - min);
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
 * @surface: A cairo image surface of format %CAIRO_FORMAT_RGB24.
 * @gradient: A false colour gradient.
 * @rectangle: (allow-none):
 *             Area in @field to render, %NULL for entire field.
 * @min: Value to map to @gradient begining.
 * @max: Value to map to @gradient end.
 *
 * Renders a field to cairo image surface using false colour gradient.
 *
 * Parameters defining the area to render are measured in pixels; the entire
 * area must line within @field.
 *
 * The value range, determined by @min and @max, can be arbitrary.  Passing
 * @max smaller than @min renders the gradient inversely.
 **/
void
gwy_field_render_cairo(const GwyField *field,
                       cairo_surface_t *surface,
                       GwyGradient *gradient,
                       cairo_rectangle_t *rectangle,
                       gdouble min, gdouble max)
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

    if (min == max) {
        // TODO
        // field_render_empty_range(pixbuf, gradient);
        return;
    }

    guint width = cairo_image_surface_get_width(surface);
    guint height = cairo_image_surface_get_height(surface);
    guint stride = cairo_image_surface_get_stride(surface);
    g_assert(stride % 4 == 0);
    guint32 *pixels = (guint32*)cairo_image_surface_get_data(surface);

    guint xres = field->xres, yres = field->yres;
    const gdouble *data = field->data;

    gdouble qc = 1.0/(max - min);
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

            z = qc*(z - min);
            // TODO: Support pixmaps with alpha
            GwyRGBA rgba;
            interpolate_color(gradpts, gradlen, CLAMP(z, 0.0, 1.0), &rgba);
            *pixrow = gwy_rgba_to_rgb8_cairo(&rgba);
        }
    }

    g_free(interpy);
    g_free(interpx);
}

/**
 * SECTION: field-render
 * @section_id: GwyField-render
 * @title: GwyField rendering
 * @short_description: Rendering of fields to raster images
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
