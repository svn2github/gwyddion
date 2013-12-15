/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-iso28600-spm">
 *   <comment>ISO 28600:2011 SPM data transfer format</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="ISO/TC 201 SPM data transfer format"/>
 *   </magic>
 *   <glob pattern="*.spm"/>
 *   <glob pattern="*.SPM"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # ISO 28600:2011
 * 0 string ISO/TC\ 201\ SPM\ data\ transfer\ format ISO 28600:2011 SPM data transfer format
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * ISO 28600:2011 SPM data transfer format
 * .spm
 * Read Export SPS:Limited[1]
 * [1] Spectra curves are imported as graphs, positional information is lost.
 **/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC "ISO/TC 201 SPM data transfer format"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define EXTENSION ".spm"

#define EOD_MAGIC "end of experiment"

enum { MAX_CHANNELS = 8 };

typedef enum {
    ISO28600_FIXED,
    ISO28600_RESERVED,
    ISO28600_INTEGER,
    ISO28600_REAL_NUM,
    ISO28600_UNIT,
    ISO28600_TEXT_LINE,
    ISO28600_ENUM,
    ISO28600_INTEGERS,
    ISO28600_REAL_NUMS,
    ISO28600_UNITS,
    ISO28600_TEXT_LIST,
} ISO28600FieldType;

/* The value 0 always corresponds to unknown. */
typedef enum {
    ISO28600_EXPERIMENT_UNKNOWN,
    ISO28600_EXPERIMENT_MAP_SC,
    ISO28600_EXPERIMENT_MAP_MC,
    ISO28600_EXPERIMENT_SPEC_SC,
    ISO28600_EXPERIMENT_SPEC_MC,
} ISO28600ExperimentMode;

typedef enum {
    ISO28600_SCAN_UNKNOWN,
    ISO28600_SCAN_REGULAR_MAPPING,
    ISO28600_SCAN_IRREGULAR_MAPPING,
} ISO28600ScanMode;

typedef enum {
    ISO28600_SCANNING_SYSTEM_UNKNOWN,
    ISO28600_SCANNING_SYSTEM_OPEN_LOOP,
    ISO28600_SCANNING_SYSTEM_XY_CLOSED_LOOP,
    ISO28600_SCANNING_SYSTEM_XYZ_CLOSED_LOOP,
} ISO28600ScanningSystemType;

typedef enum {
    ISO28600_SCANNER_UNKNOWN,
    ISO28600_SCANNER_SAMPLE_XYZ,
    ISO28600_SCANNER_PROBE_XYZ,
    ISO28600_SCANNER_SAMPLE_XY_PROBE_Z,
    ISO28600_SCANNER_SAMPLE_Z_PROBE_XY,
} ISO28600ScannerType;

typedef enum {
    ISO28600_AXIS_UNKNOWN,
    ISO28600_AXIS_X,
    ISO28600_AXIS_Y,
} ISO28600AxisType;

typedef enum {
    ISO28600_BIAS_VOLTAGE_CONTACT_UNKNOWN,
    ISO28600_BIAS_VOLTAGE_CONTACT_SAMPLE,
    ISO28600_BIAS_VOLTAGE_CONTACT_TIP,
} ISO28600BiasVoltageContactType;

typedef enum {
    ISO28600_SPECTROSCOPY_SCAN_UNKNOWN,
    ISO28600_SPECTROSCOPY_SCAN_REGULAR,
    ISO28600_SPECTROSCOPY_SCAN_IRREGULAR,
} ISO28600SpectroscopyScanMode;

typedef enum {
    ISO28600_DATA_TREATMENT_UNKNOWNS,
    ISO28600_DATA_TREATMENT_RAW,
    ISO28600_DATA_TREATMENT_PRE_TREATED,
    ISO28600_DATA_TREATMENT_POST_TREATED,
} ISO28600DataTreatmentType;

typedef union {
    gint i;
    gdouble d;
    const gchar *s;
    struct { const gchar *str; gint value; } enumerated;
    struct { GwySIUnit *unit; gint power10; } unit;
    struct { const gchar **items; guint n; } text_list;
    struct { gint *items; guint n; } int_list;
    struct { gdouble *items; guint n; } real_list;
} ISO28600FieldValue;

static gboolean            module_register    (void);
static gint                iso28600_detect    (const GwyFileDetectInfo *fileinfo,
                                               gboolean only_name);
static GwyContainer*       iso28600_load      (const gchar *filename,
                                               GwyRunType mode,
                                               GError **error);
static gboolean            iso28600_export    (GwyContainer *data,
                                               const gchar *filename,
                                               GwyRunType mode,
                                               GError **error);
static ISO28600FieldValue* load_header        (gchar **buffer,
                                               gchar **strings,
                                               GError **error);
static void                free_header        (ISO28600FieldValue *header);
static GwyContainer*       load_channels      (ISO28600FieldValue *header,
                                               gchar **strings,
                                               gchar **p,
                                               ISO28600ExperimentMode experiment,
                                               guint nchannels,
                                               guint xres,
                                               guint yres,
                                               gdouble xreal,
                                               gdouble yreal,
                                               GError **error);
static GwyContainer*       load_spectra_graphs(ISO28600FieldValue *header,
                                               gchar **p,
                                               ISO28600SpectroscopyScanMode smode,
                                               guint nord,
                                               guint npts,
                                               GError **error);
static GwyContainer*       get_meta           (ISO28600FieldValue *header,
                                               gchar **strings,
                                               guint id);
static gchar*              convert_unit       (GwySIUnit *unit);
static void                build_unit         (const gchar *str,
                                               ISO28600FieldValue *value);
static void                build_enum         (const gchar *str,
                                               guint lineno,
                                               ISO28600FieldValue *value);
static gchar**             split_line_in_place(gchar *line,
                                               gchar delimiter,
                                               gboolean nonempty,
                                               gboolean strip,
                                               guint *count);

#define field_name(i) (header_fields_name + header_fields[i].name)

/* The enum numbers are line numbers from the norm.  The real line numbers
 * are 0-based and thus one smaller.  Some lines, e.g. "Label line" are
 * repeated so this is not an enum.  All sequences are comma-separated.
 * Date/time fields 9 to 15 are -1 if unknown.
 * Unit of 34 is deg, counterclockwise.
 * Unit of 41 is V.
 * Sequences in fields 43 to 47 must have the same number of items.
 * Unit of 50 is K.
 * Unit of 51 is Pa.
 * Unit of 52 is %.
 * Unit of 57 is N/m.
 * Unit of 58 is Hz.
 * Unit of 59 is usually V/nm.
 * Unit of 60 to 62 is deg, counterclockwise.
 * Sequences in fields 82 to 85 must have the same number of items.
 * Field 89 should probably be called *plane* correction, not plain.
 */
