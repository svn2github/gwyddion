/*
 *  $Id$
 *  Copyright (C) 2012 David Necas (Yeti), Daniil Bratashov (dn2010).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * TODO: assuming cp1251 as 8bit encoding,
 * Raman scans (4d) slicing not implemented
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nanoscantech-spm">
 *   <comment>NanoScanTech SPM data</comment>
 *   <glob pattern="*.nstdat"/>
 *   <glob pattern="*.NSTDAT"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * NanoScanTech
 * .nstdat
 * Read SPS
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <unzip.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC "PK\x03\x04"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define MAGIC1 "lsdlsd"
#define MAGIC1_SIZE (sizeof(MAGIC1)-1)
#define EXTENSION ".nstdat"

static gboolean       module_register     (void);
static gint           nst_detect          (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer*  nst_load            (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static GwyDataField*  nst_read_3d         (const gchar *buffer,
                                           gchar **title);
static GwyGraphModel* nst_read_2d         (const gchar *buffer,
                                           guint channel);
static guchar*        nst_get_file_content(unzFile *zipfile,
                                           gsize *contentsize,
                                           GError **error);
static gboolean       nst_set_error       (gint status,
                                           GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports NanoScanTech .nstdat files."),
    "Daniil Bratashov (dn2010@gmail.com)",
    "0.3",
    "David Nečas (Yeti), Daniil Bratashov (dn2010)",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanoscantech",
                           N_("NanoScanTech data (.nstdat)"),
                           (GwyFileDetectFunc)&nst_detect,
                           (GwyFileLoadFunc)&nst_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
nst_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    unzFile zipfile;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    /* Generic ZIP file. */
    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    /* It contains directory Scan so this should be somewhere near the begining
     * of the file. */
    if (!gwy_memmem(fileinfo->head, fileinfo->buffer_len, MAGIC1, MAGIC1_SIZE))
        return 0;

    /* We have to realy look inside. */
    if (!(zipfile = unzOpen(fileinfo->name)))
        return 0;

    if (unzLocateFile(zipfile, "0.lsdlsd", 1) != UNZ_OK) {
        unzClose(zipfile);
        return 0;
    }

    unzClose(zipfile);

    return 100;
}

static GwyContainer*
nst_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    GwyDataField *dfield;
    GwyGraphModel *gmodel;
    unzFile zipfile;
    guint channelno = 0;
    gint status;
    gchar *buffer, *line, *p, *title, *strkey;
    gchar *titlestr = NULL;
    gsize size;

    zipfile = unzOpen(filename);
    if (!zipfile) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("Minizip cannot open the file as a ZIP file."));
        return NULL;
    }

    container = gwy_container_new();
    status = unzGoToFirstFile(zipfile);
    while (status == UNZ_OK) {
        unz_file_info fileinfo;
        gchar filename_buf[PATH_MAX+1];

        if (unzGetCurrentFileInfo(zipfile, &fileinfo, filename_buf,
                                  PATH_MAX, NULL, 0, NULL, 0) != UNZ_OK) {
            goto fail;
        }
        if (g_str_has_suffix(filename_buf, ".lsdlsd")) {
            gwy_debug("channel %d: %s\n", channelno, filename_buf);
            buffer = nst_get_file_content(zipfile, &size, NULL);
            p = buffer;
            line = gwy_str_next_line(&p);
            g_strstrip(line);
            if (gwy_strequal(line, "3d")) {
                gwy_debug("3d: %u\n", channelno);
                titlestr = NULL;
                dfield = nst_read_3d(p, &titlestr);
                if (dfield) {
                    GQuark key = gwy_app_get_data_key_for_id(channelno);

                    gwy_container_set_object(container, key, dfield);
                    g_object_unref(dfield);

                    strkey = g_strdup_printf("/%u/data/title",
                                             channelno);
                    if (!titlestr)
                        title = g_strdup_printf("Channel %u",
                                                channelno);
                    else
                        title = g_strdup_printf("%s (%u)",
                                                titlestr, channelno);
                    gwy_container_set_string_by_name(container, strkey,
                                                     title);
                    g_free(strkey);
                }
                if (titlestr) {
                    g_free(titlestr);
                }
            }
            else if (gwy_strequal(line, "2d")) {
                gwy_debug("2d: %d\n", channelno);
                gmodel = nst_read_2d(p, channelno);
                if (gmodel) {
                    strkey = g_strdup_printf("/0/graph/graph/%d",
                                             channelno+1);
                    gwy_container_set_object_by_name(container, strkey,
                                                     gmodel);
                    g_object_unref(gmodel);
                    g_free(strkey);
                }
            }
            else if (gwy_strequal(line, "4d")) {
                /* Raman images */
                gwy_debug("4d: %u\n", channelno);
            }

            g_free(buffer);
            channelno++;
        }
        status = unzGoToNextFile(zipfile);
    }

