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

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC "SIS&STB  SIScan"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".sis"

typedef enum {
    SIS_BLOCK_DOCUMENT     = 1,
    SIS_BLOCK_PREVIEW      = 2,
    SIS_BLOCK_CHANNEL      = 3,
    SIS_BLOCK_IMAGE        = 42,
} SISBlockType;

typedef enum {
    SIS_AQUISITION_MODE_CONTACT     = 1,
    SIS_AQUISITION_MODE_NON_CONTACT = 2,
} SISAquisitionMode;

typedef enum {
    SIS_SCANNING_DIRECTION_FORWARD  = 1,
    SIS_SCANNING_DIRECTION_BACKWARD = 2,
} SISScanningDirection;

typedef enum {
    SIS_OFF = FALSE,
    SIS_ON  = TRUE,
} SISOnOff;

typedef enum {
    SIS_PALETTE_GRAY    = 0,
    SIS_PALETTE_GLOW    = 1,
    SIS_PALETTE_RED     = 2,
    SIS_PALETTE_GREEN   = 3,
    SIS_PALETTE_BLUE    = 4,
    SIS_PALETTE_RAINBOW = 5,
} SISPaletteType;

typedef enum {
    SIS_DATA_TYPE_TOPOGRAPHY       = 1,
    SIS_DATA_TYPE_FIELD_CONTRAST   = 2,
    SIS_DATA_TYPE_ERROR            = 3,
    SIS_DATA_TYPE_EXTERM           = 4,
    SIS_DATA_TYPE_LOC              = 5,
    SIS_DATA_TYPE_PHASE            = 6,
    SIS_DATA_TYPE_CAPACITY         = 7,
    SIS_DATA_TYPE_AMPLITUDE        = 8,
    SIS_DATA_TYPE_FREQUENCY        = 9,
    SIS_DATA_TYPE_POTENTIAL        = 10,
    SIS_DATA_TYPE_FRICTION         = 11,
    SIS_DATA_TYPE_FORCE_MODULATION = 12,
    SIS_DATA_TYPE_USER             = 13,
} SISDataType;

typedef enum {
    SIS_SIGNAL_SOURCE_FEEDBACK         = 1,
    SIS_SIGNAL_SOURCE_ZSENSOR          = 2,
    SIS_SIGNAL_SOURCE_INTERFEROMETER   = 3,
    SIS_SIGNAL_SOURCE_FIELD            = 4,
    SIS_SIGNAL_SOURCE_NC_AMPLITUDE     = 5,
    SIS_SIGNAL_SOURCE_NC_PHASE         = 6,
    SIS_SIGNAL_SOURCE_FM_FREQUENCY     = 7,
    SIS_SIGNAL_SOURCE_LOC_AMPLITUDE    = 8,
    SIS_SIGNAL_SOURCE_LOC_PHASE        = 9,
    SIS_SIGNAL_SOURCE_PM_CHANNEL_1     = 10,
    SIS_SIGNAL_SOURCE_PM_CHANNEL_2     = 11,
    SIS_SIGNAL_SOURCE_PM_FEEDBACK      = 12,
    SIS_SIGNAL_SOURCE_CAPACITY         = 13,
    SIS_SIGNAL_SOURCE_LOC_SW_AMPLITUDE = 14,
    SIS_SIGNAL_SOURCE_LOC_SW_PHASE     = 15,
    SIS_SIGNAL_SOURCE_USER             = 16,
} SISSignalSource;

typedef struct {
    guint idx;
    GType type;
    const gchar *meta;
    const gchar *units;
} SISParameter;

typedef struct {
    guchar symbol[4];
    guint data_size;
    gchar *meta;
} SISProcessingStep;

typedef struct {
    /* image info */
    guchar processing_step[4];
    guint processing_step_index;
    guint channel_index;  /* 0 == 1st */
    guchar parent_processing_step[4];
    guint parent_processing_step_index;  /* 0 == 1st, ffff = none */
    guint parent_processing_step_channel_index;  /* 0 == 1st */
    /* memory info */
    guint width;
    guint height;
    guint bpp;
    guint priority;
    gboolean image_data_saved;
    const guchar *image_data;    /* not allocated, just pointer to buffer */
} SISImage;

