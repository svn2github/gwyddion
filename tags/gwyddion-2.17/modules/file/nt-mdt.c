/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
/* TODO: some metadata, ... */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nt-mdt-spm">
 *   <comment>NT-MDT SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="\x01\xb0\x93\xff"/>
 *   </magic>
 *   <glob pattern="*.mdt"/>
 *   <glob pattern="*.MDT"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libprocess/spectra.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphbasics.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "err.h"
#include "get.h"

#define MAGIC "\x01\xb0\x93\xff"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".mdt"

#define Angstrom (1e-10)
#define Nano (1e-9)

typedef enum {
    MDT_FRAME_SCANNED      = 0,
    MDT_FRAME_SPECTROSCOPY = 1,
    MDT_FRAME_TEXT         = 3,
    MDT_FRAME_OLD_MDA      = 105,
    MDT_FRAME_MDA          = 106,
    MDT_FRAME_PALETTE      = 107
} MDTFrameType;

typedef enum {
    MDA_DATA_INT8          = -1,
    MDA_DATA_UINT8         =  1,
    MDA_DATA_INT16         = -2,
    MDA_DATA_UINT16        =  2,
    MDA_DATA_INT32         = -4,
    MDA_DATA_UINT32        =  4,
    MDA_DATA_INT64         = -8,
    MDA_DATA_UINT64        =  8,
    MDA_DATA_FLOAT32       = -(4 + 23 * 256),
    MDA_DATA_FLOAT64       = -(8 + 52 * 256)
} MDADataType ;

typedef enum {
    MDT_UNIT_RAMAN_SHIFT     = -10,
    MDT_UNIT_RESERVED0       = -9,
    MDT_UNIT_RESERVED1       = -8,
    MDT_UNIT_RESERVED2       = -7,
    MDT_UNIT_RESERVED3       = -6,
    MDT_UNIT_METER           = -5,
    MDT_UNIT_CENTIMETER      = -4,
    MDT_UNIT_MILLIMETER      = -3,
    MDT_UNIT_MIKROMETER      = -2,
    MDT_UNIT_NANOMETER       = -1,
    MDT_UNIT_ANGSTROM        = 0,
    MDT_UNIT_NANOAMPERE      = 1,
    MDT_UNIT_VOLT            = 2,
    MDT_UNIT_NONE            = 3,
    MDT_UNIT_KILOHERZ        = 4,
    MDT_UNIT_DEGREES         = 5,
    MDT_UNIT_PERCENT         = 6,
    MDT_UNIT_CELSIUM_DEGREE  = 7,
    MDT_UNIT_VOLT_HIGH       = 8,
    MDT_UNIT_SECOND          = 9,
    MDT_UNIT_MILLISECOND     = 10,
    MDT_UNIT_MIKROSECOND     = 11,
    MDT_UNIT_NANOSECOND      = 12,
    MDT_UNIT_COUNTS          = 13,
    MDT_UNIT_PIXELS          = 14,
    MDT_UNIT_RESERVED_SFOM0  = 15,
    MDT_UNIT_RESERVED_SFOM1  = 16,
    MDT_UNIT_RESERVED_SFOM2  = 17,
    MDT_UNIT_RESERVED_SFOM3  = 18,
    MDT_UNIT_RESERVED_SFOM4  = 19,
    MDT_UNIT_AMPERE2         = 20,
    MDT_UNIT_MILLIAMPERE     = 21,
    MDT_UNIT_MIKROAMPERE     = 22,
    MDT_UNIT_NANOAMPERE2     = 23,
    MDT_UNIT_PICOAMPERE      = 24,
    MDT_UNIT_VOLT2           = 25,
    MDT_UNIT_MILLIVOLT       = 26,
    MDT_UNIT_MIKROVOLT       = 27,
    MDT_UNIT_NANOVOLT        = 28,
    MDT_UNIT_PICOVOLT        = 29,
    MDT_UNIT_NEWTON          = 30,
    MDT_UNIT_MILLINEWTON     = 31,
    MDT_UNIT_MIKRONEWTON     = 32,
    MDT_UNIT_NANONEWTON      = 33,
    MDT_UNIT_PICONEWTON      = 34,
    MDT_UNIT_RESERVED_DOS0   = 35,
    MDT_UNIT_RESERVED_DOS1   = 36,
    MDT_UNIT_RESERVED_DOS2   = 37,
    MDT_UNIT_RESERVED_DOS3   = 38,
    MDT_UNIT_RESERVED_DOS4   = 39
} MDTUnit;

typedef enum {
    MDT_MODE_STM = 0,
    MDT_MODE_AFM = 1
} MDTMode;

typedef enum {
    MDT_INPUT_EXTENSION_SLOT = 0,
    MDT_INPUT_BIAS_V         = 1,
    MDT_INPUT_GROUND         = 2
} MDTInputSignal;

typedef enum {
    MDT_TUNE_STEP  = 0,
    MDT_TUNE_FINE  = 1,
    MDT_TUNE_SLOPE = 2
} MDTLiftMode;

typedef enum {
    MDT_SPM_TECHNIQUE_CONTACT_MODE     = 0,
    MDT_SPM_TECHNIQUE_SEMICONTACT_MODE = 1,
    MDT_SPM_TECHNIQUE_TUNNEL_CURRENT   = 2,
    MDT_SPM_TECHNIQUE_SNOM             = 3
} MDTSPMTechnique;

typedef enum {
    MDT_SPM_MODE_CONSTANT_FORCE               = 0,
    MDT_SPM_MODE_CONTACT_CONSTANT_HEIGHT      = 1,
    MDT_SPM_MODE_CONTACT_ERROR                = 2,
    MDT_SPM_MODE_LATERAL_FORCE                = 3,
    MDT_SPM_MODE_FORCE_MODULATION             = 4,
    MDT_SPM_MODE_SPREADING_RESISTANCE_IMAGING = 5,
    MDT_SPM_MODE_SEMICONTACT_TOPOGRAPHY       = 6,
    MDT_SPM_MODE_SEMICONTACT_ERROR            = 7,
    MDT_SPM_MODE_PHASE_CONTRAST               = 8,
    MDT_SPM_MODE_AC_MAGNETIC_FORCE            = 9,
    MDT_SPM_MODE_DC_MAGNETIC_FORCE            = 10,
    MDT_SPM_MODE_ELECTROSTATIC_FORCE          = 11,
    MDT_SPM_MODE_CAPACITANCE_CONTRAST         = 12,
    MDT_SPM_MODE_KELVIN_PROBE                 = 13,
    MDT_SPM_MODE_CONSTANT_CURRENT             = 14,
    MDT_SPM_MODE_BARRIER_HEIGHT               = 15,
    MDT_SPM_MODE_CONSTANT_HEIGHT              = 16,
    MDT_SPM_MODE_AFAM                         = 17,
    MDT_SPM_MODE_CONTACT_EFM                  = 18,
    MDT_SPM_MODE_SHEAR_FORCE_TOPOGRAPHY       = 19,
    MDT_SPM_MODE_SFOM                         = 20,
    MDT_SPM_MODE_CONTACT_CAPACITANCE          = 21,
    MDT_SPM_MODE_SNOM_TRANSMISSION            = 22,
    MDT_SPM_MODE_SNOM_REFLECTION              = 23,
    MDT_SPM_MODE_SNOM_ALL                     = 24,
    MDT_SPM_MODE_SNOM                         = 25
} MDTSPMMode;

typedef enum {
    MDT_ADC_MODE_OFF       = -1,
    MDT_ADC_MODE_HEIGHT    = 0,
    MDT_ADC_MODE_DFL       = 1,
    MDT_ADC_MODE_LATERAL_F = 2,
    MDT_ADC_MODE_BIAS_V    = 3,
    MDT_ADC_MODE_CURRENT   = 4,
    MDT_ADC_MODE_FB_OUT    = 5,
    MDT_ADC_MODE_MAG       = 6,
    MDT_ADC_MODE_MAG_SIN   = 7,
    MDT_ADC_MODE_MAG_COS   = 8,
    MDT_ADC_MODE_RMS       = 9,
    MDT_ADC_MODE_CALCMAG   = 10,
    MDT_ADC_MODE_PHASE1    = 11,
    MDT_ADC_MODE_PHASE2    = 12,
    MDT_ADC_MODE_CALCPHASE = 13,
    MDT_ADC_MODE_EX1       = 14,
    MDT_ADC_MODE_EX2       = 15,
    MDT_ADC_MODE_HVX       = 16,
    MDT_ADC_MODE_HVY       = 17,
    MDT_ADC_MODE_SNAP_BACK = 18
} MDTADCMode;