fail:
    unzClose(zipfile);
    if (!channelno) {
        gwy_object_unref(container);
        err_NO_DATA(error);
    }

    return container;
}

static GwyDataField *nst_read_3d(const gchar *buffer, gchar **title)
{
    GwyDataField *dfield = NULL;
    GwySIUnit *siunitxy = NULL, *siunitz = NULL;
    gchar *p, *line, *attributes, *unit;
    gchar **lineparts;
    gint x, y, xmax = 0, ymax = 0, i, j;
    gint power10xy = 1, power10z = 1;
    gdouble *data, z;
    gdouble xscale = 1.0, yscale =1.0;
    gdouble xoffset = 0.0, yoffset = 0.0;
    GArray *dataarray;
    gint linecur;

    p = (gchar *)buffer;
    dataarray = g_array_new(FALSE, TRUE, sizeof(gdouble));
    while ((line = gwy_str_next_line(&p))) {
        if (gwy_strequal(line, "[BeginOfItem]")) {
            while ((line = gwy_str_next_line(&p))) {
                lineparts = g_strsplit(line, " ", 3);
                x = atoi(lineparts[0]);
                y = atoi(lineparts[1]);
                z = g_ascii_strtod(lineparts[2], NULL);
                g_array_append_val(dataarray, z);
                if (x > xmax)
                    xmax = x;
                if (y > ymax)
                    ymax = y;
                g_strfreev(lineparts);
            }
            gwy_debug("xmax = %d, ymax =  %d\n", xmax+1, ymax+1);
            break;
        }
        else if (g_str_has_prefix(line, "XCUnit")) {
            lineparts = g_strsplit(line, " ", 3);
            unit = g_convert(lineparts[1], -1, "UTF-8", "cp1251",
                             NULL, NULL, NULL);
            siunitxy = gwy_si_unit_new_parse(unit, &power10xy);
            g_free(unit);
            x = atoi(lineparts[2]);
            if (x != 0)
                power10xy *= x;
            g_strfreev(lineparts);
        }
        else if (g_str_has_prefix(line, "ZCUnit")) {
            lineparts = g_strsplit(line, " ", 3);
            unit = g_convert(lineparts[1], -1, "UTF-8", "cp1251",
                             NULL, NULL, NULL);
            siunitz = gwy_si_unit_new_parse(unit, &power10z);
            g_free(unit);
            z = atoi(lineparts[2]);
            if (z != 0)
                power10z *= z;
            g_strfreev(lineparts);
        }
        else if (g_str_has_prefix(line, "PlotsXLimits")) {
            lineparts = g_strsplit(line, " ", 3);
            xoffset = g_ascii_strtod(lineparts[1], FALSE);
            xscale = g_ascii_strtod(lineparts[2], FALSE) - xoffset;
            g_strfreev(lineparts);
        }
        else if (g_str_has_prefix(line, "PlotsYLimits")) {
            lineparts = g_strsplit(line, " ", 3);
            yoffset = g_ascii_strtod(lineparts[1], FALSE);
            yscale = g_ascii_strtod(lineparts[2], FALSE) - yoffset;
            g_strfreev(lineparts);
        }
        else if (g_str_has_prefix(line, "Name")) {
            lineparts = g_strsplit(line, " ", 2);
            *title = g_convert(lineparts[1], -1, "UTF-8", "cp1251",
                               NULL, NULL, NULL);
            g_strfreev(lineparts);
        }
        else if (g_str_has_prefix(line, "Attributes")) {
            lineparts = g_strsplit(line, " ", 2);
            attributes = g_strdup(lineparts[1]);
            g_strfreev(lineparts);
            lineparts = g_strsplit(attributes, "*_*|^_^", 1024);
            g_free(attributes);
            linecur = 0;
            while (lineparts[linecur]) {
                if (g_str_has_prefix(lineparts[linecur], "Name")) {
                    if (((*title) == NULL) && (lineparts[linecur+1]))
                        *title = g_convert(lineparts[linecur+1],
                                           -1, "UTF-8", "cp1251",
                                           NULL, NULL, NULL);
                }
                linecur++;
            }
            g_strfreev(lineparts);
        }
    }

    if (xscale <= 0.0)
        xscale = 1.0;
    if (yscale <= 0.0)
        yscale = 1.0;
    dfield = gwy_data_field_new(xmax+1, ymax+1,
                                xscale*pow10(power10xy),
                                yscale*pow10(power10xy), TRUE);
    gwy_data_field_set_xoffset (dfield, xoffset*pow10(power10xy));
    gwy_data_field_set_yoffset (dfield, yoffset*pow10(power10xy));
    if (dfield) {
        data = gwy_data_field_get_data(dfield);
        for(j = 0; j <= ymax; j++)
            for (i = 0; i <= xmax; i++)
                *(data++) = g_array_index(dataarray,
                                          gdouble, j*(xmax+1)+i)*
                                          pow10(power10z);
    }
    if (siunitxy) {
        gwy_data_field_set_si_unit_xy (dfield, siunitxy);
        g_object_unref(siunitxy);
    }
    if (siunitz) {
        gwy_data_field_set_si_unit_z (dfield, siunitz);
        g_object_unref(siunitz);
    }

    g_array_free(dataarray, TRUE);
    return dfield;
}