#ifdef GWY_RELOC_SOURCE
/* @fields: name, lineno, type */
static const ISO28600HeaderField header_fields[] = {
    { "ISO/TC 201 SPM data transfer format",         1,   ISO28600_FIXED,     },
    { "general information",                         2,   ISO28600_FIXED,     },
    { "Institution identifier",                      3,   ISO28600_TEXT_LINE, },
    { "Instrument model identifier",                 4,   ISO28600_TEXT_LINE, },
    { "Operator identifier",                         5,   ISO28600_TEXT_LINE, },
    { "Experiment identifier",                       6,   ISO28600_TEXT_LINE, },
    { "Comment (SPM summary)",                       7,   ISO28600_TEXT_LINE, },
    { "Experiment mode",                             8,   ISO28600_ENUM,      },
    { "Year in full",                                9,   ISO28600_INTEGER,   },
    { "Month",                                       10,  ISO28600_INTEGER,   },
    { "Day of month",                                11,  ISO28600_INTEGER,   },
    { "Hours",                                       12,  ISO28600_INTEGER,   },
    { "Minutes",                                     13,  ISO28600_INTEGER,   },
    { "Seconds",                                     14,  ISO28600_INTEGER,   },
    { "Number of hours in advance of GMT",           15,  ISO28600_INTEGER,   },
    { "scan information",                            16,  ISO28600_FIXED,     },
    { "Scan mode",                                   17,  ISO28600_ENUM,      },
    { "Scanning system",                             18,  ISO28600_ENUM,      },
    { "Scanner type",                                19,  ISO28600_ENUM,      },
    { "Fast scan axis",                              20,  ISO28600_ENUM,      },
    { "Fast scan direction",                         21,  ISO28600_TEXT_LINE, },
    { "Slow scan axis",                              22,  ISO28600_ENUM,      },
    { "Slow scan direction",                         23,  ISO28600_TEXT_LINE, },
    { "Number of discrete X coordintes in full map", 24,  ISO28600_INTEGER,   },
    { "Number of discrete Y coordintes in full map", 25,  ISO28600_INTEGER,   },
    { "Physical unit of X axis",                     26,  ISO28600_UNIT,      },
    { "Physical unit of Y axis",                     27,  ISO28600_UNIT,      },
    { "Field of view X",                             28,  ISO28600_REAL_NUM,  },
    { "Field of view Y",                             29,  ISO28600_REAL_NUM,  },
    { "Physical unit of X offset",                   30,  ISO28600_UNIT,      },
    { "Physical unit of Y offset",                   31,  ISO28600_UNIT,      },
    { "X offset",                                    32,  ISO28600_REAL_NUM,  },
    { "Y offset",                                    33,  ISO28600_REAL_NUM,  },
    { "Rotation angle",                              34,  ISO28600_REAL_NUM,  },
    { "Physical unit of scan speed",                 35,  ISO28600_UNIT,      },
    { "Scan speed",                                  36,  ISO28600_REAL_NUM,  },
    { "Physical unit of scan rate",                  37,  ISO28600_UNIT,      },
    { "Scan rate",                                   38,  ISO28600_REAL_NUM,  },
    { "SPM technique",                               39,  ISO28600_TEXT_LINE, },
    { "Bias voltage contact",                        40,  ISO28600_ENUM,      },
    { "Bias voltage",                                41,  ISO28600_REAL_NUM,  },
    { "Number of set items",                         42,  ISO28600_INTEGER,   },
    { "Set parameters",                              43,  ISO28600_TEXT_LIST, },
    { "Units of set parameters",                     44,  ISO28600_UNITS,     },
    { "Values of set parameters",                    45,  ISO28600_REAL_NUMS, },
    { "Calibration comments for set parameters",     46,  ISO28600_TEXT_LIST, },
    { "Calibrations for set parameters",             47,  ISO28600_REAL_NUMS, },
    { "environment description",                     48,  ISO28600_FIXED,     },
    { "Environment mode",                            49,  ISO28600_TEXT_LINE, },
    { "Sample temperature",                          50,  ISO28600_REAL_NUM,  },
    { "Surroundings pressure",                       51,  ISO28600_REAL_NUM,  },
    { "Environment humidity",                        52,  ISO28600_REAL_NUM,  },
    { "Comment (environment)",                       53,  ISO28600_TEXT_LINE, },
    { "probe description",                           54,  ISO28600_FIXED,     },
    { "Probe identifier",                            55,  ISO28600_TEXT_LINE, },
    { "Probe material",                              56,  ISO28600_TEXT_LINE, },
    { "Normal spring constant",                      57,  ISO28600_REAL_NUM,  },
    { "Resonance frequency",                         58,  ISO28600_REAL_NUM,  },
    { "Cantilever sensitvity",                       59,  ISO28600_REAL_NUM,  },
    { "Angle between probe and X axis",              60,  ISO28600_REAL_NUM,  },
    { "Angle between probe vertical movement and Z axis in X azimuth", 61, ISO28600_REAL_NUM, },
    { "Angle between probe vertical movement and Z axis in Y azimuth", 62, ISO28600_REAL_NUM, },
    { "Comment (probe)",                             63,  ISO28600_TEXT_LINE, },
    { "sample description",                          64,  ISO28600_FIXED,     },
    { "Sample identifier",                           65,  ISO28600_TEXT_LINE, },
    { "Species label",                               66,  ISO28600_TEXT_LINE, },
    { "Comment (sample)",                            67,  ISO28600_TEXT_LINE, },
    { "single-channel mapping description",          68,  ISO28600_FIXED,     },
    { "Z axis channel",                              69,  ISO28600_TEXT_LINE, },
    { "Physical unit of Z axis channel",             70,  ISO28600_UNIT,      },
    { "Comment (Z axis channel)",                    71,  ISO28600_TEXT_LINE, },
    { "spectroscopy description",                    72,  ISO28600_FIXED,     },
    { "Spectroscopy mode",                           73,  ISO28600_TEXT_LINE, },
    { "Spectroscopy scan mode",                      74,  ISO28600_ENUM,      },
    { "Abscissa label",                              75,  ISO28600_TEXT_LINE, },
    { "Abscissa unit",                               76,  ISO28600_UNIT,      },
    { "Abscissa start",                              77,  ISO28600_REAL_NUM,  },
    { "Abscissa end",                                78,  ISO28600_REAL_NUM,  },
    { "Abscissa increment",                          79,  ISO28600_REAL_NUM,  },
    { "Calibration constant for abscissa",           80,  ISO28600_REAL_NUM,  },
    { "Number of points in abscissa",                81,  ISO28600_INTEGER,   },
    { "Number of ordinate items",                    82,  ISO28600_INTEGER,   },
    { "Ordinate labels",                             83,  ISO28600_TEXT_LIST, },
    { "Ordinate units",                              84,  ISO28600_UNITS,     },
    { "Calibration constants for ordinates",         85,  ISO28600_REAL_NUMS, },
    { "Comment (spectroscopy)",                      86,  ISO28600_TEXT_LINE, },
    { "data treatment description",                  87,  ISO28600_FIXED,     },
    { "Data treatment",                              88,  ISO28600_ENUM,      },
    { "Plain correction",                            89,  ISO28600_TEXT_LINE, },
    { "Numerical filtering",                         90,  ISO28600_TEXT_LINE, },
    { "Image reconstruction",                        91,  ISO28600_TEXT_LINE, },
    { "Comment (data treatment)",                    92,  ISO28600_TEXT_LINE, },
    { "multi-channel mapping description",           93,  ISO28600_FIXED,     },
    { "Number of data channels",                     94,  ISO28600_INTEGER,   },
    { "1st data channel",                            95,  ISO28600_TEXT_LINE, },
    { "1st data channel units",                      96,  ISO28600_UNIT,      },
    { "1st data channel comment",                    97,  ISO28600_TEXT_LINE, },
    { "2st data channel",                            98,  ISO28600_TEXT_LINE, },
    { "2st data channel units",                      99,  ISO28600_UNIT,      },
    { "2st data channel comment",                    100, ISO28600_TEXT_LINE, },
    { "3st data channel",                            101, ISO28600_TEXT_LINE, },
    { "3st data channel units",                      102, ISO28600_UNIT,      },
    { "3st data channel comment",                    103, ISO28600_TEXT_LINE, },
    { "4st data channel",                            104, ISO28600_TEXT_LINE, },
    { "4st data channel units",                      105, ISO28600_UNIT,      },
    { "4st data channel comment",                    106, ISO28600_TEXT_LINE, },
    { "5st data channel",                            107, ISO28600_TEXT_LINE, },
    { "5st data channel units",                      108, ISO28600_UNIT,      },
    { "5st data channel comment",                    109, ISO28600_TEXT_LINE, },
    { "6st data channel",                            110, ISO28600_TEXT_LINE, },
    { "6st data channel units",                      111, ISO28600_UNIT,      },
    { "6st data channel comment",                    112, ISO28600_TEXT_LINE, },
    { "7st data channel",                            113, ISO28600_TEXT_LINE, },
    { "7st data channel units",                      114, ISO28600_UNIT,      },
    { "7st data channel comment",                    115, ISO28600_TEXT_LINE, },
    { "8st data channel",                            116, ISO28600_TEXT_LINE, },
    { "8st data channel units",                      117, ISO28600_UNIT,      },
    { "8st data channel comment",                    118, ISO28600_TEXT_LINE, },
    { "Comment (multi-channel mapping)",             119, ISO28600_TEXT_LINE, },
    { "",                                            120, ISO28600_RESERVED,  },
    { "",                                            121, ISO28600_RESERVED,  },
    { "",                                            122, ISO28600_RESERVED,  },
    { "",                                            123, ISO28600_RESERVED,  },
    { "",                                            124, ISO28600_RESERVED,  },
    { "",                                            125, ISO28600_RESERVED,  },
    { "",                                            126, ISO28600_RESERVED,  },
    { "",                                            127, ISO28600_RESERVED,  },
    { "end of header",                               128, ISO28600_FIXED,     },
};
#else  /* {{{ */
/* This code block was GENERATED by flatten.py.
   When you edit header_fields[] data above,
   re-run flatten.py SOURCE.c. */