enum {
    FILE_HEADER_SIZE      = 32,
    FRAME_HEADER_SIZE     = 22,
    FRAME_MODE_SIZE       = 8,
    AXIS_SCALES_SIZE      = 30,
    SCAN_VARS_MIN_SIZE    = 77,
    SPECTRO_VARS_MIN_SIZE = 38
};

typedef struct {
    gint name;
    gint value;
} GwyFlatEnum;

typedef struct {
    gdouble offset;    /* r0 (physical units) */
    gdouble step;    /* r (physical units) */
    MDTUnit unit;    /* U */
} MDTAxisScale;

typedef struct {
    MDTAxisScale x_scale;
    MDTAxisScale y_scale;
    MDTAxisScale z_scale;
    MDTADCMode channel_index;    /* s_mode */
    MDTMode mode;    /* s_dev */
    gint xres;    /* s_nx */
    gint yres;    /* s_ny */
    gint ndacq;    /* s_rv6; obsolete */
    gdouble step_length;    /* s_rs */
    guint adt;    /* s_adt */
    guint adc_gain_amp_log10;    /* s_adc_a */
    guint adc_index;    /* s_a12 */
    /* XXX: Some fields have different meaning in different versions */
    union {
        guint input_signal;    /* MDTInputSignal smp_in; s_smp_in */
        guint version;    /* s_8xx */
    } s16;
    union {
        guint substr_plane_order;    /* s_spl */
        guint pass_num;    /* z_03 */
    } s17;
    guint scan_dir;    /* s_xy TODO: interpretation */
    gboolean power_of_2;    /* s_2n */
    gdouble velocity;    /* s_vel (Angstrom/second) */
    gdouble setpoint;    /* s_i0 */
    gdouble bias_voltage;    /* s_ut */
    gboolean draw;    /* s_draw */
    gint xoff;    /* s_x00 (in DAC quants) */
    gint yoff;    /* s_y00 (in DAC quants) */
    gboolean nl_corr;    /* s_cor */
#if 0
    guint orig_format;    /* s_oem */
    MDTLiftMode tune;    /* z_tune */
    gdouble feedback_gain;    /* s_fbg */
    gint dac_scale;    /* s_s */
    gint overscan;    /* s_xov (in %) */
#endif
    /* XXX: much more stuff here */

    /* Frame mode stuff */
    guint fm_mode;    /* m_mode */
    guint fm_xres;    /* m_nx */
    guint fm_yres;    /* m_ny */
    guint fm_ndots;    /* m_nd */

    /* Data */
    const guchar *dots;
    const guchar *image;

    /* Stuff after data */
    guint title_len;
    const guchar *title;
    gchar *xmlstuff;
} MDTScannedDataFrame;

typedef struct {
    guint totLen;
    guint nameLen;
    const gchar *name;
    guint commentLen;
    const gchar *comment;
    guint unitLen;
    const gchar *unit;
    guint authorLen;
    const gchar *author;

//  guint32 totLen;
    gdouble    accuracy ;
    gdouble    scale ;
    gdouble    bias ;
    guint64    minIndex;
    guint64    maxIndex;
    gint32    dataType ;
    guint64 siUnit;
} MDTMDACalibration;

typedef struct {
    MDTMDACalibration *dimensions;
    MDTMDACalibration *mesurands;
    gint nDimensions;
    gint nMesurands;
    guint cellSize;
    const guchar *image;
    guint title_len;
    const guchar *title;
    gchar *xmlstuff;
} MDTMDAFrame;

typedef struct {
    guint size;     /* h_sz */
    MDTFrameType type;     /* h_what */
    gint version;  /* h_ver0, h_ver1 */

    gint year;    /* h_yea */
    gint month;    /* h_mon */
    gint day;    /* h_day */
    gint hour;    /* h_h */
    gint min;    /* h_m */
    gint sec;    /* h_s */

    gint var_size;    /* h_am, v6 and older only */

    gpointer frame_data;
} MDTFrame;

typedef struct {
    guint size;  /* f_sz */
    guint last_frame; /* f_nt */
    MDTFrame *frames;
} MDTFile;

