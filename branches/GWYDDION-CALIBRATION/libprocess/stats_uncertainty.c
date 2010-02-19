/*
 *  @(#) $Id: stats.c 10263 2009-10-19 12:39:41Z yeti-dn $
 *  Copyright (C) 2003-2009 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <string.h>

#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>
#include <libprocess/stats_uncertainty.h>
#include <libprocess/linestats.h>
#include <libprocess/grains.h>
#include <libprocess/inttrans.h>
#include <libprocess/simplefft.h>
#include "gwyprocessinternal.h"

/**
 * gwy_data_field_get_max_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 *
 * Finds the uncertainty of the maximum value of a data field.
 *
 *
 * Returns: The uncertainty of the maximum value.
 **/
gdouble
gwy_data_field_get_max_uncertainty(GwyDataField *data_field, GwyDataField *uncz_field)
{
    const gdouble *p, *u;
    gdouble max,max_unc;
    gint i;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), -G_MAXDOUBLE);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(uncz_field), -G_MAXDOUBLE);

    max = data_field->data[0];
    p = data_field->data;
    u = uncz_field->data;
    for (i = data_field->xres * data_field->yres; i; i--, p++, u++) {
        if (G_UNLIKELY(max < *p)){
            max = *p;
            max_unc= *u;
        }
    }

    return max_unc;
}


/**
 * gwy_data_field_area_get_max_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Finds the uncertainty of the maximum value in a rectangular part of a data field.
 *
 * Returns: The uncertainty of the maximum value.  When the number of samples to calculate
 *          maximum of is zero, %G_MAXDOUBLE is returned.
 **/
gdouble
gwy_data_field_area_get_max_uncertainty(GwyDataField *dfield,
                                        GwyDataField *uncz_field,
                                        GwyDataField *mask,
                                        gint col, gint row,
                                        gint width, gint height)
{
    gint i, j;
    gdouble max = -G_MAXDOUBLE;
    gdouble max_unc=G_MAXDOUBLE;
    const gdouble *datapos, *mpos, *uncpos;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), max_unc);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(uncz_field), max_unc);
    g_return_val_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                                   && mask->xres == dfield->xres
                                   && mask->yres == dfield->yres),
                         max_unc);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         max_unc);
    if (!width || !height)
        return max;

    if (mask) {
        datapos = dfield->data + row*dfield->xres + col;
        uncpos = uncz_field->data+row*uncz_field->xres+col;
        mpos = mask->data + row*mask->xres + col;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *urow = uncpos + i*uncz_field->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            for (j = 0; j < width; j++) {
                if (G_UNLIKELY(max < *drow) && *mrow > 0.0){
                    max = *drow;
		    max_unc = *urow;
		}
                drow++;
                urow++;
                mrow++;
            }
        }

        return max_unc;
    }

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_max_uncertainty(dfield,uncz_field);

    datapos = dfield->data + row*dfield->xres + col;
    uncpos = uncz_field->data + row*uncz_field->xres + col;
    for (i = 0; i < height; i++) {
        const gdouble *drow = datapos + i*dfield->xres;
        const gdouble *urow = uncpos + i*uncz_field->xres;

        for (j = 0; j < width; j++) {
            if (G_UNLIKELY(max < *drow)) {
                max = *drow;
		max_unc = *urow;
	    }
            drow++;
            urow++;
        }
    }

    return max_unc;
}

/**
 * gwy_data_field_get_min:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 *
 * Finds the uncertainty of the minimum value of a data field.
 *
 *
 * Returns: The uncertainty of the minimum value.
 **/
gdouble
gwy_data_field_get_min_uncertainty(GwyDataField *data_field, 
                                   GwyDataField *uncz_field)
{
    gdouble min, min_unc;
    const gdouble *p, *u;
    gint i;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), G_MAXDOUBLE);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(uncz_field), G_MAXDOUBLE);

    min = data_field->data[0];
    p = data_field->data;
    u = uncz_field->data;
    for (i = data_field->xres * data_field->yres; i; i--, p++,u++) {
        if (G_UNLIKELY(min > *p)){
            min = *p;
            min_unc= *u;
        }
    }

    return min_unc;
}


/**
 * gwy_data_field_area_get_min_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Finds the uncertainty of the minimum value in a rectangular part of a data field.
 *
 * Returns: The uncertainty of the minimum value.  When the number of samples to calculate
 *          minimum of is zero, %G_MAXDOUBLE is returned.
 **/
gdouble
gwy_data_field_area_get_min_uncertainty(GwyDataField *dfield,
                                        GwyDataField *uncz_field,
                                        GwyDataField *mask,
                                        gint col, gint row,
                                        gint width, gint height)
{
    gint i, j;
    gdouble min = G_MAXDOUBLE;
    gdouble min_unc=G_MAXDOUBLE;
    const gdouble *datapos, *mpos, *uncpos;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), min_unc);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(uncz_field), min_unc);
    g_return_val_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                                   && mask->xres == dfield->xres
                                   && mask->yres == dfield->yres),
                         min_unc);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         min_unc);
    if (!width || !height)
        return min_unc;

    if (mask) {
        datapos = dfield->data + row*dfield->xres + col;
        uncpos = uncz_field->data + row*uncz_field->xres + col;
        mpos = mask->data + row*mask->xres + col;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *urow = uncpos + i*uncz_field->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            for (j = 0; j < width; j++) {
                if (G_UNLIKELY(min > *drow) && *mrow > 0.0){
                    min = *drow;
                    min_unc = *urow;
                }
                drow++;
                urow++;
                mrow++;
            }
        }

        return min_unc;
    }

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_min_uncertainty(dfield,uncz_field);

    datapos = dfield->data + row*dfield->xres + col;
    uncpos = uncz_field->data + row*uncz_field->xres + col;
    for (i = 0; i < height; i++) {
        const gdouble *drow = datapos + i*dfield->xres;
        const gdouble *urow = uncpos + i*uncz_field->xres;

        for (j = 0; j < width; j++) {
            if (G_UNLIKELY(min > *drow)){
                min = *drow;
                min_unc = *urow;
            }
            drow++;
            urow++;
        }
    }

    return min_unc;
}

/**
 * gwy_data_field_get_min_max_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @min_unc: Location to store uncertainty of minimum to.
 * @max_unc: Location to store uncertainty maximum to.
 *
 * Finds minimum and maximum values of a data field.
 **/
void
gwy_data_field_get_min_max_uncertainty(GwyDataField *data_field,
                                       GwyDataField *uncz_field,
                                       gdouble *min_unc,
                                       gdouble *max_unc)
{
    gdouble min1, max1;
    gdouble min_unc1, max_unc1;
    const gdouble *p,*u;
    gint i;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(uncz_field));

    min1 = data_field->data[0];
    max1 = data_field->data[0];
    p = data_field->data;
    u = uncz_field->data;
    for (i = data_field->xres * data_field->yres; i; i--, p++,u++) {
        if (G_UNLIKELY(min1 > *p)){
            min1 = *p;
	    min_unc1= *u;
	}
        if (G_UNLIKELY(max1 < *p)) {
            max1 = *p;
	    max_unc1= *u;
	}
    }

    *min_unc = min_unc1;
    *max_unc = max_unc1;
}

/**
 * gwy_data_field_area_get_min_max_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @min_unc: Location to store uncertainty of minimum to.
 * @max_unc: Location to store uncertainty of maximum to.
 *
 * Finds uncertainties of the minimum and maximum values in a rectangular part of a data field.
 *
 * This function is equivalent to calling
 * @gwy_data_field_area_get_min_max_uncertainty_mask()
 * with masking mode %GWY_MASK_INCLUDE.
 **/
void
gwy_data_field_area_get_min_max_uncertainty(GwyDataField *data_field,
                                GwyDataField *uncz_field,
                                GwyDataField *mask,
                                gint col, gint row,
                                gint width, gint height,
                                gdouble *min_unc,
                                gdouble *max_unc)
{
    gwy_data_field_area_get_min_max_uncertainty_mask(data_field,uncz_field, mask, GWY_MASK_INCLUDE,
                                         col, row, width, height,
                                         min_unc, max_unc);
}

/**
 * gwy_data_field_area_get_min_max_uncertainty_mask:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @mode: Masking mode to use.  See the introduction for description of
 *        masking modes.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @min_unc: Location to store uncertainty of minimum to.
 * @max_unc: Location to store uncertainty of maximum to.
 *
 * Finds the uncertainties of the minimum and maximum values in a rectangular part of a data field.
 *
 * Since: 2.18
 **/
void
gwy_data_field_area_get_min_max_uncertainty_mask(GwyDataField *data_field,
                                                 GwyDataField *uncz_field,
                                                 GwyDataField *mask,
                                                 GwyMaskingType mode,
                                                 gint col, gint row,
                                                 gint width, gint height,
                                                 gdouble *min_unc,
                                                 gdouble *max_unc)
{
    gdouble min1 = G_MAXDOUBLE, max1 = -G_MAXDOUBLE;
    gdouble min_unc1 = G_MAXDOUBLE, max_unc1 = G_MAXDOUBLE;
    const gdouble *datapos, *mpos, *uncpos;
    gint i, j;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(uncz_field));
    g_return_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                               && mask->xres == data_field->xres
                               && mask->yres == data_field->yres));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);
    if (!width || !height) {
        if (min_unc)
            *min_unc = min_unc1;
        if (max_unc)
            *max_unc = max_unc1;
        return;
    }

    if (!min_unc && !max_unc)
        return;

    if (mask && mode != GWY_MASK_IGNORE) {
        datapos = data_field->data + row*data_field->xres + col;
        uncpos = uncz_field->data + row*uncz_field->xres + col;
        mpos = mask->data + row*mask->xres + col;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*data_field->xres;
            const gdouble *urow = uncpos + i*uncz_field->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            if (mode == GWY_MASK_INCLUDE) {
                for (j = 0; j < width; j++) {
                    if (G_UNLIKELY(min1 > *drow) && *mrow > 0.0){
                        min1 = *drow;
			min_unc1 = *urow;
		    }
                    if (G_UNLIKELY(max1 < *drow) && *mrow > 0.0){
                        max1 = *drow;
			max_unc1 = *urow;
		    }
                    drow++;
                    urow++;
                    mrow++;
                }
            }
            else {
                for (j = 0; j < width; j++) {
                    if (G_UNLIKELY(min1 > *drow) && *mrow < 1.0){
                        min1 = *drow;
			min_unc1 = *urow;
		    }
                    if (G_UNLIKELY(max1 < *drow) && *mrow < 1.0){
                        max1 = *drow;
			max_unc1 = *urow;
		    }
                    drow++;
                    urow++;
                    mrow++;
                }
            }
        }
        *min_unc = min_unc1;
        *max_unc = max_unc1;

        return;
    }

    if (col == 0 && width == data_field->xres
        && row == 0 && height == data_field->yres) {
        gwy_data_field_get_min_max_uncertainty(data_field,uncz_field, min_unc, max_unc);
        return;
    }

    if (!min_unc) {
        *max_unc = gwy_data_field_area_get_max_uncertainty(data_field, uncz_field,NULL,
                                           col, row, width, height);
        return;
    }
    if (!max_unc) {
        *min_unc = gwy_data_field_area_get_min_uncertainty(data_field,uncz_field, NULL,
                                           col, row, width, height);
        return;
    }

    datapos = data_field->data + row*data_field->xres + col;
    uncpos = uncz_field->data + row*uncz_field->xres + col;
    for (i = 0; i < height; i++) {
        const gdouble *drow = datapos + i*data_field->xres;
        const gdouble *urow = uncpos + i*uncz_field->xres;

        for (j = 0; j < width; j++) {
            if (G_UNLIKELY(min1 > *drow)){
                min1 = *drow;
		min_unc1 = *urow;
	    }
            if (G_UNLIKELY(max1 < *drow)){
                max1 = *drow;
		max_unc1 = *urow;
	    }
            drow++;
            urow++;
        }
    }

    *min_unc = min_unc1;
    *max_unc = max_unc1;
}

/**
 * gwy_data_field_get_avg_uncertainty:
 * @data_field: A data field
 * @uncz_field: The corresponding uncertainty data field
 *
 * Computes the uncertainty of the average value of a data field.
 *
 *
 * Returns: The uncertainty of the average value.
 **/
