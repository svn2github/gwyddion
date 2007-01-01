/*
 *  $Id$
 *  Copyright (C) 2004 Rok Zitko
 *  E-mail: rok.zitko@ijs.si
 *
 *  Information on the format was published in the manual
 *  for STMPRG, Copyright (C) 1989-1992 Omicron.
 *
 *  Based on nanoscope.c, Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

/* TODO
 * - store both directions
 * - other channels
 * - height vs. current, sol_z vs. sol_h etc.
 *
 * (Yeti):
 * FIXME: I do not have the specs.
 * XXX Fix the dependency on struct field alignment. XXX
 * Eliminate global variables.
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "get.h"
#include "err.h"
#include "stmprg.h"

#define MAGIC_TXT "MPAR"
#define MAGIC_SIZE (sizeof(MAGIC_TXT)-1)

static gboolean      module_register     (void);
static gint          stmprg_detect       (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static GwyContainer* stmprg_load         (const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static gboolean      read_binary_ubedata (gint n,
                                          gdouble *data,
                                          guchar *buffer,
                                          gint bpp);

/* Parameters are stored in global variables */
/* FIXME: Eliminate this */
struct STMPRG_MAINFIELD mainfield;
struct STMPRG_CONTROL control;
struct STMPRG_OTHER_CTRL other_ctrl;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Omicron STMPRG data files (tp ta)."),
    "Rok Zitko <rok.zitko@ijs.si>",
    "0.8",
    "Rok Zitko",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("stmprg",
                           N_("Omicron STMPRG files (tp ta)"),
                           (GwyFileDetectFunc)&stmprg_detect,
                           (GwyFileLoadFunc)&stmprg_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
stmprg_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return strstr(fileinfo->name, "tp") ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC_TXT, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static gboolean
read_parameters(gchar *buffer, guint size)
{
    gchar *ptr = buffer + 4;    /* 4 for MPAR */

    gwy_debug("tp file size = %i, should be %i\n", size, L_SIZE);
    if (size < L_SIZE)
        return FALSE;

    memcpy(&mainfield, ptr, L_MAINFIELD);

    ptr += L_MAINFIELD;
    memcpy(&control, ptr, L_CONTROL);

    ptr += L_CONTROL;
    memcpy(&other_ctrl, ptr, L_OTHER_CTRL);

    return TRUE;
}

static GwyDataField*
read_datafield(gchar *buffer, guint size, GError **error)
{
    gint xres, yres, bpp;
    gdouble xreal, yreal, q;
    GwyDataField *dfield;
    gdouble *data;
    GwySIUnit *unit;

    bpp = 2;                    /* words, always */

    xres = mainfield.points;
    yres = mainfield.lines;
    xreal = mainfield.field_x * 1.0e-10;        /* real size in angstroms */
    yreal = mainfield.field_y * 1.0e-10;
    q = mainfield.sol_z * 1.0e-5;       /* 5 ?? */
    /* resolution of z value in angstrom/bit, 1.0e-10 */

    if (size != bpp * xres * yres) {
        gwy_debug("Broken ta file. size = %i, should be %i\n", size,
                  bpp * xres * yres);
    }
    if (size < bpp*xres*yres) {
        err_SIZE_MISMATCH(error, bpp*xres*yres, size);
        return NULL;
    }

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    if (!read_binary_ubedata(xres * yres, data, buffer, bpp)) {
        err_BPP(error, bpp);
        g_object_unref(dfield);
        return NULL;
    }

    gwy_data_field_multiply(dfield, q);

    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    /* Assuming we are reading channel1... */
    switch (control.channel1) {
        case STMPRG_CHANNEL_OFF:
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("First channel is switched off."));
        return NULL;

        case STMPRG_CHANNEL_Z:
        unit = gwy_si_unit_new("m");
        break;

        case STMPRG_CHANNEL_I:
        case STMPRG_CHANNEL_I_I0:
        case STMPRG_CHANNEL_I0:
        unit = gwy_si_unit_new("A");
        break;

        case STMPRG_CHANNEL_ext1:
        case STMPRG_CHANNEL_ext2:
        case STMPRG_CHANNEL_U0:
        unit = gwy_si_unit_new("V");
        break;

        default:
        g_assert_not_reached();
        break;
    }
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    return dfield;
}

