/*
 *  $Id$
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
#define DEBUG 1
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <app/gwyapp.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "get.h"

#define MAGIC "\x88\x1b\x03\x6f"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define HEADER_SIZE 834

typedef enum {
    MPRO_PHASE_RES_NORMAL = 0,
    MPRO_PHASE_RES_HIGH   = 1,
} MProPhaseResType;

typedef struct {
    gchar magic[4];
    gint header_format;
    gint header_size;
    gint software_type;
    gchar software_date[30];
    gint version_major;
    gint version_minor;
    gint version_micro;
    gint intens_xoff;
    gint intens_yoff;
    gint intens_xres;
    gint intens_yres;
    gint nbuckets;
    gint intens_range;
    gint intens_nbytes;
    gint phase_xoff;
    gint phase_yoff;
    gint phase_xres;
    gint phase_yres;
    gint phase_nbytes;
    gint timestamp;
    gchar comment[82];
    gint source;
    gdouble scale_factor;
    gdouble wavelength_in;
    gdouble numeric_aperture;
    gdouble obliquity_factor;
    gdouble magnification;
    gdouble camera_res;
    gint acquire_mode;
    gint intens_avgs;
    gint pzt_cal;
    gint pzt_gain_tolerance;
    gint pzt_gain;
    gdouble part_thickness;
    gint agc;
    gdouble target_range;
    gint min_mod;
    gint min_mod_pts;
    MProPhaseResType phase_res;
    gint min_area_size;
    gint discon_action;
    gdouble discon_filter;
    gint connection_order;
    gboolean data_inverted;
    gint camera_width;
    gint camera_height;
    gint system_type;
    gint system_board;
    gint system_serial;
    gint instrument_id;
    gchar objective_name[12];
    gchar part_num[40];
    gint code_vtype;
    gint phase_avgs;
    gint subtract_sys_err;
    gchar part_set_num[40];
    gdouble refactive_index;
    gint remove_tilt_bias;
    gint remove_fringes;
    gint max_area_size;
    gint setup_type;
    gdouble pre_connect_filter;
    gint wavelength_fold;
    gdouble wavelength1;
    gdouble wavelength2;
    gdouble wavelength3;
    gdouble wavelength4;
    gchar wavelength_select[8];
    gint fda_res;
    gchar scan_description[20];
    gint nfiducials;
    gdouble fiducials[2*7];
    gdouble pixel_width;
    gdouble pixel_height;
    gdouble exit_pupil_diam;
    gdouble light_level_pct;
    gint coords_state;
    gdouble xpos;
    gdouble ypos;
    gdouble zpos;
    gdouble xrot;
    gdouble yrot;
    gdouble zrot;
    gint coherence_mode;
    gint surface_filter;
    gchar sys_err_file[28];
    gchar zoom_desc[8];

    /* Our stuff */
    GwyDataField *intensity_data;
    GwyDataField *intensity_mask;
    GwyDataField *phase_data;
    GwyDataField *phase_mask;
} MProFile;

typedef struct {
    MProFile *file;
    GwyContainer *data;
    GtkWidget *data_view;
} MProControls;

static gboolean      module_register     (const gchar *name);
static gint          mprofile_detect     (const gchar *filename,
                                          gboolean only_name);
static GwyContainer* mprofile_load       (const gchar *filename);
static gboolean      mprofile_read_header(const guchar *buffer,
                                          gsize size,
                                          MProFile *mprofile);
static void          fill_data_fields    (MProFile *mprofile,
                                          const guchar *buffer);
static void          store_metadata      (MProFile *mprofile,
                                          GwyContainer *container);
static guint         select_which_data   (MProFile *mprofile);
static void          selection_changed   (GtkWidget *button,
                                          MProControls *controls);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "metropro",
    N_("Imports MetroPro (Zygo) data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo mprofile_func_info = {
        "mprofile",
        N_("MetroPro files (.dat)"),
        (GwyFileDetectFunc)&mprofile_detect,
        (GwyFileLoadFunc)&mprofile_load,
        NULL
    };

    gwy_file_func_register(name, &mprofile_func_info);

    return TRUE;
}