gdouble
gwy_data_field_get_avg_uncertainty(GwyDataField *data_field, 
                                   GwyDataField *uncz_field)
{

	gdouble sum=0;	
	const gdouble *p;
	gint i;

	g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), sum);
	g_return_val_if_fail(GWY_IS_DATA_FIELD(uncz_field), sum);

	p = uncz_field->data;
	for (i = uncz_field->xres * uncz_field->yres; i; i--, p++)
		sum += (*p)*(*p);

	return sqrt(sum)/(uncz_field->xres*uncz_field->yres);
}
/**
 * gwy_data_field_area_get_avg_uncertainty:
 * @data_field: A data field
 * @uncz_field: The corresponding uncertainty data field
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes the uncertainty of the average value of a rectangular part of a data field.
 *
 * This function is equivalent to calling @gwy_data_field_area_get_avg_uncertainty_mask()
 * with masking mode %GWY_MASK_INCLUDE.
 *
 * Returns: The uncertainty of the average value.
 **/
gdouble
gwy_data_field_area_get_avg_uncertainty(GwyDataField *dfield,
                                        GwyDataField *uncz_field,
                                        GwyDataField *mask,
                                        gint col, gint row,
                                        gint width, gint height)
{
    return gwy_data_field_area_get_avg_uncertainty_mask(dfield,uncz_field, mask,
		    GWY_MASK_INCLUDE,
                                            col, row, width, height);
}

/**
 * gwy_data_field_area_get_avg_uncertainty_mask:
 * @data_field: A data field
 * @uncz_field: The corresponding uncertainty data field
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @mode: Masking mode to use.  See the introduction for description of
 *        masking modes.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes the uncertainty of the average value of a rectangular part of a data field.
 *
 * Returns: The uncertainty of the average value.
 *
 * Since: 2.18
 **/
gdouble
gwy_data_field_area_get_avg_uncertainty_mask(GwyDataField *dfield,
                                             GwyDataField *uncz_field,
                                             GwyDataField *mask,
                                             GwyMaskingType mode,
                                             gint col, gint row,
                                             gint width, gint height)
{
    const gdouble *datapos, *mpos;
    gdouble sum = 0;
    gint i, j;
    guint nn;

    if (!mask || mode == GWY_MASK_IGNORE) {
	    datapos = uncz_field->data + row*uncz_field->xres + col;
	    for (i = 0; i < height; i++) {
		    const gdouble *drow = datapos + i*uncz_field->xres;
		    for (j = 0; j < width; j++)
			    sum += *(drow)*(*drow);     //was *(drow)*(drow)++), or something similar, what does i mean?
	    }
	    return sqrt(sum/(width*height));
    }

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), sum);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(mask)
                         && mask->xres == dfield->xres
                         && mask->yres == dfield->yres,
                         sum);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         sum);

    datapos = uncz_field->data + row*uncz_field->xres + col;
    mpos = mask->data + row*mask->xres + col;
    nn = 0;
    for (i = 0; i < height; i++) {
        const gdouble *drow = datapos + i*uncz_field->xres;
        const gdouble *mrow = mpos + i*mask->xres;

        if (mode == GWY_MASK_INCLUDE) {
            for (j = 0; j < width; j++) {
                if (*mrow > 0.0) {
                    sum += (*drow)*(*drow);
                    nn++;
                }
                drow++;
                mrow++;
            }
        }
        else {
            for (j = 0; j < width; j++) {
                if (*mrow < 1.0) {
                    sum += (*drow)*(*drow);
                    nn++;
                }
                drow++;
                mrow++;
            }
        }
    }

    return sqrt(sum)/nn;
}

/**
 * gwy_data_field_get_rms_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 *
 * Computes uncertainty of  root mean square value of a data field.
 *
 *
 * Returns: The uncertainty of the root mean square value.
 **/
gdouble
gwy_data_field_get_rms_uncertainty(GwyDataField *data_field, 
                                   GwyDataField *uncz_field)
{
    gint i, n;
    gdouble rms_unc = 0.0, sum,avg,rms;
    gdouble *p,*u;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), rms_unc);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(uncz_field), rms_unc);


    avg = gwy_data_field_get_avg(data_field);
    rms = gwy_data_field_get_rms(data_field);
    sum = 0.0;
    p = data_field->data;
    u = data_field->data;
    for (i = data_field->xres * data_field->yres; i; i--, p++,u++)
		    sum += (*p-avg)*(*p-avg)*(*u)*(*u) ;

    n = data_field->xres * data_field->yres;
    rms_unc = sqrt(sum)/n/rms;


    return rms_unc;
}

/**
 * gwy_data_field_area_get_rms_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes uncertainty of root mean square value of a rectangular part of a data field.
 *
 * Returns: The uncertainty of root mean square value.
 *
 * This function is equivalent to calling @gwy_data_field_area_get_rms_uncertainty_mask()
 * with masking mode %GWY_MASK_INCLUDE.
 **/

gdouble
gwy_data_field_area_get_rms_uncertainty(GwyDataField *dfield,
                                        GwyDataField *uncz_field,
                                        GwyDataField *mask,
                                        gint col, gint row,
                                        gint width, gint height)
{
    return gwy_data_field_area_get_rms_uncertainty_mask(dfield, uncz_field,
		    mask, GWY_MASK_INCLUDE,
                                            col, row, width, height);
}

/**
 * gwy_data_field_area_get_rms_uncertainty_mask:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @mode: Masking mode to use.  See the introduction for description of
 *        masking modes.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes uncertainty of root mean square value of deviations of a rectangular part of a
 * data field.
 *
 * Returns: The uncertainty of root mean square value of deviations from the mean value.
 *
 * Since: 2.18
 **/
gdouble
gwy_data_field_area_get_rms_uncertainty_mask(GwyDataField *dfield,
                                             GwyDataField *uncz_field,
                                             GwyDataField *mask,
                                             GwyMaskingType mode,
                                             gint col, gint row,
                                             gint width, gint height)
{
    gint i, j;
    gdouble rms_unc = 0.0, sum = 0.0;
    gdouble avg,rms,p;
    const gdouble *datapos, *mpos, *uncpos;
    guint nn;

    if (width == 0 || height == 0)
        return rms_unc;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), rms_unc);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(uncz_field), rms_unc);
    g_return_val_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                                   && mask->xres == dfield->xres
                                   && mask->yres == dfield->yres),
                         rms_unc);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         rms_unc);
    if (!width || !height)
        return rms_unc;

    if (mask && mode != GWY_MASK_INCLUDE) {
        avg = gwy_data_field_area_get_avg_mask(dfield, mask, mode,
                                               col, row, width, height);
        rms = gwy_data_field_area_get_rms_mask(dfield, mask, mode,
                                               col, row, width, height);
        datapos = dfield->data + row*dfield->xres + col;
        uncpos = uncz_field->data + row*uncz_field->xres + col;
        mpos = mask->data + row*mask->xres + col;
        nn = 0;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *urow = uncpos + i*uncz_field->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            if (mode == GWY_MASK_INCLUDE) {
                for (j = 0; j < width; j++) {
                    if (*mrow > 0.0) {
			    p=(*drow-avg)*(*urow);
			    sum += p*p;
			    nn++;
		    }
		    drow++;
		    urow++;
                    mrow++;
                }
            }
            else {
                for (j = 0; j < width; j++) {
                    if (*mrow < 1.0) {
			    p=(*drow-avg)*(*urow);
			    sum += p*p;
                        nn++;
                    }
                    drow++;
                    urow++;
                    mrow++;
                }
            }
        }
    rms_unc = sqrt(sum)/nn/rms;

        return rms_unc;
    }

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_rms_uncertainty(dfield,uncz_field);

    avg = gwy_data_field_area_get_avg(dfield, NULL, col, row, width, height);
    rms = gwy_data_field_area_get_rms(dfield, NULL, col, row, width, height);

    datapos = dfield->data + row*dfield->xres + col;
    uncpos = uncz_field->data + row*uncz_field->xres + col;
    for (i = 0; i < height; i++) {
        const gdouble *drow = datapos + i*dfield->xres;
	const gdouble *urow = uncpos + i*uncz_field->xres;

	for (j = 0; j < width; j++) {
		p=(*drow-avg)*(*urow);
		sum += p*p;
		drow++;
		urow++;
	}
    }

    nn = width*height;

    rms_unc = sqrt(sum)/nn/rms;
    return rms_unc;
}

/**
 * gwy_data_field_get_stats_uncertainties:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @avg_unc: Where uncertainty of average height value of the surface should be stored,
 * or %NULL.
 * @ra_unc: Where uncertainty of average value of irregularities should be stored, 
 * or %NULL.
 * @rms_unc: Where uncertainty of root mean square value of irregularities (Rq) should be 
 * stored, or %NULL.
 * @skew_unc: Where uncertainty of skew (symmetry of height distribution) should be stored, or
 *        %NULL.
 * @kurtosis_unc: Where uncertainty of kurtosis (peakedness of height ditribution) should be
 *            stored, or %NULL.
 *
 * Computes the uncertainties of the basic statistical quantities of a data field.
 **/
void
gwy_data_field_get_stats_uncertainties(GwyDataField *data_field,
                                       GwyDataField *uncz_field,
                                       gdouble *avg_unc,
                                       gdouble *ra_unc,
                                       gdouble *rms_unc,
                                       gdouble *skew_unc,
                                       gdouble *kurtosis_unc)
{
    gint i;
    gdouble c_sz2, c_sz3, c_sz4, c_abs1;
    gdouble c_uabs, c_urms, c_uskew,c_ukurt,c_uavg;
    const gdouble *p = data_field->data;
    const gdouble *u = uncz_field->data;
    guint nn = data_field->xres * data_field->yres;
    gdouble dif, udif, myavg, myrms, myskew, mykurtosis, myra;
    gdouble hlp;
    gint csig=0;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(uncz_field));

    c_sz2 = c_sz3 = c_sz4 = c_abs1 = 0;
    c_uabs= c_urms = c_uskew = c_ukurt = c_uavg= 0;

    gwy_data_field_get_stats(data_field, &myavg, &myra, &myrms, &myskew, &mykurtosis);
    for (i = nn; i; i--, p++, u++) {
	    dif= (*p-myavg);
	    csig += dif/fabs(dif);
    }


    for (i = nn; i; i--, p++, u++) {
	dif = (*p-myavg);
	udif = (*u) *(*p-myavg);

	c_uavg += (*u)*(*u);
	hlp = dif/fabs(dif)-csig/nn;
	c_uabs += hlp*(*u)*hlp*(*u);
	c_urms += udif*udif;
	hlp = dif*dif/(myrms*myrms)-1-myskew/myrms*dif;
	c_uskew += hlp*(*u)*hlp*(*u);
	hlp = dif*dif*dif/(myrms*myrms*myrms)-myskew-(mykurtosis+3)*dif/myrms;
	c_ukurt += hlp *(*u)*hlp*(*u);
    }


    if (avg_unc)
	    *avg_unc = sqrt(c_uavg) /nn;
    if (ra_unc)
        *ra_unc = sqrt(c_uabs)/nn;
    if (rms_unc)
        *rms_unc = sqrt(c_urms)/(nn*myrms);
    if (skew_unc)
        *skew_unc = 3*sqrt(c_uskew)/(nn*myrms);
    if (kurtosis_unc)
        *kurtosis_unc = 4*sqrt(c_ukurt)/(nn*myrms);

}

/**
 * gwy_data_field_area_get_stats_uncertainties:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @avg_unc: Where uncertainty of average height value of the surface should be stored,
 * or %NULL.
 * @ra_unc: Where uncertainty of average value of irregularities should be stored, 
 * or %NULL.
 * @rms_unc: Where uncertainty of root mean square value of irregularities (Rq) should be 
 * stored, or %NULL.
 * @skew_unc: Where uncertainty of skew (symmetry of height distribution) should be stored, or
 *        %NULL.
 *
 * @kurtosis_unc: Where uncertainty of kurtosis (peakedness of height ditribution) should be
 *            stored, or %NULL.
 *
 * Computes the uncertainties of the basic statistical quantities of a rectangular part of a data field.
 *
 * This function is equivalent to calling @gwy_data_field_area_get_stats_uncertainties_mask()
 * with masking mode %GWY_MASK_INCLUDE.
 **/
void
gwy_data_field_area_get_stats_uncertainties(GwyDataField *dfield,
                                            GwyDataField *uncz_field,
                                            GwyDataField *mask,
                                            gint col, gint row,
                                            gint width, gint height,
                                            gdouble *avg_unc,
                                            gdouble *ra_unc,
                                            gdouble *rms_unc,
                                            gdouble *skew_unc,
                                            gdouble *kurtosis_unc)
{
    gwy_data_field_area_get_stats_uncertainties_mask(dfield,uncz_field, mask, GWY_MASK_INCLUDE,
                                       col, row, width, height,
                                       avg_unc, ra_unc, rms_unc, skew_unc, kurtosis_unc);
}