static float
FLOAT_FROM_BE(float f)
{
    const guchar *p = (const guchar*)&f;

    return get_FLOAT_BE(&p);
}

static void
byteswap_and_dump_parameters()
{                               /* below we insert the output from conv.pl ! */
    mainfield.start_X = FLOAT_FROM_BE(mainfield.start_X);
    gwy_debug("start_X=%f\n", mainfield.start_X);
    mainfield.start_Y = FLOAT_FROM_BE(mainfield.start_Y);
    gwy_debug("start_Y=%f\n", mainfield.start_Y);
    mainfield.field_x = FLOAT_FROM_BE(mainfield.field_x);
    gwy_debug("field_x=%f\n", mainfield.field_x);
    mainfield.field_y = FLOAT_FROM_BE(mainfield.field_y);
    gwy_debug("field_y=%f\n", mainfield.field_y);
    mainfield.inc_x = FLOAT_FROM_BE(mainfield.inc_x);
    gwy_debug("inc_x=%f\n", mainfield.inc_x);
    mainfield.inc_y = FLOAT_FROM_BE(mainfield.inc_y);
    gwy_debug("inc_y=%f\n", mainfield.inc_y);
    mainfield.points = GINT32_FROM_BE(mainfield.points);
    gwy_debug("points=%i\n", mainfield.points);
    mainfield.lines = GINT32_FROM_BE(mainfield.lines);
    gwy_debug("lines=%i\n", mainfield.lines);
    mainfield.angle = FLOAT_FROM_BE(mainfield.angle);
    gwy_debug("angle=%f\n", mainfield.angle);
    mainfield.sol_x = FLOAT_FROM_BE(mainfield.sol_x);
    gwy_debug("sol_x=%f\n", mainfield.sol_x);
    mainfield.sol_y = FLOAT_FROM_BE(mainfield.sol_y);
    gwy_debug("sol_y=%f\n", mainfield.sol_y);
    mainfield.sol_z = FLOAT_FROM_BE(mainfield.sol_z);
    gwy_debug("sol_z=%f\n", mainfield.sol_z);
    mainfield.sol_ext1 = FLOAT_FROM_BE(mainfield.sol_ext1);
    gwy_debug("sol_ext1=%f\n", mainfield.sol_ext1);
    mainfield.sol_ext2 = FLOAT_FROM_BE(mainfield.sol_ext2);
    gwy_debug("sol_ext2=%f\n", mainfield.sol_ext2);
    mainfield.sol_h = FLOAT_FROM_BE(mainfield.sol_h);
    gwy_debug("sol_h=%f\n", mainfield.sol_h);
    control.type = GINT16_FROM_BE(control.type);
    gwy_debug("type=%i\n", control.type);
    control.steps_x = GINT32_FROM_BE(control.steps_x);
    gwy_debug("steps_x=%i\n", control.steps_x);
    control.steps_y = GINT32_FROM_BE(control.steps_y);
    gwy_debug("steps_y=%i\n", control.steps_y);
    control.dac_speed = GINT32_FROM_BE(control.dac_speed);
    gwy_debug("dac_speed=%i\n", control.dac_speed);
    control.poi_inc = FLOAT_FROM_BE(control.poi_inc);
    gwy_debug("poi_inc=%f\n", control.poi_inc);
    control.lin_inc = FLOAT_FROM_BE(control.lin_inc);
    gwy_debug("lin_inc=%f\n", control.lin_inc);
    control.ad1_reads = GINT32_FROM_BE(control.ad1_reads);
    gwy_debug("ad1_reads=%i\n", control.ad1_reads);
    control.ad2_reads = GINT32_FROM_BE(control.ad2_reads);
    gwy_debug("ad2_reads=%i\n", control.ad2_reads);
    control.ad3_reads = GINT32_FROM_BE(control.ad3_reads);
    gwy_debug("ad3_reads=%i\n", control.ad3_reads);
    control.analog_ave = GINT32_FROM_BE(control.analog_ave);
    gwy_debug("analog_ave=%i\n", control.analog_ave);
    control.speed = GINT32_FROM_BE(control.speed);
    gwy_debug("speed=%i\n", control.speed);
    control.voltage = FLOAT_FROM_BE(control.voltage);
    gwy_debug("voltage=%f\n", control.voltage);
    control.voltage_l = FLOAT_FROM_BE(control.voltage_l);
    gwy_debug("voltage_l=%f\n", control.voltage_l);
    control.voltage_r = FLOAT_FROM_BE(control.voltage_r);
    gwy_debug("voltage_r=%f\n", control.voltage_r);
    control.volt_flag = GINT32_FROM_BE(control.volt_flag);
    gwy_debug("volt_flag=%i\n", control.volt_flag);
    control.volt_region = GINT32_FROM_BE(control.volt_region);
    gwy_debug("volt_region=%i\n", control.volt_region);
    control.current = FLOAT_FROM_BE(control.current);
    gwy_debug("current=%f\n", control.current);
    control.current_l = FLOAT_FROM_BE(control.current_l);
    gwy_debug("current_l=%f\n", control.current_l);
    control.current_r = FLOAT_FROM_BE(control.current_r);
    gwy_debug("current_r=%f\n", control.current_r);
    control.curr_flag = GINT32_FROM_BE(control.curr_flag);
    gwy_debug("curr_flag=%i\n", control.curr_flag);
    control.curr_region = GINT32_FROM_BE(control.curr_region);
    gwy_debug("curr_region=%i\n", control.curr_region);
    control.spec_lstart = FLOAT_FROM_BE(control.spec_lstart);
    gwy_debug("spec_lstart=%f\n", control.spec_lstart);
    control.spec_lend = FLOAT_FROM_BE(control.spec_lend);
    gwy_debug("spec_lend=%f\n", control.spec_lend);
    control.spec_linc = FLOAT_FROM_BE(control.spec_linc);
    gwy_debug("spec_linc=%f\n", control.spec_linc);
    control.spec_lsteps = GLONG_FROM_BE(control.spec_lsteps);
    gwy_debug("spec_lsteps=%li\n", control.spec_lsteps);
    control.spec_rstart = FLOAT_FROM_BE(control.spec_rstart);
    gwy_debug("spec_rstart=%f\n", control.spec_rstart);
    control.spec_rend = FLOAT_FROM_BE(control.spec_rend);
    gwy_debug("spec_rend=%f\n", control.spec_rend);
    control.spec_rinc = FLOAT_FROM_BE(control.spec_rinc);
    gwy_debug("spec_rinc=%f\n", control.spec_rinc);
    control.spec_rsteps = GLONG_FROM_BE(control.spec_rsteps);
    gwy_debug("spec_rsteps=%li\n", control.spec_rsteps);
    control.version = FLOAT_FROM_BE(control.version);
    gwy_debug("version=%f\n", control.version);
    control.free_lend = FLOAT_FROM_BE(control.free_lend);
    gwy_debug("free_lend=%f\n", control.free_lend);
    control.free_linc = FLOAT_FROM_BE(control.free_linc);
    gwy_debug("free_linc=%f\n", control.free_linc);
    control.free_lsteps = GLONG_FROM_BE(control.free_lsteps);
    gwy_debug("free_lsteps=%li\n", control.free_lsteps);
    control.free_rstart = FLOAT_FROM_BE(control.free_rstart);
    gwy_debug("free_rstart=%f\n", control.free_rstart);
    control.free_rend = FLOAT_FROM_BE(control.free_rend);
    gwy_debug("free_rend=%f\n", control.free_rend);
    control.free_rinc = FLOAT_FROM_BE(control.free_rinc);
    gwy_debug("free_rinc=%f\n", control.free_rinc);
    control.free_rsteps = GLONG_FROM_BE(control.free_rsteps);
    gwy_debug("free_rsteps=%li\n", control.free_rsteps);
    control.timer1 = GLONG_FROM_BE(control.timer1);
    gwy_debug("timer1=%li\n", control.timer1);
    control.timer2 = GLONG_FROM_BE(control.timer2);
    gwy_debug("timer2=%li\n", control.timer2);
    control.timer3 = GLONG_FROM_BE(control.timer3);
    gwy_debug("timer3=%li\n", control.timer3);
    control.timer4 = GLONG_FROM_BE(control.timer4);
    gwy_debug("timer4=%li\n", control.timer4);
    control.m_time = GLONG_FROM_BE(control.m_time);
    gwy_debug("m_time=%li\n", control.m_time);
    control.u_divider = FLOAT_FROM_BE(control.u_divider);
    gwy_debug("u_divider=%f\n", control.u_divider);
    control.fb_control = GINT32_FROM_BE(control.fb_control);
    gwy_debug("fb_control=%i\n", control.fb_control);
    control.fb_delay = GINT32_FROM_BE(control.fb_delay);
    gwy_debug("fb_delay=%i\n", control.fb_delay);
    control.point_time = GINT32_FROM_BE(control.point_time);
    gwy_debug("point_time=%i\n", control.point_time);
    control.spec_time = GINT32_FROM_BE(control.spec_time);
    gwy_debug("spec_time=%i\n", control.spec_time);
    control.spec_delay = GINT32_FROM_BE(control.spec_delay);
    gwy_debug("spec_delay=%i\n", control.spec_delay);
    control.fm = GINT16_FROM_BE(control.fm);
    gwy_debug("fm=%i\n", control.fm);
    control.fm_prgmode = GINT16_FROM_BE(control.fm_prgmode);
    gwy_debug("fm_prgmode=%i\n", control.fm_prgmode);
    control.fm_channel1 = GINT32_FROM_BE(control.fm_channel1);
    gwy_debug("fm_channel1=%i\n", control.fm_channel1);
    control.fm_channel2 = GINT32_FROM_BE(control.fm_channel2);
    gwy_debug("fm_channel2=%i\n", control.fm_channel2);
    control.fm_wait = GINT32_FROM_BE(control.fm_wait);
    gwy_debug("fm_wait=%i\n", control.fm_wait);
    control.fm_frames = GINT32_FROM_BE(control.fm_frames);
    gwy_debug("fm_frames=%i\n", control.fm_frames);
    control.fm_delay = GINT16_FROM_BE(control.fm_delay);
    gwy_debug("fm_delay=%i\n", control.fm_delay);
    control.spectr_edit = GINT16_FROM_BE(control.spectr_edit);
    gwy_debug("spectr_edit=%i\n", control.spectr_edit);
    control.fm_speed = GINT16_FROM_BE(control.fm_speed);
    gwy_debug("fm_speed=%i\n", control.fm_speed);
    control.fm_reads = GINT16_FROM_BE(control.fm_reads);
    gwy_debug("fm_reads=%i\n", control.fm_reads);
    other_ctrl.version = FLOAT_FROM_BE(other_ctrl.version);
    gwy_debug("version=%f\n", other_ctrl.version);
    other_ctrl.adc_data_l = GINT32_FROM_BE(other_ctrl.adc_data_l);
    gwy_debug("adc_data_l=%i\n", other_ctrl.adc_data_l);
    other_ctrl.adc_data_r = GINT32_FROM_BE(other_ctrl.adc_data_r);
    gwy_debug("adc_data_r=%i\n", other_ctrl.adc_data_r);
    other_ctrl.first_zp = GUINT16_FROM_BE(other_ctrl.first_zp);
    gwy_debug("first_zp=%i\n", other_ctrl.first_zp);
    other_ctrl.last_zp = GUINT16_FROM_BE(other_ctrl.last_zp);
    gwy_debug("last_zp=%i\n", other_ctrl.last_zp);
    other_ctrl.zdrift = FLOAT_FROM_BE(other_ctrl.zdrift);
    gwy_debug("zdrift=%f\n", other_ctrl.zdrift);
    other_ctrl.savememory = GINT32_FROM_BE(other_ctrl.savememory);
    gwy_debug("savememory=%i\n", other_ctrl.savememory);
    other_ctrl.contscan = GINT32_FROM_BE(other_ctrl.contscan);
    gwy_debug("contscan=%i\n", other_ctrl.contscan);
    other_ctrl.spec_loop = GINT32_FROM_BE(other_ctrl.spec_loop);
    gwy_debug("spec_loop=%i\n", other_ctrl.spec_loop);
    other_ctrl.ext_c = GINT32_FROM_BE(other_ctrl.ext_c);
    gwy_debug("ext_c=%i\n", other_ctrl.ext_c);
    other_ctrl.fm_zlift = FLOAT_FROM_BE(other_ctrl.fm_zlift);
    gwy_debug("fm_zlift=%f\n", other_ctrl.fm_zlift);
    other_ctrl.ext_a = GINT32_FROM_BE(other_ctrl.ext_a);
    gwy_debug("ext_a=%i\n", other_ctrl.ext_a);
    other_ctrl.vme_release = FLOAT_FROM_BE(other_ctrl.vme_release);
    gwy_debug("vme_release=%f\n", other_ctrl.vme_release);
}

