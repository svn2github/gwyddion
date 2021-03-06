/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#define GWY_DATA_FIELD_RAW_ACCESS
#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libprocess/hough.h>
#include <libprocess/arithmetic.h>

static void bresenhams_line           (GwyDataField *dfield,
                                       gint x1,
                                       gint x2,
                                       gint y1,
                                       gint y2,
                                       gdouble value);
static void bresenhams_line_polar     (GwyDataField *dfield,
                                       gdouble rho,
                                       gdouble theta,
                                       gdouble value);
static void bresenhams_circle         (GwyDataField *dfield,
                                       gdouble r,
                                       gint col,
                                       gint row,
                                       gdouble value);
static void bresenhams_circle_gradient(GwyDataField *dfield,
                                       gdouble r,
                                       gint col,
                                       gint row,
                                       gdouble value,
                                       gdouble angle);

static void
add_point(GwyDataField *result,
               gint rho, gint theta,
               gdouble value, gint hsize)
{
    gint col, row;
    gdouble *rdata, coeff;

    rdata = gwy_data_field_get_data(result);
    for (col = MAX(0, rho-hsize); col < MIN(result->xres, rho+hsize); col++)
    {
        for (row = MAX(0, theta-hsize); row < MIN(result->yres, theta+hsize); row++)
        {
            if (hsize) coeff = 1 + sqrt((col-rho)*(col-rho) + (row-theta)*(row-theta));
            else coeff = 1;
            rdata[col + row*result->xres] += value/coeff;
        }
    }
}


void
gwy_data_field_hough_line(GwyDataField *dfield,
                               GwyDataField *x_gradient,
                               GwyDataField *y_gradient,
                               GwyDataField *result,
                               gint hwidth)
{
    gint k, col, row, xres, yres, rxres, ryres;
    gdouble rho, theta, rhostep, thetastep, *data, gradangle=0, gradangle0=0, gradangle2=0, gradangle3=0, threshold;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    rxres = gwy_data_field_get_xres(result); /*rho*/
    ryres = gwy_data_field_get_yres(result); /*theta*/

    if ((x_gradient && xres != gwy_data_field_get_xres(x_gradient)) ||
        (x_gradient && yres != gwy_data_field_get_yres(x_gradient)) ||
        (y_gradient && xres != gwy_data_field_get_xres(y_gradient)) ||
        (y_gradient && yres != gwy_data_field_get_yres(y_gradient)))
    {
        g_warning("Hough: input fields must be of same size (or null).\n");
        return;
    }


    thetastep = 2*G_PI/(gdouble)ryres;
    rhostep = 2.0*sqrt(xres*xres+yres*yres)/(gdouble)rxres;

    gwy_data_field_fill(result, 0);
    data = gwy_data_field_get_data(dfield);
    for (col = 0; col < xres; col++)
    {
        for (row = 0; row < yres; row++)
        {
            if (dfield->data[col + row*xres] > 0)
            {
                if (x_gradient && y_gradient)
                {
                    /*TODO fix this maximally stupid angle wrapping*/
                    gradangle = G_PI/2.0 + atan2(y_gradient->data[col + row*xres], x_gradient->data[col + row*xres]);
                    gradangle2 = gradangle + G_PI;
                    gradangle0 = gradangle - G_PI;
                    gradangle3 = gradangle + 2*G_PI;
                }
                for (k = 0; k < result->yres; k++)
                {
                    theta = (gdouble)k*thetastep;

                    threshold = 0.1;
                    if (x_gradient && y_gradient && !(fabs(theta-gradangle)<threshold || fabs(theta-gradangle2)<threshold
                        || fabs(theta-gradangle3)<threshold || fabs(theta-gradangle0)<threshold)) continue;


                    rho = ((gdouble)col)*cos(theta) + ((gdouble)row)*sin(theta);

                    add_point(result,
                              (gint)((gdouble)(rho/rhostep)+(gdouble)rxres/2),
                              k,
                              data[col + row*xres],
                              hwidth);

                }
            }
        }
    }
    gwy_data_field_set_xreal(result, 2*sqrt(xres*xres+yres*yres));
    gwy_data_field_set_yreal(result, 2*G_PI);

}
#include <stdio.h>
void
gwy_data_field_hough_line_strenghten(GwyDataField *dfield,
                                          GwyDataField *x_gradient,
                                          GwyDataField *y_gradient,
                                          gint hwidth,
                                          gdouble threshold)
{
    GwyDataField *result;
    gdouble hmax, hmin, threshval, zdata[20];
    gint i;
    gdouble xdata[20], ydata[20];

    result = gwy_data_field_new(sqrt(gwy_data_field_get_xres(dfield)*gwy_data_field_get_xres(dfield)
                             +gwy_data_field_get_yres(dfield)*gwy_data_field_get_yres(dfield)),
                             360, 0, 10,
                             FALSE);

    gwy_data_field_hough_line(dfield, x_gradient, y_gradient, result, hwidth);

    gwy_data_field_get_min_max(result, &hmin, &hmax);
    threshval = hmin + (hmax - hmin)*threshold; /*FIXME do GUI for this parameter*/
    //threshval = hmax - (hmax - hmin)/2.5;
    
    gwy_data_field_get_local_maxima_list(result, xdata, ydata, zdata, 20, 2, threshval, TRUE);

    for (i = 0; i < 20; i++)
    {
        printf("zdata: %g %g %g\n", xdata[i], ydata[i], zdata[i]);
     
       if (zdata[i] > threshval && (ydata[i]<result->yres/4 || ydata[i]>=3*result->yres/4)) {
           //printf("point: %d %d (of %d %d), xreal: %g  yreal: %g\n", xdata[i], ydata[i], result->xres, result->yres, result->xreal, result->yreal);     
           bresenhams_line_polar(dfield,
                                      ((gdouble)xdata[i])*result->xreal/((gdouble)result->xres) - result->xreal/2,
                                      ((gdouble)ydata[i])*G_PI*2.0/((gdouble)result->yres),
                                      1);
        }
    }
}