/**
 * gwy_data_field_area_get_stats_uncertainties_mask:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @mode: Masking mode to use.  See the introduction for description of
 *        masking modes.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
  * @uncz_field: The corresponding uncertainty data field.
 * @avg_unc: Where uncertainty of average height value of the surface should be stored,
 * or %NULL.
 * @ra_unc: Where uncertainty of average value of irregularities should be stored, 
 * or %NULL.
 * @rms_unc: Where uncertainty of root mean square value of irregularities (Rq) should be 
 * stored, or %NULL.
 * @skew_unc: Where uncertainty of skew (symmetry of height distribution) should be stored, or
 *        %NULL.
 * @kurtosis_unc: Where uncertainty of kurtosis (peakedness of height ditribution) should be
 *            stored, or %NULL.
 *
 * Computes  the uncertainties of the basic statistical quantities of a rectangular part of a data field.
 *
 * Since: 2.18
 **/
void
gwy_data_field_area_get_stats_uncertainties_mask(GwyDataField *dfield,
                                                 GwyDataField *uncz_field,
                                                 GwyDataField *mask,
                                                 GwyMaskingType mode,
                                                 gint col, gint row,
                                                 gint width, gint height,
                                                 gdouble *avg_unc,
                                                 gdouble *ra_unc,
                                                 gdouble *rms_unc,
                                                 gdouble *skew_unc,
                                                 gdouble *kurtosis_unc)
{
    gdouble c_sz2, c_sz3, c_sz4, c_abs1;
    gdouble c_uabs, c_urms, c_uskew,c_ukurt,c_uavg;
    const gdouble *datapos, *mpos, *uncpos;
    gint i, j;
    guint nn;
    gdouble dif, hlp,myavg, myrms, myskew, mykurtosis, myra;
    gint csig=0;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    g_return_if_fail(GWY_IS_DATA_FIELD(uncz_field));
    g_return_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                               && mask->xres == dfield->xres
                               && mask->yres == dfield->yres));
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= dfield->xres
                     && row + height <= dfield->yres);

    c_sz2 = c_sz3 = c_sz4 = c_abs1 = 0;
    c_uabs= c_urms = c_uskew = c_ukurt = c_uavg= 0;

    myavg = gwy_data_field_area_get_avg_mask(dfield, mask, mode,
                                             col, row, width, height);
    gwy_data_field_area_get_stats_mask(dfield,mask, mode, col,row,width,height,
                                       &myavg, &myra, &myrms, &myskew, &mykurtosis);

    if (mask && mode != GWY_MASK_IGNORE) {
        datapos = dfield->data + row*dfield->xres + col;
        uncpos = uncz_field->data + row*uncz_field->xres + col;
        mpos = mask->data + row*mask->xres + col;
        nn = 0;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *mrow = mpos + i*mask->xres;
            if (mode == GWY_MASK_INCLUDE) {
                for (j = 0; j < width; j++) {
                    if (*mrow > 0.0) {
                        dif = *drow - myavg;
                        csig += dif/fabs(dif);
                    }
                    drow++;
                }
            }
        }

        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *urow = uncpos + i*uncz_field->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            if (mode == GWY_MASK_INCLUDE) {
                for (j = 0; j < width; j++) {
                    if (*mrow > 0.0) {
                        dif = *drow - myavg;

                        c_uavg += (*urow)*(*urow);

                        hlp += dif/fabs(dif)-csig/nn;
                        c_uabs += hlp*(*urow)*hlp*(*urow);   //was u

                        c_urms += dif*dif;                   //was udif

                        hlp = dif*dif/(myrms*myrms)-1-myskew/myrms*dif;
                        c_uskew += hlp*(*urow)*hlp*(*urow);  //was u

                        hlp = dif*dif*dif/(myrms*myrms*myrms)-myskew-(mykurtosis+3)*dif/myrms;
                        c_ukurt += hlp *(*urow)*hlp*(*urow); //was u

                        nn++;
                    }
                    drow++;
                    urow++;
                    mrow++;
                }
            }
            else {
                for (j = 0; j < width; j++) {
                    if (*mrow < 1.0) {
                        dif = *drow - myavg;

                        c_uavg += (*urow)*(*urow);

                        hlp += dif/fabs(dif)-csig/nn;
                        c_uabs += hlp*(*urow)*hlp*(*urow);

                        c_urms += (*urow)*dif*(*urow)*dif;

                        hlp = dif*dif/(myrms*myrms)-1-myskew/myrms*dif;
                        c_uskew += hlp*(*urow)*hlp*(*urow);

                        hlp = dif*dif*dif/(myrms*myrms*myrms)-myskew-(mykurtosis+3)*dif/myrms;
                        c_ukurt += hlp *(*urow)*hlp*(*urow);


                        nn++;
                    }
                    drow++;
                    urow++;
                    mrow++;
                }
            }
        }
    }
    else {
        nn = width*height;
        datapos = dfield->data + row*dfield->xres + col;
        uncpos = uncz_field->data + row*uncz_field->xres + col;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *urow = uncpos + i*uncz_field->xres;

            for (j = 0; j < width; j++) {
                dif = *(drow) - myavg;

                c_uavg += (*urow)*(*urow);

                hlp += dif/fabs(dif)-csig/nn;
                c_uabs += hlp*(*urow)*hlp*(*urow);

                c_urms += (*urow)*dif*(*urow)*dif;

                hlp = dif*dif/(myrms*myrms)-1-myskew/myrms*dif;
                c_uskew += hlp*(*urow)*hlp*(*urow);

                hlp = dif*dif*dif/(myrms*myrms*myrms)-myskew-(mykurtosis+3)*dif/myrms;
                c_ukurt += hlp *(*urow)*hlp*(*urow);

                drow++;
                urow++;

            }
        }
    }

    if (avg_unc)
        *avg_unc = sqrt(c_uavg) /nn;
    if (ra_unc)
        *ra_unc = sqrt(c_uabs)/nn;
    if (rms_unc)
        *rms_unc = sqrt(c_urms)/(nn*myrms);
    if (skew_unc)
        *skew_unc = 3*sqrt(c_uskew)/(nn*myrms);
    if (kurtosis_unc)
        *kurtosis_unc = 4*sqrt(c_ukurt)/(nn*myrms);

}

/*
typedef void (*GwyLameAreaUFunc)(GwyDataLine *source, GwyDataLine *usource,
                                GwyDataLine *target);

static void
gwy_data_field_area_func_lame_uncertainty(GwyDataField *data_field, GwyDataField *uncz_field,
                              GwyDataLine *target_line, 
                              GwyLameAreaUFunc func,
                              gint col, gint row,
                              gint width, gint height,
                              GwyOrientation orientation)
{
    GwyDataLine *data_line, *uline, *tmp_line;
    gint i, j, xres, yres, size;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));
    g_return_if_fail(GWY_IS_DATA_FIELD(uncz_field));
    xres = data_field->xres;
    yres = data_field->yres;
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 1 && height >= 1
                     && col + width <= xres
                     && row + height <= yres);
    g_return_if_fail(orientation == GWY_ORIENTATION_HORIZONTAL
                     || orientation == GWY_ORIENTATION_VERTICAL);

    size = (orientation == GWY_ORIENTATION_HORIZONTAL) ? width : height;
    data_line = gwy_data_line_new(size, 1.0, FALSE);
    uline = gwy_data_line_new(size, 1.0, FALSE);
    tmp_line = gwy_data_line_new(size, 1.0, FALSE);
    gwy_data_line_resample(target_line, size, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(target_line);
    gwy_data_line_set_offset(target_line, 0.0);

    switch (orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        for (i = 0; i < height; i++) {
            gwy_data_field_get_row_part(data_field, data_line, row+i,
                                        col, col+width);
            gwy_data_field_get_row_part(uncz_field, uline, row+i,
                                        col, col+width);
            func(data_line, uline, tmp_line);
            for (j = 0; j < width; j++)
                target_line->data[j] += tmp_line->data[j];
        }
        gwy_data_line_set_real(target_line,
                               gwy_data_field_jtor(data_field, width));
        for (j = 0; j < width; j++)
		target_line->data[j] = sqrt(target_line->data[j])/height;
        break;

        case GWY_ORIENTATION_VERTICAL:
        for (i = 0; i < width; i++) {
            gwy_data_field_get_column_part(data_field, data_line, col+i,
                                           row, row+height);
            gwy_data_field_get_column_part(uncz_field, uline, col+i,
                                           row, row+height);
            func(data_line, uline, tmp_line);
            for (j = 0; j < height; j++)
                target_line->data[j] += tmp_line->data[j];
        }
        gwy_data_line_set_real(target_line,
                               gwy_data_field_itor(data_field, height));
        for (j = 0; j < height; j++)
		target_line->data[j] = sqrt(target_line->data[j])/width;
        break;
    }

    g_object_unref(data_line);
    g_object_unref(uline);
    g_object_unref(tmp_line);

}
*/
/**
 * gwy_data_field_area_acf_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @target_line: A data line to store the uncertainties of the distribution to.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @orientation: Orientation of lines (ACF is simply averaged over the
 *               other orientation).
 *
 * Calculates the uncertainty of the one-dimensional autocorrelation function of a rectangular part of
 * a data field.
 **//*
void
gwy_data_field_area_acf_uncertainty(GwyDataField *data_field,
                                    GwyDataField *uncz_field,
                                    GwyDataLine *target_line, 
                                    gint col, gint row,
                                    gint width, gint height,
                                    GwyOrientation orientation)
{
    GwySIUnit *fieldunit, *lineunit;

    gwy_data_field_area_func_lame_uncertainty(data_field, uncz_field, target_line,
                                  &gwy_data_line_acf_uncertainty,
                                  col, row, width, height,
                                  orientation);

    fieldunit = gwy_data_field_get_si_unit_xy(data_field);
    lineunit = gwy_data_line_get_si_unit_x(target_line);
    gwy_serializable_clone(G_OBJECT(fieldunit), G_OBJECT(lineunit));
    lineunit = gwy_data_line_get_si_unit_y(target_line);
    gwy_si_unit_power(gwy_data_field_get_si_unit_z(data_field), 2, lineunit);
}
*/
/**
 * gwy_data_field_acf_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @target_line: A data line to store the uncertainties of the distribution to. 
 * @orientation: Orientation of lines (ACF is simply averaged over the
 *               other orientation).
 *
 * Calculates uncertainty of one-dimensional autocorrelation function of a data field.
 **/
/*
void
gwy_data_field_acf_uncertainty(GwyDataField *data_field,
                               GwyDataField *uncz_field,
                               GwyDataLine *target_line,
                               GwyOrientation orientation,
                               GwyInterpolationType interpolation)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(uncz_field));
    gwy_data_field_area_acf_uncertainty(data_field,uncz_field, target_line, 
                                        0, 0, data_field->xres, data_field->yres,
                            orientation);
}
*/
/**
 * gwy_data_field_area_hhcf_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @orientation: Orientation of lines (HHCF is simply averaged over the
 *               other orientation).
 *
 * Calculates uncertainty of the one-dimensional autocorrelation function of a rectangular part of
 * a data field.
 **/
/*
void
gwy_data_field_area_hhcf_uncertainty(GwyDataField *data_field,
                                     GwyDataLine *uncz_field,
                                     GwyDataLine *target_line,
                                     gint col, gint row,
                                     gint width, gint height,
                                     GwyOrientation orientation)
{
    GwySIUnit *fieldunit, *lineunit;

    gwy_data_field_area_func_lame(data_field, uncz_field, target_line,
                                  &gwy_data_line_hhcf,
                                  col, row, width, height,
                                  orientation);

    fieldunit = gwy_data_field_get_si_unit_xy(data_field);
    lineunit = gwy_data_line_get_si_unit_x(target_line);
    gwy_serializable_clone(G_OBJECT(fieldunit), G_OBJECT(lineunit));
    lineunit = gwy_data_line_get_si_unit_y(target_line);
    gwy_si_unit_power(gwy_data_field_get_si_unit_z(data_field), 2, lineunit);
}
*/
/**
 * gwy_data_field_hhcf_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @orientation: Orientation of lines (HHCF is simply averaged over the
 *               other orientation).
 *
 * Calculates uncertainty of one-dimensional autocorrelation function of a data field.
 **/
