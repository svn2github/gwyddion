/*
 *  $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  Roughly based on code in Kasgira by MV <kasigra@seznam.cz>.
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
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <app/gwyapp.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "get.h"

#ifndef HAVE_POW10
#define pow10(x) (exp(G_LN10*(x)))
#endif

static gboolean      module_register    (const gchar *name);
static gint          spmlab_detect      (const gchar *filename,
                                         gboolean only_name);
static GwyContainer* spmlab_load        (const gchar *filename);
static GwyDataField* read_data_field    (const guchar *buffer,
                                         guint size,
                                         guchar version);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "spmlab",
    N_("Imports Thermicroscopes SpmLab R4, R5, and R6 data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.2.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo spmlab_func_info = {
        "spmlab",
        N_("Thermicroscopes SpmLab R4, R5, R6 files"),
        (GwyFileDetectFunc)&spmlab_detect,
        (GwyFileLoadFunc)&spmlab_load,
        NULL
    };

    gwy_file_func_register(name, &spmlab_func_info);

    return TRUE;
}

static gint
spmlab_detect(const gchar *filename, gboolean only_name)
{
    gint score = 0;
    FILE *fh;
    gchar magic[12];
    gchar *s;

    if (only_name) {
        guint len;
        gchar ext[3];

        len = strlen(filename);
        if (len < 5)
            return 0;

        /* Match case insensitive *.[12zfls][fr][rp] */
        ext[0] = g_ascii_tolower(filename[len-3]);
        ext[1] = g_ascii_tolower(filename[len-2]);
        ext[2] = g_ascii_tolower(filename[len-1]);
        if (filename[len-4] == '.'
            && (ext[2] == 'r' || ext[2] == 'p')
            && (ext[1] == 'f' || ext[1] == 'r')
            && (ext[0] == '1' || ext[0] == '2' || ext[0] == 'z'
                || ext[0] == 'f' || ext[0] == 'l' || ext[0] == 's'))
            score = 15;
        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;
    if (fread(magic, 1, sizeof(magic), fh) == sizeof(magic)
        && magic[0] == '#'
        && magic[1] == 'R'
        && (s = memchr(magic+1, '#', sizeof(magic)-1))
        && (magic[2] >= '3' && magic[2] <= '6'))
        score = 15;   /* XXX: must be below plug-in score to allow overriding */
    fclose(fh);

    return score;
}

static GwyContainer*
spmlab_load(const gchar *filename)
{
    GObject *object = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    /* 2048 is wrong. moreover it differs for r5 and r4, kasigra uses 5752 for
     * r5 */
    if (size < 2048 || buffer[0] != '#' || buffer[1] != 'R') {
        g_warning("File %s is not a Thermicroscopes SpmLab file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    switch (buffer[2]) {
        case '3':
        case '4':
        case '5':
        case '6':
        dfield = read_data_field(buffer, size, buffer[2]);
        break;

        default:
        g_warning("Uknown file version %c", buffer[2]);
        break;
    }

    gwy_file_abandon_contents(buffer, size, NULL);
    if (!dfield)
        return NULL;

    object = gwy_container_new();
    gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                     G_OBJECT(dfield));
    g_object_unref(dfield);

    return (GwyContainer*)object;
}

static GwyDataField*
read_data_field(const guchar *buffer, guint size, guchar version)
{
    enum { MIN_REMAINDER = 2620 };
    /* information offsets in different versions, in r5+ relative to data
     * start, in order: data offset, pixel dimensions, physical dimensions,
     * value multiplier, unit string */
    const guint offsets34[] = { 0x0104, 0x0196, 0x01a2, 0x01b2, 0x01c2 };
    const guint offsets56[] = { 0x0104, 0x025c, 0x0268, 0x0288, 0x02a0 };
    gint xres, yres, doffset, i, power10;
    gdouble xreal, yreal, q, z0;
    GwyDataField *dfield;
    GwySIUnit *unitxy, *unitz;
    gdouble *data;
    const guint *offset;
    const guchar *p, *r, *last;
    /* get floats in single precision from r4 but double from r5+ */
    gdouble (*getflt)(const guchar**);

    if (version == '5' || version == '6') {
        /* There are more data in r5,
         * try to find something that looks like #R5. */
        last = r = buffer;
        while ((p = memchr(r, '#', size - (r - buffer) - MIN_REMAINDER))) {
            if (p[1] == 'R' && p[2] == version && p[3] == '.') {
                gwy_debug("pos: %d", p - buffer);
                last = p;
                r = p + MIN_REMAINDER-1;
            }
            else
                r = p + 1;
        }
        offset = &offsets56[0];
        buffer = last;
        getflt = &get_DOUBLE;
    }
    else {
        offset = &offsets34[0];
        getflt = &get_FLOAT;
    }

    p = buffer + *(offset++);
    doffset = get_DWORD(&p);    /* this appears to be the same number as in
                                   the ASCII miniheader -- so get it here
                                   since it's easier */
    gwy_debug("data offset = %u", doffset);
    p = buffer + *(offset++);
    xres = get_DWORD(&p);
    yres = get_DWORD(&p);
    p = buffer + *(offset++);
    xreal = -getflt(&p);
    xreal += getflt(&p);
    yreal = -getflt(&p);
    yreal += getflt(&p);
    p = buffer + *(offset++);
    q = getflt(&p);
    z0 = getflt(&p);
    gwy_debug("xreal.raw = %g, yreal.raw = %g, q.raw = %g, z0.raw = %g",
              xreal, yreal, q, z0);
    p = buffer + *(offset++);
    unitz = GWY_SI_UNIT(gwy_si_unit_new_parse(p, &power10));
    q *= pow10(power10);
    z0 *= pow10(power10);
    unitxy = GWY_SI_UNIT(gwy_si_unit_new_parse(p + 10, &power10));
    xreal *= pow10(power10);
    yreal *= pow10(power10);
    gwy_debug("xres = %d, yres = %d, xreal = %g, yreal = %g, q = %g, z0 = %g",
              xres, yres, xreal, yreal, q, z0);
    gwy_debug("unitxy = %s, unitz = %s", p, p + 10);

    p = buffer + doffset;
    if (size - (p - buffer) < 2*xres*yres) {
        g_warning("Truncated data?");
        return NULL;
    }

    dfield = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, xreal, yreal,
                                               FALSE));
    data = gwy_data_field_get_data(dfield);
    gwy_data_field_set_si_unit_xy(dfield, unitxy);
    g_object_unref(unitxy);
    gwy_data_field_set_si_unit_z(dfield, unitz);
    g_object_unref(unitz);
    for (i = 0; i < xres*yres; i++)
        data[i] = (p[2*i] + 256.0*p[2*i + 1])*q + z0;

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