void
gwy_data_field_hough_circle(GwyDataField *dfield,
                               GwyDataField *x_gradient,
                               GwyDataField *y_gradient,
                               GwyDataField *result,
                               gdouble radius)
{
    gint col, row, xres, yres, rxres, ryres;
    gdouble *data, angle=0;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    rxres = gwy_data_field_get_xres(result); /*rho*/
    ryres = gwy_data_field_get_yres(result); /*theta*/

    if ((x_gradient && xres != gwy_data_field_get_xres(x_gradient)) ||
        (x_gradient && yres != gwy_data_field_get_yres(x_gradient)) ||
        (y_gradient && xres != gwy_data_field_get_xres(y_gradient)) ||
        (y_gradient && yres != gwy_data_field_get_yres(y_gradient)))
    {
        g_warning("Hough: input fields must be of same size (or null).\n");
        return;
    }

    gwy_data_field_fill(result, 0);
    data = gwy_data_field_get_data(result);
    for (col = 0; col < xres; col++)
    {
        for (row = 0; row < yres; row++)
        {
            if (dfield->data[col + row*xres] > 0)
            {
                if (x_gradient && y_gradient)
                angle = atan2(y_gradient->data[col + row*xres], x_gradient->data[col + row*xres]);

                if (x_gradient && y_gradient) bresenhams_circle_gradient(result, radius, col, row, 1, angle);
                else bresenhams_circle(result, radius, col, row, 1);
            }
        }
    }

}

void
gwy_data_field_hough_circle_strenghten(GwyDataField *dfield,
                                          GwyDataField *x_gradient,
                                          GwyDataField *y_gradient,
                                          gdouble radius,
                                          gdouble threshold)
{
    GwyDataField *result, *buffer;
    gdouble hmax, hmin, threshval, zdata[200];
    gint i;
    gdouble xdata[200], ydata[200];

    result = gwy_data_field_new_alike(dfield, FALSE);

    gwy_data_field_hough_circle(dfield, x_gradient, y_gradient, result, radius);

    gwy_data_field_get_min_max(result, &hmin, &hmax);
    threshval = hmax + (hmax - hmin)*threshold; /*FIXME do GUI for this parameter*/
    gwy_data_field_get_local_maxima_list(result, xdata, ydata, zdata, 200, 2, threshval, TRUE);

    buffer = gwy_data_field_duplicate(dfield);
    gwy_data_field_fill(buffer, 0);

    for (i = 0; i < 200; i++)
    {
        if (zdata[i]>threshval) {
                bresenhams_circle(buffer,
                                      (gint)radius,
                                      xdata[i],
                                      ydata[i],
                                      1);

        }
    }
    gwy_data_field_threshold(buffer, 1, 0, 2);
    gwy_data_field_sum_fields(dfield, dfield, buffer);

}