static gboolean       module_register       (void);
static gint           mdt_detect            (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static GwyContainer*  mdt_load              (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
static GwyContainer*  mdt_get_metadata      (MDTFile *mdtfile,
                                             guint i);
static void           mdt_add_frame_metadata(MDTScannedDataFrame *sdframe,
                                             GwyContainer *meta);
static gboolean       mdt_real_load         (const guchar *buffer,
                                             guint size,
                                             MDTFile *mdtfile,
                                             GError **error);
static GwyDataField*  extract_scanned_data  (MDTScannedDataFrame *dataframe);
static GwyDataField*  extract_mda_data      (MDTMDAFrame *dataframe);
static GwyGraphModel* extract_mda_spectrum  (MDTMDAFrame *dataframe);

#ifdef DEBUG
static const GwyEnum frame_types[] = {
    { "Scanned",      MDT_FRAME_SCANNED },
    { "Spectroscopy", MDT_FRAME_SPECTROSCOPY },
    { "Text",         MDT_FRAME_TEXT },
    { "Old MDA",      MDT_FRAME_OLD_MDA },
    { "MDA",          MDT_FRAME_MDA },
    { "Palette",      MDT_FRAME_PALETTE },
};
#endif

#ifdef GWY_RELOC_SOURCE
static const GwyEnum mdt_units[] = {
    { "1/cm", MDT_UNIT_RAMAN_SHIFT },
    { "",     MDT_UNIT_RESERVED0 },
    { "",     MDT_UNIT_RESERVED1 },
    { "",     MDT_UNIT_RESERVED2 },
    { "",     MDT_UNIT_RESERVED3 },
    { "m",    MDT_UNIT_METER },
    { "cm",   MDT_UNIT_CENTIMETER },
    { "mm",   MDT_UNIT_MILLIMETER },
    { "µm",   MDT_UNIT_MIKROMETER },
    { "nm",   MDT_UNIT_NANOMETER },
    { "Å",    MDT_UNIT_ANGSTROM },
    { "nA",   MDT_UNIT_NANOAMPERE },
    { "V",    MDT_UNIT_VOLT },
    { "",     MDT_UNIT_NONE },
    { "kHz",  MDT_UNIT_KILOHERZ },
    { "deg",  MDT_UNIT_DEGREES },
    { "%",    MDT_UNIT_PERCENT },
    { "°C",   MDT_UNIT_CELSIUM_DEGREE },
    { "V",    MDT_UNIT_VOLT_HIGH },
    { "s",    MDT_UNIT_SECOND },
    { "ms",   MDT_UNIT_MILLISECOND },
    { "µs",   MDT_UNIT_MIKROSECOND },
    { "ns",   MDT_UNIT_NANOSECOND },
    { "",     MDT_UNIT_COUNTS },
    { "px",   MDT_UNIT_PIXELS },
    { "",     MDT_UNIT_RESERVED_SFOM0 },
    { "",     MDT_UNIT_RESERVED_SFOM1 },
    { "",     MDT_UNIT_RESERVED_SFOM2 },
    { "",     MDT_UNIT_RESERVED_SFOM3 },
    { "",     MDT_UNIT_RESERVED_SFOM4 },
    { "A",    MDT_UNIT_AMPERE2 },
    { "mA",   MDT_UNIT_MILLIAMPERE },
    { "µA",   MDT_UNIT_MIKROAMPERE },
    { "nA",   MDT_UNIT_NANOAMPERE2 },
    { "pA",   MDT_UNIT_PICOAMPERE },
    { "V",    MDT_UNIT_VOLT2 },
    { "mV",   MDT_UNIT_MILLIVOLT },
    { "µV",   MDT_UNIT_MIKROVOLT },
    { "nV",   MDT_UNIT_NANOVOLT },
    { "pV",   MDT_UNIT_PICOVOLT },
    { "N",    MDT_UNIT_NEWTON },
    { "mN",   MDT_UNIT_MILLINEWTON },
    { "µN",   MDT_UNIT_MIKRONEWTON },
    { "nN",   MDT_UNIT_NANONEWTON },
    { "pN",   MDT_UNIT_PICONEWTON },
    { "",     MDT_UNIT_RESERVED_DOS0 },
    { "",     MDT_UNIT_RESERVED_DOS1 },
    { "",     MDT_UNIT_RESERVED_DOS2 },
    { "",     MDT_UNIT_RESERVED_DOS3 },
    { "",     MDT_UNIT_RESERVED_DOS4 },
};
#else  /* {{{ */
/* This code block was GENERATED by flatten.py.
   When you edit mdt_units[] data above,
   re-run flatten.py SOURCE.c. */
static const gchar mdt_units_name[] =
    "1/cm\000\000\000\000\000m\000cm\000mm\000µm\000nm\000Å\000nA\000V"
    "\000\000kHz\000deg\000%\000°C\000V\000s\000ms\000µs\000ns\000\000px"
    "\000\000\000\000\000\000A\000mA\000µA\000nA\000pA\000V\000mV\000µV"
    "\000nV\000pV\000N\000mN\000µN\000nN\000pN\000\000\000\000\000";

static const GwyFlatEnum mdt_units[] = {
    { 0, MDT_UNIT_RAMAN_SHIFT },
    { 5, MDT_UNIT_RESERVED0 },
    { 6, MDT_UNIT_RESERVED1 },
    { 7, MDT_UNIT_RESERVED2 },
    { 8, MDT_UNIT_RESERVED3 },
    { 9, MDT_UNIT_METER },
    { 11, MDT_UNIT_CENTIMETER },
    { 14, MDT_UNIT_MILLIMETER },
    { 17, MDT_UNIT_MIKROMETER },
    { 21, MDT_UNIT_NANOMETER },
    { 24, MDT_UNIT_ANGSTROM },
    { 27, MDT_UNIT_NANOAMPERE },
    { 30, MDT_UNIT_VOLT },
    { 32, MDT_UNIT_NONE },
    { 33, MDT_UNIT_KILOHERZ },
    { 37, MDT_UNIT_DEGREES },
    { 41, MDT_UNIT_PERCENT },
    { 43, MDT_UNIT_CELSIUM_DEGREE },
    { 47, MDT_UNIT_VOLT_HIGH },
    { 49, MDT_UNIT_SECOND },
    { 51, MDT_UNIT_MILLISECOND },
    { 54, MDT_UNIT_MIKROSECOND },
    { 58, MDT_UNIT_NANOSECOND },
    { 61, MDT_UNIT_COUNTS },
    { 62, MDT_UNIT_PIXELS },
    { 65, MDT_UNIT_RESERVED_SFOM0 },
    { 66, MDT_UNIT_RESERVED_SFOM1 },
    { 67, MDT_UNIT_RESERVED_SFOM2 },
    { 68, MDT_UNIT_RESERVED_SFOM3 },
    { 69, MDT_UNIT_RESERVED_SFOM4 },
    { 70, MDT_UNIT_AMPERE2 },
    { 72, MDT_UNIT_MILLIAMPERE },
    { 75, MDT_UNIT_MIKROAMPERE },
    { 79, MDT_UNIT_NANOAMPERE2 },
    { 82, MDT_UNIT_PICOAMPERE },
    { 85, MDT_UNIT_VOLT2 },
    { 87, MDT_UNIT_MILLIVOLT },
    { 90, MDT_UNIT_MIKROVOLT },
    { 94, MDT_UNIT_NANOVOLT },
    { 97, MDT_UNIT_PICOVOLT },
    { 100, MDT_UNIT_NEWTON },
    { 102, MDT_UNIT_MILLINEWTON },
    { 105, MDT_UNIT_MIKRONEWTON },
    { 109, MDT_UNIT_NANONEWTON },
    { 112, MDT_UNIT_PICONEWTON },
    { 115, MDT_UNIT_RESERVED_DOS0 },
    { 116, MDT_UNIT_RESERVED_DOS1 },
    { 117, MDT_UNIT_RESERVED_DOS2 },
    { 118, MDT_UNIT_RESERVED_DOS3 },
    { 119, MDT_UNIT_RESERVED_DOS4 },
};
#endif  /* }}} */

#ifdef GWY_RELOC_SOURCE
static const GwyEnum mdt_spm_techniques[] = {
    { "Contact Mode",     MDT_SPM_TECHNIQUE_CONTACT_MODE,     },
    { "Semicontact Mode", MDT_SPM_TECHNIQUE_SEMICONTACT_MODE, },
    { "Tunnel Current",   MDT_SPM_TECHNIQUE_TUNNEL_CURRENT,   },
    { "SNOM",             MDT_SPM_TECHNIQUE_SNOM,             },
};
#else  /* {{{ */
/* This code block was GENERATED by flatten.py.
   When you edit mdt_spm_techniques[] data above,
   re-run flatten.py SOURCE.c. */
static const gchar mdt_spm_techniques_name[] =
    "Contact Mode\000Semicontact Mode\000Tunnel Current\000SNOM";

static const GwyFlatEnum mdt_spm_techniques[] = {
    { 0, MDT_SPM_TECHNIQUE_CONTACT_MODE },
    { 13, MDT_SPM_TECHNIQUE_SEMICONTACT_MODE },
    { 30, MDT_SPM_TECHNIQUE_TUNNEL_CURRENT },
    { 45, MDT_SPM_TECHNIQUE_SNOM },
};
#endif  /* }}} */

#ifdef GWY_RELOC_SOURCE
static const GwyEnum mdt_spm_modes[] = {
    { "Constant Force",               MDT_SPM_MODE_CONSTANT_FORCE,           },
    { "Contact Constant Height",      MDT_SPM_MODE_CONTACT_CONSTANT_HEIGHT,  },
    { "Contact Error",                MDT_SPM_MODE_CONTACT_ERROR,            },
    { "Lateral Force",                MDT_SPM_MODE_LATERAL_FORCE,            },
    { "Force Modulation",             MDT_SPM_MODE_FORCE_MODULATION,         },
    { "Spreading Resistance Imaging", MDT_SPM_MODE_SPREADING_RESISTANCE_IMAGING, },
    { "Semicontact Topography",       MDT_SPM_MODE_SEMICONTACT_TOPOGRAPHY,   },
    { "Semicontact Error",            MDT_SPM_MODE_SEMICONTACT_ERROR,        },
    { "Phase Contrast",               MDT_SPM_MODE_PHASE_CONTRAST,           },
    { "AC Magnetic Force",            MDT_SPM_MODE_AC_MAGNETIC_FORCE,        },
    { "DC Magnetic Force",            MDT_SPM_MODE_DC_MAGNETIC_FORCE,        },
    { "Electrostatic Force",          MDT_SPM_MODE_ELECTROSTATIC_FORCE,      },
    { "Capacitance Contrast",         MDT_SPM_MODE_CAPACITANCE_CONTRAST,     },
    { "Kelvin Probe",                 MDT_SPM_MODE_KELVIN_PROBE,             },
    { "Constant Current",             MDT_SPM_MODE_CONSTANT_CURRENT,         },
    { "Barrier Height",               MDT_SPM_MODE_BARRIER_HEIGHT,           },
    { "Constant Height",              MDT_SPM_MODE_CONSTANT_HEIGHT,          },
    { "AFAM",                         MDT_SPM_MODE_AFAM,                     },
    { "Contact EFM",                  MDT_SPM_MODE_CONTACT_EFM,              },
    { "Shear Force Topography",       MDT_SPM_MODE_SHEAR_FORCE_TOPOGRAPHY,   },
    { "SFOM",                         MDT_SPM_MODE_SFOM,                     },
    { "Contact Capacitance",          MDT_SPM_MODE_CONTACT_CAPACITANCE,      },
    { "SNOM Transmission",            MDT_SPM_MODE_SNOM_TRANSMISSION,        },
    { "SNOM Reflection",              MDT_SPM_MODE_SNOM_REFLECTION,          },
    { "SNOM All",                     MDT_SPM_MODE_SNOM_ALL,                 },
    { "SNOM",                         MDT_SPM_MODE_SNOM,                     },
};
#else  /* {{{ */
/* This code block was GENERATED by flatten.py.
   When you edit mdt_spm_modes[] data above,
   re-run flatten.py SOURCE.c. */
static const gchar mdt_spm_modes_name[] =
    "Constant Force\000Contact Constant Height\000Contact Error\000Lateral "
    "Force\000Force Modulation\000Spreading Resistance Imaging\000Semiconta"
    "ct Topography\000Semicontact Error\000Phase Contrast\000AC Magnetic Fo"
    "rce\000DC Magnetic Force\000Electrostatic Force\000Capacitance Contras"
    "t\000Kelvin Probe\000Constant Current\000Barrier Height\000Constant He"
    "ight\000AFAM\000Contact EFM\000Shear Force Topography\000SFOM\000Conta"
    "ct Capacitance\000SNOM Transmission\000SNOM Reflection\000SNOM All\000"
    "SNOM";

static const GwyFlatEnum mdt_spm_modes[] = {
    { 0, MDT_SPM_MODE_CONSTANT_FORCE },
    { 15, MDT_SPM_MODE_CONTACT_CONSTANT_HEIGHT },
    { 39, MDT_SPM_MODE_CONTACT_ERROR },
    { 53, MDT_SPM_MODE_LATERAL_FORCE },
    { 67, MDT_SPM_MODE_FORCE_MODULATION },
    { 84, MDT_SPM_MODE_SPREADING_RESISTANCE_IMAGING },
    { 113, MDT_SPM_MODE_SEMICONTACT_TOPOGRAPHY },
    { 136, MDT_SPM_MODE_SEMICONTACT_ERROR },
    { 154, MDT_SPM_MODE_PHASE_CONTRAST },
    { 169, MDT_SPM_MODE_AC_MAGNETIC_FORCE },
    { 187, MDT_SPM_MODE_DC_MAGNETIC_FORCE },
    { 205, MDT_SPM_MODE_ELECTROSTATIC_FORCE },
    { 225, MDT_SPM_MODE_CAPACITANCE_CONTRAST },
    { 246, MDT_SPM_MODE_KELVIN_PROBE },
    { 259, MDT_SPM_MODE_CONSTANT_CURRENT },
    { 276, MDT_SPM_MODE_BARRIER_HEIGHT },
    { 291, MDT_SPM_MODE_CONSTANT_HEIGHT },
    { 307, MDT_SPM_MODE_AFAM },
    { 312, MDT_SPM_MODE_CONTACT_EFM },
    { 324, MDT_SPM_MODE_SHEAR_FORCE_TOPOGRAPHY },
    { 347, MDT_SPM_MODE_SFOM },
    { 352, MDT_SPM_MODE_CONTACT_CAPACITANCE },
    { 372, MDT_SPM_MODE_SNOM_TRANSMISSION },
    { 390, MDT_SPM_MODE_SNOM_REFLECTION },
    { 406, MDT_SPM_MODE_SNOM_ALL },
    { 415, MDT_SPM_MODE_SNOM },
};
#endif  /* }}} */

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports NT-MDT data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.12",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nt-mdt",
                           N_("NT-MDT files (.mdt)"),
                           (GwyFileDetectFunc)&mdt_detect,
                           (GwyFileLoadFunc)&mdt_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
mdt_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

/***** Generic **************************************************************/
static const gchar*
gwy_flat_enum_to_string(gint enumval,
                        guint nentries,
                        const GwyFlatEnum *table,
                        const gchar *names)
{
    gint j;

    for (j = 0; j < nentries; j++) {
        if (enumval == table[j].value)
            return names + table[j].name;
    }

    return NULL;
}
/****************************************************************************/

static GwyContainer*
mdt_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    guchar *buffer;
    gsize size;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GwyContainer *meta, *data = NULL;
    MDTFile mdtfile;
    GString *key;
    guint n,i;

    gwy_debug("");
    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    gwy_clear(&mdtfile, 1);
    if (!mdt_real_load(buffer, size, &mdtfile, error)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    n = 0;
    data = gwy_container_new();
    key = g_string_new(NULL);
    for (i = 0; i <= mdtfile.last_frame; i++) {
        if (mdtfile.frames[i].type == MDT_FRAME_SCANNED) {
            MDTScannedDataFrame *sdframe;

            sdframe = (MDTScannedDataFrame*)mdtfile.frames[i].frame_data;
            dfield = extract_scanned_data(sdframe);
            g_string_printf(key, "/%d/data", n);
            gwy_container_set_object_by_name(data, key->str, dfield);
            g_object_unref(dfield);

            if (sdframe->title) {
                g_string_append(key, "/title");
                gwy_container_set_string_by_name(data, key->str,
                                                 g_strndup(sdframe->title,
                                                           sdframe->title_len));
            }
            else
                gwy_app_channel_title_fall_back(data, n);

            meta = mdt_get_metadata(&mdtfile, n);
            mdt_add_frame_metadata(sdframe, meta);
            g_string_printf(key, "/%d/meta", n);
            gwy_container_set_object_by_name(data, key->str, meta);
            g_object_unref(meta);

            n++;
        }
        else if (mdtfile.frames[i].type == MDT_FRAME_MDA) {
            MDTMDAFrame *mdaframe;
            mdaframe = (MDTMDAFrame*)mdtfile.frames[i].frame_data;
            gwy_debug("dimensions %d ; measurands %d",
                      mdaframe->nDimensions, mdaframe->nMesurands);
            if (mdaframe->nDimensions == 2 && mdaframe->nMesurands == 1) {
                // scan
                dfield = extract_mda_data(mdaframe);
                g_string_printf(key, "/%d/data", n);
                gwy_container_set_object_by_name(data, key->str, dfield);
                g_object_unref(dfield);
                gwy_app_channel_title_fall_back(data, n);
                n++;
            }
            else if (mdaframe->nDimensions == 0 && mdaframe->nMesurands == 2) {
                // raman spectra
                GwyGraphModel *gmodel;

                gmodel = extract_mda_spectrum(mdaframe);
                g_string_printf(key, "/0/graph/graph/%d", n+1);
                gwy_container_set_object_by_name(data, key->str, gmodel);
                g_object_unref(gmodel);
                n++;
            }
        }
    }
    g_string_free(key, TRUE);
    gwy_file_abandon_contents(buffer, size, NULL);
    if (!n) {
        g_object_unref(data);
        err_NO_DATA(error);
        return NULL;
    }

    return data;
}

#define HASH_SET_META(fmt, val, key) \
    g_string_printf(s, fmt, val); \
    gwy_container_set_string_by_name(meta, key, g_strdup(s->str))

static GwyContainer*
mdt_get_metadata(MDTFile *mdtfile,
                 guint i)
{
    GwyContainer *meta;
    MDTFrame *frame;
    MDTScannedDataFrame *sdframe;
    const gchar *v;
    GString *s;

    meta = gwy_container_new();

    g_return_val_if_fail(i <= mdtfile->last_frame, meta);
    frame = mdtfile->frames + i;
    g_return_val_if_fail(frame->type == MDT_FRAME_SCANNED, meta);
    sdframe = (MDTScannedDataFrame*)frame->frame_data;

    s = g_string_new(NULL);
    g_string_printf(s, "%d-%02d-%02d %02d:%02d:%02d",
                    frame->year, frame->month, frame->day,
                    frame->hour, frame->min, frame->sec);
    gwy_container_set_string_by_name(meta, "Date", g_strdup(s->str));

    g_string_printf(s, "%d.%d",
                    frame->version/0x100, frame->version % 0x100);
    gwy_container_set_string_by_name(meta, "Version", g_strdup(s->str));

    g_string_printf(s, "%s, %s %s %s",
                    (sdframe->scan_dir & 0x01) ? "Horizontal" : "Vertical",
                    (sdframe->scan_dir & 0x02) ? "Left" : "Right",
                    (sdframe->scan_dir & 0x04) ? "Bottom" : "Top",
                    (sdframe->scan_dir & 0x80) ? " (double pass)" : "");
    gwy_container_set_string_by_name(meta, "Scan direction", g_strdup(s->str));

    HASH_SET_META("%d", sdframe->adc_index + 1, "ADC index");
    HASH_SET_META("%d", sdframe->mode, "Mode");
    HASH_SET_META("%d", sdframe->ndacq, "Step (DAC)");
    HASH_SET_META("%.2f nm", sdframe->step_length/Nano, "Step length");
    HASH_SET_META("%.0f nm/s", sdframe->velocity/Nano, "Scan velocity");
    HASH_SET_META("%.2f nA", sdframe->setpoint/Nano, "Setpoint value");
    HASH_SET_META("%.2f V", sdframe->bias_voltage, "Bias voltage");

    g_string_free(s, TRUE);

    if ((v = gwy_enuml_to_string(sdframe->channel_index,
                                 "Off", MDT_ADC_MODE_OFF,
                                 "Height", MDT_ADC_MODE_HEIGHT,
                                 "DFL", MDT_ADC_MODE_DFL,
                                 "Lateral F", MDT_ADC_MODE_LATERAL_F,
                                 "Bias V", MDT_ADC_MODE_BIAS_V,
                                 "Current", MDT_ADC_MODE_CURRENT,
                                 "FB-Out", MDT_ADC_MODE_FB_OUT,
                                 "MAG", MDT_ADC_MODE_MAG,
                                 "MAG*Sin", MDT_ADC_MODE_MAG_SIN,
                                 "MAG*Cos", MDT_ADC_MODE_MAG_COS,
                                 "RMS", MDT_ADC_MODE_RMS,
                                 "CalcMag", MDT_ADC_MODE_CALCMAG,
                                 "Phase1", MDT_ADC_MODE_PHASE1,
                                 "Phase2", MDT_ADC_MODE_PHASE2,
                                 "CalcPhase", MDT_ADC_MODE_CALCPHASE,
                                 "Ex1", MDT_ADC_MODE_EX1,
                                 "Ex2", MDT_ADC_MODE_EX2,
                                 "HvX", MDT_ADC_MODE_HVX,
                                 "HvY", MDT_ADC_MODE_HVY,
                                 "Snap Back", MDT_ADC_MODE_SNAP_BACK,
                                 NULL)))
        gwy_container_set_string_by_name(meta, "ADC Mode", g_strdup(v));

    return meta;
}

static void
mdt_frame_xml_text(GMarkupParseContext *context,
                   const gchar *text,
                   gsize text_len,
                   gpointer user_data,
                   G_GNUC_UNUSED GError **error)
{
    static const struct {
        const gchar *elem;
        const gchar *name;
        guint len;
        const GwyFlatEnum *table;
        const gchar *names;
    }
    metas[] = {
        {
            "Technique", "SPM Technique",
            G_N_ELEMENTS(mdt_spm_techniques),
            mdt_spm_techniques, mdt_spm_techniques_name,
        },
        {
            "SPMMode", "SPM Mode",
            G_N_ELEMENTS(mdt_spm_modes),
            mdt_spm_modes, mdt_spm_modes_name,
        },
    };
    GwyContainer *meta = (GwyContainer*)user_data;
    gchar *t, *end;
    const gchar *elem, *value;
    guint i, v;

    elem = g_markup_parse_context_get_element(context);
    for (i = 0; i < G_N_ELEMENTS(metas); i++) {
        if (!gwy_strequal(elem, metas[i].elem))
            continue;

        t = g_strndup(text, text_len);
        v = strtol(t, &end, 10);
        if (end != t) {
            value = gwy_flat_enum_to_string(v, metas[i].len,
                                            metas[i].table, metas[i].names);
            if (value && *value)
                gwy_container_set_string_by_name(meta, metas[i].name,
                                                 g_strdup(value));
        }
        g_free(t);

        break;
    }
}

static void
mdt_add_frame_metadata(MDTScannedDataFrame *sdframe,
                       GwyContainer *meta)
{
    GMarkupParseContext *context;
    GMarkupParser parser;

    if (!sdframe->xmlstuff)
        return;

    gwy_clear(&parser, 1);
    parser.text = &mdt_frame_xml_text;

    context = g_markup_parse_context_new(&parser, 0, meta, NULL);
    g_markup_parse_context_parse(context, sdframe->xmlstuff, -1, NULL);
    g_markup_parse_context_free(context);

    g_free(sdframe->xmlstuff);
    sdframe->xmlstuff = NULL;
}

static void
mdt_read_axis_scales(const guchar *p,
                     MDTAxisScale *x_scale,
                     MDTAxisScale *y_scale,
                     MDTAxisScale *z_scale)
{
    x_scale->offset = gwy_get_gfloat_le(&p);
    x_scale->step = gwy_get_gfloat_le(&p);
    x_scale->unit = (gint16)gwy_get_guint16_le(&p);
    gwy_debug("x: *%g +%g [%d:%s]",
              x_scale->step, x_scale->offset, x_scale->unit,
              gwy_flat_enum_to_string(x_scale->unit,
                                      G_N_ELEMENTS(mdt_units),
                                      mdt_units, mdt_units_name));
    x_scale->step = fabs(x_scale->step);
    if (!x_scale->step) {
        g_warning("x_scale.step == 0, changing to 1");
        x_scale->step = 1.0;
    }

    y_scale->offset = gwy_get_gfloat_le(&p);
    y_scale->step = gwy_get_gfloat_le(&p);
    y_scale->unit = (gint16)gwy_get_guint16_le(&p);
    gwy_debug("y: *%g +%g [%d:%s]",
              y_scale->step, y_scale->offset, y_scale->unit,
              gwy_flat_enum_to_string(y_scale->unit,
                                      G_N_ELEMENTS(mdt_units),
                                      mdt_units, mdt_units_name));
    y_scale->step = fabs(y_scale->step);
    if (!y_scale->step) {
        g_warning("y_scale.step == 0, changing to 1");
        y_scale->step = 1.0;
    }

    z_scale->offset = gwy_get_gfloat_le(&p);
    z_scale->step = gwy_get_gfloat_le(&p);
    z_scale->unit = (gint16)gwy_get_guint16_le(&p);
    gwy_debug("z: *%g +%g [%d:%s]",
              z_scale->step, z_scale->offset, z_scale->unit,
              gwy_flat_enum_to_string(z_scale->unit,
                                      G_N_ELEMENTS(mdt_units),
                                      mdt_units, mdt_units_name));
    if (!z_scale->step) {
        g_warning("z_scale.step == 0, changing to 1");
        z_scale->step = 1.0;
    }
}

static gboolean
mdt_scanned_data_vars(const guchar *p,
                      const guchar *fstart,
                      MDTScannedDataFrame *frame,
                      guint frame_size,
                      guint vars_size,
                      GError **error)
{
    mdt_read_axis_scales(p, &frame->x_scale, &frame->y_scale, &frame->z_scale);
    p += AXIS_SCALES_SIZE;

    frame->channel_index = (gint)(*p++);
    frame->mode = (gint)(*p++);
    frame->xres = gwy_get_guint16_le(&p);
    frame->yres = gwy_get_guint16_le(&p);
    gwy_debug("channel_index = %d, mode = %d, xres = %d, yres = %d",
              frame->channel_index, frame->mode, frame->xres, frame->yres);
    frame->ndacq = gwy_get_guint16_le(&p);
    frame->step_length = Angstrom*gwy_get_gfloat_le(&p);
    frame->adt = gwy_get_guint16_le(&p);
    frame->adc_gain_amp_log10 = (guint)(*p++);
    frame->adc_index = (guint)(*p++);
    frame->s16.version = (guint)(*p++);
    frame->s17.pass_num = (guint)(*p++);
    frame->scan_dir = (guint)(*p++);
    frame->power_of_2 = (gboolean)(*p++);
    frame->velocity = Angstrom*gwy_get_gfloat_le(&p);
    frame->setpoint = Nano*gwy_get_gfloat_le(&p);
    frame->bias_voltage = gwy_get_gfloat_le(&p);
    frame->draw = (gboolean)(*p++);
    p++;
    frame->xoff = gwy_get_gint32_le(&p);
    frame->yoff = gwy_get_gint32_le(&p);
    frame->nl_corr = (gboolean)(*p++);

    p = fstart + FRAME_HEADER_SIZE + vars_size;
    if ((guint)(p - fstart) + FRAME_MODE_SIZE > frame_size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Frame is too short for Frame Mode."));
        return FALSE;
    }
    frame->fm_mode = gwy_get_guint16_le(&p);
    frame->fm_xres = gwy_get_guint16_le(&p);
    frame->fm_yres = gwy_get_guint16_le(&p);
    frame->fm_ndots = gwy_get_guint16_le(&p);
    gwy_debug("mode = %u, xres = %u, yres = %u, ndots = %u",
              frame->fm_mode, frame->fm_xres, frame->fm_yres, frame->fm_ndots);

    if ((guint)(p - fstart)
        + sizeof(gint16)*(2*frame->fm_ndots + frame->fm_xres * frame->fm_yres)
        > frame_size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Frame is too short for dots or data."));
        return FALSE;
    }

    if (frame->fm_ndots) {
        frame->dots = p;
        p += sizeof(gint16)*2*frame->fm_ndots;
    }
    if (frame->fm_xres * frame->fm_yres) {
        frame->image = p;
        p += sizeof(gint16)*frame->fm_xres*frame->fm_yres;
    }

    gwy_debug("remaining stuff size: %u", (guint)(frame_size - (p - fstart)));

    /* Title */
    if ((frame_size - (p - fstart)) > 4) {
        frame->title_len = gwy_get_guint32_le(&p);
        if (frame->title_len
            && (guint)(frame_size - (p - fstart)) >= frame->title_len) {
            frame->title = p;
            p += frame->title_len;
            gwy_debug("title = <%.*s>", frame->title_len, frame->title);
        }
    }

    /* XML stuff */
    if ((frame_size - (p - fstart)) > 4) {
        guint len = gwy_get_guint32_le(&p);

        if (len && (guint)(frame_size - (p - fstart)) >= len) {
            frame->xmlstuff = g_convert((const gchar*)p, len, "UTF-8", "UTF-16",
                                        NULL, NULL, NULL);
            p += len;
        }
    }

#ifdef DEBUG
    {
        GString *str;
        guint i;

        str = g_string_new(NULL);
        for (i = 0; i < (guint)(frame_size - (p -fstart)); i++) {
            if (g_ascii_isprint(p[i]))
                g_string_append_c(str, p[i]);
            else
                g_string_append_printf(str, "." /*, p[i] */);
        }
        gwy_debug("stuff: %s", str->str);
        g_string_free(str, TRUE);
    }
#endif

    return TRUE;
}

static void
mdt_read_mda_calibration(const guchar *p, MDTMDACalibration *calibration)
{
    guint  structLen;
    const guchar *sp;

    gwy_debug("Reading MDA calibration");
    calibration->totLen = gwy_get_guint32_le(&p);
    structLen = gwy_get_guint32_le(&p);
    sp = p+structLen;
    calibration->nameLen = gwy_get_guint32_le(&p);
    calibration->commentLen = gwy_get_guint32_le(&p);
    calibration->unitLen = gwy_get_guint32_le(&p);

    calibration->siUnit   = gwy_get_guint64_le(&p);
    calibration->accuracy = gwy_get_gdouble_le(&p);
    p+=8;  // skip function id and dimensions
    calibration->bias  = gwy_get_gdouble_le(&p);
    calibration->scale  = gwy_get_gdouble_le(&p);
    gwy_debug("Scale= %f", calibration->scale);
    calibration->minIndex  = gwy_get_guint64_le(&p);
    calibration->maxIndex  = gwy_get_guint64_le(&p);
    gwy_debug("minIndex %d, maxIndex %d", (gint)calibration->minIndex,(gint)calibration->maxIndex);
    calibration->dataType  = gwy_get_gint32_le(&p);
    calibration->authorLen  = gwy_get_guint32_le(&p);

    p = sp;
    if (calibration->nameLen > 0) {
        calibration->name = p;
        p += calibration->nameLen;
        gwy_debug("name = %.*s", calibration->nameLen, calibration->name);
    }
    else
        calibration->name = NULL;

    if (calibration->commentLen > 0) {
        calibration->comment = p;
        p += calibration->commentLen;
        gwy_debug("comment = %.*s", calibration->commentLen,
                  calibration->comment);
    }
    else
        calibration->comment = NULL;

    if (calibration->unitLen > 0) {
        calibration->unit = p;
        p += calibration->unitLen;
        gwy_debug("unit = %.*s", calibration->unitLen, calibration->unit);
    }
    else
        calibration->unit = NULL;

    if (calibration->authorLen > 0) {
        calibration->author = p;
        p += calibration->authorLen;
        gwy_debug("author = %.*s", calibration->authorLen, calibration->author);
    }
    else
        calibration->author = NULL;
}

static gboolean
mdt_mda_vars(const guchar *p,
             const guchar *fstart,
             MDTMDAFrame *frame,
             guint frame_size,
             guint vars_size,
             G_GNUC_UNUSED GError **error)
{

    guint headSize, totLen, NameSize, CommSize, ViewInfoSize, SpecSize;
    guint SourceInfoSize, VarSize, DataSize, StructLen, CellSize;
    guint64  num;
    const guchar *recordPointer = p;
    const guchar *structPointer;

    gwy_debug("Rerad MDA header");
    headSize    = gwy_get_guint32_le(&p);
    gwy_debug("headSize %u\n",headSize);
    totLen      = gwy_get_guint32_le(&p);
    gwy_debug("totLen %u\n",totLen);
    p += 16*2 + 4; // skip guids and frame status

    NameSize    = gwy_get_guint32_le(&p);
    gwy_debug("NameSize %u\n",NameSize);
    CommSize    = gwy_get_guint32_le(&p);
    gwy_debug("CommSize %u\n",CommSize);
    ViewInfoSize= gwy_get_guint32_le(&p);
    gwy_debug("ViewInfoSize %u\n",ViewInfoSize);
    SpecSize    = gwy_get_guint32_le(&p);
    gwy_debug("SpecSize %u\n",SpecSize);
    SourceInfoSize= gwy_get_guint32_le(&p);
    gwy_debug("SourceInfoSize %u\n",SourceInfoSize);
    VarSize        = gwy_get_guint32_le(&p);
    gwy_debug("VarSize %u\n",VarSize);
    p +=4 ; // skip data offset
    DataSize    = gwy_get_guint32_le(&p);
    gwy_debug("DataSize %u\n",DataSize);
    p = recordPointer + headSize;
    frame->title_len = NameSize;

    if (NameSize && (guint)(frame_size - (p - fstart)) >= NameSize) {
        frame->title = p;
        p += NameSize;
    }
    else
        frame->title = NULL;


    if (CommSize && (guint) (frame_size - (p - fstart)) >= CommSize) {
        frame->xmlstuff =
            g_convert((const gchar *)p, CommSize, "UTF-8", "UTF-16", NULL, NULL,
                      NULL);
        p += CommSize;
    }
    else
        frame->xmlstuff = NULL;

    p += SpecSize + ViewInfoSize + SourceInfoSize;      // skip FrameSpec ViewInfo SourceInfo and vars

    p += 4;                     // skip total size
    StructLen = gwy_get_guint32_le(&p);
    structPointer = p;
    num = gwy_get_guint64_le(&p);
    CellSize = gwy_get_guint32_le(&p);

    frame->nDimensions = gwy_get_guint32_le(&p);
    frame->nMesurands = gwy_get_guint32_le(&p);
    p = structPointer + StructLen;

    if (frame->nDimensions) {
        int i;

        frame->dimensions = g_new0(MDTMDACalibration, frame->nDimensions);
        for (i = 0; i < frame->nDimensions; i++) {
            mdt_read_mda_calibration(p, &frame->dimensions[i]);
            p += frame->dimensions[i].totLen;
        }

    }
    else
        frame->dimensions = NULL;

    if (frame->nMesurands) {
        int i;

        frame->mesurands = g_new0(MDTMDACalibration, frame->nMesurands);
        for (i = 0; i < frame->nMesurands; i++) {
            mdt_read_mda_calibration(p, &frame->mesurands[i]);
            p += frame->mesurands[i].totLen;
        }

    }
    else
        frame->mesurands = NULL;

    frame->image = p;

    return TRUE;
}

static gboolean
mdt_real_load(const guchar *buffer,
              guint size,
              MDTFile *mdtfile,
              GError **error)
{
    guint i;
    const guchar *p, *fstart;
    MDTScannedDataFrame *scannedframe;
    MDTMDAFrame *mdaframe;

    /* File Header */
    if (size < 32) {
        err_TOO_SHORT(error);
        return FALSE;
    }
    p = buffer + 4;  /* magic header */
    mdtfile->size = gwy_get_guint32_le(&p);
    gwy_debug("File size (w/o header): %u", mdtfile->size);
    p += 4;  /* reserved */
    mdtfile->last_frame = gwy_get_guint16_le(&p);
    gwy_debug("Last frame: %u", mdtfile->last_frame);
    p += 18;  /* reserved */
    /* XXX: documentation specifies 32 bytes long header, but zeroth frame
     * starts at 33th byte in reality */
    p++;

    if (err_SIZE_MISMATCH(error, size, mdtfile->size + 33, TRUE))
        return FALSE;

    /* Frames */
    mdtfile->frames = g_new0(MDTFrame, mdtfile->last_frame + 1);
    for (i = 0; i <= mdtfile->last_frame; i++) {
        MDTFrame *frame = mdtfile->frames + i;

        fstart = p;
        if ((guint)(p - buffer) + FRAME_HEADER_SIZE > size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("End of file reached in frame header #%u."), i);
            return FALSE;
        }
        frame->size = gwy_get_guint32_le(&p);
        gwy_debug("Frame #%u size: %u", i, frame->size);
        if ((guint)(p - buffer) + frame->size - 4 > size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("End of file reached in frame data #%u."), i);
            return FALSE;
        }
        frame->type = gwy_get_guint16_le(&p);
#ifdef DEBUG
        gwy_debug("Frame #%u type: %s", i,
                  gwy_enum_to_string(frame->type,
                                     frame_types, G_N_ELEMENTS(frame_types)));
#endif
        frame->version = ((guint)p[0] << 8) + (gsize)p[1];
        p += 2;
        gwy_debug("Frame #%u version: %d.%d",
                  i, frame->version/0x100, frame->version % 0x100);
        frame->year = gwy_get_guint16_le(&p);
        frame->month = gwy_get_guint16_le(&p);
        frame->day = gwy_get_guint16_le(&p);
        frame->hour = gwy_get_guint16_le(&p);
        frame->min = gwy_get_guint16_le(&p);
        frame->sec = gwy_get_guint16_le(&p);
        gwy_debug("Frame #%u datetime: %d-%02d-%02d %02d:%02d:%02d",
                  i, frame->year, frame->month, frame->day,
                  frame->hour, frame->min, frame->sec);
        frame->var_size = gwy_get_guint16_le(&p);
        gwy_debug("Frame #%u var size: %u", i, frame->var_size);
        if (err_SIZE_MISMATCH(error, frame->var_size + FRAME_HEADER_SIZE,
                              frame->size, FALSE))
            return FALSE;

        switch (frame->type) {
            case MDT_FRAME_SCANNED:
            if (frame->var_size < AXIS_SCALES_SIZE + SCAN_VARS_MIN_SIZE) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Frame #%u is too short for "
                              "scanned data header."), i);
                return FALSE;
            }
            scannedframe = g_new0(MDTScannedDataFrame, 1);
            if (!mdt_scanned_data_vars(p, fstart, scannedframe,
                                       frame->size, frame->var_size, error))
                return FALSE;
            frame->frame_data = scannedframe;
            break;

            case MDT_FRAME_SPECTROSCOPY:
            gwy_debug("Spectroscropy frames make little sense to read now");
            break;

            case MDT_FRAME_TEXT:
            gwy_debug("Cannot read text frame");
            /*
            p = fstart + FRAME_HEADER_SIZE + frame->var_size;
            p += 16;
            for (j = 0; j < frame->size - (p - fstart); j++)
                g_print("%c", g_ascii_isprint(p[j]) ? p[j] : '.');
            g_printerr("%s\n", g_convert(p, frame->size - (p - fstart),
                                         "UCS-2", "UTF-8", NULL, &j, NULL));
                                         */
            break;

            case MDT_FRAME_OLD_MDA:
            gwy_debug("Cannot read old MDA frame");
            break;

            case MDT_FRAME_MDA:
            mdaframe = g_new0(MDTMDAFrame, 1);
            if (!mdt_mda_vars(p, fstart, mdaframe,
                              frame->size, frame->var_size, error))
                return FALSE;
            frame->frame_data = mdaframe;

            break;

            case MDT_FRAME_PALETTE:
            gwy_debug("Cannot read palette frame");
            break;

            default:
            g_warning("Unknown frame type %d", frame->type);
            break;
        }

        p = fstart + frame->size;
    }

    return TRUE;
}