static const gchar header_fields_name[] =
    "ISO/TC 201 SPM data transfer format\000general information\000Institut"
    "ion identifier\000Instrument model identifier\000Operator identifier"
    "\000Experiment identifier\000Comment (SPM summary)\000Experiment mode"
    "\000Year in full\000Month\000Day of month\000Hours\000Minutes\000Secon"
    "ds\000Number of hours in advance of GMT\000scan information\000Scan mo"
    "de\000Scanning system\000Scanner type\000Fast scan axis\000Fast scan d"
    "irection\000Slow scan axis\000Slow scan direction\000Number of discret"
    "e X coordintes in full map\000Number of discrete Y coordintes in full "
    "map\000Physical unit of X axis\000Physical unit of Y axis\000Field of "
    "view X\000Field of view Y\000Physical unit of X offset\000Physical uni"
    "t of Y offset\000X offset\000Y offset\000Rotation angle\000Physical un"
    "it of scan speed\000Scan speed\000Physical unit of scan rate\000Scan r"
    "ate\000SPM technique\000Bias voltage contact\000Bias voltage\000Number"
    " of set items\000Set parameters\000Units of set parameters\000Values o"
    "f set parameters\000Calibration comments for set parameters\000Calibra"
    "tions for set parameters\000environment description\000Environment mod"
    "e\000Sample temperature\000Surroundings pressure\000Environment humidi"
    "ty\000Comment (environment)\000probe description\000Probe identifier"
    "\000Probe material\000Normal spring constant\000Resonance frequency"
    "\000Cantilever sensitvity\000Angle between probe and X axis\000Angle b"
    "etween probe vertical movement and Z axis in X azimuth\000Angle betwee"
    "n probe vertical movement and Z axis in Y azimuth\000Comment (probe)"
    "\000sample description\000Sample identifier\000Species label\000Commen"
    "t (sample)\000single-channel mapping description\000Z axis channel\000"
    "Physical unit of Z axis channel\000Comment (Z axis channel)\000spectro"
    "scopy description\000Spectroscopy mode\000Spectroscopy scan mode\000Ab"
    "scissa label\000Abscissa unit\000Abscissa start\000Abscissa end\000Abs"
    "cissa increment\000Calibration constant for abscissa\000Number of poin"
    "ts in abscissa\000Number of ordinate items\000Ordinate labels\000Ordin"
    "ate units\000Calibration constants for ordinates\000Comment (spectrosc"
    "opy)\000data treatment description\000Data treatment\000Plain correcti"
    "on\000Numerical filtering\000Image reconstruction\000Comment (data tre"
    "atment)\000multi-channel mapping description\000Number of data channel"
    "s\0001st data channel\0001st data channel units\0001st data channel co"
    "mment\0002st data channel\0002st data channel units\0002st data channe"
    "l comment\0003st data channel\0003st data channel units\0003st data ch"
    "annel comment\0004st data channel\0004st data channel units\0004st dat"
    "a channel comment\0005st data channel\0005st data channel units\0005st"
    " data channel comment\0006st data channel\0006st data channel units"
    "\0006st data channel comment\0007st data channel\0007st data channel u"
    "nits\0007st data channel comment\0008st data channel\0008st data chann"
    "el units\0008st data channel comment\000Comment (multi-channel mapping"
    ")\000\000\000\000\000\000\000\000\000end of header";