static gint
signum(gint x)
{
    if (x<0) {
        return -1;
    }
    else if (x==0) {
        return 0;
    }
    return 1;
}

#include <stdio.h>

void
gwy_data_field_hough_polar_line_to_datafield(GwyDataField *dfield, 
                      gdouble rho, gdouble theta,
                     gint *px1, gint *px2, gint *py1, gint *py2)
{
     gint x_top, x_bottom, y_left, y_right;
     gboolean x1set = FALSE;

     x_top = (gint)(rho/cos(theta));
     x_bottom = (gint)((rho - dfield->yres*sin(theta))/cos(theta));
     y_left = (gint)(rho/sin(theta));
     y_right = (gint)((rho - dfield->xres*cos(theta))/sin(theta));

     printf("%g %g %d %d %d %d\n", rho, theta, x_top, x_bottom, y_left, y_right);
     if (x_top >= 0 && x_top < dfield->xres)
     {
         *px1 = x_top;
         *py1 = 0;
         x1set = TRUE;
     }
     if (x_bottom >= 0 && x_bottom < dfield->xres)
     {
         if (x1set) {
             *px2 = x_bottom;
             *py2 = dfield->yres - 1;
         }
         else {
             *px1 = x_bottom;
             *py1 = dfield->yres - 1;
             x1set = TRUE;
         }
     }
     if (y_left >= 0 && y_left < dfield->yres)
     {
         if (x1set) {
             *px2 = 0;
             *py2 = y_left;
         }
         else {
             *px1 = 0;
             *py1 = y_left;
             x1set = TRUE;
         }
     }
     if (y_right >= 0 && y_right < dfield->yres)
     {
         *px2 = dfield->xres - 1;
         *py2 = y_right;
     }
     if (!x1set) {
         g_warning("line does not intersect image\n");
         return;
     }
}

static void
bresenhams_line_polar(GwyDataField *dfield, gdouble rho, gdouble theta, gdouble value)
{
     gint px1, px2, py1, py2;

     gwy_data_field_hough_polar_line_to_datafield(dfield, rho, theta, &px1, &px2, &py1, &py2);
     
     bresenhams_line(dfield, px1, px2, py1, py2, value);
}

static void
bresenhams_line(GwyDataField *dfield, gint x1, gint x2, gint y1, gint y2, gdouble value)
{
     gint i, dx, dy, sdx, sdy, dxabs, dyabs;
     gint x, y, px, py;

     dx = x2 - x1;
     dy = y2 - y1;
     dxabs = (gint)fabs(dx);
     dyabs = (gint)fabs(dy);
     sdx = signum(dx);
     sdy = signum(dy);
     x = dyabs>>1;
     y = dxabs>>1;
     px = x1;
     py = y1;

     dfield->data[px + py*dfield->xres] = value;

     if (dxabs >= dyabs)
     {
         for (i = 0; i < dxabs; i++)
         {
             y += dyabs;
             if (y >= dxabs)
             {
                 y -= dxabs;
                 py += sdy;
             }
             px += sdx;
             dfield->data[px + py*dfield->xres] = value;
         }
     }
     else
     {
         for (i = 0; i < dyabs; i++)
         {
             x += dxabs;
             if (x >= dyabs)
             {
                 x -= dyabs;
                 px += sdx;
             }
             py += sdy;
             dfield->data[px + py*dfield->xres] = value;
         }
     }
}

static void
plot_pixel_safe(GwyDataField *dfield, gint idx, gdouble value)
{
    if (idx > 0 && idx < dfield->xres*dfield->yres) dfield->data[idx] += value;
}

static void
bresenhams_circle(GwyDataField *dfield, gdouble r, gint col, gint row, gdouble value)
{
    gdouble n=0,invradius=1/(gdouble)r;
    gint dx = 0, dy = r-1;
    gint dxoffset, dyoffset;
    gint offset = col + row*dfield->xres;
    while (dx<=dy)
    {
         dxoffset = dfield->xres*dx;
         dyoffset = dfield->xres*dy;
         plot_pixel_safe(dfield, offset + dy - dxoffset, value);
         plot_pixel_safe(dfield, offset + dx - dyoffset, value);
         plot_pixel_safe(dfield, offset - dx - dyoffset, value);
         plot_pixel_safe(dfield, offset - dy - dxoffset, value);
         plot_pixel_safe(dfield, offset - dy + dxoffset, value);
         plot_pixel_safe(dfield, offset - dx + dyoffset, value);
         plot_pixel_safe(dfield, offset + dx + dyoffset, value);
         plot_pixel_safe(dfield, offset + dy + dxoffset, value);
         dx++;
         n += invradius;
         dy = r * sin(acos(n));
     }
}