/*
void
gwy_data_field_hhcf_uncertainty(GwyDataField *data_field,
                                GwyDataField *uncz_field,
                                GwyDataLine *target_line,
                                GwyOrientation orientation)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_hhcf_uncertainty(data_field,uncz_field, target_line,
                                         0, 0, data_field->xres, data_field->yres,
                                         orientation);
}
*/
/**
 * square_area1_uncertainty:
 * @z1: Z-value in first corner.
 * @z2: Z-value in second corner.
 * @z3: Z-value in third corner.
 * @z4: Z-value in fourth corner.
 * @uz1: uncertainty of Z-value in first corner.
 * @uz2: uncertainty of Z-value in second corner.
 * @uz3: uncertainty of Z-value in third corner.
 * @uz4: uncertainty of Z-value in fourth corner.
 * @ux1: uncertainty of X-value in first corner.
 * @ux2:  uncertainty of X-value in second corner.
 * @ux3:  uncertainty  X-value in third corner.
 * @ux4:   uncertainty X-value in fourth corner.
 * @uy1: uncertainty of Y-value in first corner.
 * @uy2:  uncertainty of Y-value in second corner.
 * @uy3:  uncertainty  Y-value in third corner.
 * @uy4:   uncertainty Y-value in fourth corner.
 * @q: One fourth of rectangle projected area (x-size * ysize).
 *
 *
 * Calculates square of uncertainty of approximate area of a one square pixel.
 *
 * Returns: The uncertainty squared of the area.
 **/
/*
static inline gdouble
square_area1_uncertainty(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                         gdouble uz1, gdouble uz2, gdouble uz3, gdouble uz4,
                         gdouble ux1, gdouble ux2, gdouble ux3, gdouble ux4,
                         gdouble uy1, gdouble uy2, gdouble uy3, gdouble uy4,
                         gdouble q)
{
    gdouble c;
    gdouble a12,a23,a34,a41;
    gdouble sum=0;


    c = (z1 + z2 + z3 + z4)/4.0;
    z1 -= c;
    z2 -= c;
    z3 -= c;
    z4 -= c;

    a12 = sqrt(1.0 + 2.0*(z1*z1 + z2*z2)/q);
    a23 = sqrt(2.0 + 2.0*(z2*z2 + z3*z3)/q);
    a34 = sqrt(3.0 + 2.0*(z3*z3 + z4*z4)/q);
    a41 = sqrt(1.0 + 2.0*(z4*z4 + z1*z1)/q);

    // z contribution
    // A12
    sum += 0.25/(a12*a12) *(
		    (3*z1-z2)*(3*z1-z2)*uz1*uz1+
		    (3*z2-z1)*(3*z2-z1)*uz2*uz2+
		    (z1+z2)*(z1+z2)*(uz3*uz3+uz4*uz4));
    // A23
    sum += 0.25/(a23*a23) *(
		    (3*z2-z3)*(3*z2-z3)*uz2*uz2+
		    (3*z3-z2)*(3*z3-z2)*uz3*uz3+
		    (z2+z3)*(z2+z3)*(uz1*uz1+uz4*uz4));
    // A34
    sum += 0.25/(a34*a34) *(
		    (3*z3-z4)*(3*z3-z4)*uz3*uz3+
		    (3*z4-z3)*(3*z4-z3)*uz4*uz4+
		    (z3+z4)*(z3+z4)*(uz1*uz1+uz2*uz2));
    // A41
    sum += 0.25/(a41*a41) *(
		    (3*z4-z1)*(3*z4-z1)*uz4*uz4+
		    (3*z1-z4)*(3*z1-z4)*uz1*uz1+
		    (z4+z1)*(z4+z1)*(uz2*uz2+uz3*uz3));
    sum *= q*q/16;

    // x contribution
    // A12
    hlp1 = -2-2*(c-2*z2)*(c-z1-z2)/q;
    hlp2 = 2+2*(c-2*z1)*(c-z1-z2)/q;
    hlp2 = -4*(z2-z1)*(c-z1-z2)/q;
    sum += 1./(64*area12*area12)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*ux1*ux1+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*ux2*ux2+
		    hlp3*hlp3*(uz3*uz3+uz4*uz4));

    // A23
    hlp1 = -2 + 2*(z3-z2)*(c-2*z3)/q;
    hlp2 = -2 - 2*(z3-z2)*(c-2*z2)/q;
    hlp3 = 4 + 4*(z3-z2)*(z3-z2)/q;
    sum += 1./(64*area23*area23)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy2*uy2+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy3*uy3+
		    hlp3*hlp3*(uy1*uy1+uy4*uy4));

    // A34
    hlp1 = -2-2*(c-2*z4)*(c-z3-z4)/q;
    hlp2 = 2+2*(c-2*z3)*(c-z3-z4)/q;
    hlp2 = -4*(z4-z2)*(c-z3-z4)/q;
    sum += 1./(64*area34*area34)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*ux3*ux3+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*ux4*ux4+
		    hlp3*hlp3*(uz1*uz1+uz2*uz2));

    // A41
    hlp1 = -2 + 2*(z1-z4)*(c-2*z1)/q;
    hlp2 = -2 - 2*(z1-z4)*(c-2*z4)/q;
    hlp3 = 4 + 4*(z1-z4)*(z1-z4)/q;
    sum += 1./(64*area41*area41)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy4*uy4+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy1*uy1+
		    hlp3*hlp3*(uy2*uy2+uy3*uy3));

    // y contribution
    // A12
    hlp1 = -2 + 2*(z2-z1)*(c-2*z2)/q;
    hlp2 = -2 - 2*(z2-z1)*(c-2*z1)/q;
    hlp3 = 4 + 4*(z2-z1)*(z2-z1)/q;
    sum += 1./(64*area12*area12)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy1*uy1+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy2*uy2+
		    hlp3*hlp3*(uy3*uy3+uy4*uy4));

    // A23
    hlp1 = -2-2*(c-2*z3)*(c-z2-z3)/q;
    hlp2 = 2+2*(c-2*z2)*(c-z2-z3)/q;
    hlp2 = -4*(z3-z2)*(c-z2-z3)/q;
    sum += 1./(64*area23*area23)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy2*uy2+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy3*uy3+
		    hlp3*hlp3*(uy1*uy1+uy4*uy4));

    // A34
    hlp1 = -2 + 2*(z4-z3)*(c-2*z4)/q;
    hlp2 = -2 - 2*(z4-z3)*(c-2*z3)/q;
    hlp3 = 4 + 4*(z4-z3)*(z4-z3)/q;
    sum += 1./(64*area34*area34)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy3*uy3+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy4*uy4+
		    hlp3*hlp3*(uy1*uy1+uy2*uy2));

    // A41
    hlp1 = -2-2*(c-2*z1)*(c-z4-z1)/q;
    hlp2 = 2+2*(c-2*z4)*(c-z4-z1)/q;
    hlp2 = -4*(z1-z4)*(c-z4-z1)/q;
    sum += 1./(64*area41*area41)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy4*uy4+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy1*uy1+
		    hlp3*hlp3*(uy2*uy2+uy3*uy3));
	
    return sum;
}
*/
/**
 * square_area1w_uncertainty:
 * @z1: Z-value in first corner.
 * @z2: Z-value in second corner.
 * @z3: Z-value in third corner.
 * @z4: Z-value in fourth corner.
 * @uz1: uncertainty of Z-value in first corner.
 * @uz2:  uncertainty of Z-value in second corner.
 * @uz3:  uncertainty  Z-value in third corner.
 * @uz4:   uncertainty Z-value in fourth corner.
 * @ux1: uncertainty of X-value in first corner.
 * @ux2:  uncertainty of X-value in second corner.
 * @ux3:  uncertainty  X-value in third corner.
 * @ux4:   uncertainty X-value in fourth corner.
 * @uy1: uncertainty of Y-value in first corner.
 * @uy2:  uncertainty of Y-value in second corner.
 * @uy3:  uncertainty  Y-value in third corner.
 * @uy4:   uncertainty Y-value in fourth corner.
 * @w1: Weight of first corner (0 or 1).
 * @w2: Weight of second corner (0 or 1).
 * @w3: Weight of third corner (0 or 1).
 * @w4: Weight of fourth corner (0 or 1).
 * @q: One fourth of rectangle projected area (x-size * ysize).
 *
 * Calculates the uncertainty of the approximate area of a one square pixel with some corners possibly
 * missing.
 *
 * Returns: The uncertainty of the area squared.
 **/
/*
static inline gdouble
square_area1w_uncertainty(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                          gdouble uz1, gdouble uz2, gdouble uz3, gdouble uz4,
                          gdouble ux1, gdouble ux2, gdouble ux3, gdouble ux4,
                          gdouble uy1, gdouble uy2, gdouble uy3, gdouble uy4,
                          gint w1, gint w2, gint w3, gint w4,
                          gdouble q)
{
    gdouble c;
    gdouble a12,a23,a34,a41;
    gdouble sum=0;


    c = (z1 + z2 + z3 + z4)/4.0;
    z1 -= c;
    z2 -= c;
    z3 -= c;
    z4 -= c;

    a12 = sqrt(1.0 + 2.0*(z1*z1 + z2*z2)/q);
    a23 = sqrt(2.0 + 2.0*(z2*z2 + z3*z3)/q);
    a34 = sqrt(3.0 + 2.0*(z3*z3 + z4*z4)/q);
    a41 = sqrt(1.0 + 2.0*(z4*z4 + z1*z1)/q);

    // A12
    sum += (w1 + w2)*(w1 + w2)*0.25/(a12*a12) *(
		    (3*z1-z2)*(3z1-z2)*uz1*uz1+
		    (3*z2-z1)*(3*z2-z1)*uz2*uz2+
		    (z1+z2)*(z1+z2)*(uz3*uz3+uz4*uz4));
    // A23
    sum += (w2 + w3)*(w2 + w3)*0.25/(a23*a23) *(
		    (3*z2-z3)*(3*z2-z3)*uz2*uz2+
		    (3*z3-z2)*(3*z3-z2)*uz3*uz3+
		    (z2+z3)*(z2+z3)*(uz1*uz1+uz4*uz4));
    // A34
    sum += (w3 + w4)*(w3 + w4)*0.25/(a34*a34) *(
		    (3*z3-z4)*(3*z3-z4)*uz3*uz3+
		    (3*z4-z3)*(3*z4-z3)*uz4*uz4+
		    (z3+z4)*(z3+z4)*(uz1*uz1+uz2*uz2));
    // A41
    sum += (w1 + w4)*0.25/(a41*a41) *(
		    (3*z4-z1)*(3*z4-z1)*uz4*uz4+
		    (3*z1-z4)*(3*z1-z4)*uz1*uz1+
		    (z4+z1)*(z4+z1)*(uz2*uz2+uz3*uz3));
    
    sum *= q*q/16;
 // x contribution
    // A12
    hlp1 = -2-2*(c-2*z2)*(c-z1-z2)/q;
    hlp2 = 2+2*(c-2*z1)*(c-z1-z2)/q;
    hlp2 = -4*(z2-z1)*(c-z1-z2)/q;
    sum += (w1+w2)*(w1+w2)/(64*area12*area12)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*ux1*ux1+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*ux2*ux2+
		    hlp3*hlp3*(uz3*uz3+uz4*uz4));

    // A23
    hlp1 = -2 + 2*(z3-z2)*(c-2*z3)/q;
    hlp2 = -2 - 2*(z3-z2)*(c-2*z2)/q;
    hlp3 = 4 + 4*(z3-z2)*(z3-z2)/q;
    sum += (w2+w3)*(w2+w3)/(64*area23*area23)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy2*uy2+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy3*uy3+
		    hlp3*hlp3*(uy1*uy1+uy4*uy4));

    // A34
    hlp1 = -2-2*(c-2*z4)*(c-z3-z4)/q;
    hlp2 = 2+2*(c-2*z3)*(c-z3-z4)/q;
    hlp2 = -4*(z4-z2)*(c-z3-z4)/q;
    sum += (w3+w4)*(w3+w4)/(64*area34*area34)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*ux3*ux3+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*ux4*ux4+
		    hlp3*hlp3*(uz1*uz1+uz2*uz2));

    // A41
    hlp1 = -2 + 2*(z1-z4)*(c-2*z1)/q;
    hlp2 = -2 - 2*(z1-z4)*(c-2*z4)/q;
    hlp3 = 4 + 4*(z1-z4)*(z1-z4)/q;
    sum += (w4+w1)*(w4+w1)/(64*area41*area41)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy4*uy4+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy1*uy1+
		    hlp3*hlp3*(uy2*uy2+uy3*uy3));

    // y contribution
    // A12
    hlp1 = -2 + 2*(z2-z1)*(c-2*z2)/q;
    hlp2 = -2 - 2*(z2-z1)*(c-2*z1)/q;
    hlp3 = 4 + 4*(z2-z1)*(z2-z1)/q;
    sum += (w1+w2)*(w1+w2)/(64*area12*area12)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy1*uy1+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy2*uy2+
		    hlp3*hlp3*(uy3*uy3+uy4*uy4));

    // A23
    hlp1 = -2-2*(c-2*z3)*(c-z2-z3)/q;
    hlp2 = 2+2*(c-2*z2)*(c-z2-z3)/q;
    hlp2 = -4*(z3-z2)*(c-z2-z3)/q;
    sum += (w2+w3)*(w2+w3)/(64*area23*area23)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy2*uy2+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy3*uy3+
		    hlp3*hlp3*(uy1*uy1+uy4*uy4));

    // A34
    hlp1 = -2 + 2*(z4-z3)*(c-2*z4)/q;
    hlp2 = -2 - 2*(z4-z3)*(c-2*z3)/q;
    hlp3 = 4 + 4*(z4-z3)*(z4-z3)/q;
    sum += (w3+w4)*(w3+w4)/(64*area34*area34)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy3*uy3+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy4*uy4+
		    hlp3*hlp3*(uy1*uy1+uy2*uy2));

    // A41
    hlp1 = -2-2*(c-2*z1)*(c-z4-z1)/q;
    hlp2 = 2+2*(c-2*z4)*(c-z4-z1)/q;
    hlp2 = -4*(z1-z4)*(c-z4-z1)/q;
    sum += (w4+w1)*(w4+w1)/(64*area41*area41)*q*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy4*uy4+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy1*uy1+
		    hlp3*hlp3*(uy2*uy2+uy3*uy3));
	
    return sum;

}
*/
/**
 * square_area2_uncertainty:
 * @z1: Z-value in first corner.
 * @z2: Z-value in second corner.
 * @z3: Z-value in third corner.
 * @z4: Z-value in fourth corner.
 * @uz1: uncertainty of Z-value in first corner.
 * @uz2: uncertainty of Z-value in second corner.
 * @uz3: uncertainty of Z-value in third corner.
 * @uz4: uncertainty of Z-value in fourth corner.
 * @ux1: uncertainty of X-value in first corner.
 * @ux2:  uncertainty of X-value in second corner.
 * @ux3:  uncertainty  X-value in third corner.
 * @ux4:   uncertainty X-value in fourth corner.
 * @uy1: uncertainty of Y-value in first corner.
 * @uy2:  uncertainty of Y-value in second corner.
 * @uy3:  uncertainty  Y-value in third corner.
 * @uy4:   uncertainty Y-value in fourth corner.
 * @x: One fourth of square of rectangle width (x-size).
 * @y: One fourth of square of rectangle height (y-size).
 *
 * Calculates uncertainty of approximate area of a one general rectangular pixel.
 *
 * Returns: The uncertainty of the area squared.
 **/