static const struct {
    gint name;
    gint lineno;
    gint type;
}
header_fields[] = {
    { 0, 1, ISO28600_FIXED },
    { 36, 2, ISO28600_FIXED },
    { 56, 3, ISO28600_TEXT_LINE },
    { 79, 4, ISO28600_TEXT_LINE },
    { 107, 5, ISO28600_TEXT_LINE },
    { 127, 6, ISO28600_TEXT_LINE },
    { 149, 7, ISO28600_TEXT_LINE },
    { 171, 8, ISO28600_ENUM },
    { 187, 9, ISO28600_INTEGER },
    { 200, 10, ISO28600_INTEGER },
    { 206, 11, ISO28600_INTEGER },
    { 219, 12, ISO28600_INTEGER },
    { 225, 13, ISO28600_INTEGER },
    { 233, 14, ISO28600_INTEGER },
    { 241, 15, ISO28600_INTEGER },
    { 275, 16, ISO28600_FIXED },
    { 292, 17, ISO28600_ENUM },
    { 302, 18, ISO28600_ENUM },
    { 318, 19, ISO28600_ENUM },
    { 331, 20, ISO28600_ENUM },
    { 346, 21, ISO28600_TEXT_LINE },
    { 366, 22, ISO28600_ENUM },
    { 381, 23, ISO28600_TEXT_LINE },
    { 401, 24, ISO28600_INTEGER },
    { 445, 25, ISO28600_INTEGER },
    { 489, 26, ISO28600_UNIT },
    { 513, 27, ISO28600_UNIT },
    { 537, 28, ISO28600_REAL_NUM },
    { 553, 29, ISO28600_REAL_NUM },
    { 569, 30, ISO28600_UNIT },
    { 595, 31, ISO28600_UNIT },
    { 621, 32, ISO28600_REAL_NUM },
    { 630, 33, ISO28600_REAL_NUM },
    { 639, 34, ISO28600_REAL_NUM },
    { 654, 35, ISO28600_UNIT },
    { 682, 36, ISO28600_REAL_NUM },
    { 693, 37, ISO28600_UNIT },
    { 720, 38, ISO28600_REAL_NUM },
    { 730, 39, ISO28600_TEXT_LINE },
    { 744, 40, ISO28600_ENUM },
    { 765, 41, ISO28600_REAL_NUM },
    { 778, 42, ISO28600_INTEGER },
    { 798, 43, ISO28600_TEXT_LIST },
    { 813, 44, ISO28600_UNITS },
    { 837, 45, ISO28600_REAL_NUMS },
    { 862, 46, ISO28600_TEXT_LIST },
    { 902, 47, ISO28600_REAL_NUMS },
    { 934, 48, ISO28600_FIXED },
    { 958, 49, ISO28600_TEXT_LINE },
    { 975, 50, ISO28600_REAL_NUM },
    { 994, 51, ISO28600_REAL_NUM },
    { 1016, 52, ISO28600_REAL_NUM },
    { 1037, 53, ISO28600_TEXT_LINE },
    { 1059, 54, ISO28600_FIXED },
    { 1077, 55, ISO28600_TEXT_LINE },
    { 1094, 56, ISO28600_TEXT_LINE },
    { 1109, 57, ISO28600_REAL_NUM },
    { 1132, 58, ISO28600_REAL_NUM },
    { 1152, 59, ISO28600_REAL_NUM },
    { 1174, 60, ISO28600_REAL_NUM },
    { 1205, 61, ISO28600_REAL_NUM },
    { 1267, 62, ISO28600_REAL_NUM },
    { 1329, 63, ISO28600_TEXT_LINE },
    { 1345, 64, ISO28600_FIXED },
    { 1364, 65, ISO28600_TEXT_LINE },
    { 1382, 66, ISO28600_TEXT_LINE },
    { 1396, 67, ISO28600_TEXT_LINE },
    { 1413, 68, ISO28600_FIXED },
    { 1448, 69, ISO28600_TEXT_LINE },
    { 1463, 70, ISO28600_UNIT },
    { 1495, 71, ISO28600_TEXT_LINE },
    { 1520, 72, ISO28600_FIXED },
    { 1545, 73, ISO28600_TEXT_LINE },
    { 1563, 74, ISO28600_ENUM },
    { 1586, 75, ISO28600_TEXT_LINE },
    { 1601, 76, ISO28600_UNIT },
    { 1615, 77, ISO28600_REAL_NUM },
    { 1630, 78, ISO28600_REAL_NUM },
    { 1643, 79, ISO28600_REAL_NUM },
    { 1662, 80, ISO28600_REAL_NUM },
    { 1696, 81, ISO28600_INTEGER },
    { 1725, 82, ISO28600_INTEGER },
    { 1750, 83, ISO28600_TEXT_LIST },
    { 1766, 84, ISO28600_UNITS },
    { 1781, 85, ISO28600_REAL_NUMS },
    { 1817, 86, ISO28600_TEXT_LINE },
    { 1840, 87, ISO28600_FIXED },
    { 1867, 88, ISO28600_ENUM },
    { 1882, 89, ISO28600_TEXT_LINE },
    { 1899, 90, ISO28600_TEXT_LINE },
    { 1919, 91, ISO28600_TEXT_LINE },
    { 1940, 92, ISO28600_TEXT_LINE },
    { 1965, 93, ISO28600_FIXED },
    { 1999, 94, ISO28600_INTEGER },
    { 2023, 95, ISO28600_TEXT_LINE },
    { 2040, 96, ISO28600_UNIT },
    { 2063, 97, ISO28600_TEXT_LINE },
    { 2088, 98, ISO28600_TEXT_LINE },
    { 2105, 99, ISO28600_UNIT },
    { 2128, 100, ISO28600_TEXT_LINE },
    { 2153, 101, ISO28600_TEXT_LINE },
    { 2170, 102, ISO28600_UNIT },
    { 2193, 103, ISO28600_TEXT_LINE },
    { 2218, 104, ISO28600_TEXT_LINE },
    { 2235, 105, ISO28600_UNIT },
    { 2258, 106, ISO28600_TEXT_LINE },
    { 2283, 107, ISO28600_TEXT_LINE },
    { 2300, 108, ISO28600_UNIT },
    { 2323, 109, ISO28600_TEXT_LINE },
    { 2348, 110, ISO28600_TEXT_LINE },
    { 2365, 111, ISO28600_UNIT },
    { 2388, 112, ISO28600_TEXT_LINE },
    { 2413, 113, ISO28600_TEXT_LINE },
    { 2430, 114, ISO28600_UNIT },
    { 2453, 115, ISO28600_TEXT_LINE },
    { 2478, 116, ISO28600_TEXT_LINE },
    { 2495, 117, ISO28600_UNIT },
    { 2518, 118, ISO28600_TEXT_LINE },
    { 2543, 119, ISO28600_TEXT_LINE },
    { 2575, 120, ISO28600_RESERVED },
    { 2576, 121, ISO28600_RESERVED },
    { 2577, 122, ISO28600_RESERVED },
    { 2578, 123, ISO28600_RESERVED },
    { 2579, 124, ISO28600_RESERVED },
    { 2580, 125, ISO28600_RESERVED },
    { 2581, 126, ISO28600_RESERVED },
    { 2582, 127, ISO28600_RESERVED },
    { 2583, 128, ISO28600_FIXED },
};
#endif  /* }}} */

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports and exports ISO 28600:2011 SPM data transfer format."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    G_GNUC_UNUSED int check_table_size[G_N_ELEMENTS(header_fields) == 128
                                       ? 1: -1];

    gwy_file_func_register("iso28600",
                           N_("ISO 28600:2011 SPM data transfer files (.spm)"),
                           (GwyFileDetectFunc)&iso28600_detect,
                           (GwyFileLoadFunc)&iso28600_load,
                           NULL,
                           (GwyFileSaveFunc)&iso28600_export);

    return TRUE;
}