static void
bresenhams_circle_gradient(GwyDataField *dfield, gdouble r, gint col, gint row,
                                gdouble value, gdouble gradient)
{
    gdouble n=0,invradius=1/(gdouble)r;
    gint dx = 0, dy = r-1;
    gint dxoffset, dyoffset, i;
    gdouble diff;
    gint offset = col + row*dfield->xres;

    gdouble multoctant[8];
    for (i = 0; i<8; i++)
    {
        diff = fabs((G_PI*(gdouble)i/4.0 - G_PI) - gradient);
        if (diff > G_PI) diff = 2*G_PI - diff;
        multoctant[i] = G_PI - diff;
    }

    while (dx<=dy)
    {
         dxoffset = dfield->xres*dx;
         dyoffset = dfield->xres*dy;
         plot_pixel_safe(dfield, offset + dy - dxoffset, value*multoctant[0]);
         plot_pixel_safe(dfield, offset + dx - dyoffset, value*multoctant[1]);
         plot_pixel_safe(dfield, offset - dx - dyoffset, value*multoctant[2]);
         plot_pixel_safe(dfield, offset - dy - dxoffset, value*multoctant[3]);
         plot_pixel_safe(dfield, offset - dy + dxoffset, value*multoctant[4]);
         plot_pixel_safe(dfield, offset - dx + dyoffset, value*multoctant[5]);
         plot_pixel_safe(dfield, offset + dx + dyoffset, value*multoctant[6]);
         plot_pixel_safe(dfield, offset + dy + dxoffset, value*multoctant[7]);
         dx++;
         n += invradius;
         dy = r * sin(acos(n));
     }
}


gint
find_smallest_index(gdouble *data, gint n)
{
    gint i, imin;
    gdouble valmin = G_MAXDOUBLE;

    for (i = 0; i < n; i++)
    {
        if (data[i] < valmin) {
            imin = i;
            valmin = data[i];
        }
    }
    return imin;

}

gboolean
find_isthere(gdouble *xdata, gdouble *ydata, gint mcol, gint mrow, gint n)
{
    gint i;
    for (i = 0; i < n; i++)
    {
        if (fabs(xdata[i]-mcol)<1 && fabs(ydata[i]-mrow)<1) return TRUE;
    }
    return FALSE;
}