typedef struct {
    SISDataType data_type;
    SISSignalSource signal_source;
    SISScanningDirection scanning_direction;
    guint processing_steps;
    /* images */
    guint nimages;
    SISImage *images;
} SISChannel;

typedef struct {
    guint version_maj;
    guint version_min;
    GHashTable *params;
    guint nchannels;
    SISChannel *channels;
} SISFile;

static const GwyEnum sis_onoff[] = {
    { "Off", SIS_OFF },
    { "On",  SIS_ON },
};

static const GwyEnum sis_aquisitions[] = {
    { "Contact",     SIS_AQUISITION_MODE_CONTACT     },
    { "Non contact", SIS_AQUISITION_MODE_NON_CONTACT },
};

/* Map between SIS palettes and Gwyddion palettes */
static const GwyEnum sis_palettes[] = {
    { "Gray",    SIS_PALETTE_GRAY    },
    { "Sky",     SIS_PALETTE_GLOW    },
    { "Red",     SIS_PALETTE_RED     },
    { "Green",   SIS_PALETTE_GREEN   },
    { "Blue",    SIS_PALETTE_BLUE    },
    { "Rainbow", SIS_PALETTE_RAINBOW },
};

static const GwyEnum sis_data_types[] = {
    { "Topography",             SIS_DATA_TYPE_TOPOGRAPHY       },
    { "Field Contrast",         SIS_DATA_TYPE_FIELD_CONTRAST   },
    { "Error",                  SIS_DATA_TYPE_ERROR            },
    { "Exterm",                 SIS_DATA_TYPE_EXTERM           },
    { "Loc",                    SIS_DATA_TYPE_LOC              },
    { "Phase",                  SIS_DATA_TYPE_PHASE            },
    { "Capacity",               SIS_DATA_TYPE_CAPACITY         },
    { "Amplitude",              SIS_DATA_TYPE_AMPLITUDE        },
    { "Frequency",              SIS_DATA_TYPE_FREQUENCY        },
    { "Potential",              SIS_DATA_TYPE_POTENTIAL        },
    { "Friction",               SIS_DATA_TYPE_FRICTION         },
    { "Force Modulation (FMM)", SIS_DATA_TYPE_FORCE_MODULATION },
    { "User",                   SIS_DATA_TYPE_USER             },
};

static const GwyEnum sis_signal_sources[] = {
    { "Feedback",               SIS_SIGNAL_SOURCE_FEEDBACK         },
    { "ZSensor",                SIS_SIGNAL_SOURCE_ZSENSOR          },
    { "Interferometer",         SIS_SIGNAL_SOURCE_INTERFEROMETER   },
    { "Field",                  SIS_SIGNAL_SOURCE_FIELD            },
    { "NC Amplitude",           SIS_SIGNAL_SOURCE_NC_AMPLITUDE     },
    { "NC Phase",               SIS_SIGNAL_SOURCE_NC_PHASE         },
    { "FM Frequency",           SIS_SIGNAL_SOURCE_FM_FREQUENCY     },
    { "LOC amplitude",          SIS_SIGNAL_SOURCE_LOC_AMPLITUDE    },
    { "LOC phase",              SIS_SIGNAL_SOURCE_LOC_PHASE        },
    { "PM Channel 1",           SIS_SIGNAL_SOURCE_PM_CHANNEL_1     },
    { "PM Channel 2",           SIS_SIGNAL_SOURCE_PM_CHANNEL_2     },
    { "PM Feedback",            SIS_SIGNAL_SOURCE_PM_FEEDBACK      },
    { "Capacity",               SIS_SIGNAL_SOURCE_CAPACITY         },
    { "LOC Software Amplitude", SIS_SIGNAL_SOURCE_LOC_SW_AMPLITUDE },
    { "LOC Software Phase",     SIS_SIGNAL_SOURCE_LOC_SW_PHASE     },
    { "User",                   SIS_SIGNAL_SOURCE_USER             },
};