static GwyGraphModel* nst_read_2d(const gchar *buffer, guint channel)
{
    GwyGraphCurveModel *spectra;
    GwyGraphModel *gmodel;
    GwySIUnit *siunitx = NULL, *siunity = NULL;
    gchar *p, *line, *unit;
    gchar **lineparts;
    gint linecur;
    gdouble *xdata, *ydata, x, y;
    GArray *xarray, *yarray;
    guint i, numpoints = 0, power10x = 1, power10y = 1;
    gchar *framename = NULL, *title = NULL, *attributes = NULL;

    p = (gchar *)buffer;
    gmodel = gwy_graph_model_new();
    while ((line = gwy_str_next_line(&p))) {
        if (g_str_has_prefix(line, "Loved")) {
            numpoints = 0;
            xarray = g_array_new(FALSE, TRUE, sizeof(gdouble));
            yarray = g_array_new(FALSE, TRUE, sizeof(gdouble));
            while ((line = gwy_str_next_line(&p))&&
                   (!gwy_strequal(line, "[EndOfItem]"))) {
                                lineparts = g_strsplit(line, " ", 3);
                lineparts = g_strsplit(line, " ", 2);
                x = g_ascii_strtod(lineparts[0], NULL);
                g_array_append_val(xarray, x);
                y = g_ascii_strtod(lineparts[1], NULL);
                g_array_append_val(yarray, y);
                g_strfreev(lineparts);
                numpoints++;
            }

            if (numpoints) {
                xdata = (gdouble *)g_malloc(numpoints * sizeof(gdouble));
                ydata = (gdouble *)g_malloc(numpoints * sizeof(gdouble));

                for (i = 0; i < numpoints; i++) {
                    xdata[i] = g_array_index(xarray, gdouble, i)
                               * pow10(power10x);
                    ydata[i] = g_array_index(yarray, gdouble, i)
                               * pow10(power10y);
                }

                spectra = gwy_graph_curve_model_new();
                if (!framename) {
                    framename = g_strdup_printf("Unknown spectrum");
                }
                g_object_set(spectra,
                             "description", framename,
                             "mode", GWY_GRAPH_CURVE_LINE,
                             NULL);
                gwy_graph_curve_model_set_data(spectra,
                                               xdata, ydata, numpoints);
                gwy_graph_model_add_curve(gmodel, spectra);

                g_object_unref(spectra);
                g_free(xdata);
                g_free(ydata);
            }
            g_array_free(xarray, TRUE);
            g_array_free(yarray, TRUE);
        }
        else if (g_str_has_prefix(line, "Name")) {
            lineparts = g_strsplit(line, " ", 2);
            if (framename)
                g_free(framename);
            framename = g_convert(lineparts[1], -1, "UTF-8", "cp1251",
                                  NULL, NULL, NULL);
            g_strfreev(lineparts);
        }
        else if (g_str_has_prefix(line, "XCUnit")) {
            lineparts = g_strsplit(line, " ", 3);
            unit = g_convert(lineparts[1], -1, "UTF-8", "cp1251",
                             NULL, NULL, NULL);
            siunitx = gwy_si_unit_new_parse(unit, &power10x);
            g_free(unit);
            x = atoi(lineparts[2]);
            if (x != 0)
                power10x *= x;
            g_strfreev(lineparts);
        }
        else if (g_str_has_prefix(line, "YCUnit")) {
            lineparts = g_strsplit(line, " ", 3);
            unit = g_convert(lineparts[1], -1, "UTF-8", "cp1251",
                             NULL, NULL, NULL);
            siunity = gwy_si_unit_new_parse(unit, &power10y);
            g_free(unit);
            y = atoi(lineparts[2]);
            if (y != 0)
                power10y *= y;
            g_strfreev(lineparts);
        }
        else if (g_str_has_prefix(line, "Attributes")) {
            lineparts = g_strsplit(line, " ", 2);
            attributes = g_strdup(lineparts[1]);
            g_strfreev(lineparts);
            lineparts = g_strsplit(attributes, "*_*|^_^", 1024);
            g_free(attributes);
            linecur = 0;
            while (lineparts[linecur]) {
                if (g_str_has_prefix(lineparts[linecur], "Name")) {
                    if ((!framename) && (lineparts[linecur+1]))
                        framename = g_convert(lineparts[linecur+1],
                                              -1, "UTF-8", "cp1251",
                                              NULL, NULL, NULL);
                }
                linecur++;
            }
            g_strfreev(lineparts);
        }
    }

    if(!framename)
        title = g_strdup_printf("Graph %u", channel);
    else {
        title = g_strdup_printf("%s (%u)", framename, channel);
        g_free(framename);
    }
    g_object_set(gmodel,
                 "title", title,
                 NULL);
    g_free(title);

    if (siunitx) {
        g_object_set(gmodel,
                     "si-unit-x", siunitx,
                     NULL);
        g_object_unref(siunitx);
    }

    if (siunity) {
        g_object_set(gmodel,
                     "si-unit-y", siunity,
                     NULL);
        g_object_unref(siunity);
    }

    return gmodel;
}