static gint
iso28600_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    return 100;
}

static GwyContainer*
iso28600_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    GwyContainer *container = NULL;
    ISO28600FieldValue *header = NULL;
    ISO28600ExperimentMode experiment;
    gchar *p, *buffer = NULL;
    gchar **strings = NULL;
    GError *err = NULL;
    gsize size;
    guint i;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    p = buffer;
    strings = g_new0(gchar*, G_N_ELEMENTS(header_fields));
    if (!(header = load_header(&p, strings, error)))
        goto fail;

    experiment = header[7].enumerated.value;
    if (experiment == ISO28600_EXPERIMENT_MAP_SC
        || experiment == ISO28600_EXPERIMENT_MAP_MC) {
        guint xres, yres, nchannels;
        gdouble xreal, yreal;

        if (header[16].enumerated.value != ISO28600_SCAN_REGULAR_MAPPING) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Only regular mappings are implemented but "
                          "file has mapping type ‘%s’."),
                        header[16].enumerated.str);
            goto fail;
        }
        xres = header[23].i;
        yres = header[24].i;
        if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
            goto fail;

        xreal = header[27].d;
        yreal = header[28].d;
        if (!((xreal = fabs(xreal)) > 0)) {
            g_warning("Real x size is 0.0, fixing to 1.0");
            xreal = 1.0;
        }
        if (!((yreal = fabs(yreal)) > 0)) {
            g_warning("Real y size is 0.0, fixing to 1.0");
            yreal = 1.0;
        }
        xreal *= pow10(header[25].unit.power10);
        yreal *= pow10(header[26].unit.power10);
        if (!gwy_si_unit_equal(header[25].unit.unit, header[26].unit.unit))
            g_warning("X and Y units differ, using X");

        nchannels = 1;
        if (experiment == ISO28600_EXPERIMENT_MAP_MC) {
            nchannels = header[93].i;
            if (nchannels < 1 || nchannels > MAX_CHANNELS) {
                err_INVALID(error, field_name(93));
                goto fail;
            }
        }

        container = load_channels(header, strings, &p,
                                  experiment, nchannels,
                                  xres, yres, xreal, yreal,
                                  error);
    }
    if (experiment == ISO28600_EXPERIMENT_SPEC_SC
        || experiment == ISO28600_EXPERIMENT_SPEC_MC) {
        ISO28600SpectroscopyScanMode smode;
        guint npts, nord;

        if (!(smode = header[73].enumerated.value)) {
            err_INVALID(error, field_name(73));
            goto fail;
        }

        npts = header[80].i;
        if (err_DIMENSION(error, npts))
            goto fail;

        nord = header[81].i;
        if (nord < 1) {
            err_INVALID(error, field_name(81));
            goto fail;
        }

        if (header[82].real_list.n && header[82].text_list.n != nord) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("List ‘%s’ has %u items which differs from "
                          "the number %u given by ‘%s’."),
                        field_name(82), header[82].text_list.n,
                        nord, field_name(81));
            goto fail;
        }
        if (header[83].real_list.n && header[83].text_list.n != nord) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("List ‘%s’ has %u items which differs from "
                          "the number %u given by ‘%s’."),
                        field_name(83), header[83].text_list.n,
                        nord, field_name(81));
            goto fail;
        }
        if (header[84].real_list.n && header[84].real_list.n != nord) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("List ‘%s’ has %u items which differs from "
                          "the number %u given by ‘%s’."),
                        field_name(84), header[84].text_list.n,
                        nord, field_name(81));
            goto fail;
        }

        container = load_spectra_graphs(header, &p, smode, nord, npts, error);
    }
    else {
        err_NO_DATA(error);
    }

fail:
    if (strings) {
        for (i = 0; i < G_N_ELEMENTS(header_fields); i++)
            g_free(strings[i]);
        g_free(strings);
    }
    free_header(header);
    g_free(buffer);

    return container;
}

static GwyContainer*
load_channels(ISO28600FieldValue *header,
              gchar **strings,
              gchar **p,
              ISO28600ExperimentMode experiment,
              guint nchannels,
              guint xres, guint yres,
              gdouble xreal, gdouble yreal,
              GError **error)
{
    GwyContainer *container = NULL;
    GwyDataField *fields[MAX_CHANNELS];
    gdouble *datas[MAX_CHANNELS];
    gdouble powers10[MAX_CHANNELS];
    gchar *line, *end;
    guint id, k;

    for (id = 0; id < nchannels; id++) {
        GwySIUnit *unit;

        fields[id] = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
        datas[id] = gwy_data_field_get_data(fields[id]);
        unit = gwy_data_field_get_si_unit_xy(fields[id]);
        gwy_serializable_clone(G_OBJECT(header[25].unit.unit), G_OBJECT(unit));
        unit = gwy_data_field_get_si_unit_z(fields[id]);
        if (experiment == ISO28600_EXPERIMENT_MAP_SC) {
            gwy_serializable_clone(G_OBJECT(header[69].unit.unit),
                                   G_OBJECT(unit));
            powers10[id] = pow10(header[69].unit.power10);
        }
        else {
            gwy_serializable_clone(G_OBJECT(header[95 + 3*id].unit.unit),
                                   G_OBJECT(unit));
            powers10[id] = pow10(header[95 + 3*id].unit.power10);
        }
    }

    for (line = gwy_str_next_line(p), k = 0;
         k < xres*yres;
         line = gwy_str_next_line(p), k++) {
        if (!line) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("End of file reached when reading sample #%d of %d"),
                        k, xres*yres);
            goto fail;
        }
        for (id = 0; id < nchannels; id++) {
            datas[id][k] = powers10[id]*g_ascii_strtod(line, &end);
            if (line == end) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Malformed data encountered when reading sample "
                              "#%d of %d"),
                            k, xres*yres);
                goto fail;
            }
            line = end;
        }
    }
    if (!line || !gwy_strequal(line, EOD_MAGIC)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Missing end-of-data marker."));
        goto fail;
    }

    container = gwy_container_new();
    for (id = 0; id < nchannels; id++) {
        GwyContainer *meta;
        const gchar *title;
        GQuark quark = gwy_app_get_data_key_for_id(id);

        gwy_container_set_object(container, quark, fields[id]);

        meta = get_meta(header, strings, id);
        if (meta) {
            gchar key[40];

            g_snprintf(key, sizeof(key), "/%d/meta", id);
            gwy_container_set_object_by_name(container, key, meta);
            g_object_unref(meta);
        }

        if (experiment == ISO28600_EXPERIMENT_MAP_SC)
            title = header[68].s;
        else
            title = header[94 + 3*id].s;

        if (strlen(title)) {
            gchar key[40];

            g_snprintf(key, sizeof(key), "/%d/data/title", id);
            gwy_container_set_string_by_name(container, key, g_strdup(title));
        }
    }

fail:
    for (id = 0; id < nchannels; id++)
        g_object_unref(fields[id]);

    return container;
}

/* XXX: The data format for spectroscopy is not well described so I estimate
 * it is how I would do it. */