static const SISProcessingStep processing_steps[] = {
    { "BLOB", 2,                 "Particle count"                },
    { "3DJS", 5*2 + 2*8 + 4*2,   "3DJS"                          },
    { "ACOR", 0,                 "Autocorrelation"               },
    { "ALNC", 6*2,               "Autocorrelation LineCut"       },
    { "BFFT", 0,                 "Biqudratic Fourier filter fit" },
    { "CONT", 2*2,               "Contrast histogram"            },
    { "DIF2", 2*2,               "Differentiation"               },
    { "EDGE", 0,                 "Edge detection filter"         },
    { "FFBP", 2*8 + 2*2,         "Band pass frequency filter"    },
    { "FFLP", 0,                 "Low pass frequency filter"     },
    { "FFMP", 0,                 "High pass frequency filter"    },
    { "FFT2", 0,                 "Twodimensional FFT"            },
    { "FLIP", 0,                 "Y axis flip"                   },
    { "HIST", 2*8 + 4*2 + 8,     "Histogram"                     },
    { "IFT2", 0,                 "Fourier filter back 2D"        },
    { "LNCT", 8*2,               "Line profile"                  },
    { "MEDN", 0,                 "Median filter"                 },
    { "MIRR", 0,                 "X axis mirror"                 },
    { "PAVE", 0,                 "Profile average"               },
    { "RAWR", 0,                 "Raw raster data"               },
    { "RGOI", 4*2,               "Region of interest"            },
    { "ROTN", 2,                 "Rotation"                      },
    { "SHRP", 0,                 "Sharpening filter"             },
    { "SMTH", 0,                 "Smoothing filter"              },
    { "STAT", 0,                 "Statistics in z"               },
    { "STEP", 0,                 "Step correction"               },
    { "SURF", 2*2,               "Surface area"                  },
    { "TIBQ", 0,                 "Biquadratic plane correction"  },
    { "TIL3", 6*2,               "Three point plane correction"  },
    { "TILT", 0,                 "Automatic plane correction"    },
};