/*
static inline gdouble
square_area2_uncertainty(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                         gdouble uz1, gdouble uz2, gdouble uz3, gdouble uz4,
                         gdouble ux1, gdouble ux2, gdouble ux3, gdouble ux4,
                         gdouble uy1, gdouble uy2, gdouble uy3, gdouble uy4,
                         gdouble x, gdouble y)
{
    gdouble c;
    gdouble a12,a23,a34,a41;
    gdouble sum=0;
    gdouble hlp1,hlp2,hlp3,hlp4;

    c = (z1 + z2 + z3 + z4)/2.0;

    a12 = sqrt(1.0 + (z1 - z2)*(z1 - z2)/x  + (z1 + z2 - c)*(z1 + z2 - c)/y);
    a23 = sqrt(1.0 + (z2 - z3)*(z2 - z3)/y  + (z2 + z3 - c)*(z2 + z3 - c)/x);
    a34 = sqrt(1.0 + (z3 - z4)*(z3 - z4)/x  + (z3 + z4 - c)*(z3 + z4 - c)/y);
    a41 = sqrt(1.0 + (z1 - z4)*(z1 - z4)/y  + (z1 + z4 - c)*(z1 + z4 - c)/x);

    // z contribution
    // A12
    hlp1 = ((z1 - z2)/x + 0.5* (z1+z2 - c)/y);
    hlp2 = ((z2 - z1)/x + 0.5* (z1+z2 - c)/y);
    hlp3 = -0.5* (z1+z2 - c)/y;
    sum += 1/(a12*a12) *(
		    hlp1*hlp1*uz1*uz1+
		    hlp2*hlp2*uz2*uz2+
		    hlp3*hlp3*(uz3*uz3+uz4*uz4));

    // A23
    hlp1 = ((z2 - z3)/y + 0.5* (z2+z3 - c)/x);
    hlp2 = ((z3 - z2)/y + 0.5* (z2+z3 - c)/x);
    hlp3 = -0.5* (z2+z3 - c)/x;
    sum += 1/(a23*a23) *(
		    hlp1*hlp1*uz2*uz2+
		    hlp2*hlp2*uz3*uz3+
		    hlp3*hlp3*(uz4*uz4+uz1*uz1));

    // A34
    hlp1 = ((z3 - z4)/x + 0.5* (z3+z4 - c)/y);
    hlp2 = ((z4 - z3)/x + 0.5* (z3+z4 - c)/y);
    hlp3 = -0.5* (z3+z4 - c)/y;
    sum += 1/(a34*a34) *(
		    hlp1*hlp1*uz3*uz3+
		    hlp2*hlp2*uz4*uz4+
		    hlp3*hlp3*(uz1*uz1+uz2*uz2));
    // A41
    hlp1 = ((z4 - z1)/y + 0.5* (z4+z1 - c)/x);
    hlp2 = ((z1 - z4)/y + 0.5* (z4+z1 - c)/x);
    hlp3 = -0.5* (z4+z1 - c)/x;
    sum += 1/(a41*a41) *(
		    hlp1*hlp1*uz4*uz4+
		    hlp2*hlp2*uz1*uz1+
		    hlp3*hlp3*(uz2*uz2+uz3*uz3));

    sum *= x*y/14;

    // x contribution
    // A12
    hlp1 = -2-2*(c-2*z2)*(c-z1-z2)/y;
    hlp2 = 2+2*(c-2*z1)*(c-z1-z2)/y;
    hlp2 = -4*(z2-z1)*(c-z1-z2)/y;
    sum += 1./(64*area12*area12)*y*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*ux1*ux1+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*ux2*ux2+
		    hlp3*hlp3*(uz3*uz3+uz4*uz4));

    // A23
    hlp1 = -2 + 2*(z3-z2)*(c-2*z3)/y;
    hlp2 = -2 - 2*(z3-z2)*(c-2*z2)/y;
    hlp3 = 4 + 4*(z3-z2)*(z3-z2)/y;
    sum += 1./(64*area23*area23)*y*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy2*uy2+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy3*uy3+
		    hlp3*hlp3*(uy1*uy1+uy4*uy4));

    // A34
    hlp1 = -2-2*(c-2*z4)*(c-z3-z4)/y;
    hlp2 = 2+2*(c-2*z3)*(c-z3-z4)/y;
    hlp2 = -4*(z4-z2)*(c-z3-z4)/y;
    sum += 1./(64*area34*area34)*y*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*ux3*ux3+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*ux4*ux4+
		    hlp3*hlp3*(uz1*uz1+uz2*uz2));

    // A41
    hlp1 = -2 + 2*(z1-z4)*(c-2*z1)/y;
    hlp2 = -2 - 2*(z1-z4)*(c-2*z4)/y;
    hlp3 = 4 + 4*(z1-z4)*(z1-z4)/y;
    sum += 1./(64*area41*area41)*y*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy4*uy4+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy1*uy1+
		    hlp3*hlp3*(uy2*uy2+uy3*uy3));

    // y contribution
    // A12
    hlp1 = -2 + 2*(z2-z1)*(c-2*z2)/x;
    hlp2 = -2 - 2*(z2-z1)*(c-2*z1)/x;
    hlp3 = 4 + 4*(z2-z1)*(z2-z1)/x;
    sum += 1./(64*area12*area12)*x*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy1*uy1+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy2*uy2+
		    hlp3*hlp3*(uy3*uy3+uy4*uy4));

    // A23
    hlp1 = -2-2*(c-2*z3)*(c-z2-z3)/x;
    hlp2 = 2+2*(c-2*z2)*(c-z2-z3)/x;
    hlp2 = -4*(z3-z2)*(c-z2-z3)/x;
    sum += 1./(64*area23*area23)*x*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy2*uy2+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy3*uy3+
		    hlp3*hlp3*(uy1*uy1+uy4*uy4));

    // A34
    hlp1 = -2 + 2*(z4-z3)*(c-2*z4)/x;
    hlp2 = -2 - 2*(z4-z3)*(c-2*z3)/x;
    hlp3 = 4 + 4*(z4-z3)*(z4-z3)/x;
    sum += 1./(64*area34*area34)*x*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy3*uy3+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy4*uy4+
		    hlp3*hlp3*(uy1*uy1+uy2*uy2));

    // A41
    hlp1 = -2-2*(c-2*z1)*(c-z4-z1)/x;
    hlp2 = 2+2*(c-2*z4)*(c-z4-z1)/x;
    hlp2 = -4*(z1-z4)*(c-z4-z1)/x;
    sum += 1./(64*area41*area41)*x*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy4*uy4+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy1*uy1+
		    hlp3*hlp3*(uy2*uy2+uy3*uy3));
	
    return sum;
}
*/
/**
 * square_area2w_uncertainty:
 * @z1: Z-value in first corner.
 * @z2: Z-value in second corner.
 * @z3: Z-value in third corner.
 * @z4: Z-value in fourth corner.
 * @uz1: uncertainty of Z-value in first corner.
 * @uz2:  uncertainty of Z-value in second corner.
 * @uz3:  uncertainty  Z-value in third corner.
 * @uz4:   uncertainty Z-value in fourth corner.
 * @ux1: uncertainty of X-value in first corner.
 * @ux2:  uncertainty of X-value in second corner.
 * @ux3:  uncertainty  X-value in third corner.
 * @ux4:   uncertainty X-value in fourth corner.
 * @uy1: uncertainty of Y-value in first corner.
 * @uy2:  uncertainty of Y-value in second corner.
 * @uy3:  uncertainty  Y-value in third corner.
 * @uy4:   uncertainty Y-value in fourth corner.
 * @w1: Weight of first corner (0 or 1).
 * @w2: Weight of second corner (0 or 1).
 * @w3: Weight of third corner (0 or 1).
 * @w4: Weight of fourth corner (0 or 1).
 * @x: One fourth of square of rectangle width (x-size).
 * @y: One fourth of square of rectangle height (y-size).
 *
 * Calculates the uncertainty of the approximate area of a one general rectangular pixel with some
 * corners possibly missing.
 *
 * Returns: The uncertainty of the area squared.
 **/