static GwyContainer*
load_spectra_graphs(ISO28600FieldValue *header,
                    gchar **p,
                    ISO28600SpectroscopyScanMode smode,
                    guint nord,
                    guint npts,
                    GError **error)
{
    GwyContainer *container = NULL;
    GwySIUnit **units;
    gdouble *data, *powers10;
    gchar *line, *end;
    guint id, k, from;

    /* For an irregular scan, the ordinates are preceeded by abscissa values.
     * At least, I suppose. */
    units = g_new(GwySIUnit*, nord + 1);
    powers10 = g_new(gdouble, nord + 1);
    units[0] = g_object_ref(header[75].unit.unit);
    powers10[0] = pow10(header[75].unit.power10);
    from = (smode == ISO28600_SPECTROSCOPY_SCAN_REGULAR) ? 1 : 0;
    for (id = 0; id < nord; id++) {
        if (header[83].text_list.n) {
            gint power10;
            units[id+1] = gwy_si_unit_new_parse(header[83].text_list.items[id],
                                                &power10);
            powers10[id+1] = pow10(power10);
        }
        else {
            units[id+1] = gwy_si_unit_new(NULL);
            powers10[id+1] = 1.0;
        }
    }

    data = g_new(gdouble, (nord + 1)*npts);
    if (smode == ISO28600_SPECTROSCOPY_SCAN_REGULAR) {
        gdouble q = powers10[0] * header[78].d;
        gdouble x0 = powers10[0] * header[76].d;

        for (k = 0; k < npts; k++)
            data[k] = q*k + x0;
    }
    for (line = gwy_str_next_line(p), k = 0;
         k < npts;
         line = gwy_str_next_line(p), k++) {
        if (!line) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("End of file reached when reading sample #%d of %d"),
                        k, npts);
            goto fail;
        }
        for (id = from; id < nord + 1; id++) {
            data[id*npts + k] = powers10[id]*g_ascii_strtod(line, &end);
            if (line == end) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Malformed data encountered when reading sample "
                              "#%d of %d"),
                            k, npts);
                goto fail;
            }
            line = end;
        }
    }
    if (!line || !gwy_strequal(line, EOD_MAGIC)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Missing end-of-data marker."));
        goto fail;
    }

    container = gwy_container_new();
    for (id = 1; id <= nord; id++) {
        GwyGraphModel *gmodel = gwy_graph_model_new();
        GwyGraphCurveModel *gcmodel = gwy_graph_curve_model_new();
        const gchar *label = "";

        gwy_graph_curve_model_set_data(gcmodel, data, data + id*npts, npts);
        if (header[82].text_list.n)
            label = header[82].text_list.items[id-1];

        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "description", label,
                     NULL);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);

        g_object_set(gmodel,
                     "si-unit-x", units[0],
                     "si-unit-y", units[id],
                     "title", header[72].s,
                     "axis-label-left", label,
                     "axis-label-bottom", header[74].s,
                     NULL);

        gwy_container_set_object(container, gwy_app_get_graph_key_for_id(id),
                                 gmodel);
        g_object_unref(gmodel);
    }

fail:
    g_free(data);
    g_free(units);
    g_free(powers10);

    return container;
}

static GwyContainer*
get_meta(ISO28600FieldValue *header,
         gchar **strings,
         guint id)
{
    static const guint fields[] = {
        2, 3, 4, 5, 6, 17, 18, 19, 20, 21, 22, 38, 39, 42, 43, 44, 45, 46, 52,
        54, 55, 58, 62, 64, 65, 66, 70, 79, 84, 85, 87, 88, 89, 90, 91,
    };
    static const guint fields_with_units[] = {
        35, 34, 37, 36,
    };
    static const GwyEnum fields_without_units[] = {
        { "deg", 33 }, { "V", 40 }, { "K", 49 }, { "Pa", 50 }, { "%", 51 },
        { "N/m", 56 }, { "Hz", 57 }, { "deg", 59 }, { "deg", 60 },
        { "deg", 61 }, 
    };
    gint year, month, day, hour, minute, second, offset;
    GwyContainer *meta = gwy_container_new();
    guint k;

    for (k = 0; k < G_N_ELEMENTS(fields); k++) {
        guint i = fields[k];

        if (((header_fields[i].type == ISO28600_TEXT_LINE
              || header_fields[i].type == ISO28600_REAL_NUMS
              || header_fields[i].type == ISO28600_UNITS
              || header_fields[i].type == ISO28600_TEXT_LIST
              || header_fields[i].type == ISO28600_ENUM)
             && strlen(strings[i]))
            || (header_fields[i].type == ISO28600_INTEGER
                && header[i].i)
            || (header_fields[i].type == ISO28600_REAL_NUM
                && header[i].d))
            gwy_container_set_string_by_name(meta, field_name(i),
                                             g_strdup(strings[i]));
    }

    for (k = 0; k < G_N_ELEMENTS(fields_with_units)/2; k++) {
        guint i = fields_with_units[2*k],
              j = fields_with_units[2*k + 1];

        if (header[i].d)
            gwy_container_set_string_by_name(meta, field_name(i),
                                             g_strconcat(strings[i], " ",
                                                         strings[j], NULL));
    }

    for (k = 0; k < G_N_ELEMENTS(fields_without_units); k++) {
        const gchar *units = fields_without_units[k].name;
        guint i = fields_without_units[k].value;

        if (header[i].d)
            gwy_container_set_string_by_name(meta, field_name(i),
                                             g_strconcat(strings[i], " ",
                                                         units, NULL));
    }

    year = header[8].i;
    month = header[9].i;
    day = header[10].i;
    hour = header[11].i;
    minute = header[12].i;
    second = header[13].i;
    offset = header[14].i;
    if (year >= 0 && month >= 0 && day >= 0
        && hour >= 0 && minute >= 0 && second >= 0) {
        gchar *value;

        if (offset)
            value = g_strdup_printf("%04u-%02u-%02u %02u:%02u:%02u (+%u)",
                                    year, month, day, hour, minute, second,
                                    offset);
        else
            value = g_strdup_printf("%04u-%02u-%02u %02u:%02u:%02u",
                                    year, month, day, hour, minute, second);
        gwy_container_set_string_by_name(meta, "Date", value);
    }

    if (strlen(strings[96 + 3*id])) {
        gwy_container_set_string_by_name(meta, "Comment",
                                         g_strdup(strings[96 + 3*id]));
    }

    if (!gwy_container_get_n_items(meta)) {
        g_object_unref(meta);
        meta = NULL;
    }

    return meta;
}