static const SISParameter sis_parameters[] = {
    {   0, G_TYPE_STRING, "Name of the sample", NULL },
    {   1, G_TYPE_STRING, "Comment of the sample", NULL },
    {   2, G_TYPE_DOUBLE, "Scanning range in x direction", "nm" },
    {   3, G_TYPE_DOUBLE, "Scanning range in y direction", "nm" },
    {   4, G_TYPE_DOUBLE, "Range in z direction", "nm" },
    {   5, G_TYPE_DOUBLE, "Offset in z direction", NULL },  /* ?? */
    {   6, G_TYPE_INT,    "Type of aquisition", NULL },
    {   7, G_TYPE_INT,    "Number of pixels in x direction", NULL },
    {   8, G_TYPE_INT,    "Number of pixels in y direction", NULL },
    {   9, G_TYPE_DOUBLE, "Speed of scanning", "lines/s" },
    {  10, G_TYPE_STRING, "Type of tip", NULL },
    {  11, G_TYPE_INT,    "Bits per pixels", NULL },
    {  12, G_TYPE_DOUBLE, "Value of the proportional part of feedback", NULL },
    {  13, G_TYPE_DOUBLE, "Value of the integral part of feedback", "µs" },
    {  14, G_TYPE_DOUBLE, "Load force of the tip", "nN" },
    {  15, G_TYPE_DOUBLE, "Resonance frequency of the cantilever", "kHz" },
    {  16, G_TYPE_STRING, "Date of the measurement", NULL },
    {  17, G_TYPE_DOUBLE, "Feedback", NULL },
    {  18, G_TYPE_DOUBLE, "Scanning direction", "°" },
    {  19, G_TYPE_DOUBLE, "Spring constant", "N/m" },
    {  20, G_TYPE_STRING, "HighVoltage in x and y direction", NULL },
    {  21, G_TYPE_STRING, "Measurement with x and y linearisation", NULL },
    {  22, G_TYPE_STRING, "Amplification of the interferometer signal", NULL },  /* ?? */
    {  23, G_TYPE_DOUBLE, "Free amplitude of the cantilever", "nm" },
    {  24, G_TYPE_DOUBLE, "Damping of the free amplitude of the cantilever during the measurement", "%" },
    {  25, G_TYPE_DOUBLE, "Voltage between the tip and the electrode under the sample", "V" },
    {  26, G_TYPE_DOUBLE, "Oscilation frequency of the cantilever during the measurement", "kHz" },
    {  27, G_TYPE_DOUBLE, "Field contrast", "nm" },
    {  28, G_TYPE_INT,    "Type of palette", NULL },
    { 100, G_TYPE_STRING, "Units of data in channel 1", NULL },
    { 101, G_TYPE_STRING, "Units of data in channel 2", NULL },
    { 102, G_TYPE_STRING, "Units of data in channel 3", NULL },
    { 103, G_TYPE_STRING, "Units of data in channel 4", NULL },
    { 104, G_TYPE_STRING, "Units of data in channel 5", NULL },
    { 105, G_TYPE_STRING, "Units of data in channel 6", NULL },
    { 106, G_TYPE_STRING, "Units of data in channel 7", NULL },
    { 107, G_TYPE_STRING, "Units of data in channel 8", NULL },
    { 108, G_TYPE_DOUBLE, "Range of of data in channel 1", NULL },
    { 109, G_TYPE_DOUBLE, "Range of of data in channel 2", NULL },
    { 110, G_TYPE_DOUBLE, "Range of of data in channel 3", NULL },
    { 111, G_TYPE_DOUBLE, "Range of of data in channel 4", NULL },
    { 112, G_TYPE_DOUBLE, "Range of of data in channel 5", NULL },
    { 113, G_TYPE_DOUBLE, "Range of of data in channel 6", NULL },
    { 114, G_TYPE_DOUBLE, "Range of of data in channel 7", NULL },
    { 115, G_TYPE_DOUBLE, "Range of of data in channel 8", NULL },
    { 116, G_TYPE_INT,    "Number of channels", NULL },
    { 117, G_TYPE_DOUBLE, "Offset in x direction in the scanning range", "nm" },
    { 118, G_TYPE_DOUBLE, "Offset in y direction in the scanning range", "nm" },
    { 119, G_TYPE_DOUBLE, "Maximum scanning range in x direction", "nm" },
    { 120, G_TYPE_DOUBLE, "Maximum scanning range in y direction", "nm" },
    { 121, G_TYPE_DOUBLE, "Minimum range of of data in channel 1", NULL },
    { 122, G_TYPE_DOUBLE, "Minimum range of of data in channel 2", NULL },
    { 123, G_TYPE_DOUBLE, "Minimum range of of data in channel 3", NULL },
    { 124, G_TYPE_DOUBLE, "Minimum range of of data in channel 4", NULL },
    { 125, G_TYPE_DOUBLE, "Minimum range of of data in channel 5", NULL },
    { 126, G_TYPE_DOUBLE, "Minimum range of of data in channel 6", NULL },
    { 127, G_TYPE_DOUBLE, "Minimum range of of data in channel 7", NULL },
    { 128, G_TYPE_DOUBLE, "Minimum range of of data in channel 8", NULL },
    { 129, G_TYPE_DOUBLE, "Maximum range of of data in channel 1", NULL },
    { 130, G_TYPE_DOUBLE, "Maximum range of of data in channel 2", NULL },
    { 131, G_TYPE_DOUBLE, "Maximum range of of data in channel 3", NULL },
    { 132, G_TYPE_DOUBLE, "Maximum range of of data in channel 4", NULL },
    { 133, G_TYPE_DOUBLE, "Maximum range of of data in channel 5", NULL },
    { 134, G_TYPE_DOUBLE, "Maximum range of of data in channel 6", NULL },
    { 135, G_TYPE_DOUBLE, "Maximum range of of data in channel 7", NULL },
    { 136, G_TYPE_DOUBLE, "Maximum range of of data in channel 8", NULL },
    { 137, G_TYPE_STRING, "Name of data in channel 1", NULL },
    { 138, G_TYPE_STRING, "Name of data in channel 2", NULL },
    { 139, G_TYPE_STRING, "Name of data in channel 3", NULL },
    { 140, G_TYPE_STRING, "Name of data in channel 4", NULL },
    { 141, G_TYPE_STRING, "Name of data in channel 5", NULL },
    { 142, G_TYPE_STRING, "Name of data in channel 6", NULL },
    { 143, G_TYPE_STRING, "Name of data in channel 7", NULL },
    { 144, G_TYPE_STRING, "Name of data in channel 8", NULL },
};