static guchar*
nst_get_file_content(unzFile *zipfile, gsize *contentsize, GError **error)
{
    unz_file_info fileinfo;
    guchar *buffer;
    gulong size;
    glong readbytes;
    gint status;

    gwy_debug("calling unzGetCurrentFileInfo() to figure out buffer size");
    status = unzGetCurrentFileInfo(zipfile, &fileinfo,
                                   NULL, 0,
                                   NULL, 0,
                                   NULL, 0);
    if (status != UNZ_OK) {
        nst_set_error(status, error);
        return NULL;
    }

    gwy_debug("calling unzGetCurrentFileInfo()");
    status = unzOpenCurrentFile(zipfile);
    if (status != UNZ_OK) {
        nst_set_error(status, error);
        return NULL;
    }

    size = fileinfo.uncompressed_size;
    buffer = g_new(guchar, size + 1);
    gwy_debug("calling unzReadCurrentFile()");
    readbytes = unzReadCurrentFile(zipfile, buffer, size);
    if (readbytes != size) {
        nst_set_error(status, error);
        unzCloseCurrentFile(zipfile);
        g_free(buffer);
        return NULL;
    }
    gwy_debug("calling unzCloseCurrentFile()");
    unzCloseCurrentFile(zipfile);

    buffer[size] = '\0';
    if (contentsize)
        *contentsize = size;
    return buffer;
}

static gboolean
nst_set_error(gint status, GError **error)
{
    const gchar *errstr = _("Unknown error");

    if (status == UNZ_ERRNO)
        errstr = g_strerror(errno);
    else if (status == UNZ_EOF)
        errstr = _("End of file");
    else if (status == UNZ_END_OF_LIST_OF_FILE)
        errstr = _("End of list of files");
    else if (status == UNZ_PARAMERROR)
        errstr = _("Parameter error");
    else if (status == UNZ_BADZIPFILE)
        errstr = _("Bad zip file");
    else if (status == UNZ_INTERNALERROR)
        errstr = _("Internal error");
    else if (status == UNZ_CRCERROR)
        errstr = _("CRC error");

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                _("Minizip error while reading the zip file: %s."),
                errstr);
    return FALSE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
