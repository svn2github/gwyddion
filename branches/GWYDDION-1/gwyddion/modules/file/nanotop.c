/*
 *  $Id: nanotop.c, v 1.2 2006/04/06 16:32:48
 *  Copyright (C) 2006 Alexander Kovalev, Metal-Polymer Research Institute
 *  E-mail: av_kov@tut.by
 *
 *  Partially based on apefile.c,
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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


#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#include <stdio.h>
#include <string.h>

#include "get.h"

#define EXTENSION ".spm"
#define nanometer (1e-9)
#define HEADER_SIZE 512

/* =============== header of SPM file======================================= */
typedef struct {
 guint16 tx, mx;                 /* number of points for one step and full X-size in points; tx - not used */
 guint16 ty, my;                 /* number of points for one step and full Y-size in points; ty - not used */
 gdouble Kx, Ky, Kz;             /* scale factor for X,Y and Z axes (nm/point)                             */
 gchar ZUnit[6];                 /* label of z-axis                                                        */
 gchar XYUnit[6];                /* label of scanning plane                                                */
 guint16 min;                    /* min of data                                                            */
 guint16 max;                    /* max of data                                                            */
 guint16 timeline;               /* time of scanning line                                                  */
 gchar date[8];                  /* date of creation data                                                  */
 gchar time[5];                  /* time of creation data                                                  */
 gchar note[301];                /* notes                                                                  */
 gchar void_field[94];           /* reserved                                                               */
 gchar Version[64];              /* version of SPM "Nanotop"                                               */
} SPMFile;                       /* it's consists of 512 bytes                                             */
/* ========================================================================= */


static gboolean      module_register  (const gchar *name);
static gint          nanotop_detect   (const gchar *filename,
                                       gboolean only_name);
static GwyContainer* nanotop_load     (const gchar *filename);
static GwyDataField* read_data_field  (SPMFile *spmfile, const guchar *ptr);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "nanotop",
    N_("Imports NANOTOP AFM files"),
    "Alexander Kovalev <av_kov@tut.by>",
    "1.3",
    "Alexander Kovalev, Metal-Polymer Research Institute",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo nanotop_func_info = {
        "nanotop_afm",
        N_("Nanotop files (.spm)"),
        (GwyFileDetectFunc)&nanotop_detect,
        (GwyFileLoadFunc)&nanotop_load,
        NULL
    };

    gwy_file_func_register(name, &nanotop_func_info);

    return TRUE;
}

static gint
nanotop_detect(const gchar *filename,
               gboolean only_name)
{
    gint score = 0;
    FILE *fh;
    gint flength = 0;
    gsize expected = 0;
    guint xres, yres;
    guchar bFile[4*sizeof(guint16)];
    const guchar *p;

    if (only_name) {
        gchar *filename_lc;

        filename_lc = g_ascii_strdown(filename, -1);
        score = g_str_has_suffix(filename_lc, EXTENSION) ? 20 : 0;
        g_free(filename_lc);

        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;

    /* determination of file size */
    fseek(fh, 0, SEEK_SET);                /* for any case */
    fseek(fh, 0, SEEK_END);
    flength = ftell(fh);                    /* real file size */
    fseek(fh, 0, SEEK_SET);

    if (fread(&bFile, sizeof(bFile), 1, fh) != 1) {
        fclose(fh);
        return 0;
    }
    fclose(fh);

    p = bFile + 2;
    xres = get_WORD_LE(&p);
    p += 2;
    yres = get_WORD_LE(&p);

    expected = 2*xres*yres + HEADER_SIZE; /* expected file size */
    if (expected == flength)
        score = 100;      /* that is fine! */

    return score;
}


static GwyContainer*
nanotop_load(const gchar *filename)
{
    SPMFile spmfile;
    GObject *object = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }

    /* read file header */
    if (size < HEADER_SIZE + 2) {
        g_warning("File is too short");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    p = buffer;
    spmfile.tx = get_WORD(&p);
    spmfile.mx = get_WORD(&p);
    spmfile.ty = get_WORD(&p);
    spmfile.my = get_WORD(&p);
    spmfile.Kx = get_FLOAT(&p);
    spmfile.Ky = get_FLOAT(&p);
    spmfile.Kz = get_FLOAT(&p);
    get_CHARARRAY0(spmfile.ZUnit, &p);
    get_CHARARRAY0(spmfile.XYUnit, &p);
    spmfile.min = get_WORD(&p);
    spmfile.max = get_WORD(&p);
    spmfile.timeline = get_WORD(&p);
    get_CHARARRAY(spmfile.date, &p);
    get_CHARARRAY(spmfile.time, &p);
    get_CHARARRAY(spmfile.note, &p);
    get_CHARARRAY(spmfile.void_field, &p);
    get_CHARARRAY(spmfile.Version, &p);

    if (size != HEADER_SIZE + 2*spmfile.mx*spmfile.my) {
        g_warning("File size does not match header");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    /* read data from buffer */
    dfield = read_data_field(&spmfile, p + 2);

    if (dfield) {
        object = gwy_container_new();
        gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                         G_OBJECT(dfield));
        g_object_unref(dfield);
    }

    gwy_file_abandon_contents(buffer, size, NULL);

    return (GwyContainer*)object;

}

static GwyDataField*
read_data_field(SPMFile *spmfile, const guchar *ptr)
{
    GwyDataField *dfield;
    GwySIUnit *unit = NULL;
    gdouble *data;
    gint i, n;
    const guint16 *p;

    dfield = GWY_DATA_FIELD(gwy_data_field_new(spmfile->mx,
                                               spmfile->my,
                                               spmfile->mx*spmfile->Kx*nanometer,
                                               spmfile->my*spmfile->Ky*nanometer,
                                               FALSE));
    data = gwy_data_field_get_data(dfield);
    p = (const guint16*)ptr;

    n = spmfile->mx * spmfile->my;

    for (i = 0; i < n; i++)
        *(data++) = GINT16_FROM_LE(*(p++));

    if (strcmp(spmfile->ZUnit, "deg") != 0) {
      gwy_data_field_multiply(dfield, spmfile->Kz*nanometer);
      unit = GWY_SI_UNIT(gwy_si_unit_new("m"));
      gwy_data_field_set_si_unit_xy(dfield, unit);
      g_object_unref(unit);
    }
    else {
      gwy_data_field_multiply(dfield, spmfile->Kz);
      unit = GWY_SI_UNIT(gwy_si_unit_new("deg"));
      gwy_data_field_set_si_unit_z(dfield, unit);
      g_object_unref(unit);
    }

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