static ISO28600FieldValue*
load_header(gchar **buffer,
            gchar **strings,
            GError **error)
{
    ISO28600FieldValue *header = g_new0(ISO28600FieldValue,
                                        G_N_ELEMENTS(header_fields));
    guint i, j;

    for (i = 0; i < G_N_ELEMENTS(header_fields); i++) {
        ISO28600FieldValue *hi = header + i;
        const gchar *name = field_name(i);
        ISO28600FieldType type = header_fields[i].type;
        gchar *line;

        if (!(line = gwy_str_next_line(buffer))) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("File header is truncated."));
            goto fail;
        }
        g_strstrip(line);
        strings[i] = g_strdup(line);

        if (type == ISO28600_FIXED) {
            if (!gwy_strequal(line, name)) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Line %u does not contain mandatory label ‘%s’."),
                            i+1, name);
                goto fail;
            }
            hi->s = line;
        }
        else if (type == ISO28600_INTEGER)
            hi->i = atoi(line);
        else if (type == ISO28600_REAL_NUM)
            hi->d = g_ascii_strtod(line, NULL);
        else if (type == ISO28600_UNIT)
            build_unit(line, header + i);
        else if (type == ISO28600_TEXT_LINE || type == ISO28600_RESERVED)
            hi->s = line;
        else if (type == ISO28600_TEXT_LIST || type == ISO28600_INTEGERS
                 || type == ISO28600_REAL_NUMS || type == ISO28600_UNITS) {
            guint n;
            gchar **items = split_line_in_place(line, ',', FALSE, TRUE, &n);

            if (type == ISO28600_INTEGERS) {
                hi->int_list.items = g_new(gint, n);
                hi->int_list.n = n;
                for (j = 0; j < n; j++)
                    hi->int_list.items[j] = atoi(items[j]);
                g_free(items);
            }
            else if (type == ISO28600_REAL_NUMS) {
                hi->real_list.items = g_new(gdouble, n);
                hi->real_list.n = n;
                for (j = 0; j < n; j++)
                    hi->real_list.items[j] = g_ascii_strtod(items[j], NULL);
                g_free(items);
            }
            else {
                hi->text_list.items = (const gchar**)items;
                hi->text_list.n = n;
            }
        }
        else if (type == ISO28600_ENUM)
            build_enum(line, i+1, header + i);
        else {
            g_warning("Unimplemented type: %u.", header_fields[i].type);
            header[i].s = line;
        }
    }

    return header;

fail:
    free_header(header);
    return NULL;
}

static void
build_unit(const gchar *str,
           ISO28600FieldValue *value)
{
    if (gwy_stramong(str, "d", "n", NULL))
        str = NULL;

    value->unit.unit = gwy_si_unit_new_parse(str, &value->unit.power10);
}

static gchar**
split_line_in_place(gchar *line,
                    gchar delimiter,
                    gboolean nonempty,
                    gboolean strip,
                    guint *count)
{
    gchar **items;
    guint i, j, n = 1;

    for (i = 0; line[i]; i++) {
        if (line[i] == delimiter)
            n++;
    }
    items = g_new(gchar*, n+1);
    items[0] = line;
    j = 1;
    for (i = 0; line[i]; i++) {
        if (line[i] == delimiter) {
            line[i] = '\0';
            items[j++] = line+1;
        }
    }
    g_assert(j == n);
    items[n] = NULL;

    if (strip) {
        for (i = 0; i < n; i++)
            g_strstrip(items[i]);
    }

    if (nonempty) {
        for (i = j = 0; i < n; i++) {
            if (*(items[i]))
                items[j++] = items[i];
        }
        items[j] = NULL;
        n = j;
    }

    if (count)
        *count = n;

    return items;
}

static void
build_enum(const gchar *str,
           guint lineno,
           ISO28600FieldValue *value)
{
    value->enumerated.str = str;
    value->enumerated.value = 0;

    if (lineno == 8) {
        if (gwy_strequal(str, "MAP_SC"))
            value->enumerated.value = ISO28600_EXPERIMENT_MAP_SC;
        else if (gwy_strequal(str, "MAP_MC"))
            value->enumerated.value = ISO28600_EXPERIMENT_MAP_MC;
        else if (gwy_strequal(str, "SPEC_SC"))
            value->enumerated.value = ISO28600_EXPERIMENT_SPEC_SC;
        else if (gwy_strequal(str, "SPEC_MC"))
            value->enumerated.value = ISO28600_EXPERIMENT_SPEC_MC;
    }
    else if (lineno == 17) {
        if (gwy_strequal(str, "REGULAR MAPPING"))
            value->enumerated.value = ISO28600_SCAN_REGULAR_MAPPING;
        else if (gwy_strequal(str, "IRREGULAR MAPPING"))
            value->enumerated.value = ISO28600_SCAN_IRREGULAR_MAPPING;
    }
    else if (lineno == 18) {
        if (gwy_strequal(str, "open-loop scanner"))
            value->enumerated.value = ISO28600_SCANNING_SYSTEM_OPEN_LOOP;
        else if (gwy_strequal(str, "XY closed-loop scanner"))
            value->enumerated.value = ISO28600_SCANNING_SYSTEM_XY_CLOSED_LOOP;
        else if (gwy_strequal(str, "XYZ closed-loop scanner"))
            value->enumerated.value = ISO28600_SCANNING_SYSTEM_XYZ_CLOSED_LOOP;
    }
    else if (lineno == 19) {
        if (gwy_strequal(str, "sample XYZ scan"))
            value->enumerated.value = ISO28600_SCANNER_SAMPLE_XYZ;
        else if (gwy_strequal(str, "probe XYZ scan"))
            value->enumerated.value = ISO28600_SCANNER_PROBE_XYZ;
        else if (gwy_strequal(str, "sample XY scan and probe Z scan"))
            value->enumerated.value = ISO28600_SCANNER_SAMPLE_XY_PROBE_Z;
        else if (gwy_strequal(str, "sample Z scan and probe XY scan"))
            value->enumerated.value = ISO28600_SCANNER_SAMPLE_Z_PROBE_XY;
    }
    else if (lineno == 20 || lineno == 22) {
        if (gwy_strequal(str, "X"))
            value->enumerated.value = ISO28600_AXIS_X;
        else if (gwy_strequal(str, "Y"))
            value->enumerated.value = ISO28600_AXIS_Y;
    }
    else if (lineno == 40) {
        if (gwy_strequal(str, "sample biased"))
            value->enumerated.value = ISO28600_BIAS_VOLTAGE_CONTACT_SAMPLE;
        else if (gwy_strequal(str, "tip biased"))
            value->enumerated.value = ISO28600_BIAS_VOLTAGE_CONTACT_TIP;
    }
    else if (lineno == 74) {
        if (gwy_strequal(str, "REGULAR"))
            value->enumerated.value = ISO28600_SPECTROSCOPY_SCAN_REGULAR;
        else if (gwy_strequal(str, "IRREGULAR"))
            value->enumerated.value = ISO28600_SPECTROSCOPY_SCAN_IRREGULAR;
    }
    else if (lineno == 88) {
        if (gwy_strequal(str, "raw data"))
            value->enumerated.value = ISO28600_DATA_TREATMENT_RAW;
        else if (gwy_strequal(str, "pre-treated data"))
            value->enumerated.value = ISO28600_DATA_TREATMENT_PRE_TREATED;
        else if (gwy_strequal(str, "post-treated data"))
            value->enumerated.value = ISO28600_DATA_TREATMENT_POST_TREATED;
    }
    else {
        g_assert_not_reached();
    }
}