static GwyDataField*
extract_scanned_data(MDTScannedDataFrame *dataframe)
{
    GwyDataField *dfield;
    GwySIUnit *siunitxy, *siunitz;
    guint i;
    gdouble *data;
    gdouble xreal, yreal, zscale;
    gint power10xy, power10z;
    const gint16 *p;
    const gchar *unit;

    if (dataframe->x_scale.unit != dataframe->y_scale.unit)
        g_warning("Different x and y units, using x for both (incorrect).");
    unit = gwy_flat_enum_to_string(dataframe->x_scale.unit,
                                   G_N_ELEMENTS(mdt_units),
                                   mdt_units, mdt_units_name);
    siunitxy = gwy_si_unit_new_parse(unit, &power10xy);
    xreal = dataframe->fm_xres*pow10(power10xy)*dataframe->x_scale.step;
    yreal = dataframe->fm_yres*pow10(power10xy)*dataframe->y_scale.step;

    unit = gwy_flat_enum_to_string(dataframe->z_scale.unit,
                                   G_N_ELEMENTS(mdt_units),
                                   mdt_units, mdt_units_name);
    siunitz = gwy_si_unit_new_parse(unit, &power10z);
    zscale = pow10(power10z)*dataframe->z_scale.step;

    dfield = gwy_data_field_new(dataframe->fm_xres, dataframe->fm_yres,
                                xreal, yreal,
                                FALSE);
    gwy_data_field_set_si_unit_xy(dfield, siunitxy);
    g_object_unref(siunitxy);
    gwy_data_field_set_si_unit_z(dfield, siunitz);
    g_object_unref(siunitz);

    data = gwy_data_field_get_data(dfield);
    p = (gint16*)dataframe->image;
    for (i = 0; i < dataframe->fm_xres*dataframe->fm_yres; i++)
        data[i] = zscale*GINT16_FROM_LE(p[i]);

    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);

    return dfield;
}