/*
static inline gdouble
square_area2w_uncertainty(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                          gdouble uz1, gdouble uz2, gdouble uz3, gdouble uz4,
                          gdouble ux1, gdouble ux2, gdouble ux3, gdouble ux4,
                          gdouble uy1, gdouble uy2, gdouble uy3, gdouble uy4,
                          gint w1, gint w2, gint w3, gint w4,
                          gdouble x, gdouble y)
{
    gdouble c;
    gdouble a12,a23,a34,a41;
    gdouble sum=0;
    gdouble hlp1,hlp2,hlp3,hlp4;

    c = (z1 + z2 + z3 + z4)/2.0;

    a12 = sqrt(1.0 + (z1 - z2)*(z1 - z2)/x  + (z1 + z2 - c)*(z1 + z2 - c)/y);
    a23 = sqrt(1.0 + (z2 - z3)*(z2 - z3)/y  + (z2 + z3 - c)*(z2 + z3 - c)/x);
    a34 = sqrt(1.0 + (z3 - z4)*(z3 - z4)/x  + (z3 + z4 - c)*(z3 + z4 - c)/y);
    a41 = sqrt(1.0 + (z1 - z4)*(z1 - z4)/y  + (z1 + z4 - c)*(z1 + z4 - c)/x);

    // z contribution
    // A12
    hlp1 = ((z1 - z2)/x + 0.5* (z1+z2 - c)/y);
    hlp2 = ((z2 - z1)/x + 0.5* (z1+z2 - c)/y);
    hlp3 = -0.5* (z1+z2 - c)/y;
    sum += (w1+w2)*(w1+w2)/(a12*a12) *(
		    hlp1*hlp1*uz1*uz1+
		    hlp2*hlp2*uz2*uz2+
		    hlp3*hlp3*(uz3*uz3+uz4*uz4));

    // A23
    hlp1 = ((z2 - z3)/y + 0.5* (z2+z3 - c)/x);
    hlp2 = ((z3 - z2)/y + 0.5* (z2+z3 - c)/x);
    hlp3 = -0.5* (z2+z3 - c)/x;
    sum += (w2+w3)*(w2+w3)/(a23*a23) *(
		    hlp1*hlp1*uz2*uz2+
		    hlp2*hlp2*uz3*uz3+
		    hlp3*hlp3*(uz4*uz4+uz1*uz1));

    // A34
    hlp1 = ((z3 - z4)/x + 0.5* (z3+z4 - c)/y);
    hlp2 = ((z4 - z3)/x + 0.5* (z3+z4 - c)/y);
    hlp3 = -0.5* (z3+z4 - c)/y;
    sum += (w3+w4)*(w3+w4)/(a34*a34) *(
		    hlp1*hlp1*uz3*uz3+
		    hlp2*hlp2*uz4*uz4+
		    hlp3*hlp3*(uz1*uz1+uz2*uz2));
    // A41
    hlp1 = ((z4 - z1)/y + 0.5* (z4+z1 - c)/x);
    hlp2 = ((z1 - z4)/y + 0.5* (z4+z1 - c)/x);
    hlp3 = -0.5* (z4+z1 - c)/x;
    sum += (w4+w1)*(w4+w1)/(a41*a41) *(
		    hlp1*hlp1*uz4*uz4+
		    hlp2*hlp2*uz1*uz1+
		    hlp3*hlp3*(uz2*uz2+uz3*uz3));
    
    sum *= q*q/16;

    // x contribution
    // A12
    hlp1 = -2-2*(c-2*z2)*(c-z1-z2)/y;
    hlp2 = 2+2*(c-2*z1)*(c-z1-z2)/y;
    hlp2 = -4*(z2-z1)*(c-z1-z2)/y;
    sum += (w1+w2)*(w1+w2)/(64*area12*area12)*y*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*ux1*ux1+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*ux2*ux2+
		    hlp3*hlp3*(uz3*uz3+uz4*uz4));

    // A23
    hlp1 = -2 + 2*(z3-z2)*(c-2*z3)/y;
    hlp2 = -2 - 2*(z3-z2)*(c-2*z2)/y;
    hlp3 = 4 + 4*(z3-z2)*(z3-z2)/y;
    sum += (w2+w3)*(w2+w3)/(64*area23*area23)*y*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy2*uy2+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy3*uy3+
		    hlp3*hlp3*(uy1*uy1+uy4*uy4));

    // A34
    hlp1 = -2-2*(c-2*z4)*(c-z3-z4)/y;
    hlp2 = 2+2*(c-2*z3)*(c-z3-z4)/y;
    hlp2 = -4*(z4-z2)*(c-z3-z4)/y;
    sum += (w3+w4)*(w3+w4)/(64*area34*area34)*y*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*ux3*ux3+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*ux4*ux4+
		    hlp3*hlp3*(uz1*uz1+uz2*uz2));

    // A41
    hlp1 = -2 + 2*(z1-z4)*(c-2*z1)/y;
    hlp2 = -2 - 2*(z1-z4)*(c-2*z4)/y;
    hlp3 = 4 + 4*(z1-z4)*(z1-z4)/y;
    sum += (w4+w1)*(w4+w1)/(64*area41*area41)*y*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy4*uy4+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy1*uy1+
		    hlp3*hlp3*(uy2*uy2+uy3*uy3));

    // y contribution
    // A12
    hlp1 = -2 + 2*(z2-z1)*(c-2*z2)/x;
    hlp2 = -2 - 2*(z2-z1)*(c-2*z1)/x;
    hlp3 = 4 + 4*(z2-z1)*(z2-z1)/x;
    sum += (w1+w2)*(w1+w2)/(64*area12*area12)*x*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy1*uy1+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy2*uy2+
		    hlp3*hlp3*(uy3*uy3+uy4*uy4));

    // A23
    hlp1 = -2-2*(c-2*z3)*(c-z2-z3)/x;
    hlp2 = 2+2*(c-2*z2)*(c-z2-z3)/x;
    hlp2 = -4*(z3-z2)*(c-z2-z3)/x;
    sum += (w2+w3)*(w2+w3)/(64*area23*area23)*x*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy2*uy2+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy3*uy3+
		    hlp3*hlp3*(uy1*uy1+uy4*uy4));

    // A34
    hlp1 = -2 + 2*(z4-z3)*(c-2*z4)/x;
    hlp2 = -2 - 2*(z4-z3)*(c-2*z3)/x;
    hlp3 = 4 + 4*(z4-z3)*(z4-z3)/x;
    sum += (w3+w4)*(w3+w4)/(64*area34*area34)*x*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy3*uy3+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy4*uy4+
		    hlp3*hlp3*(uy1*uy1+uy2*uy2));

    // A41
    hlp1 = -2-2*(c-2*z1)*(c-z4-z1)/x;
    hlp2 = 2+2*(c-2*z4)*(c-z4-z1)/x;
    hlp2 = -4*(z1-z4)*(c-z4-z1)/x;
    sum += (w4+w1)*(w4+w1)/(64*area41*area41)*x*(
		    (hlp1+0.25*hlp3)*(hlp1+0.25*hlp3)*uy4*uy4+
		    (hlp2+0.25*hlp3)*(hlp2+0.25*hlp3)*uy1*uy1+
		    hlp3*hlp3*(uy2*uy2+uy3*uy3));
	
    return sum;

}
*/
/**
 * stripe_area1_uncertainty:
 * @n: The number of values in @r, @rr, @m.
 * @stride: Stride in @r, @rr, @m.
 * @r: Array of @n z-values of vertices, this row of vertices is considered
 *     inside.
 * @rr: Array of @n z-values of vertices, this row of vertices is considered
 *      outside.
 * @uz: Array of @n uz-values of uncertainties of vertices, this row of vertices is 
 * considered  inside.
 * @uuz: Array of @n uz-values of  uncertainties of vertices, this row of vertices is considered
 *      outside.
 * @ux: Array of @n ux-values of uncertainties of vertices, this row of vertices is 
 * considered  inside.
 * @uux: Array of @n ux-values of  uncertainties of vertices, this row of vertices is considered
 *      outside.
 * @uy: Array of @n uy-values of uncertainties of vertices, this row of vertices is 
 * considered  inside.
 * @uuy: Array of @n uy-values of  uncertainties of vertices, this row of vertices is considered
 *      outside.
 * @m: Mask for @r (@rr does not need mask since it has zero weight by
 *     definition), or %NULL to sum over all @r vertices.
 * @mode: Masking mode.
 * @q: One fourth of rectangle projected area (x-size * ysize).
 *
 * Calculates uncertainty of the approximate area of a half-pixel stripe.
 *
 * Returns: The uncertainty of the area squared.
 **/
/*
static gdouble
stripe_area1_uncertainty(gint n,
                         gint stride,
                         const gdouble *r,
                         const gdouble *rr,
                         const gdouble *uz,
                         const gdouble *uuz,
                         const gdouble *m,
                         GwyMaskingType mode,
                         gdouble q)
{
    gdouble sum = 0.0;
    gint j;

    if (m && mode != GWY_MASK_IGNORE) {
        if (mode == GWY_MASK_INCLUDE) {
            for (j = 0; j < n-1; j++)
                sum += square_area1w_uncertainty(r[j*stride], r[(j + 1)*stride],
                                     rr[(j + 1)*stride], rr[j*stride],
				     uz[j*stride], uz[(j + 1)*stride],
				     uuz[(j + 1)*stride], uuz[j*stride],
				     ux[j*stride], ux[(j + 1)*stride],
				     uux[(j + 1)*stride], uux[j*stride],
				     uy[j*stride], uy[(j + 1)*stride],
				     uuy[(j + 1)*stride], uuy[j*stride],
                                     m[j*stride] > 0.0, m[(j + 1)*stride] > 0.0,
                                     0, 0,
                                     q);
        }
        else {
            for (j = 0; j < n-1; j++)
                sum += square_area1w_uncertainty(r[j*stride], r[(j + 1)*stride],
                                     rr[(j + 1)*stride], rr[j*stride],
				     uz[j*stride], uz[(j + 1)*stride],
				     uuz[(j + 1)*stride], uuz[j*stride],
				     ux[j*stride], ux[(j + 1)*stride],
				     uux[(j + 1)*stride], uux[j*stride],
				     uy[j*stride], uy[(j + 1)*stride],
				     uuy[(j + 1)*stride], uuy[j*stride],
                                     m[j*stride] < 1.0, m[(j + 1)*stride] < 1.0,
                                     0, 0,
                                     q);
        }
    }
    else {
        for (j = 0; j < n-1; j++)
            sum += square_area1w_uncertainty(r[j*stride], r[(j + 1)*stride],
                                 rr[(j + 1)*stride], rr[j*stride],
				 uz[j*stride], uz[(j + 1)*stride],
                                 uuz[(j + 1)*stride], uuz[j*stride],
				 ux[j*stride], ux[(j + 1)*stride],
                                 uux[(j + 1)*stride], uux[j*stride],
				 uy[j*stride], uy[(j + 1)*stride],
                                 uuy[(j + 1)*stride], uuy[j*stride],
                                 1, 1, 0, 0,
                                 q);
    }

    return sum;
}
*/
/**
 * stripe_area2_uncertainty:
 * @n: The number of values in @r, @rr, @m.
 * @stride: Stride in @r, @rr, @m.
 * @r: Array of @n z-values of vertices, this row of vertices is considered
 *     inside.
 * @rr: Array of @n z-values of vertices, this row of vertices is considered
 *      outside.
 * @uz: Array of @n uz-values of uncertainties of vertices, this row of vertices is 
 * considered  inside.
 * @uuz: Array of @n uz-values of  uncertainties of vertices, this row of vertices is considered
 *      outside.
 * @ux: Array of @n ux-values of uncertainties of vertices, this row of vertices is 
 * considered  inside.
 * @uux: Array of @n ux-values of  uncertainties of vertices, this row of vertices is considered
 *      outside.
 * @uy: Array of @n uy-values of uncertainties of vertices, this row of vertices is 
 * considered  inside.
 * @uuy: Array of @n uy-values of  uncertainties of vertices, this row of vertices is considered
 *      outside.
 * @m: Mask for @r (@rr does not need mask since it has zero weight by
 *     definition), or %NULL to sum over all @r vertices.
 * @x: One fourth of square of rectangle width (x-size).
 * @y: One fourth of square of rectangle height (y-size).
 *
 * Calculates uncertainty of approximate area of a half-pixel stripe.
 *
 * Returns: The squared uncertainty of the area.
 **/