gdouble
find_nmax(GwyDataField *dfield, gint *mcol, gint *mrow)
{

    if ((*mcol) > 0 && dfield->data[(*mcol) - 1 + (*mrow)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mcol) -= 1;
        find_nmax(dfield, mcol, mrow);
    }
    if ((*mrow) > 0 && dfield->data[(*mcol) + ((*mrow) - 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mrow) -= 1;
        find_nmax(dfield, mcol, mrow);
    }
     if ((*mcol) < (dfield->xres - 1) &&
         dfield->data[(*mcol) + 1 + (*mrow)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mcol) += 1;
        find_nmax(dfield, mcol, mrow);
    }
     if ((*mrow) < (dfield->yres - 1) &&
         dfield->data[(*mcol) + ((*mrow) + 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mrow) += 1;
        find_nmax(dfield, mcol, mrow);
    }
     if ((*mcol) > 0  && (*mrow) > 0 &&
         dfield->data[(*mcol) - 1 + ((*mrow) - 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mcol) -= 1;
        (*mrow) -= 1;
        find_nmax(dfield, mcol, mrow);
    }
     if ((*mcol) > 0 && (*mrow) < (dfield->yres - 1) &&
         dfield->data[(*mcol) - 1 + ((*mrow) + 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mcol) -= 1;
        (*mrow) += 1;
        find_nmax(dfield, mcol, mrow);
    }
     if ((*mcol) < (dfield->xres - 1) && (*mrow) > 0 &&
         dfield->data[(*mcol) + 1 + ((*mrow) - 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mcol) += 1;
        (*mrow) -= 1;
        find_nmax(dfield, mcol, mrow);
    }
     if ((*mcol) < (dfield->xres - 1) &&
         (*mrow) < (dfield->yres - 1) && dfield->data[(*mcol) + 1 + ((*mrow) + 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mcol) += 1;
        (*mrow) += 1;
        find_nmax(dfield, mcol, mrow);
    }

     
    return dfield->data[(*mcol) + (*mrow)*dfield->xres];
}

static void
get_local_maximum(GwyDataField *dfield, gint mcol, gint mrow,
                                gdouble *xval, gdouble *yval, gdouble *zval)
{
    
    gdouble zm, zp, z0;

    z0 = gwy_data_field_get_val(dfield, mcol, mrow);
    if (mcol > 0)
        zm = gwy_data_field_get_val(dfield, mcol - 1, mrow);
    else
        zm = z0;
    
    if (mcol < (gwy_data_field_get_xres(dfield)-1))
        zp = gwy_data_field_get_val(dfield, mcol + 1, mrow);
    else
        zp = z0;
 
    if (zm == z0 && zp == z0) *xval = (gdouble)mcol;
    else
        *xval = (gdouble)mcol + (zm - zp)/(zm + zp - 2*z0)/2;

    if (mrow > 0)
        zm = gwy_data_field_get_val(dfield, mcol, mrow - 1);
    else
        zm = z0;
    
    if (mrow < (gwy_data_field_get_yres(dfield)-1))
        zp = gwy_data_field_get_val(dfield, mcol, mrow + 1);
    else
        zp = z0;
 
    if (zm == z0 && zp == z0) *yval = (gdouble)mcol;
    else
        *yval = (gdouble)mrow + (zm - zp)/(zm + zp - 2*z0)/2;

    
}


gint
gwy_data_field_get_local_maxima_list(GwyDataField *dfield,
                                          gdouble *xdata,
                                          gdouble *ydata,
                                          gdouble *zdata,
                                          gint ndata,
                                          gint skip,
                                          gdouble threshold,
                                          gboolean subpixel)
{
    gint col, row, mcol, mrow, i, count;
    gdouble xval, yval, zval;
    gdouble value;

    for (i = 0; i < ndata; i++)
    {
        xdata[i] = 0;
        ydata[i] = 0;
        zdata[i] = -G_MAXDOUBLE;
    }

    count = 0;
    for (col=skip; col<(dfield->xres - skip); col += (1 + skip))
    {
        for (row=skip; row<(dfield->yres - skip); row += (1 + skip))
        {
            mcol = col;
            mrow = row;
            value = find_nmax(dfield, &mcol, &mrow);

            if (find_isthere(xdata, ydata, mcol, mrow, ndata) || value<threshold) continue;

            i = find_smallest_index(zdata, ndata);
            if (zdata[i] < value) {
                if (subpixel)
                {
                    get_local_maximum(dfield, mcol, mrow,
                                      &xval, &yval, &zval);
                    zdata[i] = value;
                    xdata[i] = xval;
                    ydata[i] = yval;
                    count++;
                }
                else
                {
                    zdata[i] = value;
                    xdata[i] = (gdouble)mcol;
                    ydata[i] = (gdouble)mrow;
                    count++;
                }
            }
        }
    }

    return count;
}

void 
gwy_data_field_hough_datafield_line_to_polar(GwyDataField *dfield,
                                                  gint px1,
                                                  gint px2,
                                                  gint py1,
                                                  gint py2,
                                                  gdouble *rho,
                                                  gdouble *theta)
{
    gdouble k, q;
   
    
    k = ((gdouble)py2 - (gdouble)py1)/((gdouble)px2 - (gdouble)px1);
    q = (gdouble)py1 - ((gdouble)py2 - (gdouble)py1)/((gdouble)px2 - (gdouble)px1)*px1;

    *rho = q/sqrt(k*k + 1);
    *theta = asin(1/sqrt(k*k + 1));
    
    printf("line: p1 (%d, %d), p2 (%d, %d), k=%g q=%g rho=%g theta=%g\n",
           px1, py1, px2, py2, k, q, *rho, *theta);

}


/************************** Documentation ****************************/

/**
 * SECTION:hough
 * @title: hough
 * @short_description: Hough transform
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