/* Macros for storing meta data */

#define HASH_STORE(format, keystr, val) \
    value = g_strdup_printf(format, val); \
    gwy_debug("key = %s, val = %s\n", keystr, value); \
    gwy_container_set_string_by_name(meta, keystr, value);

#define HASH_STORE_F(keystr, valf) HASH_STORE("%f", keystr, valf)
#define HASH_STORE_S(keystr, vals) HASH_STORE("%s", keystr, vals)
#define HASH_STORE_I(keystr, vali) HASH_STORE("%i", keystr, vali)

static GwyContainer*
stmprg_get_metadata(void)
{
    GwyContainer *meta;
    gchar *value;

    meta = gwy_container_new();

    HASH_STORE_F("inc_x", mainfield.inc_x);
    HASH_STORE_F("inc_y", mainfield.inc_y);
    HASH_STORE_F("angle", mainfield.angle);
    HASH_STORE_F("sol_z", mainfield.sol_z);
    HASH_STORE_F("voltage", control.voltage);
    HASH_STORE_F("current", control.current);
    HASH_STORE_I("point_time", control.point_time);
    HASH_STORE_S("date", other_ctrl.date);
    HASH_STORE_S("comment", other_ctrl.comment);
    HASH_STORE_S("username", other_ctrl.username);

    return meta;
}