/*
static gdouble
stripe_area2_uncertainty(gint n,
                         gint stride,
                         const gdouble *r,
                         const gdouble *rr,
                         const gdouble *uz,
                         const gdouble *uuz,
                         const gdouble *ux,
                         const gdouble *uux,
                         const gdouble *uy,
                         const gdouble *uuy,
                         const gdouble *m,
                         gdouble x,
                         gdouble y)
{
    gdouble sum = 0.0;
    gint j;

    if (m) {
        for (j = 0; j < n-1; j++)
            sum += square_area2w_uncertainty(r[j*stride], r[(j + 1)*stride],
                                 rr[(j + 1)*stride], rr[j*stride],
				     uz[j*stride], uz[(j + 1)*stride],
				     uuz[(j + 1)*stride], uuz[j*stride],
				     ux[j*stride], ux[(j + 1)*stride],
				     uux[(j + 1)*stride], uux[j*stride],
				     uy[j*stride], uy[(j + 1)*stride],
				     uuy[(j + 1)*stride], uuy[j*stride],
                                 m[j*stride] > 0.0, m[(j + 1)*stride] > 0.0,
                                 0, 0,
                                 x, y);
    }
    else {
        for (j = 0; j < n-1; j++)
            sum += square_area2w_uncertainty(r[j*stride], r[(j + 1)*stride],
                                 rr[(j + 1)*stride], rr[j*stride],
				     uz[j*stride], uz[(j + 1)*stride],
				     uuz[(j + 1)*stride], uuz[j*stride],
				     ux[j*stride], ux[(j + 1)*stride],
				     uux[(j + 1)*stride], uux[j*stride],
				     uy[j*stride], uy[(j + 1)*stride],
				     uuy[(j + 1)*stride], uuy[j*stride],
                                 1, 1, 0, 0,
                                 x, y);
    }

    return sum;
}

static gdouble
calculate_surface_area_uncertainty(GwyDataField *dfield,
                                   GwyDataField *unczfield, 
                                   GwyDataField *uncx_field, GwyDataField *uncy_field,
                                   GwyDataField *mask,
                                   GwyMaskingType mode,
                                   gint col, gint row,
                                   gint width, gint height)
{
    const gdouble *r, *m, *dataul, *maskul, *uzul, *uxul, *uyul, *ux, *uz, *uy;
    gint i, j, xres, yres, s;
    gdouble x, y, q, sum = 0.0;

    if (!width || !height)
        return sum;

    xres = dfield->xres;
    yres = dfield->yres;
    x = dfield->xreal/dfield->xres;
    y = dfield->yreal/dfield->yres;
    q = x*y;
    x = x*x;
    y = y*y;
    dataul = dfield->data + xres*row + col;
    uxul = uncx_field->data + xres*row + col;
    uyul = uncy_field->data + xres*row + col;
    uzul = uncz_field->data + xres*row + col;

    if (mask && mode != GWY_MASK_IGNORE) {
        maskul = mask->data + xres*row + col;
        if (fabs(log(x/y)) < 1e-7) {
         //    Inside 
            for (i = 0; i < height-1; i++) {
                r = dataul + xres*i;
                uz = uzul + xres*i;
                ux = uxul + xres*i;
                uy = uyul + xres*i;
                m = maskul + xres*i;
                if (mode == GWY_MASK_INCLUDE) {
                    for (j = 0; j < width-1; j++)
                        sum += square_area1w_uncertainty(r[j], r[j+1],
                                             r[j+xres+1], r[j+xres],
					     uz[j], uz[j+1],
                                             uz[j+xres+1], uz[j+xres],
					     ux[j], ux[j+1],
                                             ux[j+xres+1], ux[j+xres],
					     uy[j], uy[j+1],
                                             uy[j+xres+1], uy[j+xres],
                                             m[j] > 0.0, m[j+1] > 0.0,
                                             m[j+xres+1] > 0.0, m[j+xres] > 0.0,
                                             q);
                }
                else {
                    for (j = 0; j < width-1; j++)
                        sum += square_area1w_uncertainty(r[j], r[j+1],
                                             r[j+xres+1], r[j+xres],
					     uz[j], uz[j+1],
                                             uz[j+xres+1], uz[j+xres],
					     ux[j], ux[j+1],
                                             ux[j+xres+1], ux[j+xres],
					     uy[j], uy[j+1],
                                             uy[j+xres+1], uy[j+xres],
                                             m[j] < 1.0, m[j+1] < 1.0,
                                             m[j+xres+1] < 1.0, m[j+xres] < 1.0,
                                             q);
                }
            }

            // Top row 
            s = !(row == 0);
            sum += stripe_area1_uncertainty(width, 1, dataul, dataul - s*xres,
			    uzul, uzul - s*xres,
			    uxul, uxul - s*xres, uyul, uyul - s*xres,
			    maskul, mode, q);

            // Bottom row 
            s = !(row + height == yres);
            sum += stripe_area1_uncertainty(width, 1,
                                dataul + xres*(height-1),
                                dataul + xres*(height-1 + s),
                                uzul + xres*(height-1),
                                uzul + xres*(height-1 + s),
                                uxul + xres*(height-1),
                                uxul + xres*(height-1 + s),
                                uyul + xres*(height-1),
                                uyul + xres*(height-1 + s),
                                maskul + xres*(height-1), mode, q);

            // Left column 
            s = !(col == 0);
            sum += stripe_area1_uncertainty(height, xres, dataul, dataul - s,
			    uzul, uzul - s,
			    uxul, uxul - s, uyul, uyul - s,
			    maskul, mode, q);
            // Right column 
            s = !(col + width == xres);
            sum += stripe_area1_uncertainty(height, xres,
                                dataul + width-1, dataul + width-1 + s,
                                uzul + width-1, uzul + width-1 + s,
                                uxul + width-1, uxul + width-1 + s,
                                uyul + width-1, uyul + width-1 + s,
                                maskul + width-1, mode, q);
        }
        else {
            // Inside 
            for (i = 0; i < height-1; i++) {
                r = dataul + xres*i;
                m = maskul + xres*i;
		uz = uzul + xres*i;
		ux = uxul + xres*i;
		uy = uyul + xres*i;
                for (j = 0; j < width-1; j++)
                    sum += square_area2w_uncertainty(r[j], r[j+1],
                                         r[j+xres+1], r[j+xres],
					     uz[j], uz[j+1],
                                             uz[j+xres+1], uz[j+xres],
					     ux[j], ux[j+1],
                                             ux[j+xres+1], ux[j+xres],
					     uy[j], uy[j+1],
                                             uy[j+xres+1], uy[j+xres],
                                         m[j] > 0.0, m[j+1] > 0.0,
                                         m[j+xres+1] > 0.0, m[j+xres] > 0.0,
                                         x, y);
            }

            // Top row 
            s = !(row == 0);
            sum += stripe_area2_uncertainty(width, 1, dataul, dataul - s*xres
			    uzul, uzul - s*xres,
			    uxul, uxul - s*xres,
			    uyul, uyul - s*xres,
			    , maskul, x, y);

            // Bottom row 
            s = !(row + height == yres);
            sum += stripe_area2(width, 1,
                                dataul + xres*(height-1),
                                dataul + xres*(height-1 + s),
                                uzul + xres*(height-1),
                                uzul + xres*(height-1 + s),
                                uxul + xres*(height-1),
                                uxul + xres*(height-1 + s),
                                uyul + xres*(height-1),
                                uyul + xres*(height-1 + s),
                                maskul + xres*(height-1),
                                x, y);

            // Left column 
            s = !(col == 0);
            sum += stripe_area2(height, xres, dataul, dataul - s,
			   uzul, uzul - s, 
			   uyul, uyul - s, 
			   uxul, uxul - s, 
			   maskul, y, x);


            // Right column 
            s = !(col + width == xres);
            sum += stripe_area2(height, xres,
                                dataul + width-1, dataul + width-1 + s,
                                uzul + width-1, uzul + width-1 + s,
                                uyul + width-1, uyul + width-1 + s,
                                uxul + width-1, uxul + width-1 + s,
                                maskul + width-1,
                                y, x,);
        }

        // Four corner quater-pixels are flat, so no uncertainty comes from them.  
    }
    else {
        if (fabs(log(x/y)) < 1e-7) {
            // Inside 
            for (i = 0; i < height-1; i++) {
                r = dataul + xres*i;
                for (j = 0; j < width-1; j++)
                    sum += square_area1_uncertainty(r[j], r[j+1], r[j+xres+1], r[j+xres],
				    uz[j], uz[j+1], uz[j+xres+1], uz[j+xres],
				    ux[j], ux[j+1], ux[j+xres+1], ux[j+xres],
				    uy[j], uy[j+1], uy[j+xres+1], uy[j+xres],
                                        q,);
            }

            // Top row 
            s = !(row == 0);
            sum += stripe_area1_uncertainty(width, 1, dataul, dataul - s*xres,
			    uzul, uzul - s*xres,
			    uxul, uxul - s*xres,
			    uyul, uyul - s*xres,
                                NULL, GWY_MASK_IGNORE, q);

            // Bottom row 
            s = !(row + height == yres);
            sum += stripe_area1_uncertainty(width, 1,
                                dataul + xres*(height-1),
                                dataul + xres*(height-1 + s),
                                uzul + xres*(height-1),
                                uzul + xres*(height-1 + s),
                                uxul + xres*(height-1),
                                uxul + xres*(height-1 + s),
                                uyul + xres*(height-1),
                                uyul + xres*(height-1 + s),
                                NULL, GWY_MASK_IGNORE, q);

            // Left column 
            s = !(col == 0);
            sum += stripe_area1_uncertainty(height, xres, dataul, dataul - s,
                                            uzul, uzul - s,
                                            uxul, uxul - s,
                                            uyul, uyul - s,
                                            NULL, GWY_MASK_IGNORE, q);

            // Right column 
            s = !(col + width == xres);
            sum += stripe_area1_uncertainty(height, xres,
                                            dataul + width-1, dataul + width-1 + s,
                                            uzul + width-1, uzul + width-1 + s,
                                            uxul + width-1, uxul + width-1 + s,
                                            uyul + width-1, uyul + width-1 + s,
                                            NULL, GWY_MASK_IGNORE, q);
        }
        else {
            for (i = 0; i < height-1; i++) {
                r = dataul + xres*i;
                for (j = 0; j < width-1; j++)
                    sum += square_area2_uncertainty(r[j], r[j+1], r[j+xres+1], r[j+xres],
				    uz[j], uz[j+1], uz[j+xres+1], u[j+xres],
				    ux[j], ux[j+1], ux[j+xres+1], u[j+xres],
				    uy[j], uy[j+1], uy[j+xres+1], u[j+xres],
                                        x, y);
            }

            // Top row 
            s = !(row == 0);
            sum += stripe_area2_uncertainty(width, 1, dataul, dataul - s*xres,
			   uzul, uzul - s*xres,
			   uxul, uxul - s*xres,
			   uyul, uyul - s*xres,
			   NULL, x, y);

            // Bottom row 
            s = !(row + height == yres);
            sum += stripe_area2(width, 1,
                                dataul + xres*(height-1),
                                dataul + xres*(height-1 + s),
                                uzul + xres*(height-1),
                                uzul + xres*(height-1 + s),
                                uxul + xres*(height-1),
                                uxul + xres*(height-1 + s),
                                uyul + xres*(height-1),
                                uyul + xres*(height-1 + s),
                                NULL,
                                x, y);

            // Left column 
            s = !(col == 0);
            sum += stripe_area2_uncertainty(height, xres, dataul, dataul - s,
			   uzul,uzul -s, 
			   uyul,uyul -s, 
			   uxul,uxul -s, 
			   NULL, y, x);

            // Right column 
            s = !(col + width == xres);
            sum += stripe_area2_uncertainty(height, xres,
                                dataul + width-1, dataul + width-1 + s, 
				uzul + width-1, uzul + width-1 + s, NULL,
				uyul + width-1, uyul + width-1 + s, NULL,
				uxul + width-1, uxul + width-1 + s, NULL,
                                y, x);
        }

        // The four corner quater-pixels as flat, so their z uncertainty is 0.  
    }
    return sum;
}
*/
/**
 * gwy_data_field_get_surface_area_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @uncx_field: The uncertainty in the x direction.
 * @uncy_field: The uncertainty in the y direction.
 *
 * Computes uncertainty of surface area of a data field.
 *
 *
 * Returns: uncertainty of surface area
 **/
/*
gdouble
gwy_data_field_get_surface_area_uncertainty(GwyDataField *data_field, 
                                GwyDataField *uncz_field, 
                                GwyDataField *uncx_field, 
                                GwyDataField *uncy_field)
{
    gdouble uarea = 0.0;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), uarea);

    uarea = calculate_surface_area_uncertainty(data_field, uncz_field,uncx_field,
		    uncy_field,
		    NULL,GWY_MASK_IGNORE,
                                  0, 0, data_field->xres, data_field->yres);

    return sqrt(uarea);
}
*/
/**
 * gwy_data_field_area_get_surface_area_mask:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @uncx_field: The uncertainty in the x direction.
 * @uncy_field: The uncertainty in the y direction.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes uncertainty of surface area of a rectangular part of a data field.
 *
 * This function is equivalent to calling
 * @gwy_data_field_area_get_surface_area_mask()
 * with masking mode %GWY_MASK_INCLUDE.
 *
 * Returns: The uncertainty of surface area.
 **/
/*
gdouble
gwy_data_field_area_get_surface_area_uncertainty(GwyDataField *data_field, 
                                     GwyDataField *uncz_field,
                                     GwyDataField *uncx_field, 
                                     GwyDataField uncy_field
                                     GwyDataField *mask,
                                     gint col, gint row,
                                     gint width, gint height)
{
    return sqrt(gwy_data_field_area_get_surface_area_mask_uncertainty(data_field,
			    uncz_field, uncx_field,uncy_field, mask,
                                                     GWY_MASK_INCLUDE,
                                                     col, row, width, height));
}
*/
/**
 * gwy_data_field_area_get_surface_area_mask_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty data field.
 * @uncx_field: The uncertainty in the x direction.
 * @uncy_field: The uncertainty in the y direction.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @mode: Masking mode to use.  See the introduction for description of
 *        masking modes.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes uncertainty of surface area of a rectangular part of a data field.
 *
 * This quantity makes sense only if the lateral dimensions and values of
 * @data_field are the same physical quantities.
 *
 * Returns: The uncertainty of the surface area.
 *
 * Since: 2.18
 **/