static gint
unitCodeForSiCode(guint64 siCode)
{

    switch (siCode) {
        case G_GUINT64_CONSTANT(0x0000000000000001):
        return MDT_UNIT_NONE;

        case G_GUINT64_CONSTANT(0x0000000000000101):
        return MDT_UNIT_METER; // Meter

        case G_GUINT64_CONSTANT(0x0000000000100001):
        return MDT_UNIT_AMPERE2; // Ampere

        case G_GUINT64_CONSTANT(0x000000fffd010200):
        return MDT_UNIT_VOLT2; // volt

        case G_GUINT64_CONSTANT(0x0000000001000001):
        return MDT_UNIT_SECOND; // second

        default:
        return MDT_UNIT_NONE;
    }
    return MDT_UNIT_NONE; // dimensionless
}

static GwyDataField *
extract_mda_data(MDTMDAFrame * dataframe)
{
    GwyDataField *dfield;
    gdouble *data, *end_data;
    gdouble xreal, yreal, zscale;
    gint power10xy, power10z;
    GwySIUnit *siunitxy, *siunitz;
    gint total;
    const guchar *p;
    const gchar *cunit;
    gchar *unit;

    MDTMDACalibration *xAxis = &dataframe->dimensions[0],
        *yAxis = &dataframe->dimensions[0], *zAxis = &dataframe->mesurands[0];

    if (xAxis->unit && xAxis->unitLen) {
        unit = g_strndup(xAxis->unit, xAxis->unitLen);
        siunitxy = gwy_si_unit_new_parse(unit, &power10xy);
        g_free(unit);
    }
    else {
        cunit = gwy_flat_enum_to_string(unitCodeForSiCode(xAxis->siUnit),
                                        G_N_ELEMENTS(mdt_units),
                                        mdt_units, mdt_units_name);
        siunitxy = gwy_si_unit_new_parse(cunit, &power10xy);
    }
    gwy_debug("xy unit power %d", power10xy);

    if (zAxis->unit && zAxis->unitLen) {
        unit = g_strndup(zAxis->unit, zAxis->unitLen);
        siunitz = gwy_si_unit_new_parse(unit, &power10z);
        g_free(unit);
    }
    else {
        cunit = gwy_flat_enum_to_string(unitCodeForSiCode(zAxis->siUnit),
                                        G_N_ELEMENTS(mdt_units),
                                        mdt_units, mdt_units_name);
        siunitz = gwy_si_unit_new_parse(cunit, &power10z);
    }
    gwy_debug("z unit power %d", power10xy);

    xreal = pow10(power10xy) * xAxis->scale;
    yreal = pow10(power10xy) * yAxis->scale;
    zscale = pow10(power10z) * zAxis->scale;

    dfield = gwy_data_field_new(xAxis->maxIndex - xAxis->minIndex + 1,
                                yAxis->maxIndex - yAxis->minIndex + 1,
                                xreal, yreal, FALSE);
    total =
        (xAxis->maxIndex - xAxis->minIndex + 1) * (yAxis->maxIndex -
                                                   yAxis->minIndex + 1);
    gwy_data_field_set_si_unit_xy(dfield, siunitxy);
    g_object_unref(siunitxy);
    gwy_data_field_set_si_unit_z(dfield, siunitz);
    g_object_unref(siunitz);

    data = gwy_data_field_get_data(dfield);
    p = (gchar *)dataframe->image;
    gwy_debug("total points %d; data type %d; cell size %d",
              total, zAxis->dataType, dataframe->cellSize);
    end_data = data + total;
    switch (zAxis->dataType) {
        case MDA_DATA_INT8:
        {
            const gchar *tp = p;

            while (data < end_data)
                *(data++) = zscale * (*(tp++));
        }
        break;

        case MDA_DATA_UINT8:
        {
            const guchar *tp = (const guchar *)p;

            while (data < end_data)
                *(data++) = zscale * (*(tp++));
        }
        break;

        case MDA_DATA_INT16:
        {
            const gint16 *tp = (const gint16 *)p;

            while (data < end_data) {
                *(data++) = zscale * GINT16_FROM_LE(*tp);
                tp++;
            }
        }
        break;

        case MDA_DATA_UINT16:
        {
            const guint16 *tp = (const guint16 *)p;

            while (data < end_data) {
                *(data++) = zscale * GUINT16_FROM_LE(*tp);
                tp++;
            }
        }
        break;

        case MDA_DATA_INT32:
        {
            const gint32 *tp = (const gint32 *)p;

            while (data < end_data) {
                *(data++) = zscale * GINT32_FROM_LE(*tp);
                tp++;
            }
        }
        break;

        case MDA_DATA_UINT32:
        {
            const guint32 *tp = (const guint32 *)p;

            while (data < end_data) {
                *(data++) = zscale * GUINT32_FROM_LE(*tp);
                tp++;
            }
        }
        break;

        case MDA_DATA_INT64:
        {
            const gint64 *tp = (const gint64 *)p;

            while (data < end_data) {
                /* for some reason, MSVC6 spits an unsigned int64 conversion
                 * error also here */
                *(data++) = zscale * (gint64)GINT64_FROM_LE(*tp);
                tp++;
            }
        }
        break;

        case MDA_DATA_UINT64:
        {
            const guint64 *tp = (const guint64 *)p;

            while (data < end_data) {
                /* Fucking MSVC6 cannot convert unsigned 64bit int to double. */
#ifdef _MSC_VER
                guint u32h = *tp >> 32u;
                guint u32l = *tp & 0xffffffffu;
                *(data++) = 4294967296.0*u32h + u32l;
#else
                *(data++) = zscale * GUINT64_FROM_LE(*tp);
#endif
                tp++;
            }
        }
        break;

        case MDA_DATA_FLOAT32:
        while (data < end_data)
            *(data++) = zscale * gwy_get_gfloat_le(&p);
        break;

        case MDA_DATA_FLOAT64:
        while (data < end_data)
            *(data++) = zscale * gwy_get_gdouble_le(&p);
        break;

        default:
        g_assert_not_reached();
        break;
    }
    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);
    return dfield;
}