typedef struct {
    GHashTable *hash;
    GwyDataField *data_field;
} SISData;

static gboolean       module_register     (void);
static gint           sis_detect          (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer*  sis_load            (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static GwyDataField*  extract_data        (SISFile *sisfile,
                                           guint ch,
                                           guint im,
                                           GError **error);
static void           add_metadata        (SISFile *sisfile,
                                           gint id,
                                           guint ch,
                                           guint im,
                                           GwyContainer *data);
static gboolean       sis_real_load       (const guchar *buffer,
                                           guint size,
                                           SISFile *sisfile,
                                           GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports SIS (Surface Imaging Systems) data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.14",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("sis",
                           N_("SIS files (.sis)"),
                           (GwyFileDetectFunc)&sis_detect,
                           (GwyFileLoadFunc)&sis_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
sis_detect(const GwyFileDetectInfo *fileinfo,
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

static GwyContainer*
sis_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    guchar *buffer;
    gsize size;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GwyContainer *data = NULL;
    SISFile sisfile;
    SISImage *image;
    GString *key;
    const gchar *s;
    guint n;
    guint i, j;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    n = 0;
    memset(&sisfile, 0, sizeof(sisfile));
    sisfile.params = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                           NULL, g_free);
    if (sis_real_load(buffer, size, &sisfile, error)) {
        data = gwy_container_new();
        key = g_string_new("");
        for (i = 0; i < sisfile.nchannels; i++) {
            for (j = 0; j < sisfile.channels[i].nimages; j++) {
                image = sisfile.channels[i].images + j;
                if (!image->image_data)
                    continue;

                dfield = extract_data(&sisfile, i, j, &err);
                if (dfield) {
                    g_clear_error(&err);
                    g_string_printf(key, "/%u/data", n);
                    gwy_container_set_object_by_name(data, key->str, dfield);
                    g_object_unref(dfield);
                    g_string_append(key, "/title");
                    s = gwy_enum_to_string(sisfile.channels[i].data_type,
                                           sis_data_types,
                                           G_N_ELEMENTS(sis_data_types));
                    if (!s || !*s)
                        s = "Unknown";
                    gwy_container_set_string_by_name(data, key->str,
                                                     g_strdup(s));
                    add_metadata(&sisfile, n, i, j, data);
                    n++;
                }
            }
        }
        g_string_free(key, TRUE);

        if (!n) {
            if (err)
                g_propagate_error(error, err);
            else
                err_NO_DATA(error);
            gwy_object_unref(data);
        }
    }

    gwy_file_abandon_contents(buffer, size, NULL);
    g_hash_table_destroy(sisfile.params);
    for (i = 0; i < sisfile.nchannels; i++)
        g_free(sisfile.channels[i].images);
    g_free(sisfile.channels);

    return data;
}

static GwyDataField*
extract_data(SISFile *sisfile,
             guint ch,
             guint im,
             GError **error)
{
    GwyDataField *dfield;
    GwySIUnit *siunit;
    SISChannel *channel;
    SISImage *image;
    gdouble xreal, yreal, zreal;
    gdouble *d;
    guint i;

    channel = sisfile->channels + ch;
    image = channel->images + im;
    if (image->bpp != 1 && image->bpp != 2 && image->bpp != 4) {
        err_BPP(error, image->bpp);
        return NULL;
    }

    xreal = yreal = 100e-9;    /* XXX: whatever */
    zreal = 1e-9;
    if ((d = g_hash_table_lookup(sisfile->params, GUINT_TO_POINTER(2))))
        xreal = *d * 1e-9;
    if ((d = g_hash_table_lookup(sisfile->params, GUINT_TO_POINTER(3))))
        yreal = *d * 1e-9;
    if ((d = g_hash_table_lookup(sisfile->params, GUINT_TO_POINTER(4))))
        zreal = *d * 1e-9;
    dfield = gwy_data_field_new(image->width, image->height,
                                xreal, yreal,
                                FALSE);

    d = gwy_data_field_get_data(dfield);
    switch (image->bpp) {
        case 1:
        for (i = 0; i < image->width*image->height; i++)
            d[i] = image->image_data[i]/255.0;
        break;

        case 2:
        for (i = 0; i < image->width*image->height; i++)
            d[i] = (image->image_data[2*i]
                    + 256.0*image->image_data[2*i + 1])/65535.0;
        break;

        case 4:
        for (i = 0; i < image->width*image->height; i++)
            d[i] = (image->image_data[4*i]
                    + 256.0*(image->image_data[4*i + 1]
                             + 256.0*(image->image_data[4*i + 2]
                                      + 256.0*image->image_data[4*i + 3])))
                    /4294967296.0;
        break;

        default:
        g_assert_not_reached();
        break;
    }
    for (i = 0; i < image->width*image->height; i++)
        d[i] *= zreal;

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    gwy_object_unref(siunit);

    switch (channel->data_type) {
        case SIS_DATA_TYPE_TOPOGRAPHY:
        siunit = gwy_si_unit_new("m");
        break;

        default:
        /* FIXME */
        break;
    }

    if (siunit) {
        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);
    }

    return dfield;
}

static void
add_metadata(SISFile *sisfile,
             gint id,
             guint ch,
             guint im,
             GwyContainer *data)
{
    static const guint good_metadata[] = {
        0, 1, 9, 10, 12, 13, 14, 15, 16, 18, 20, 21, 22, 23, 24, 25, 26, 27,
    };
    GwyContainer *meta;
    SISChannel *channel;
    SISImage *image;
    guint i, j;
    guchar *value, *key;
    gpointer *p;

    meta = gwy_container_new();
    channel = sisfile->channels + ch;
    image = channel->images + im;
    for (i = 0; i < G_N_ELEMENTS(good_metadata); i++) {
        for (j = 0; j < G_N_ELEMENTS(sis_parameters); j++) {
            if (sis_parameters[j].idx == good_metadata[i])
                break;
        }
        g_assert(j < G_N_ELEMENTS(sis_parameters));
        p = g_hash_table_lookup(sisfile->params, GUINT_TO_POINTER(j));
        if (!p)
            continue;

        switch (sis_parameters[j].type) {
            case G_TYPE_STRING:
            value = g_strdup((gchar*)p);
            break;

            case G_TYPE_INT:
            if (sis_parameters[j].units)
                value = g_strdup_printf("%d %s",
                                        *(guint*)p, sis_parameters[j].units);
            else
                value = g_strdup_printf("%d", *(guint*)p);
            break;

            case G_TYPE_DOUBLE:
            if (sis_parameters[j].units)
                value = g_strdup_printf("%.5g %s",
                                        *(gdouble*)p, sis_parameters[j].units);
            else
                value = g_strdup_printf("%.5g", *(gdouble*)p);
            break;

            default:
            g_assert_not_reached();
            value = NULL;
            break;
        }
        gwy_container_set_string_by_name(meta, sis_parameters[j].meta, value);
    }

    /* Special metadata */
    if ((p = g_hash_table_lookup(sisfile->params, GUINT_TO_POINTER(28)))) {
        value = g_strdup(gwy_enum_to_string(*(guint*)p,
                                            sis_palettes,
                                            G_N_ELEMENTS(sis_palettes)));
        key = g_strdup_printf("/%d/base/palette", id);
        gwy_container_set_string_by_name(data, key, value);
        g_free(key);
    }

    if ((p = g_hash_table_lookup(sisfile->params, GUINT_TO_POINTER(6)))) {
        value = g_strdup(gwy_enum_to_string(*(guint*)p,
                                            sis_aquisitions,
                                            G_N_ELEMENTS(sis_aquisitions)));
        gwy_container_set_string_by_name(meta, "Aqusition type", value);
    }

    value = g_strdup(gwy_enum_to_string(channel->signal_source,
                                        sis_signal_sources,
                                        G_N_ELEMENTS(sis_signal_sources)));
    gwy_container_set_string_by_name(meta, "Signal source", value);

    key = g_strdup_printf("/%d/meta", id);
    gwy_container_set_object_by_name(data, key, meta);
    g_free(key);
    g_object_unref(meta);
}

/* FIXME: what a mess. And in reality, the files look different than the
 * specs say anyway... */
static gboolean
sis_real_load(const guchar *buffer,
              guint size,
              SISFile *sisfile,
              GError **error)
{
    const SISParameter *sisparam;
    const SISProcessingStep *procstep;
    SISChannel *channel = NULL;
    SISImage *image;
    guint start, id, i, j, len;
    guint docinfosize, nparams;
    const guchar *p;
    gpointer idp;
    gdouble d;

    p = memchr(buffer, '\x1a', size);
    if (!p) {
        err_FILE_TYPE(error, "SIS");
        return FALSE;
    }
    start = p-buffer + 1;
    gwy_debug("%.*s", start, buffer);

    if (size - start < 6) {
        err_TOO_SHORT(error);
        return FALSE;
    }

    p = buffer + start;
    id = gwy_get_guint16_le(&p);
    gwy_debug("block id = %u", id);
    if (id != SIS_BLOCK_DOCUMENT) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Block not a document block."));
        return FALSE;
    }

    docinfosize = gwy_get_guint32_le(&p);
    gwy_debug("doc info size = %u", docinfosize);
    if (size - (p - buffer) < docinfosize - 6
        || docinfosize < 8) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Too short document info."));
        return FALSE;
    }

    sisfile->version_maj = gwy_get_guint16_le(&p);
    sisfile->version_min = gwy_get_guint16_le(&p);
    gwy_debug("version = %d.%d", sisfile->version_maj, sisfile->version_min);

    nparams = gwy_get_guint16_le(&p);
    sisfile->nchannels = gwy_get_guint16_le(&p);
    gwy_debug("nparams = %d, nchannels = %d", nparams, sisfile->nchannels);
    if (!sisfile->nchannels) {
        err_NO_DATA(error);
        return FALSE;
    }
    sisfile->channels = g_new0(SISChannel, sisfile->nchannels);

    for (i = 0; i < nparams; i++) {
        if (size - (p - buffer) < 4) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Too short parameter info."));
            return FALSE;
        }
        id = gwy_get_guint16_le(&p);
        len = gwy_get_guint16_le(&p);
        if (!len) {
            gwy_debug("ZERO length parameter %u, ignoring", id);
            continue;
        }
        if (size - (p - buffer) < len) {
            gwy_debug("FAILED: Truncated parameter data, param len = %u", len);
            return FALSE;
        }

        sisparam = NULL;
        for (j = 0; j < G_N_ELEMENTS(sis_parameters); j++) {
            if (sis_parameters[j].idx == id) {
                gwy_debug("Parameter %s", sis_parameters[j].meta);
                sisparam = sis_parameters + j;
                break;
            }
        }
        if (!sisparam) {
            g_warning("UNKNOWN parameter id %u", id);
            p += len;
            continue;
        }

        idp = GUINT_TO_POINTER(id);
        switch (sisparam->type) {
            case G_TYPE_STRING:
            g_hash_table_insert(sisfile->params, idp, g_strndup(p, len));
            gwy_debug("Value = %s",
                      (gchar*)g_hash_table_lookup(sisfile->params, idp));
            p += len;
            break;

            case G_TYPE_INT:
            g_assert(len == 2);
            j = gwy_get_guint16_le(&p);
            g_hash_table_insert(sisfile->params, idp, g_memdup(&j, sizeof(j)));
            gwy_debug("Value = %u", j);
            break;

            case G_TYPE_DOUBLE:
            g_assert(len == sizeof(double));
            d = gwy_get_gdouble_le(&p);
            g_hash_table_insert(sisfile->params, idp, g_memdup(&d, sizeof(d)));
            gwy_debug("Value = %g", d);
            break;

            default:
            g_assert_not_reached();
            p += len;
            break;
        }
    }

    for (i = 0; i <= sisfile->nchannels; ) {
        gwy_debug("0x%06x", p - buffer);
        /* this looks like end-of-data */
        if (i == sisfile->nchannels
            && channel->nimages == channel->processing_steps) {
            gwy_debug("OK!");
            return TRUE;
        }

        /* we've got out of sync, try to return what we have, if anything */
        if (size - (p - buffer) < 6) {
            if (i || channel->nimages) {
                gwy_debug("Got out of sync, but managed to read something");
                sisfile->nchannels = i-1;
                return TRUE;
            }
            err_FILE_TYPE(error, "SIS");
            return FALSE;
        }

        id = gwy_get_guint16_le(&p);
        len = gwy_get_guint32_le(&p);
        gwy_debug("id = %u, len = %u", id, len);
        /* we've got out of sync, try to return what we have, if anything */
        if (!len || size - (p - buffer) < len) {
            if (i || channel->nimages) {
                gwy_debug("Got out of sync, but managed to read something");
                sisfile->nchannels = i;
                return TRUE;
            }
            err_FILE_TYPE(error, "SIS");
            return FALSE;
        }

        switch (id) {
            case SIS_BLOCK_PREVIEW:
            gwy_debug("Preview");
            p += len;
            break;

            case SIS_BLOCK_IMAGE:
            channel->nimages++;
            channel->images = g_renew(SISImage, channel->images,
                                      channel->nimages);
            image = channel->images + channel->nimages-1;
            gwy_debug("Image #%u of channel %u", channel->nimages, i);
            if (!i || !channel || len < 26) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Unexpected image block."));
                return FALSE;
            }
            /* This is really a guchar[4], not int32 */
            get_CHARARRAY(image->processing_step, &p);
            procstep = NULL;
            for (j = 0; j < G_N_ELEMENTS(processing_steps); j++) {
                if (memcmp(image->processing_step, processing_steps[j].symbol,
                           4) == 0) {
                    procstep = processing_steps + j;
                    gwy_debug("Processing step %.4s (%s), data size = %u",
                              image->processing_step, procstep->meta,
                              procstep->data_size);
                    break;
                }
            }
            if (!procstep) {
                g_warning("UNKNOWN processing step %.4s",
                          image->processing_step);
            }
            image->processing_step_index = gwy_get_guint16_le(&p);
            image->channel_index = gwy_get_guint16_le(&p);
            /* This is really a guchar[4], not int32 */
            memcpy(image->parent_processing_step, p, 4);
            p += 4;
            image->parent_processing_step_index = gwy_get_guint16_le(&p);
            image->parent_processing_step_channel_index = gwy_get_guint16_le(&p);
            p += procstep ? procstep->data_size : 0;
            if (size - (p - buffer) < 10) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("End of file reached in image block."));
                return FALSE;
            }

            image->width = gwy_get_guint16_le(&p);
            image->height = gwy_get_guint16_le(&p);
            image->bpp = gwy_get_guint16_le(&p);
            image->priority = gwy_get_guint16_le(&p);
            image->image_data_saved = gwy_get_guint16_le(&p);
            gwy_debug("width = %u, height = %u, bpp = %u, saved = %s",
                      image->width, image->height, image->bpp,
                      image->image_data_saved ? "NO" : "YES");
            /* XXX: len is unreliable bogus, some data files have samples
             * instead of bytes here... but we have to figure out whether
             * there is some data or not */
            /*p += len;*/
            if (len == 26) {
                gwy_debug("assuming no data");
                image->image_data = NULL;
                p += procstep ? procstep->data_size : 0;
            }
            else {
                len = image->width * image->height * image->bpp;
                gwy_debug("assuming data of size %u", len);
                if (size - (p - buffer) < len) {
                    g_set_error(error, GWY_MODULE_FILE_ERROR,
                                GWY_MODULE_FILE_ERROR_DATA,
                                _("End of file reached in image block."));
                    return FALSE;
                }
                image->image_data = p;
                p += len;
            }
            break;

            case SIS_BLOCK_CHANNEL:
            i++;
            gwy_debug("Channel %u", i);
            if (len < 8) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("End of file reached in channel block."));
                return FALSE;
            }
            channel = sisfile->channels + i-1;
            channel->data_type = gwy_get_guint16_le(&p);
            channel->signal_source = gwy_get_guint16_le(&p);
            channel->scanning_direction = gwy_get_guint16_le(&p);
            channel->processing_steps = gwy_get_guint16_le(&p);
            gwy_debug("data type = %u, signal source = %u",
                      channel->data_type, channel->signal_source);
            gwy_debug("scanning direction = %u, processing steps = %u",
                      channel->scanning_direction, channel->processing_steps);
            /* skip whatever undocumented remains */
            p += len - 8;
            break;

            default:
            gwy_debug("Funny stuff (alien block id)");
            p += len;
            break;
        }
    }

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("End of file reached when another channel was expected."));
    return FALSE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