/*
gdouble
gwy_data_field_area_get_surface_area_mask_uncertainty(GwyDataField *data_field,
                                                      GwyDataField *uncz_field, 
                                                      GwyDataField *uncx_field, 
                                                      GwyDataField uncy_field,
                                                      GwyDataField *mask,
                                                      GwyMaskingType mode,
                                                      gint col, gint row,
                                                      gint width, gint height)
{
    gdouble uarea = 0.0;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), uarea);
    g_return_val_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                                   && mask->xres == data_field->xres
                                   && mask->yres == data_field->yres), uarea);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= data_field->xres
                         && row + height <= data_field->yres,
                         uarea);

    // The result is the same, but it can be cached. 
    if (!mask
        && row == 0 && col == 0
        && width == data_field->xres && height == data_field->yres)
        return sqrt(gwy_data_field_get_surface_area_uncertainty(data_field,uncz_field,
				uncx_field,uncy_field));

    return sqrt(calculate_surface_area_uncertainty(data_field,uncz_field,
			    uncx_field,uncy_field, 
			    mask, mode,
                                  col, row, width, height));
}

*/

/**
 * gwy_data_field_area_get_median_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty field
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes uncertainty of median value of a data field area.
 *
 * This function is equivalent to calling
 * @gwy_data_field_area_get_median_uncertainty_mask()
 * with masking mode %GWY_MASK_INCLUDE.
 *
 * Returns: The uncertainty of the median value.
 **/

gdouble
gwy_data_field_area_get_median_uncertainty(GwyDataField *dfield,
                                           GwyDataField *uncz_field,
                                           GwyDataField *mask,
                                           gint col, gint row,
                                           gint width, gint height)
{
    return gwy_data_field_area_get_median_uncertainty_mask(dfield, uncz_field, mask, GWY_MASK_INCLUDE,
                                                           col, row, width, height);
}

/**
 * gwy_data_field_area_get_median_uncertainty_mask:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty field
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @mode: Masking mode to use.  See the introduction for description of
 *        masking modes.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes uncertainty of  median value of a data field area.
 *
 * Returns: The uncertainty of the median value.
 *
 * Since: 2.18
 **/

gdouble
gwy_data_field_area_get_median_uncertainty_mask(GwyDataField *dfield,
                                                GwyDataField *uncz_field,
                                                GwyDataField *mask,
                                                GwyMaskingType mode,
                                                gint col, gint row,
                                                gint width, gint height)
{
    gdouble med_unc = G_MAXDOUBLE;
    const gdouble *datapos, *mpos, *uncpos;
    gdouble *buffer, *ubuffer;
    gint i, j;
    guint nn;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), med_unc);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), med_unc);

    g_return_val_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                                   && mask->xres == dfield->xres
                                   && mask->yres == dfield->yres),
                         med_unc);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         med_unc);
    if (!width || !height)
        return med_unc;

    if (mask && mode != GWY_MASK_IGNORE) {
        buffer = g_new(gdouble, width*height);
        ubuffer = g_new(gdouble, width*height);
        datapos = dfield->data + row*dfield->xres + col;
        uncpos = uncz_field->data + row*uncz_field->xres + col;
        mpos = mask->data + row*mask->xres + col;
        nn = 0;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *urow = uncpos + i*uncz_field->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            if (mode == GWY_MASK_INCLUDE) {
                for (j = 0; j < width; j++) {
                    if (*mrow > 0.0) {
                        buffer[nn] = *drow;
                        ubuffer[nn] = *urow;
                        nn++;
                    }
                    drow++;
                    urow++;
                    mrow++;
                }
            }
            else {
                for (j = 0; j < width; j++) {
                    if (*mrow < 1.0) {
                        buffer[nn] = *drow;
                        ubuffer[nn] = *urow;
                        nn++;
                    }
                    drow++;
                    urow++;
                    mrow++;
                }
            }
        }

        if (nn){
		med_unc = gwy_math_median_uncertainty(nn, buffer,ubuffer);
	}

        g_free(buffer);
        g_free(ubuffer);

        return med_unc;
    }

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_median_uncertainty(dfield,uncz_field);

    buffer = g_new(gdouble, width*height);
    ubuffer = g_new(gdouble, width*height);
    datapos = dfield->data + row*dfield->xres + col;
    uncpos = uncz_field->data + row*uncz_field->xres + col;
    if (height == 1 || (col == 0 && width == dfield->xres)){
        memcpy(buffer, datapos, width*height*sizeof(gdouble));
        memcpy(ubuffer, uncpos, width*height*sizeof(gdouble));
    }
    else {
        for (i = 0; i < height; i++){
            memcpy(buffer + i*width, datapos + i*dfield->xres,
                   width*sizeof(gdouble));
            memcpy(ubuffer + i*width, uncpos + i*uncz_field->xres,
                   width*sizeof(gdouble));
	}
    }
    med_unc = gwy_math_median_uncertainty(width*height, buffer,ubuffer);
    g_free(buffer);
    g_free(ubuffer);

    return med_unc;
}

/**
 * gwy_data_field_get_median_uncertainty:
 * @data_field: A data field.
 * @uncz_field: The corresponding uncertainty field
 *
 * Computes uncertainty of median value of a data field.
 *
 *
 * Returns: The uncertainty of the median value.
 **/

gdouble
gwy_data_field_get_median_uncertainty(GwyDataField *data_field, 
                                      GwyDataField *uncz_field)
{
    gint xres,yres;
    gdouble *buffer, *ubuffer;
    gdouble med_unc;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0.0);


    xres = data_field->xres;
    yres = data_field->yres;
    buffer = g_memdup(data_field->data, xres*yres*sizeof(gdouble));
    ubuffer = g_memdup(uncz_field->data, xres*yres*sizeof(gdouble));
    med_unc = gwy_math_median_uncertainty(xres*yres, buffer, ubuffer);

    g_free(buffer);
    g_free(ubuffer);

    return med_unc;
}


/**
 * gwy_data_field_area_get_normal_coeffs_uncertainty:
 * @data_field: A data field.
 * @uncz_field: Corresponding uncertainty field.
 * @uncx: The uncertainty in the x direction.
 * @uncy: The uncertainty in the y direction.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @nx: Where x-component of average normal vector should be stored, or %NULL.
 * @ny: Where y-component of average normal vector should be stored, or %NULL.
 * @nz: Where z-component of average normal vector should be stored, or %NULL.
 * @ux: Where squared uncertainty of x-component of average normal vector should be stored, or %NULL.
 * @uy: Where  squared uncertainty of y-component of average normal vector should be stored, or %NULL.
 * @uz: Where  squared uncertainty of z-component of average normal vector should be stored, or %NULL.
 *
 * Computes squared uncertainty of average normal vector of an area of a data field.
 **/
/*void
gwy_data_field_area_get_normal_coeffs_uncertainty(GwyDataField *data_field,
                                                  GwyDataField *uncz_field, 
                                                  gdouble uncx, 
                                                  gdouble uncy,
                                                  gint col, gint row,
                                                  gint width, gint height,
                                                  gdouble *nx, gdouble *ny, gdouble *nz,
                                                  gdouble *ux, gdouble *uy, gdouble *uz)
{
    gint i, j;
    int ctr = 0;
    gdouble d1x, d1y, d1z, d2x, d2y, d2z, dcx, dcy, dcz, dd;
    gdouble sumdx = 0.0, sumdy = 0.0, sumdz = 0.0, sumw = 0.0;
    gdouble avgdx, avgdy, avgdz;
    gdouble **nn, **dxz, **dyz;
    gdouble *u;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(uncz_field));
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);
    ///* This probably should not be enforced */
    /*
    //g_return_if_fail(gwy_si_unit_equal(gwy_data_field_get_si_unit_xy(a),
    //                                   gwy_data_field_get_si_unit_z(a)),
    //                 FALSE);
    //                 

    nn = (double **)malloc(width*sizeof(double *));
    dxz = (double **)malloc(width*sizeof(double *));
    dyz = (double **)malloc(width*sizeof(double *));
    for (i = col; i < col + width; i++) {
       nn[i] = calloc(height, sizeof(double));
       dxz[i] = calloc(height, sizeof(double));
       dyz[i] = calloc(height, sizeof(double));
    }

    // normal coefficients and table of x,y derivatives and normal vector sizes 
    for (i = col; i < col + width; i++) {
        for (j = row; j < row + height; j++) {
            d1x = 1.0;
            d1y = 0.0;
            d1z = gwy_data_field_get_xder(data_field, i, j);
	    dxz[i-col][j-row] = d1z;
            d2x = 0.0;
            d2y = 1.0;
            d2z = gwy_data_field_get_yder(data_field, i, j);
	    dyz[i-col][j-row] = d2z;
            // Cross product = normal vector 
            dcx = d1y*d2z - d1z*d2y;
            dcy = d1z*d2x - d1x*d2z;
            dcz = d1x*d2y - d1y*d2x; // Always 1 
            // Normalize and add 
            dd = sqrt(dcx*dcx + dcy*dcy + dcz*dcz);
            nn[i-col][j-row] = dd;
            dcx /= dd;
            sumdx += dcx;
            dcy /= dd;
            sumdy += dcy;
            dcz /= dd;
            sumdz += dcz;
            sumw += 1.0/dd;
            ctr++;
        }
    }

    avgdx = sumdx/ctr;
    avgdy = sumdy/ctr;
    avgdz = sumdz/ctr;

    if (nx)
        *nx = avgdx;
    if (ny)
        *ny = avgdy;
    if (nz)
        *nz = avgdz;


    // uncertainties 
    hx = dfield->xreal/dfield->xres;
    hy = dfield->yreal/dfield->yres;
    sumdx = sumdy = sumdz = 0;
    upos = uncz_field->data + row*uncz_field->xres + col;
    for (j = 0; j < height; j++) {
	    const gdouble *urow = upos + i*uncz_field->xres;
	    for (i = 0; i < width; i++) {
		    val = 0;
		    if (i + col == 0 )
			    val += - 1./(hx)*( dxz[i][j]*dxz[i][j]/
				    (nn[i][j]*nn[i][j]*nn[i][j]) - 1./nn[i][j]);
		    else 
			    val += -1./(2*hx)*( dxz[i+1][j]*dxz[i+1][j]/
				    (nn[i+1][j]*nn[i+1][j]*nn[i+1][j]) - 1./nn[i+1][j]);	
		    if (i + col == 1)
			    val += 1./(hx)*( dxz[i-1][j]*dxz[i-1][j]/
				    (nn[i-1][j]*nn[i-1][j]*nn[i-1][j]) - 1./nn[i-1][j]);
		    else
			    val += 1./(2*hx)*( dxz[i-1][j]*dxz[i-1][j]/
				    (nn[i-1][j]*nn[i-1][j]*nn[i-1][j]) - 1./nn[i-1][j]);	
		    else if (i + col == dfield->xres-2)
			    else if (i + col == dfield->xres-1)
					    else{
			    val += dxz[i-1][j]*dxz[i-1][j]/
				    (nn[i-1][j]*nn[i-1][j]*nn[i+1][j])-	
				    dxz[i+1][j]*dxz[i+1][j]/
				    (nn[i+1][j]*nn[i+1][j+1]*nn[i+1][j])-	
				    1./nn[i-1][j] + 1./nn[i+1][j];
			    val /= (2*hx);
					    }
			    val -= (dxz[i][j-1]*dyz[i][j-1]/
					    (nn[i][j-1]*nn[i][j-1]*nn[i][j-1])
					    -dxz[i][j+1]*dyz[i][j+1]/
					    (nn[i][j+1]*nn[i][j+1]*nn[i][j+1]))/(2*hy);
			    val /=ctr;
			    sumdx += val*val*(*urow)*(*urow);

		    nn[i][j] = dd;
            dcx /= dd;
            sumdx += dcx;
            dcy /= dd;
            sumdy += dcy;
            dcz /= dd;
            sumdz += dcz;
            sumw += 1.0/dd;
	    urow ++;
        }
    }
    sumdx /= ctr*ctr;
    sumdy /= ctr*ctr;
    sumdz /= ctr*ctr;
}
*/

/**
 * gwy_data_field_get_normal_coeffs_uncertainty:
 * @data_field: A data field.
 * @uncz_field: Corresponding uncertainty field.
 * @uncx: The uncertainty in the x direction.
 * @uncy: The uncertainty in the y direction.
 * @nx: Where x-component of average normal vector should be stored, or %NULL.
 * @ny: Where y-component of average normal vector should be stored, or %NULL.
 * @nz: Where z-component of average normal vector should be stored, or %NULL.
 *
 * Computes squared uncertainty of average normal vector of a data field.
 **/
/*
void
gwy_data_field_get_normal_coeffs(GwyDataField *data_field,
                                 gdouble *nx, gdouble *ny, gdouble *nz,
                                 gboolean normalize1)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_get_normal_coeffs(data_field,
                                          0, 0,
                                          data_field->xres, data_field->yres,
                                          nx, ny, nz, normalize1);
}
*/



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