static GwyGraphModel*
extract_mda_spectrum(MDTMDAFrame *dataframe)
{
    /* FIXME: I don't know where to find this 1024 points per spectra */
    enum { res = 1024 };
    GwyGraphCurveModel *spectra;
    GwyGraphModel *gmodel;
    gdouble xscale, yscale;
    gint power10x, power10y;
    GwySIUnit *siunitx, *siunity;
    gdouble xdata[res], ydata[res];
    const guchar *p;
    const gchar *cunit;
    gchar *unit;
    gint i;

    MDTMDACalibration *xAxis = &dataframe->mesurands[0],
                      *yAxis = &dataframe->mesurands[1];

    if (xAxis->unit && xAxis->unitLen) {
        unit = g_strndup(xAxis->unit, xAxis->unitLen);
        siunitx = gwy_si_unit_new_parse(unit, &power10x);
        g_free(unit);
    }
    else {
        cunit = gwy_flat_enum_to_string(unitCodeForSiCode(xAxis->siUnit),
                                        G_N_ELEMENTS(mdt_units),
                                        mdt_units, mdt_units_name);
        siunitx = gwy_si_unit_new_parse(cunit, &power10x);
    }
    gwy_debug("x unit power %d", power10x);

    if (yAxis->unit && yAxis->unitLen) {
        unit = g_strndup(yAxis->unit, yAxis->unitLen);
        siunity = gwy_si_unit_new_parse(unit, &power10y);
        g_free(unit);
    }
    else {
        cunit = gwy_flat_enum_to_string(unitCodeForSiCode(yAxis->siUnit),
                                        G_N_ELEMENTS(mdt_units),
                                        mdt_units, mdt_units_name);
        siunity = gwy_si_unit_new_parse(cunit, &power10y);
    }
    gwy_debug("y unit power %d", power10y);

    xscale = pow10(power10x) * xAxis->scale;
    yscale = pow10(power10y) * yAxis->scale;

    spectra = gwy_graph_curve_model_new();
    g_object_set(spectra,
                 "description", "Raman spectra",
                 "mode", GWY_GRAPH_CURVE_LINE,
                 NULL);

    p = (gchar*)dataframe->image;

    for (i = 0; i < res; i++) {
        xdata[i] = xscale * gwy_get_gfloat_le(&p);
        ydata[i] = yscale * gwy_get_gfloat_le(&p);
    }
    gwy_graph_curve_model_set_data(spectra, xdata, ydata, res);

    gmodel = gwy_graph_model_new();
    g_object_set(gmodel,
                 "title", "Raman spectra",
                 "si-unit-x", siunitx,
                 "si-unit-y", siunity,
                 NULL);
    gwy_graph_model_add_curve(gmodel, spectra);
    g_object_unref(spectra);
    g_object_unref(siunitx);
    g_object_unref(siunity);

    return gmodel;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