static gint
mprofile_detect(const gchar *filename, gboolean only_name)
{
    FILE *fh;
    gchar header[HEADER_SIZE];
    gint score = 0;

    if (only_name) {
        if (g_str_has_suffix(filename, ".dat"))
            score = 10;

        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;
    /* read complete header to make sure the file is at least this long */
    if (!(fread(header, HEADER_SIZE, 1, fh) == 1)) {
        fclose(fh);
        return 0;
    }
    fclose(fh);

    if (memcmp(header, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
mprofile_load(const gchar *filename)
{
    MProFile mprofile;
    GwyContainer *container = NULL;
    GwyDataField *dfield = NULL, *vpmask = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    gsize expected;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }

    if (!mprofile_read_header(buffer, size, &mprofile)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    expected = mprofile.header_size
               + 2*mprofile.nbuckets*mprofile.intens_xres*mprofile.intens_yres
               + 4*mprofile.phase_xres*mprofile.phase_yres;
    if (expected != size) {
        g_warning("Calculated file size %lu does not match real size %lu",
                  (gulong)expected, (gulong)size);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    fill_data_fields(&mprofile, buffer);
    gwy_file_abandon_contents(buffer, size, NULL);

    /*
    n = select_which_data(&mprofile);
    if (n != (guint)-1)
        dfield = mprofile.data[n];
    */
    dfield = mprofile.phase_data;
    vpmask = mprofile.phase_mask;
    if (dfield) {
        container = GWY_CONTAINER(gwy_container_new());
        gwy_container_set_object_by_name(container, "/0/data",
                                         G_OBJECT(dfield));
        if (vpmask)
            gwy_container_set_object_by_name(container, "/0/mask",
                                             G_OBJECT(vpmask));
        store_metadata(&mprofile, container);
    }

    gwy_object_unref(mprofile.intensity_data);
    gwy_object_unref(mprofile.phase_data);

    return container;
}

static gboolean
mprofile_read_header(const guchar *buffer,
                     gsize size,
                     MProFile *mprofile)
{
    const guchar *p;
    guint i;

    p = buffer;
    if (size < HEADER_SIZE + 2) {
        g_warning("File is too short");
        return FALSE;
    }
    get_CHARARRAY(mprofile->magic, &p);
    if (memcmp(mprofile->magic, MAGIC, MAGIC_SIZE) != 0) {
        g_warning("File magic header mistmatch");
        return FALSE;
    }

    mprofile->header_format = get_WORD_BE(&p);
    if (mprofile->header_format != 1) {
        g_warning("Only files with format version 1 are supported");
        return FALSE;
    }
    mprofile->header_size = get_DWORD_BE(&p);
    gwy_debug("header_format: %d, header_size: %d",
              mprofile->header_format, mprofile->header_size);
    if (mprofile->header_size < 570) {
        g_warning("File header is too short");
        return FALSE;
    }
    if (mprofile->header_size > size) {
        g_warning("File header is larger than file");
        return FALSE;
    }
    mprofile->software_type = get_WORD_BE(&p);
    get_CHARARRAY0(mprofile->software_date, &p);
    gwy_debug("software_type: %d, software_date: %s",
              mprofile->software_type, mprofile->software_date);

    mprofile->version_major = get_WORD_BE(&p);
    mprofile->version_minor = get_WORD_BE(&p);
    mprofile->version_micro = get_WORD_BE(&p);
    gwy_debug("version: %d.%d.%d",
              mprofile->version_major,
              mprofile->version_minor,
              mprofile->version_micro);

    mprofile->intens_xoff = get_WORD_BE(&p);
    mprofile->intens_yoff = get_WORD_BE(&p);
    mprofile->intens_xres = get_WORD_BE(&p);
    mprofile->intens_yres = get_WORD_BE(&p);
    gwy_debug("INTENS xres: %d, yres: %d, xoff: %d, yoff: %d",
              mprofile->intens_xres, mprofile->intens_yres,
              mprofile->intens_xoff, mprofile->intens_yoff);
    mprofile->nbuckets = get_WORD_BE(&p);
    if (mprofile->nbuckets != 0 && mprofile->nbuckets != 1)
        g_warning("Only 1 NBuckets is supported");
    mprofile->intens_range = get_WORD_BE(&p);
    mprofile->intens_nbytes = get_DWORD_BE(&p);
    gwy_debug("intens_nbytes: %d, expecting: %d",
              mprofile->intens_nbytes,
              2*mprofile->intens_xres*mprofile->intens_yres*mprofile->nbuckets);

    mprofile->phase_xoff = get_WORD_BE(&p);
    mprofile->phase_yoff = get_WORD_BE(&p);
    mprofile->phase_xres = get_WORD_BE(&p);
    mprofile->phase_yres = get_WORD_BE(&p);
    gwy_debug("PHASE xres: %d, yres: %d, xoff: %d, yoff: %d",
              mprofile->phase_xres, mprofile->phase_yres,
              mprofile->phase_xoff, mprofile->phase_yoff);
    mprofile->phase_nbytes = get_DWORD_BE(&p);
    gwy_debug("phase_nbytes: %d, expecting: %d",
              mprofile->phase_nbytes,
              4*mprofile->phase_xres*mprofile->phase_yres);

    mprofile->timestamp = get_DWORD_BE(&p);
    get_CHARARRAY0(mprofile->comment, &p);
    gwy_debug("comment: %s", mprofile->comment);
    mprofile->source = get_WORD_BE(&p);

    mprofile->scale_factor = get_FLOAT_BE(&p);
    mprofile->wavelength_in = get_FLOAT_BE(&p);
    mprofile->numeric_aperture = get_FLOAT_BE(&p);
    mprofile->obliquity_factor = get_FLOAT_BE(&p);
    mprofile->magnification = get_FLOAT_BE(&p);
    mprofile->camera_res = get_FLOAT_BE(&p);

    mprofile->acquire_mode = get_WORD_BE(&p);
    gwy_debug("acquire_mode: %d", mprofile->acquire_mode);
    mprofile->intens_avgs = get_WORD_BE(&p);
    mprofile->pzt_cal = get_WORD_BE(&p);
    mprofile->pzt_gain_tolerance = get_WORD_BE(&p);
    mprofile->pzt_gain = get_WORD_BE(&p);
    mprofile->part_thickness = get_FLOAT_BE(&p);
    mprofile->agc = get_WORD_BE(&p);
    mprofile->target_range = get_FLOAT_BE(&p);

    p += 2;
    mprofile->min_mod = get_DWORD_BE(&p);
    mprofile->min_mod_pts = get_DWORD_BE(&p);
    mprofile->phase_res = get_WORD_BE(&p);
    mprofile->min_area_size = get_DWORD_BE(&p);
    mprofile->discon_action = get_WORD_BE(&p);
    mprofile->discon_filter = get_FLOAT_BE(&p);
    mprofile->connection_order = get_WORD_BE(&p);
    mprofile->data_inverted = get_WORD_BE(&p);
    mprofile->camera_width = get_WORD_BE(&p);
    mprofile->camera_height = get_WORD_BE(&p);
    mprofile->system_type = get_WORD_BE(&p);
    mprofile->system_board = get_WORD_BE(&p);
    mprofile->system_serial = get_WORD_BE(&p);
    mprofile->instrument_id = get_WORD_BE(&p);
    get_CHARARRAY0(mprofile->objective_name, &p);
    get_CHARARRAY0(mprofile->part_num, &p);
    gwy_debug("part_num: %s", mprofile->part_num);
    mprofile->code_vtype = get_WORD_BE(&p);
    mprofile->phase_avgs = get_WORD_BE(&p);
    mprofile->subtract_sys_err = get_WORD_BE(&p);
    p += 16;
    get_CHARARRAY0(mprofile->part_set_num, &p);
    mprofile->refactive_index = get_FLOAT_BE(&p);
    mprofile->remove_tilt_bias = get_WORD_BE(&p);
    mprofile->remove_fringes = get_WORD_BE(&p);
    mprofile->max_area_size = get_DWORD_BE(&p);
    mprofile->setup_type = get_WORD_BE(&p);
    p += 2;
    mprofile->pre_connect_filter = get_FLOAT_BE(&p);

    mprofile->wavelength2 = get_FLOAT_BE(&p);
    mprofile->wavelength_fold = get_WORD_BE(&p);
    mprofile->wavelength1 = get_FLOAT_BE(&p);
    mprofile->wavelength3 = get_FLOAT_BE(&p);
    mprofile->wavelength4 = get_FLOAT_BE(&p);
    get_CHARARRAY0(mprofile->wavelength_select, &p);
    mprofile->fda_res = get_WORD_BE(&p);
    get_CHARARRAY0(mprofile->scan_description, &p);
    gwy_debug("scan_description: %s", mprofile->scan_description);

    mprofile->nfiducials = get_WORD_BE(&p);
    for (i = 0; i < G_N_ELEMENTS(mprofile->fiducials); i++)
        mprofile->fiducials[i] = get_FLOAT_BE(&p);

    mprofile->pixel_width = get_FLOAT_BE(&p);
    mprofile->pixel_height = get_FLOAT_BE(&p);
    mprofile->exit_pupil_diam = get_FLOAT_BE(&p);
    mprofile->light_level_pct = get_FLOAT_BE(&p);
    mprofile->coords_state = get_DWORD_BE(&p);
    mprofile->xpos = get_FLOAT_BE(&p);
    mprofile->ypos = get_FLOAT_BE(&p);
    mprofile->zpos = get_FLOAT_BE(&p);
    mprofile->xrot = get_FLOAT_BE(&p);
    mprofile->yrot = get_FLOAT_BE(&p);
    mprofile->zrot = get_FLOAT_BE(&p);
    mprofile->coherence_mode = get_WORD_BE(&p);
    mprofile->surface_filter = get_WORD_BE(&p);
    get_CHARARRAY0(mprofile->sys_err_file, &p);
    get_CHARARRAY0(mprofile->zoom_desc, &p);

    return TRUE;
}

static void
fix_void_pixels(GwyDataField *dfield,
                GwyDataField *vpmask,
                gdouble avg)
{
    GwySIUnit *siunit;
    const gdouble *mask;
    gdouble *data;
    gint i, n;

    data = gwy_data_field_get_data(dfield);
    mask = gwy_data_field_get_data_const(vpmask);
    n = gwy_data_field_get_xres(dfield)*gwy_data_field_get_yres(dfield);
    for (i = 0; i < n; i++) {
        if (mask[i])
            data[i] = avg;
    }

    siunit = gwy_data_field_get_si_unit_xy(dfield);
    siunit = gwy_si_unit_duplicate(siunit);
    gwy_data_field_set_si_unit_xy(vpmask, siunit);
    g_object_unref(siunit);

    siunit = GWY_SI_UNIT(gwy_si_unit_new(""));
    gwy_data_field_set_si_unit_z(vpmask, siunit);
    g_object_unref(siunit);
}

static void
fill_data_fields(MProFile *mprofile,
                 const guchar *buffer)
{
    GwyDataField *dfield, *vpmask;
    GwySIUnit *siunit;
    gdouble *data, *mask;
    gdouble xreal, yreal, q, avg;
    const guchar *p;
    guint n, i, j, nvoid;

    mprofile->intensity_data = NULL;
    mprofile->phase_data = NULL;

    p = buffer + mprofile->header_size;

    /* Intensity data */
    n = mprofile->intens_xres * mprofile->intens_yres;
    if (n) {
        const guint16 *d16;
        guint16 d;

        q = mprofile->data_inverted ? -1.0 : 1.0;

        if (mprofile->camera_res) {
            xreal = mprofile->intens_xres * mprofile->camera_res;
            yreal = mprofile->intens_yres * mprofile->camera_res;
        }
        else {
            /* whatever */
            xreal = mprofile->intens_xres;
            yreal = mprofile->intens_yres;
        }
        dfield = GWY_DATA_FIELD(gwy_data_field_new(mprofile->intens_xres,
                                                   mprofile->intens_yres,
                                                   xreal, yreal,
                                                   FALSE));
        vpmask = GWY_DATA_FIELD(gwy_data_field_new(mprofile->intens_xres,
                                                   mprofile->intens_yres,
                                                   xreal, yreal,
                                                   TRUE));
        data = gwy_data_field_get_data(dfield);
        mask = gwy_data_field_get_data(vpmask);
        d16 = (const guint16*)p;
        avg = 0.0;
        nvoid = 0;
        for (i = 0; i < mprofile->intens_yres; i++) {
            for (j = 0; j < mprofile->intens_xres; j++) {
                d = q*GUINT16_FROM_BE(*d16);
                *data = q*d;
                if (*d16 >= 65412) {
                    nvoid++;
                    mask[i*mprofile->intens_xres + j] = 1.0;
                }
                else
                    avg += *data;
                d16++;
                data++;
            }
        }

        if (mprofile->camera_res)
            siunit = GWY_SI_UNIT(gwy_si_unit_new("m"));
        else
            siunit = GWY_SI_UNIT(gwy_si_unit_new(""));
        gwy_data_field_set_si_unit_xy(dfield, siunit);
        g_object_unref(siunit);

        siunit = GWY_SI_UNIT(gwy_si_unit_new(""));
        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);

        gwy_debug("intens_nvoid: %u", nvoid);
        if (nvoid)
            fix_void_pixels(dfield, vpmask,
                            nvoid == n ? 0.0 : avg/(n - nvoid));
        else
            gwy_object_unref(vpmask);

        mprofile->intensity_data = dfield;
        mprofile->intensity_mask = vpmask;
        p += sizeof(guint16)*n*mprofile->nbuckets;
    }

    /* Phase data */
    n = mprofile->phase_xres * mprofile->phase_yres;
    if (n) {
        const gint32 *d32;
        gint32 d;

        i = 4096;
        if (mprofile->phase_res == 1)
            i = 32768;

        q = mprofile->scale_factor * mprofile->obliquity_factor
            * mprofile->wavelength_in/i;
        if (mprofile->data_inverted)
            q = -q;
        gwy_debug("q: %g", q);

        if (mprofile->camera_res) {
            xreal = mprofile->phase_xres * mprofile->camera_res;
            yreal = mprofile->phase_yres * mprofile->camera_res;
        }
        else {
            /* whatever */
            xreal = mprofile->phase_xres;
            yreal = mprofile->phase_yres;
        }
        dfield = GWY_DATA_FIELD(gwy_data_field_new(mprofile->phase_xres,
                                                   mprofile->phase_yres,
                                                   xreal, yreal,
                                                   FALSE));
        vpmask = GWY_DATA_FIELD(gwy_data_field_new(mprofile->phase_xres,
                                                   mprofile->phase_yres,
                                                   xreal, yreal,
                                                   TRUE));
        data = gwy_data_field_get_data(dfield);
        mask = gwy_data_field_get_data(vpmask);
        d32 = (const gint32*)p;
        avg = 0.0;
        nvoid = 0;
        for (i = 0; i < mprofile->phase_yres; i++) {
            for (j = 0; j < mprofile->phase_xres; j++) {
                d = GINT32_FROM_BE(*d32);
                *data = q*d;
                if (d >= 2147483640) {
                    nvoid++;
                    mask[i*mprofile->intens_xres + j] = 1.0;
                }
                else
                    avg += *data;
                d32++;
                data++;
            }
        }

        if (mprofile->camera_res)
            siunit = GWY_SI_UNIT(gwy_si_unit_new("m"));
        else
            siunit = GWY_SI_UNIT(gwy_si_unit_new(""));
        gwy_data_field_set_si_unit_xy(dfield, siunit);
        g_object_unref(siunit);

        siunit = GWY_SI_UNIT(gwy_si_unit_new("m"));
        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);

        gwy_debug("phase_nvoid: %u", nvoid);
        if (nvoid)
            fix_void_pixels(dfield, vpmask,
                            nvoid == n ? 0.0 : avg/(n - nvoid));
        else
            gwy_object_unref(vpmask);

        mprofile->phase_data = dfield;
        mprofile->phase_mask = vpmask;
        p += sizeof(gint32)*n;
    }
}

#define HASH_STORE(key, fmt, field) \
    gwy_container_set_string_by_name(container, "/meta/" key, \
                                     g_strdup_printf(fmt, mprofile->field))

static void
store_metadata(MProFile *mprofile,
               GwyContainer *container)
{
    /*
    gchar *p;

    HASH_STORE("Version", "%u", version);
    HASH_STORE("Tip oscilation frequency", "%g Hz", freq_osc_tip);
    HASH_STORE("Acquire delay", "%.6f s", acquire_delay);
    HASH_STORE("Raster delay", "%.6f s", raster_delay);
    HASH_STORE("Tip distance", "%g nm", tip_dist);

    if (mprofile->remark && *mprofile->remark
        && (p = g_convert(mprofile->remark, strlen(mprofile->remark),
                          "UTF-8", "ISO-8859-1", NULL, NULL, NULL)))
        gwy_container_set_string_by_name(container, "/meta/Comment", p);
    gwy_container_set_string_by_name
        (container, "/meta/SPM mode",
         g_strdup(gwy_enum_to_string(mprofile->spm_mode, spm_modes,
                                     G_N_ELEMENTS(spm_modes))));
    gwy_container_set_string_by_name(container, "/meta/Date",
                                     format_vt_date(mprofile->scan_date));
                                     */
}

#if 0
static guint
select_which_data(MProFile *mprofile)
{
    MProControls controls;
    GtkWidget *dialog, *label, *vbox, *hbox, *align;
    GwyEnum *choices;
    GtkObject *layer;
    GSList *radio, *rl;
    guint i, b;

    if (!mprofile->ndata)
        return (guint)-1;

    if (mprofile->ndata == 1)
        return 0;

    controls.file = mprofile;
    choices = g_new(GwyEnum, mprofile->ndata);
    b = 0;
    for (i = 0; i < mprofile->ndata; i++) {
        while (!((1 << b) & mprofile->channels))
            b++;
        b++;
        choices[i].value = i;
        choices[i].name = g_strdup_printf(_("Channel %u"), b);
    }

    dialog = gtk_dialog_new_with_buttons(_("Select Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    vbox = gtk_vbox_new(TRUE, 0);
    gtk_container_add(GTK_CONTAINER(align), vbox);

    label = gtk_label_new(_("Data to load:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

    radio = gwy_radio_buttons_create(choices, mprofile->ndata, "data",
                                     G_CALLBACK(selection_changed), &controls,
                                     0);
    for (i = 0, rl = radio; rl; i++, rl = g_slist_next(rl))
        gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(rl->data), TRUE, TRUE, 0);

    /* preview */
    align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    controls.data = GWY_CONTAINER(gwy_container_new());
    gwy_container_set_object_by_name(controls.data, "/0/data",
                                     G_OBJECT(mprofile->data[0]));

    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.data_view),
                           120.0/mprofile->res);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.data_view),
                                 GWY_PIXMAP_LAYER(layer));
    gtk_container_add(GTK_CONTAINER(align), controls.data_view);

    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
    switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
        gtk_widget_destroy(dialog);
        case GTK_RESPONSE_NONE:
        b = (guint)-1;
        break;

        case GTK_RESPONSE_OK:
        b = GPOINTER_TO_UINT(gwy_radio_buttons_get_current(radio, "data"));
        gtk_widget_destroy(dialog);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    for (i = 0; i < mprofile->ndata; i++)
        g_free((gpointer)choices[i].name);
    g_free(choices);

    return b;
}

static void
selection_changed(GtkWidget *button,
                  MProControls *controls)
{
    guint i;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    i = gwy_radio_buttons_get_current_from_widget(button, "data");
    g_assert(i != (guint)-1);
    gwy_container_set_object_by_name(controls->data, "/0/data",
                                     G_OBJECT(controls->file->data[i]));
    gwy_data_view_update(GWY_DATA_VIEW(controls->data_view));
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