static GwyContainer*
stmprg_load(const gchar *filename,
            G_GNUC_UNUSED GwyRunType mode,
            GError **error)
{
    GwyContainer *meta, *container = NULL;
    gchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield;
    char *filename_ta, *ptr;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < MAGIC_SIZE || memcmp(buffer, MAGIC_TXT, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Omicron STMPRG");
        g_free(buffer);
        return NULL;
    }

    if (!read_parameters(buffer, size)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Parameter file is too short."));
        g_free(buffer);
        return NULL;
    }

    g_free(buffer);
    byteswap_and_dump_parameters();

    filename_ta = g_strdup(filename);
    ptr = filename_ta + strlen(filename_ta) - 1;
    while (g_ascii_isdigit(*ptr) && ptr > filename_ta+1)
        ptr--;
    if (*ptr == 'p' && *(ptr - 1) == 't')
        *ptr = 'a';

    if (!g_file_get_contents(filename_ta, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    dfield = read_datafield(buffer, size, error);
    if (!dfield)
        return NULL;

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    /* FIXME: with documentation, we could perhaps do better */
    gwy_app_channel_title_fall_back(container, 0);

    meta = stmprg_get_metadata();
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    g_free(filename_ta);
    g_free(buffer);

    return container;
}

/*
 * Warning: read_binary_ubedata reads UNSIGNED BIGENDIAN data.
 */
static gboolean
read_binary_ubedata(gint n, gdouble *data, guchar *buffer, gint bpp)
{
    gint i;
    gdouble q;

    q = 1.0/(1 << (8 * bpp));
    switch (bpp) {
        case 1:
            for (i = 0; i < n; i++)
                data[i] = q * buffer[i];
            break;

        case 2: {
            guint16 *p = (guint16 *) buffer;

            for (i = 0; i < n; i++) {
                data[i] = q * GUINT16_FROM_BE(p[i]);
            }
        }
        break;

        case 4: {
            guint32 *p = (guint32 *)buffer;

            for (i = 0; i < n; i++) {
                data[i] = q * GUINT32_FROM_BE(p[i]);
            }
        }
        break;

        default:
        return FALSE;
        break;
    }

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