static void
free_header(ISO28600FieldValue *header)
{
    guint i;

    if (header) {
        for (i = 0; i < G_N_ELEMENTS(header_fields); i++) {
            if (header_fields[i].type == ISO28600_UNIT)
                gwy_object_unref(header[i].unit.unit);
            else if (header_fields[i].type == ISO28600_TEXT_LIST)
                g_free(header[i].text_list.items);
            else if (header_fields[i].type == ISO28600_INTEGERS)
                g_free(header[i].int_list.items);
            else if (header_fields[i].type == ISO28600_REAL_NUMS)
                g_free(header[i].real_list.items);
            else if (header_fields[i].type == ISO28600_UNITS)
                g_free(header[i].text_list.items);
        }
        g_free(header);
    }
}

static gboolean
iso28600_export(GwyContainer *container,
                const gchar *filename,
                G_GNUC_UNUSED GwyRunType mode,
                GError **error)
{
    static const gchar header_template[] = /* {{{ */
    "ISO/TC 201 SPM data transfer format\n"
    "general information\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "Created by an image processing software.  Bogus acquisition parameters.\n"
    "MAP_SC\n"
    "-1\n"
    "-1\n"
    "-1\n"
    "-1\n"
    "-1\n"
    "-1\n"
    "-1\n"
    "scan information\n"
    "REGULAR MAPPING\n"
    "XYZ closed-loop scanner\n"
    "sample XYZ scan\n"
    "X\n"
    "left to right\n"
    "Y\n"
    "top to bottom\n"
    "%u\n"  /* XRes */
    "%u\n"  /* YRes */
    "%s\n"  /* XUnit */
    "%s\n"  /* YUnit */
    "%s\n"  /* XReal */
    "%s\n"  /* YReal */
    "%s\n"  /* XUnit */
    "%s\n"  /* YUnit */
    "%s\n"  /* XOffset */
    "%s\n"  /* YOffset */
    "0\n"
    "m/s\n"
    "0.0\n"
    "Hz\n"
    "0.0\n"
    "\n"
    "sample biased\n"
    "0.0\n"
    "0\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "environment description\n"
    "software\n"
    "300\n"
    "1.0e5\n"
    "40\n"
    "\n"
    "probe description\n"
    "software\n"
    "\n"
    "0.0\n"
    "0.0\n"
    "0.0\n"
    "0\n"
    "0\n"
    "0\n"
    "\n"
    "sample description\n"
    "%s\n"  /* Title */
    "\n"
    "\n"
    "single-channel mapping description\n"
    "%s\n"  /* Title */
    "%s\n"  /* ZUnit */
    "\n"
    "spectroscopy description\n"
    "\n"
    "REGULAR\n"
    "\n"
    "n\n"
    "0.0\n"
    "0.0\n"
    "0.0\n"
    "0.0\n"
    "0\n"
    /* The norm says ‘one or more’ but that would *require* spectra to be
     * present in every file.  Put the true value here. */
    "0\n"
    "\n"
    "n\n"
    "0.0\n"
    "\n"
    "data treatment description\n"
    "post-treated data\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "multi-channel mapping description\n"
    /* The norm says ‘two or more’ but that would *require* two channels to be
     * present in every file.  Put the true value here. */
    "1\n"
    "%s\n"  /* Title */
    "%s\n"  /* ZUnit */
    "%s\n"  /* Title */
    "\n"
    "n\n"
    "\n"
    "\n"
    "n\n"
    "\n"
    "\n"
    "n\n"
    "\n"
    "\n"
    "n\n"
    "\n"
    "\n"
    "n\n"
    "\n"
    "\n"
    "n\n"
    "\n"
    "\n"
    "n\n"
    "\n"
    "\n"
    "n\n"
    "\n"
    "\n"
    "n\n"
    "\n"
    "\n"
    "n\n"
    "\n"
    "end of header\n"; /* }}} */

    gchar xreal[32], yreal[32], xoff[32], yoff[32];
    gchar *unitxy = NULL, *unitz = NULL, *title = NULL;
    GwyDataField *dfield;
    const gdouble *d;
    guint xres, yres, i, j, n;
    gint id;
    gboolean ok = FALSE;
    FILE *fh;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    if (!dfield) {
        err_NO_CHANNEL_EXPORT(error);
        return FALSE;
    }

    /* Both kind of EOLs are fine so write Unix EOLs everywhere. */
    if (!(fh = g_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    d = gwy_data_field_get_data_const(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    unitxy = convert_unit(gwy_data_field_get_si_unit_xy(dfield));
    unitz = convert_unit(gwy_data_field_get_si_unit_z(dfield));

    title = gwy_app_get_data_field_title(container, id);
    n = strlen(title);
    for (i = 0; i < n; i++) {
        if ((guchar)title[i] & 0x80)
            break;
    }
    if (i < n) {
        g_free(title);
        title = g_strdup("Not representable in ASCII. Ask the committee to "
                         "fix the standard to permit UTF-8.");
    }

    g_ascii_formatd(xreal, sizeof(xreal), "%.8g",
                    gwy_data_field_get_xreal(dfield));
    g_ascii_formatd(yreal, sizeof(yreal), "%.8g",
                    gwy_data_field_get_yreal(dfield));
    g_ascii_formatd(xoff, sizeof(xoff), "%.8g",
                    gwy_data_field_get_xoffset(dfield));
    g_ascii_formatd(yoff, sizeof(yoff), "%.8g",
                    gwy_data_field_get_yoffset(dfield));

    if (fprintf(fh, header_template,
                xres, yres,
                unitxy, unitxy, xreal, yreal,
                unitxy, unitxy, xoff, yoff,
                title, title, unitz,
                title, unitz, title) < 0) {
        err_WRITE(error);
        goto fail;
    }

    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++, d++) {
            g_ascii_formatd(xreal, sizeof(xreal), "%.8g", *d);
            if (fwrite(xreal, strlen(xreal), 1, fh) != 1) {
                err_WRITE(error);
                goto fail;
            }
            if (fputc('\n', fh) == (int)EOF) {
                err_WRITE(error);
                goto fail;
            }
        }
    }

    if (fprintf(fh, "end of experiment\n") < 0) {
        err_WRITE(error);
        goto fail;
    }

    ok = TRUE;

fail:
    fclose(fh);
    g_free(title);
    g_free(unitxy);
    g_free(unitz);

    return ok;
}

static gchar*
convert_unit(GwySIUnit *unit)
{
    gchar *str = gwy_si_unit_get_string(unit, GWY_SI_UNIT_FORMAT_PLAIN);
    const gchar *convstr;

    /* Ignore non-base units altogether because we never produce them. */
    if (gwy_stramong(str, "A", "C", "eV", "Hz", "K", "m", "m/s", "N", "N/m",
                     "Pa", "s", "V", NULL))
        return str;

    if (gwy_strequal(str, "deg"))
        convstr = "degree";
    else if (gwy_strequal(str, "cps"))
        convstr = "c/s";
    else if (gwy_strequal(str, ""))
        convstr = "d";
    else
        convstr = "n";

    g_free(str);
    return g_strdup(convstr);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
