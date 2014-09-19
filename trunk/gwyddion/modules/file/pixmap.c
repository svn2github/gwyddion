/*
 *  @(#) $Id$
 *  Copyright (C) 2004-2014 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 * [FILE-MAGIC-USERGUIDE]
 * Pixmap images
 * .png .jpeg .tiff .tga .pnm .bmp
 * Read[1] Export[2]
 * [1] Import support relies on Gdk-Pixbuf and hence may vary among systems.
 * [2] Usually lossy, intended for presentational purposes.  16bit grayscale
 * export is possible to PNG, TIFF and PNM.
 **/
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <glib/gstdio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <cairo.h>
/* FIXME: We also need pangocairo, i.e. Pango 1.8+, maybe newer. */

/* We want cairo_ps_surface_set_eps().  So if we don't get it we just pretend
 * cairo doesn't have the PS surface at all. */
#if (CAIRO_VERSION_MAJOR < 1 || (CAIRO_VERSION_MAJOR == 1 && CAIRO_VERSION_MINOR < 6))
#undef CAIRO_HAS_PS_SURFACE
#endif

#ifdef CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif

#ifdef CAIRO_HAS_PS_SURFACE
#include <cairo-ps.h>
#endif

#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif

#ifdef HAVE_PNG
#include <png.h>
#ifdef HAVE_ZLIB
#include <zlib.h>
#else
#define Z_BEST_COMPRESSION 9
#endif
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyversion.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libdraw/gwypixfield.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "gwytiff.h"
#include "image-keys.h"

#define GWY_PNG_EXTENSIONS   ".png"
#define GWY_JPEG_EXTENSIONS  ".jpeg,.jpg,.jpe"
#define GWY_TIFF_EXTENSIONS  ".tiff,.tif"
#define GWY_PPM_EXTENSIONS   ".ppm,.pnm"
#define GWY_BMP_EXTENSIONS   ".bmp"
#define GWY_TARGA_EXTENSIONS ".tga,.targa"

#define pangoscale ((gdouble)PANGO_SCALE)

#define ZOOM2LW(x) ((x) > 1 ? ((x) + 0.4) : 1)

enum {
    BITS_PER_SAMPLE = 8,
    TICK_LENGTH     = 10,
    PREVIEW_SIZE    = 240,
    FONT_SIZE       = 12
};

/* What is present on the exported image */
typedef enum {
    PIXMAP_NONE,
    PIXMAP_RULERS,
    PIXMAP_FMSCALE = PIXMAP_RULERS,
    PIXMAP_SCALEBAR
} PixmapOutput;

typedef enum {
    INSET_POS_TOP_LEFT,
    INSET_POS_TOP_CENTER,
    INSET_POS_TOP_RIGHT,
    INSET_POS_BOTTOM_LEFT,
    INSET_POS_BOTTOM_CENTER,
    INSET_POS_BOTTOM_RIGHT,
    INSET_NPOS
} InsetPosType;

/* What value is used when importing from image */
typedef enum {
    PIXMAP_MAP_NONE = 0,
    PIXMAP_MAP_RED,
    PIXMAP_MAP_GREEN,
    PIXMAP_MAP_BLUE,
    PIXMAP_MAP_VALUE,
    PIXMAP_MAP_SUM,
    PIXMAP_MAP_ALPHA,
    PIXMAP_MAP_LUMA,
    PIXMAP_MAP_ALL,
    PIXMAP_MAP_LAST
} PixmapMapType;

typedef enum {
    GWY_COLORSPACE_RGB = GDK_COLORSPACE_RGB,
    GWY_COLORSPACE_GRAY,
} GwyColorspace;

/* GdkPixbuf does not support 16bit depth nor grayscale images, use something
 * that is able to represent these. */
typedef struct {
    GwyColorspace colorspace;
    guint bit_depth;
    guint nchannels;
    guint width;
    guint height;
    guint rowstride;
    gpointer pixels;
    GdkPixbuf *owner;
} GwyPixbuf;

typedef struct {
    gdouble zoom;
    PixmapOutput xytype;
    PixmapOutput ztype;
    GwyRGBA inset_color;
    InsetPosType inset_pos;
    gboolean draw_mask;
    gboolean draw_selection;
    gboolean text_antialias;
    gchar *font;
    gdouble font_size;
    gboolean scale_font;
    gboolean inset_draw_ticks;
    gboolean inset_draw_label;
    gdouble fmscale_gap;
    gdouble inset_gap;
    guint grayscale;
    gchar *inset_length;
    /* Interface only */
    GwyDataView *data_view;
    GwyDataField *dfield;
    gboolean supports_16bit;
    /* These two are `1:1' sizes, i.e. they differ from data field sizes when
     * realsquare is TRUE. */
    gint xres;
    gint yres;
    gboolean realsquare;

    /* New args */
    gboolean transparent;
    gdouble lw;
} PixmapSaveArgs;

typedef struct {
    gdouble xreal;
    gdouble yreal;
    gint32 xyexponent;
    gboolean xymeasureeq;
    gchar *xyunit;
    gdouble zreal;
    gint32 zexponent;
    gchar *zunit;
    PixmapMapType maptype;
    GdkPixbuf *pixbuf;
} PixmapLoadArgs;

typedef struct {
    PixmapSaveArgs *args;
    GtkWidget *dialog;
    GtkWidget *left_column;
    GtkWidget *right_column;
    GSList *xytypes;
    GSList *ztypes;
    GtkWidget *inset_color_label;
    GtkWidget *inset_color;
    GQuark rb_quark;
    GSList *inset_pos;
    GtkWidget *inset_pos_label[6];
    GtkWidget *inset_length_label;
    GtkWidget *inset_length;
    GtkWidget *inset_length_auto;
    GtkWidget *inset_draw_ticks;
    GtkWidget *inset_draw_label;
    GtkObject *inset_gap;
    GtkObject *zoom;
    GtkObject *width;
    GtkObject *height;
    GtkWidget *font;
    GtkWidget *font_size;
    GtkObject *fmscale_gap;
    GtkWidget *image;
    GtkWidget *draw_mask;
    GtkWidget *draw_selection;
    GtkWidget *scale_font;
    GtkWidget *text_antialias;
    GtkWidget *grayscale;
    GwyContainer *data;
    gboolean in_update;
} PixmapSaveControls;

typedef struct {
    GtkWidget *dialog;
    GdkPixbuf *small_pixbuf;
    GtkWidget *xreal;
    GtkWidget *yreal;
    GtkWidget *xyexponent;
    GtkWidget *xymeasureeq;
    GtkWidget *xyunits;
    GtkWidget *zreal;
    GtkWidget *zexponent;
    GtkWidget *zunits;
    GtkWidget *maptype;
    GtkWidget *view;
    gint xres;
    gint yres;
    PixmapLoadArgs *args;
} PixmapLoadControls;

/* there is a information duplication here,
 * however, we may invent an export format GdkPixbuf cannot load */
typedef struct {
    const gchar *name;
    const gchar *description;
    const gchar *extensions;
    const GdkPixbufFormat *pixbuf_format;
} PixmapFormatInfo;

static gboolean          module_register             (void);
static gint              pixmap_detect               (const GwyFileDetectInfo *fileinfo,
                                                      gboolean only_name,
                                                      const gchar *name);
static GwyContainer*     pixmap_load                 (const gchar *filename,
                                                      GwyRunType mode,
                                                      GError **error,
                                                      const gchar *name);
static void              pixmap_load_set_field       (GwyContainer *container,
                                                      gint id,
                                                      GwyDataField *dfield,
                                                      const PixmapLoadArgs *args,
                                                      const gchar *title);
static void              pixmap_load_pixbuf_to_dfield(GdkPixbuf *pixbuf,
                                                      GwyDataField *dfield,
                                                      PixmapMapType maptype);
static gboolean          pixmap_load_dialog          (PixmapLoadArgs *args,
                                                      const gchar *name,
                                                      gint xres,
                                                      gint yres,
                                                      gboolean mapknown,
                                                      gboolean grayscale,
                                                      gboolean has_alpha);
static void              pixmap_load_create_preview  (PixmapLoadArgs *args,
                                                      PixmapLoadControls *controls);
static void              pixmap_load_map_type_update (GtkWidget *combo,
                                                      PixmapLoadControls *controls);
static void              xyreal_changed_cb           (GtkAdjustment *adj,
                                                      PixmapLoadControls *controls);
static void              xymeasureeq_changed_cb      (PixmapLoadControls *controls);
static void              set_combo_from_unit         (GtkWidget *combo,
                                                      const gchar *str);
static void              units_change_cb             (GtkWidget *button,
                                                      PixmapLoadControls *controls);
static void              pixmap_add_import_log       (GwyContainer *data,
                                                      gint id,
                                                      const gchar *filetype,
                                                      const gchar *filename);
static void              pixmap_load_update_controls (PixmapLoadControls *controls,
                                                      PixmapLoadArgs *args);
static void              pixmap_load_update_values   (PixmapLoadControls *controls,
                                                      PixmapLoadArgs *args);
static GwyPixbuf*        pixmap_draw                 (GwyContainer *data,
                                                      PixmapSaveArgs *args);
static void              gwy_pixbuf_free             (GwyPixbuf *gwypixbuf);
static GwyPixbuf*        pixmap_draw_pixbuf          (GwyContainer *data,
                                                      const gchar *format_name,
                                                      gboolean supports_16bit,
                                                      GwyRunType mode,
                                                      GError **error);
static GdkPixbuf*        pixmap_draw_presentational  (GwyContainer *data,
                                                      PixmapSaveArgs *args);
static GwyPixbuf*        pixmap_draw_heightfield     (PixmapSaveArgs *args);
static gboolean          pixmap_save_dialog          (GwyContainer *data,
                                                      PixmapSaveArgs *args,
                                                      const gchar *name);
static gboolean          pixmap_save_png             (GwyContainer *data,
                                                      const gchar *filename,
                                                      GwyRunType mode,
                                                      GError **error);
static gboolean          pixmap_save_jpeg            (GwyContainer *data,
                                                      const gchar *filename,
                                                      GwyRunType mode,
                                                      GError **error);
static gboolean          pixmap_save_tiff            (GwyContainer *data,
                                                      const gchar *filename,
                                                      GwyRunType mode,
                                                      GError **error);
static gboolean          pixmap_save_ppm             (GwyContainer *data,
                                                      const gchar *filename,
                                                      GwyRunType mode,
                                                      GError **error);
static gboolean          pixmap_save_bmp             (GwyContainer *data,
                                                      const gchar *filename,
                                                      GwyRunType mode,
                                                      GError **error);
static gboolean          pixmap_save_targa           (GwyContainer *data,
                                                      const gchar *filename,
                                                      GwyRunType mode,
                                                      GError **error);
static GdkPixbuf*        hruler                      (GtkWidget *widget,
                                                      gint size,
                                                      gint extra,
                                                      gdouble real,
                                                      gdouble zoom,
                                                      gdouble offset,
                                                      const gchar *font,
                                                      GwySIUnit *siunit,
                                                      GwySIValueFormat **pformat);
static GdkPixbuf*        vruler                      (GtkWidget *widget,
                                                      gint size,
                                                      gint extra,
                                                      gdouble real,
                                                      gdouble zoom,
                                                      gdouble offset,
                                                      const gchar *font,
                                                      GwySIValueFormat *format);
static gint              gwy_pixmap_step_to_prec     (gdouble d);
static void              unantialias                 (GdkPixbuf *pixbuf);
static GdkPixbuf*        fmscale                     (GtkWidget *widget,
                                                      gint size,
                                                      gdouble bot,
                                                      gdouble top,
                                                      gdouble zoom,
                                                      const gchar *font,
                                                      GwySIUnit *siunit,
                                                      gboolean inner_ticks);
static GdkPixbuf*        scalebar                    (GtkWidget *widget,
                                                      gint size,
                                                      const gchar *length,
                                                      gdouble real,
                                                      gdouble zoom,
                                                      const GwyRGBA *color,
                                                      const gchar *font,
                                                      gboolean draw_ticks,
                                                      gboolean draw_label);
static GdkDrawable*      prepare_drawable            (GtkWidget *widget,
                                                      gint width,
                                                      gint height,
                                                      gint lw,
                                                      GdkGC **gc);
static PangoLayout*      prepare_layout              (gdouble zoom,
                                                      const gchar *font);
static PixmapFormatInfo* find_format                 (const gchar *name);
static void              pixmap_save_free_args       (PixmapSaveArgs *args);
static void              pixmap_save_load_args       (GwyContainer *container,
                                                      PixmapSaveArgs *args);
static void              pixmap_save_save_args       (GwyContainer *container,
                                                      PixmapSaveArgs *args);
static void              pixmap_save_sanitize_args   (PixmapSaveArgs *args);
static void              pixmap_load_load_args       (GwyContainer *container,
                                                      PixmapLoadArgs *args);
static void              pixmap_load_save_args       (GwyContainer *container,
                                                      PixmapLoadArgs *args);
static void              pixmap_load_sanitize_args   (PixmapLoadArgs *args);
static gchar*            scalebar_auto_length        (GwyDataField *dfield,
                                                      gdouble *p);
static gboolean          inset_length_ok             (GwyDataField *dfield,
                                                      const gchar *inset_length);
static gboolean          pixmap_save_cairo           (GwyContainer *data,
                                                      const gchar *filename,
                                                      GwyRunType mode,
                                                      GError **error,
                                                      const gchar *name);

static const GwyEnum value_map_types[] = {
    { N_("All channels"), PIXMAP_MAP_ALL,   },
    { N_("Red"),          PIXMAP_MAP_RED,   },
    { N_("Green"),        PIXMAP_MAP_GREEN, },
    { N_("Blue"),         PIXMAP_MAP_BLUE,  },
    { N_("Value (max)"),  PIXMAP_MAP_VALUE, },
    { N_("RGB sum"),      PIXMAP_MAP_SUM,   },
    { N_("Luma"),         PIXMAP_MAP_LUMA,  },
    { N_("Alpha"),        PIXMAP_MAP_ALPHA, },
};

static struct {
    const gchar *name;
    const gchar *description;
    const gchar *extensions;
    GwyFileSaveFunc save;
}
saveable_formats[] = {
    {
        "png",
        N_("Portable Network Graphics (.png)"),
        GWY_PNG_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_png,
    },
    {
        "jpeg",
        N_("JPEG (.jpeg,.jpg)"),
        GWY_JPEG_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_jpeg,
    },
    {
        "tiff",
        N_("TIFF (.tiff,.tif)"),
        GWY_TIFF_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_tiff,
    },
    {
        "pnm",
        N_("Portable Pixmap (.ppm,.pnm)"),
        GWY_PPM_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_ppm,
    },
    {
        "bmp",
        N_("Windows or OS2 Bitmap (.bmp)"),
        GWY_BMP_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_bmp
    },
    {
        "tga",
        N_("TARGA (.tga,.targa)"),
        GWY_TARGA_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_targa
    },
#ifdef CAIRO_HAS_PDF_SURFACE
    {
        "pdf",
        N_("Portable document format (.pdf)"),
        ".pdf",
        &pixmap_save_cairo
    },
#endif
#ifdef CAIRO_HAS_PS_SURFACE
    {
        "eps",
        N_("Encapsulated PostScript (.eps)"),
        ".eps",
        &pixmap_save_cairo
    },
#endif
#ifdef CAIRO_HAS_SVG_SURFACE
    {
        "svg",
        N_("Scalable Vector Graphics (.svg)"),
        ".svg",
        &pixmap_save_cairo
    },
#endif
};

static struct {
    const gchar *name;
    const gchar *description;
}
known_formats[] = {
    {
        "gif",
        N_("Graphics Interchange Format (.gif)"),
    },
    {
        "jpeg2000",
        N_("JPEG 2000 (.jpx)"),
    },
    {
        "pcx",
        N_("PCX (.pcx)"),
    },
    {
        "xpm",
        N_("X Pixmap (.xpm)"),
    },
    {
        "ras",
        N_("Sun raster image (.ras)"),
    },
    {
        "icns",
        N_("Apple icon (.icns)"),
    },
};

/* List of PixmapFormatInfo for all formats.
 * FIXME: this is never freed */
static GSList *pixmap_formats = NULL;

static const PixmapSaveArgs pixmap_save_defaults = {
    1.0, PIXMAP_RULERS, PIXMAP_FMSCALE,
    { 1.0, 1.0, 1.0, 1.0 }, INSET_POS_BOTTOM_RIGHT,
    TRUE, TRUE, TRUE,
    "Helvetica", FONT_SIZE, TRUE, TRUE, TRUE,
    1.0, 1.0,
    0, "",
    /* Interface only */
    NULL, NULL, FALSE, 0, 0, FALSE,

    /* New args */
    FALSE,
    1.0,
};

static const PixmapLoadArgs pixmap_load_defaults = {
    100.0, 100.0, -6, TRUE, "m", 1.0, -6, "m", PIXMAP_MAP_VALUE, NULL
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Renders data into pixmap images and imports data from pixmap images. "
       "It supports the following image formats for export: "
       "PNG, JPEG, TIFF, PPM, BMP, TARGA. "
       "Import support relies on GDK and thus may be installation-dependent."),
    "Yeti <yeti@gwyddion.net>",
    "7.29",
    "David Nečas (Yeti)",
    "2004-2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    PixmapFormatInfo *format_info;
    GSList *formats, *l;
    gboolean registered[G_N_ELEMENTS(saveable_formats)];
    guint i;

    gwy_clear(registered, G_N_ELEMENTS(registered));
    formats = gdk_pixbuf_get_formats();
    for (l = formats; l; l = g_slist_next(l)) {
        GdkPixbufFormat *pixbuf_format = (GdkPixbufFormat*)l->data;
        GwyFileSaveFunc save = NULL;
        gchar *fmtname;

        /* Ignore all vector formats */
        fmtname = gdk_pixbuf_format_get_name(pixbuf_format);
        gwy_debug("Found format %s", fmtname);
        if (gdk_pixbuf_format_is_scalable(pixbuf_format)) {
            gwy_debug("Ignoring scalable GdkPixbuf format %s.", fmtname);
            continue;
        }

        /* Use a whitelist of safe formats, namely those with an explicit
         * detection in pixmap_detect().  GdkPixbuf loaders tend to accept
         * any rubbish as their format and then crash because it isn't. */
        if (!gwy_stramong(fmtname,
                          "bmp", "gif", "icns", "jpeg", "jpeg2000", "pcx",
                          "png", "pnm", "ras", "tga", "tiff", "xpm", NULL)) {
            gwy_debug("Ignoring GdkPixbuf format %s because it is not on "
                      "the whitelist.", fmtname);
            continue;
        }

        format_info = g_new0(PixmapFormatInfo, 1);
        format_info->name = fmtname;
        format_info->pixbuf_format = pixbuf_format;
        for (i = 0; i < G_N_ELEMENTS(saveable_formats); i++) {
            /* FIXME: hope we have the same format names */
            if (gwy_strequal(fmtname, saveable_formats[i].name)) {
                gwy_debug("Found GdkPixbuf loader for known type: %s", fmtname);
                format_info->description = saveable_formats[i].description;
                save = saveable_formats[i].save;
                format_info->extensions = saveable_formats[i].extensions;
                registered[i] = TRUE;
                break;
            }
        }
        if (!save) {
            gchar *s, **ext;

            gwy_debug("Found GdkPixbuf loader for new type: %s", fmtname);
            format_info->description
                = gdk_pixbuf_format_get_description(pixbuf_format);
            ext = gdk_pixbuf_format_get_extensions(pixbuf_format);
            s = g_strjoinv(",.", ext);
            format_info->extensions = g_strconcat(".", s, NULL);
            g_free(s);
            g_strfreev(ext);

            /* Fix silly descriptions starting `The image format...' or
             * something like that that are unusable for a sorted list. */
            for (i = 0; i < G_N_ELEMENTS(known_formats); i++) {
                if (gwy_strequal(fmtname, known_formats[i].name)) {
                    gwy_debug("Fixing the description of known type: %s",
                              fmtname);
                    format_info->description = known_formats[i].description;
                    break;
                }
            }
        }
        gwy_file_func_register(format_info->name,
                               format_info->description,
                               &pixmap_detect,
                               &pixmap_load,
                               NULL,
                               save);
        pixmap_formats = g_slist_append(pixmap_formats, format_info);
    }

    for (i = 0; i < G_N_ELEMENTS(saveable_formats); i++) {
        if (registered[i])
            continue;
        gwy_debug("Saveable format %s not known to GdkPixbuf",
                  saveable_formats[i].name);
        format_info = g_new0(PixmapFormatInfo, 1);
        format_info->name = saveable_formats[i].name;
        format_info->description = saveable_formats[i].description;
        format_info->extensions = saveable_formats[i].extensions;

        gwy_file_func_register(format_info->name,
                               format_info->description,
                               &pixmap_detect,
                               NULL,
                               NULL,
                               saveable_formats[i].save);
        pixmap_formats = g_slist_append(pixmap_formats, format_info);
    }

    g_slist_free(formats);

    return TRUE;
}

/***************************************************************************
 *
 *  detect
 *
 ***************************************************************************/

static gint
pixmap_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name,
              const gchar *name)
{
    GdkPixbufLoader *loader;
    GError *err = NULL;
    PixmapFormatInfo *format_info;
    gint score;
    gchar **extensions;
    guint ext;

    gwy_debug("Running detection for file type %s", name);

    format_info = find_format(name);
    g_return_val_if_fail(format_info, 0);

    extensions = g_strsplit(format_info->extensions, ",", 0);
    g_assert(extensions);
    for (ext = 0; extensions[ext]; ext++) {
        if (g_str_has_suffix(fileinfo->name_lowercase, extensions[ext]))
            break;
    }
    score = extensions[ext] ? 19 : 0;
    g_strfreev(extensions);
    if (only_name) /* || !score)*/
        return score;

    /* FIXME: this is incorrect, but no one is going to import data from such
     * a small valid image anyway */
    if (fileinfo->buffer_len < 64)
        return 0;

    /* GdkPixbuf does a terrible job regarding detection so we do some sanity
     * check ourselves */
    score = 70;
    if (gwy_strequal(name, "png")) {
        if (memcmp(fileinfo->head, "\x89PNG\r\n\x1a\n", 8) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "bmp")) {
        if (strncmp(fileinfo->head, "BM", 2) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "pnm")) {
        if (fileinfo->head[0] != 'P' || !g_ascii_isdigit(fileinfo->head[1]))
            return 0;
    }
    else if (gwy_strequal(name, "xpm")) {
        if (strncmp(fileinfo->head, "/* XPM */", 9) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "tiff")) {
        /* The pixbuf loader is unlikely to load BigTIFFs any time soon. */
        GwyTIFFVersion version = GWY_TIFF_CLASSIC;
#ifdef __WIN64
        /* The TIFF loader (supposedly GDI-based) crashes on Win64.  Unclear
         * why.  TIFF is madness.  Note there is a fallback GwyTIFF loader in
         * hdrimage which will take over when we do this.  */
        return 0;
#else
        gwy_debug("Checking TIFF header");
        if (!gwy_tiff_detect(fileinfo->head, fileinfo->buffer_len,
                             &version, NULL))
            return 0;
        gwy_debug("TIFF header OK (type %.2s)", fileinfo->head);
#endif
    }
    else if (gwy_strequal(name, "jpeg")) {
        if (memcmp(fileinfo->head, "\xff\xd8", 2) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "pcx")) {
        if (fileinfo->head[0] != '\x0a' || fileinfo->head[1] > 0x05)
            return 0;
    }
    else if (gwy_strequal(name, "gif")) {
        if (strncmp(fileinfo->head, "GIF8", 4) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "ras")) {
        if (memcmp(fileinfo->head, "\x59\xa6\x6a\x95", 4) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "icns")) {
        if (memcmp(fileinfo->head, "icns", 4) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "jpeg2000")) {
        if (memcmp(fileinfo->head, "\x00\x00\x00\x0C\x6A\x50\x20\x20\x0D\x0A\x87\x0A\x00\x00\x00\x14\x66\x74\x79\x70\x6A\x70\x32", 23) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "tga")) {
        guint8 cmtype = fileinfo->head[1];
        guint8 dtype = fileinfo->head[2];

        if (dtype == 1 || dtype == 9 || dtype == 32 || dtype == 33) {
            if (cmtype != 1)
                return 0;
        }
        else if (dtype == 2 || dtype == 3 || dtype == 10 || dtype == 11) {
            if (cmtype != 0)
                return 0;
        }
        else
            return 0;
    }
    else {
        /* Assign lower score to loaders we found by trying if they accept
         * the header because they often have no clue. */
        score = 55;
    }

    gwy_debug("Creating a loader for type %s", name);
    loader = gdk_pixbuf_loader_new_with_type(name, NULL);
    gwy_debug("Loader for type %s: %p", name, loader);
    if (!loader)
        return 0;

    /* The TIFF loaders (both libTIFF and GDI-based) seem to crash on broken
     * TIFFs a way too often.  Do not try to feed anything to it just accept
     * the file is a TIFF and hope some other loader of a TIFF-based format
     * will claim it with a higher score. */
    if (gwy_strequal(name, "tiff")) {
        gwy_debug("Avoiding feeding data to TIFF loader, calling "
                  "gdk_pixbuf_loader_close().");
        gdk_pixbuf_loader_close(loader, NULL);
        gwy_debug("Unreferencing the TIFF loader");
        g_object_unref(loader);
        gwy_debug("Returning score %d for TIFF", score - 10);
        return score - 10;
    }

    /* For sane readers, try to feed the start of the file and see if it fails.
     * Success rarely means anything though. */
    if (!gdk_pixbuf_loader_write(loader,
                                 fileinfo->head, fileinfo->buffer_len, &err)) {
        gwy_debug("%s", err->message);
        g_clear_error(&err);
        score = 0;
    }
    gdk_pixbuf_loader_close(loader, NULL);
    g_object_unref(loader);

    return score;
}

/***************************************************************************
 *
 *  load
 *
 ***************************************************************************/

static GwyContainer*
pixmap_load(const gchar *filename,
            GwyRunType mode,
            GError **error,
            const gchar *name)
{
    enum { buffer_length = 4096, nmaptypes = G_N_ELEMENTS(value_map_types) };
    guchar pixmap_buf[buffer_length];
    PixmapFormatInfo *format_info;
    GdkPixbufLoader *loader;
    GwyDataField *dfield;
    GwyContainer *data, *settings;
    GdkPixbuf *pixbuf;
    GError *err = NULL;
    FILE *fh;
    guint n, bpp;
    guchar *pixels, *p;
    gint i, j, width, height, rowstride;
    gboolean has_alpha, maptype_known, ok;
    gint not_grayscale, any_red, any_green, any_blue, alpha_important;
    PixmapLoadArgs args;

    gwy_debug("Loading <%s> as %s", filename, name);

    format_info = find_format(name);
    if (!format_info) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_UNIMPLEMENTED,
                    _("Pixmap has not registered file type `%s'."), name);
        return NULL;
    }

    if (!(fh = g_fopen(filename, "rb"))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Cannot open file for reading: %s."), g_strerror(errno));
        return NULL;
    }

    gwy_debug("Creating a loader for type %s", name);
    loader = gdk_pixbuf_loader_new_with_type(name, &err);
    if (!loader) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("Cannot get pixbuf loader: %s."), err->message);
        g_clear_error(&err);
        fclose(fh);
        return NULL;
    }

    gwy_debug("Reading file content.");
    do {
        n = fread(pixmap_buf, 1, buffer_length, fh);
        gwy_debug("loaded %u bytes", n);
        if (!gdk_pixbuf_loader_write(loader, pixmap_buf, n, &err)) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Pixbuf loader refused data: %s."), err->message);
            g_clear_error(&err);
            g_object_unref(loader);
            fclose(fh);
            return NULL;
        }
    } while (n == buffer_length);
    fclose(fh);

    gwy_debug("Closing the loader.");
    if (!gdk_pixbuf_loader_close(loader, &err)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_DATA,
                    _("Pixbuf loader refused data: %s."), err->message);
        g_clear_error(&err);
        g_object_unref(loader);
        return NULL;
    }

    gwy_debug("Trying to get the pixbuf.");
    pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    gwy_debug("Pixbuf is: %p.", pixbuf);
    g_assert(pixbuf);
    g_object_ref(pixbuf);
    gwy_debug("Finalizing loader.");
    g_object_unref(loader);

    settings = gwy_app_settings_get();
    pixmap_load_load_args(settings, &args);
    args.pixbuf = pixbuf;

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    bpp = has_alpha ? 4 : 3;
    /* check which value mapping methods seem feasible */
    not_grayscale = any_red = any_green = any_blue = alpha_important = 0;
    for (i = 0; i < height; i++) {
        p = pixels + i*rowstride;
        for (j = 0; j < width; j++) {
            guchar red = p[bpp*j], green = p[bpp*j+1], blue = p[bpp*j+2];

            not_grayscale |= (green ^ red) | (red ^ blue);
            any_green |= green;
            any_blue |= blue;
            any_red |= red;
            if (has_alpha)
                alpha_important |= 0xff ^ p[bpp*j+3];
        }
    }
    if (!has_alpha && args.maptype == PIXMAP_MAP_ALPHA)
        args.maptype = pixmap_load_defaults.maptype;

    maptype_known = FALSE;
    if (alpha_important) {
        args.maptype = PIXMAP_MAP_ALPHA;
    }
    else if (!not_grayscale) {
        args.maptype = PIXMAP_MAP_VALUE;
        maptype_known = TRUE;
    }
    else if (!any_green && !any_blue) {
        args.maptype = PIXMAP_MAP_RED;
        maptype_known = TRUE;
    }
    else if (!any_red && !any_blue) {
        args.maptype = PIXMAP_MAP_GREEN;
        maptype_known = TRUE;
    }
    else if (!any_red && !any_green) {
        args.maptype = PIXMAP_MAP_BLUE;
        maptype_known = TRUE;
    }

    /* ask user what she thinks */
    if (mode == GWY_RUN_INTERACTIVE) {
        gwy_debug("Manual import is necessary.");

        ok = pixmap_load_dialog(&args, name, width, height,
                                maptype_known, !not_grayscale, alpha_important);
        pixmap_load_save_args(settings, &args);
        if (!ok) {
            err_CANCELLED(error);
            g_object_unref(pixbuf);
            g_free(args.xyunit);
            g_free(args.zunit);
            return NULL;
        }
    }
    else {
        gwy_debug("Running non-interactively, reusing the last parameters.");
        /* Nothing to do here; the args have been already loaded. */
    }

    data = gwy_container_new();

    if (args.maptype == PIXMAP_MAP_ALL) {
        for (i = 0; i < bpp; i++) {
            PixmapMapType maptype = (i < 3) ? i + 1 : 6;
            dfield = gwy_data_field_new(width, height, args.xreal, args.yreal,
                                        FALSE);
            pixmap_load_pixbuf_to_dfield(pixbuf, dfield, maptype);
            pixmap_load_set_field(data, i, dfield, &args,
                                  gwy_enum_to_string(maptype,
                                                     value_map_types,
                                                     nmaptypes));
            g_object_unref(dfield);
            pixmap_add_import_log(data, i, name, filename);
        }
    }
    else {
        dfield = gwy_data_field_new(width, height, args.xreal, args.yreal,
                                    FALSE);
        pixmap_load_pixbuf_to_dfield(pixbuf, dfield, args.maptype);
        pixmap_load_set_field(data, 0, dfield, &args,
                              gwy_enum_to_string(args.maptype,
                                                 value_map_types, nmaptypes));
        g_object_unref(dfield);
        pixmap_add_import_log(data, 0, name, filename);
    }

    g_object_unref(pixbuf);
    g_free(args.xyunit);
    g_free(args.zunit);

    return data;
}

static void
pixmap_load_set_field(GwyContainer *container,
                      gint id,
                      GwyDataField *dfield,
                      const PixmapLoadArgs *args,
                      const gchar *title)
{
    GwySIUnit *siunit;
    GQuark quark;
    gchar *key;

    gwy_data_field_set_xreal(dfield, args->xreal*pow10(args->xyexponent));
    gwy_data_field_set_yreal(dfield, args->yreal*pow10(args->xyexponent));
    gwy_data_field_multiply(dfield, args->zreal*pow10(args->zexponent));
    siunit = gwy_si_unit_new(args->xyunit);
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);
    siunit = gwy_si_unit_new(args->zunit);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    quark = gwy_app_get_data_key_for_id(id);
    gwy_container_set_object(container, quark, dfield);

    key = g_strdup_printf("%s/title", g_quark_to_string(quark));
    gwy_container_set_string_by_name(container, key, g_strdup(title));
    g_free(key);
}

static void
pixmap_load_pixbuf_to_dfield(GdkPixbuf *pixbuf,
                             GwyDataField *dfield,
                             PixmapMapType maptype)
{
    gint width, height, rowstride, i, j, bpp;
    guchar *pixels, *p;
    gdouble *val, *r;

    gwy_debug("%d", maptype);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    bpp = gdk_pixbuf_get_has_alpha(pixbuf) ? 4 : 3;
    gwy_data_field_resample(dfield, width, height, GWY_INTERPOLATION_NONE);
    val = gwy_data_field_get_data(dfield);

    for (i = 0; i < height; i++) {
        p = pixels + i*rowstride;
        r = val + i*width;

        switch (maptype) {
            case PIXMAP_MAP_ALPHA:
            p++;
            case PIXMAP_MAP_BLUE:
            p++;
            case PIXMAP_MAP_GREEN:
            p++;
            case PIXMAP_MAP_RED:
            for (j = 0; j < width; j++)
                r[j] = p[bpp*j]/255.0;
            break;

            case PIXMAP_MAP_VALUE:
            for (j = 0; j < width; j++) {
                guchar red = p[bpp*j], green = p[bpp*j+1], blue = p[bpp*j+2];
                guchar v = MAX(red, green);

                r[j] = MAX(v, blue)/255.0;
            }
            break;

            case PIXMAP_MAP_SUM:
            for (j = 0; j < width; j++) {
                guchar red = p[bpp*j], green = p[bpp*j+1], blue = p[bpp*j+2];

                r[j] = (red + green + blue)/(3*255.0);
            }
            break;

            case PIXMAP_MAP_LUMA:
            for (j = 0; j < width; j++) {
                guchar red = p[bpp*j], green = p[bpp*j+1], blue = p[bpp*j+2];

                r[j] = (0.2126*red + 0.7152*green + 0.0722*blue)/255.0;
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }
}

static const gchar*
describe_channels(gboolean grayscale, gboolean has_alpha)
{
    if (grayscale)
        return has_alpha ? "GA" : "G";
    else
        return has_alpha ? "RGBA" : "RGB";
}

static gboolean
pixmap_load_dialog(PixmapLoadArgs *args,
                   const gchar *name,
                   gint xres,
                   gint yres,
                   gboolean mapknown,
                   gboolean grayscale,
                   gboolean has_alpha)
{
    enum { RESPONSE_RESET = 1 };

    PixmapLoadControls controls;
    GwyContainer *data;
    GwyPixmapLayer *layer;
    GtkObject *adj;
    GtkAdjustment *adj2;
    GtkWidget *dialog, *table, *label, *align, *button, *hbox, *hbox2;
    GtkSizeGroup *sizegroup;
    GwySIUnit *unit;
    gint response;
    gchar *s, *title;
    gdouble zoom;
    gchar buf[16];
    gint row, n;

    controls.args = args;
    controls.xres = xres;
    controls.yres = yres;

    sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    s = g_ascii_strup(name, -1);
    /* TRANSLATORS: Dialog title; %s is PNG, TIFF, ... */
    title = g_strdup_printf(_("Import %s"), s);
    g_free(s);
    dialog = gtk_dialog_new_with_buttons(title, NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    controls.dialog = dialog;
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_file_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    g_free(title);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    table = gtk_table_new(4, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_add(GTK_CONTAINER(align), table);
    row = 0;

    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(_("Image Information")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    g_snprintf(buf, sizeof(buf), "%u", xres);
    label = gtk_label_new(buf);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gwy_table_attach_row(table, row++, _("Horizontal size:"), _("px"),
                         label);

    g_snprintf(buf, sizeof(buf), "%u", yres);
    label = gtk_label_new(buf);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gwy_table_attach_row(table, row++, _("Vertical size:"), _("px"),
                         label);

    label = gtk_label_new(describe_channels(grayscale, has_alpha));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gwy_table_attach_row(table, row++, _("Channels:"), NULL,
                         label);

    align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    zoom = PREVIEW_SIZE/(gdouble)MAX(xres, yres);
    controls.small_pixbuf = gdk_pixbuf_scale_simple(args->pixbuf,
                                                    MAX(zoom*xres, 1),
                                                    MAX(zoom*yres, 1),
                                                    GDK_INTERP_TILES);
    gwy_debug_objects_creation(G_OBJECT(controls.small_pixbuf));
    data = gwy_container_new();
    controls.view = gwy_data_view_new(data);
    g_object_unref(data);
    pixmap_load_create_preview(args, &controls);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gtk_container_add(GTK_CONTAINER(align), controls.view);

    table = gtk_table_new(6, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 0);
    row = 0;

    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(_("Physical Dimensions")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    adj = gtk_adjustment_new(args->xreal, 0.01, 10000, 1, 100, 0);
    controls.xreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls.xreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.xreal,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("_Width:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.xreal);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    align = gtk_alignment_new(0.0, 0.5, 1.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), align, 2, 3, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_container_add(GTK_CONTAINER(align), hbox2);

    unit = gwy_si_unit_new(args->xyunit);
    controls.xyexponent = gwy_combo_box_metric_unit_new(NULL, NULL,
                                                        args->xyexponent - 6,
                                                        args->xyexponent + 6,
                                                        unit,
                                                        args->xyexponent);
    gtk_size_group_add_widget(sizegroup, controls.xyexponent);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.xyexponent, FALSE, FALSE, 0);

    controls.xyunits = gtk_button_new_with_label(gwy_sgettext("verb|Change"));
    g_object_set_data(G_OBJECT(controls.xyunits), "id", (gpointer)"xy");
    g_signal_connect(controls.xyunits, "clicked",
                     G_CALLBACK(units_change_cb), &controls);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.xyunits, FALSE, FALSE, 0);
    row++;

    adj = gtk_adjustment_new(args->yreal, 0.01, 10000, 1, 100, 0);
    controls.yreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls.yreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.yreal,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("H_eight:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.yreal);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Identical _measures"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls.xymeasureeq = button;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    adj = gtk_adjustment_new(args->zreal, 0.01, 10000, 1, 100, 0);
    controls.zreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls.zreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.zreal,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("_Z-scale (per sample unit):"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.zreal);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    align = gtk_alignment_new(0.0, 0.5, 1.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), align, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_container_add(GTK_CONTAINER(align), hbox2);

    gwy_si_unit_set_from_string(unit, args->zunit);
    controls.zexponent = gwy_combo_box_metric_unit_new(NULL, NULL,
                                                       args->zexponent - 6,
                                                       args->zexponent + 6,
                                                       unit,
                                                       args->zexponent);
    gtk_size_group_add_widget(sizegroup, controls.zexponent);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.zexponent, FALSE, FALSE, 0);
    g_object_unref(unit);

    controls.zunits = gtk_button_new_with_label(gwy_sgettext("verb|Change"));
    g_object_set_data(G_OBJECT(controls.zunits), "id", (gpointer)"z");
    g_signal_connect(controls.zunits, "clicked",
                     G_CALLBACK(units_change_cb), &controls);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.zunits, FALSE, FALSE, 0);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    if (!mapknown) {
        label = gtk_label_new(_("Warning: Colorful images cannot be reliably "
                                "mapped to meaningful values."));
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 3, row, row+1,
                         GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
        gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
        row++;

        n = G_N_ELEMENTS(value_map_types);
        if (!gdk_pixbuf_get_has_alpha(args->pixbuf))
            n--;

        controls.maptype
            = gwy_enum_combo_box_new(value_map_types, n,
                                     G_CALLBACK(pixmap_load_map_type_update),
                                     &controls,
                                     args->maptype, TRUE);
        gwy_table_attach_row(table, row++,
                             gwy_sgettext("verb|Use"), _("as data"),
                             controls.maptype);
    }
    else
        controls.maptype = NULL;

    g_signal_connect_swapped(controls.xymeasureeq, "toggled",
                             G_CALLBACK(xymeasureeq_changed_cb), &controls);
    adj2 = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls.xreal));
    g_signal_connect(adj2, "value-changed",
                     G_CALLBACK(xyreal_changed_cb), &controls);
    adj2 = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls.yreal));
    g_signal_connect(adj2, "value-changed",
                     G_CALLBACK(xyreal_changed_cb), &controls);
    pixmap_load_update_controls(&controls, args);

    g_object_unref(sizegroup);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            pixmap_load_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            g_object_unref(controls.small_pixbuf);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            args->xreal = pixmap_load_defaults.xreal;
            args->yreal = pixmap_load_defaults.yreal;
            args->xyexponent = pixmap_load_defaults.xyexponent;
            args->xymeasureeq = pixmap_load_defaults.xymeasureeq;
            g_free(args->xyunit);
            args->xyunit = g_strdup(pixmap_load_defaults.xyunit);
            args->zreal = pixmap_load_defaults.zreal;
            args->zexponent = pixmap_load_defaults.zexponent;
            g_free(args->zunit);
            args->zunit = g_strdup(pixmap_load_defaults.zunit);
            args->maptype = pixmap_load_defaults.maptype;
            pixmap_load_update_controls(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    pixmap_load_update_values(&controls, args);
    gtk_widget_destroy(dialog);
    g_object_unref(controls.small_pixbuf);

    return TRUE;
}

static void
pixmap_load_create_preview(PixmapLoadArgs *args,
                           PixmapLoadControls *controls)
{
    PixmapMapType maptype = args->maptype;
    GwyContainer *data;
    GwyDataField *dfield;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    if (!gwy_container_gis_object_by_name(data, "/0/data", &dfield)) {
        dfield = gwy_data_field_new(1, 1, 1.0, 1.0, FALSE);
        gwy_container_set_object_by_name(data, "/0/data", dfield);
        g_object_unref(dfield);
    }
    if (maptype == PIXMAP_MAP_ALL)
        maptype = PIXMAP_MAP_RED;
    pixmap_load_pixbuf_to_dfield(controls->small_pixbuf, dfield, maptype);
    gwy_data_field_data_changed(dfield);
}

static void
pixmap_load_map_type_update(GtkWidget *combo,
                            PixmapLoadControls *controls)
{

    controls->args->maptype
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    pixmap_load_create_preview(controls->args, controls);
}

static void
pixmap_load_update_controls(PixmapLoadControls *controls,
                            PixmapLoadArgs *args)
{
    GtkAdjustment *adj;

    /* TODO: Units */
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xreal));
    gtk_adjustment_set_value(adj, args->xreal);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yreal));
    gtk_adjustment_set_value(adj, args->yreal);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->xymeasureeq),
                                 args->xymeasureeq);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->xyexponent),
                                   args->xyexponent);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->zreal));
    gtk_adjustment_set_value(adj, args->zreal);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->zexponent),
                                  args->zexponent);
    if (controls->maptype)
        gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->maptype),
                                      args->maptype);
}

static void
pixmap_load_update_values(PixmapLoadControls *controls,
                          PixmapLoadArgs *args)
{
    GtkAdjustment *adj;

    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xreal));
    args->xreal = gtk_adjustment_get_value(adj);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yreal));
    args->yreal = gtk_adjustment_get_value(adj);
    args->xyexponent
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->xyexponent));
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->zreal));
    args->zreal = gtk_adjustment_get_value(adj);
    args->zexponent
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->zexponent));
    if (controls->maptype)
        args->maptype
            = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->maptype));
}

static void
xyreal_changed_cb(GtkAdjustment *adj,
                  PixmapLoadControls *controls)
{
    static gboolean in_update = FALSE;
    GtkAdjustment *xadj, *yadj;
    gdouble value;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->xymeasureeq))
        || in_update)
        return;

    value = gtk_adjustment_get_value(adj);
    xadj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xreal));
    yadj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yreal));
    in_update = TRUE;
    if (xadj == adj)
        gtk_adjustment_set_value(yadj, value*controls->yres/controls->xres);
    else
        gtk_adjustment_set_value(xadj, value*controls->xres/controls->yres);
    in_update = FALSE;
}

static void
xymeasureeq_changed_cb(PixmapLoadControls *controls)
{
    GtkAdjustment *xadj, *yadj;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->xymeasureeq)))
        return;

    xadj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xreal));
    yadj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yreal));
    gtk_adjustment_set_value(yadj,
                             gtk_adjustment_get_value(xadj)
                             *controls->yres/controls->xres);
}

static void
set_combo_from_unit(GtkWidget *combo,
                    const gchar *str)
{
    GwySIUnit *unit;
    gint power10;

    unit = gwy_si_unit_new_parse(str, &power10);
    gwy_combo_box_metric_unit_set_unit(GTK_COMBO_BOX(combo),
                                       power10 - 6, power10 + 6, unit);
    g_object_unref(unit);
}

static void
units_change_cb(GtkWidget *button,
                PixmapLoadControls *controls)
{
    GtkWidget *dialog, *hbox, *label, *entry;
    const gchar *id, *unit;
    gint response;

    pixmap_load_update_values(controls, controls->args);
    id = g_object_get_data(G_OBJECT(button), "id");
    dialog = gtk_dialog_new_with_buttons(_("Change Units"),
                                         GTK_WINDOW(controls->dialog),
                                         GTK_DIALOG_MODAL
                                         | GTK_DIALOG_NO_SEPARATOR,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(_("New _units:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    entry = gtk_entry_new();
    if (gwy_strequal(id, "xy"))
        gtk_entry_set_text(GTK_ENTRY(entry), controls->args->xyunit);
    else if (gwy_strequal(id, "z"))
        gtk_entry_set_text(GTK_ENTRY(entry), controls->args->zunit);
    else
        g_return_if_reached();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return;
    }

    unit = gtk_entry_get_text(GTK_ENTRY(entry));
    if (gwy_strequal(id, "xy")) {
        set_combo_from_unit(controls->xyexponent, unit);
        g_free(controls->args->xyunit);
        controls->args->xyunit = g_strdup(unit);
    }
    else if (gwy_strequal(id, "z")) {
        set_combo_from_unit(controls->zexponent, unit);
        g_free(controls->args->zunit);
        controls->args->zunit = g_strdup(unit);
    }

    gtk_widget_destroy(dialog);
}

static void
pixmap_add_import_log(GwyContainer *data,
                      gint id,
                      const gchar *filetype,
                      const gchar *filename)
{
    GwyContainer *settings;
    GQuark quark;
    gchar *myfilename = NULL, *fskey, *qualname;

    g_return_if_fail(filename);
    g_return_if_fail(filetype);
    g_return_if_fail(data);

    if (g_utf8_validate(filename, -1, NULL))
        myfilename = g_strdup(filename);
    if (!myfilename)
        myfilename = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
    if (!myfilename)
        myfilename = g_strescape(filename, NULL);

    fskey = g_strdup_printf("/module/%s/filename", filetype);
    quark = g_quark_from_string(fskey);
    g_free(fskey);

    /* Eats myfilename. */
    settings = gwy_app_settings_get();
    gwy_container_set_string(settings, quark, myfilename);

    qualname = g_strconcat("file::", filetype, NULL);
    gwy_app_channel_log_add(data, -1, id, qualname, NULL);
    g_free(qualname);

    /* We know pixmap functions have no such setting as "filename". */
    gwy_container_remove(settings, quark);
}

/***************************************************************************
 *
 *  writers
 *
 ***************************************************************************/

#ifdef HAVE_PNG
static void
add_png_text_chunk_string(png_text *chunk,
                          const gchar *key,
                          const gchar *str,
                          gboolean take)
{
    chunk->compression = PNG_TEXT_COMPRESSION_NONE;
    chunk->key = (char*)key;
    chunk->text = take ? (char*)str : g_strdup(str);
    chunk->text_length = strlen(chunk->text);
}

static void
add_png_text_chunk_float(png_text *chunk,
                         const gchar *key,
                         gdouble value)
{
    gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];

    chunk->compression = PNG_TEXT_COMPRESSION_NONE;
    chunk->key = (char*)key;
    g_ascii_dtostr(buffer, sizeof(buffer), value);
    chunk->text = g_strdup(buffer);
    chunk->text_length = strlen(chunk->text);
}

static gboolean
pixmap_save_png_gray(GwyPixbuf *pixbuf,
                     const gchar *filename,
                     GwyDataField *dfield,
                     const gchar *title,
                     GError **error)
{
    enum { NCHUNKS = 11 };
    png_structp writer;
    png_infop writer_info;
    png_byte **rows = NULL;
    png_text *text_chunks = NULL;
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    guint transform_flags = PNG_TRANSFORM_SWAP_ENDIAN;
#endif
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    guint transform_flags = PNG_TRANSFORM_IDENTITY;
#endif
    /* A bit of convoluted typing to get a png_charpp equivalent. */
    gchar param0[G_ASCII_DTOSTR_BUF_SIZE], param1[G_ASCII_DTOSTR_BUF_SIZE];
    gchar *units, *params[2];
    gdouble min, max;
    FILE *fw;
    guint i;

    writer = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                     NULL, NULL, NULL);

    if (!writer) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("libpng initialization error (in %s)"),
                    "png_create_write_struct");
        return FALSE;
    }

    writer_info = png_create_info_struct(writer);
    if (!writer_info) {
        png_destroy_read_struct(&writer, NULL, NULL);
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("libpng initialization error (in %s)"),
                    "png_create_info_struct");
        return FALSE;
    }

    fw = g_fopen(filename, "wb");

    /* Create the chunks dynamically because the fields of png_text are
     * variable. */
    text_chunks = g_new0(png_text, NCHUNKS);
    i = 0;
    /* Standard PNG keys */
    add_png_text_chunk_string(text_chunks + i++, "Title", title, FALSE);
    add_png_text_chunk_string(text_chunks + i++, "Software", "Gwyddion", FALSE);
    /* Gwyddion GSF keys */
    gwy_data_field_get_min_max(dfield, &min, &max);
    add_png_text_chunk_float(text_chunks + i++, GWY_IMGKEY_XREAL,
                             gwy_data_field_get_xreal(dfield));
    add_png_text_chunk_float(text_chunks + i++, GWY_IMGKEY_YREAL,
                             gwy_data_field_get_yreal(dfield));
    add_png_text_chunk_float(text_chunks + i++, GWY_IMGKEY_XOFFSET,
                             gwy_data_field_get_xoffset(dfield));
    add_png_text_chunk_float(text_chunks + i++, GWY_IMGKEY_YOFFSET,
                             gwy_data_field_get_yoffset(dfield));
    add_png_text_chunk_float(text_chunks + i++, GWY_IMGKEY_ZMIN, min);
    add_png_text_chunk_float(text_chunks + i++, GWY_IMGKEY_ZMAX, max);
    add_png_text_chunk_string(text_chunks + i++, GWY_IMGKEY_XYUNIT,
                              gwy_si_unit_get_string
                                      (gwy_data_field_get_si_unit_xy(dfield),
                                       GWY_SI_UNIT_FORMAT_PLAIN),
                              TRUE);
    add_png_text_chunk_string(text_chunks + i++, GWY_IMGKEY_ZUNIT,
                              gwy_si_unit_get_string
                                      (gwy_data_field_get_si_unit_z(dfield),
                                       GWY_SI_UNIT_FORMAT_PLAIN),
                              TRUE);
    add_png_text_chunk_string(text_chunks + i++, GWY_IMGKEY_TITLE, title, FALSE);
    g_assert(i == NCHUNKS);

    png_set_text(writer, writer_info, text_chunks, NCHUNKS);

    /* Present the scaling information also as calibration chunks.
     * Unfortunately, they cannot represent it fully – the rejected xCAL and
     * yCAL chunks would be necessary for that. */
    png_set_sCAL(writer, writer_info, PNG_SCALE_METER,  /* Usually... */
                 gwy_data_field_get_xreal(dfield),
                 gwy_data_field_get_yreal(dfield));
    units = gwy_si_unit_get_string(gwy_data_field_get_si_unit_z(dfield),
                                   GWY_SI_UNIT_FORMAT_PLAIN);
    g_ascii_dtostr(param0, sizeof(param0), min);
    g_ascii_dtostr(param1, sizeof(param1), (max - min)/G_MAXUINT16);
    params[0] = param0;
    params[1] = param1;
    png_set_pCAL(writer, writer_info, "Z", 0, G_MAXUINT16, 0, 2, units, params);
    g_free(units);

    if (setjmp(png_jmpbuf(writer))) {
        /* FIXME: Not very helpful.  Thread-unsafe. */
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("libpng error occured"));
        png_destroy_write_struct(&writer, &writer_info);
        fclose(fw);
        g_unlink(filename);
        g_free(rows);
        for (i = 0; i < NCHUNKS; i++)
            g_free(text_chunks[i].text);
        g_free(text_chunks);
        return FALSE;
    }

    rows = g_new(png_bytep, pixbuf->height);
    for (i = 0; i < pixbuf->height; i++)
        rows[i] = (png_bytep)pixbuf->pixels + i*pixbuf->rowstride;

    png_init_io(writer, fw);
    png_set_filter(writer, 0, PNG_ALL_FILTERS);
    png_set_compression_level(writer, Z_BEST_COMPRESSION);
    png_set_IHDR(writer, writer_info, pixbuf->width, pixbuf->height,
                 pixbuf->bit_depth, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    /* TODO */
    png_set_rows(writer, writer_info, rows);
    png_write_png(writer, writer_info, transform_flags, NULL);

    /* Cleanup */
    fclose(fw);
    g_free(rows);
    png_destroy_write_struct(&writer, &writer_info);
    for (i = 0; i < NCHUNKS; i++)
        g_free(text_chunks[i].text);
    g_free(text_chunks);

    return TRUE;
}
#else
static gboolean
pixmap_save_png_gray(G_GNUC_UNUSED GwyPixbuf *pixbuf,
                     G_GNUC_UNUSED const gchar *filename,
                     G_GNUC_UNUSED GwyDataField *dfield,
                     G_GNUC_UNUSED const gchar *title,
                     G_GNUC_UNUSED GError **error)
{
    g_assert_not_reached();
    return FALSE;
}
#endif

static gboolean
pixmap_save_png(GwyContainer *data,
                const gchar *filename,
                GwyRunType mode,
                GError **error)
{
    GwyDataField *dfield;
    GwyPixbuf *pixbuf;
    GError *err = NULL;
    gboolean ok;
    gint id;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);

    pixbuf = pixmap_draw_pixbuf(data, "PNG",
#ifdef HAVE_PNG
                                TRUE,
#else
                                FALSE,
#endif
                                mode, error);
    if (!pixbuf)
        return FALSE;

    if (pixbuf->colorspace == GWY_COLORSPACE_GRAY) {
        const guchar *title = "Data";
        gchar *s = g_strdup_printf("/%d/data/title", id);

        gwy_container_gis_string_by_name(data, s, &title);
        g_free(s);

        ok = pixmap_save_png_gray(pixbuf, filename, dfield, title, error);
    }
    else {
        ok = gdk_pixbuf_save(pixbuf->owner, filename, "png", &err, NULL);
        if (!ok) {
            g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                        _("Pixbuf save failed: %s."), err->message);
            g_clear_error(&err);
        }
    }
    gwy_pixbuf_free(pixbuf);

    return ok;
}

static gboolean
pixmap_save_jpeg(GwyContainer *data,
                 const gchar *filename,
                 GwyRunType mode,
                 GError **error)
{
    GwyPixbuf *pixbuf;
    GError *err = NULL;
    gboolean ok;

    pixbuf = pixmap_draw_pixbuf(data, "JPEG", FALSE, mode, error);
    if (!pixbuf)
        return FALSE;

    ok = gdk_pixbuf_save(pixbuf->owner, filename, "jpeg", &err,
                         "quality", "98",
                         NULL);
    if (!ok) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Pixbuf save failed: %s."), err->message);
        g_clear_error(&err);
    }
    gwy_pixbuf_free(pixbuf);

    return ok;
}

/* Expand a word and double-word into LSB-ordered sequence of bytes */
#define W(x) (x)&0xff, (x)>>8
#define Q(x) (x)&0xff, ((x)>>8)&0xff, ((x)>>16)&0xff, (x)>>24

static gboolean
write_tiff_head_rgb(FILE *fh,
                    guint width,
                    guint height,
                    guint bit_depth)
{
    enum {
        N_ENTRIES = 14,
        ESTART = 4 + 4 + 2,
        HEAD_SIZE = ESTART + 12*N_ENTRIES + 4,  /* head + 0th directory */
        /* offsets of things we have to fill run-time */
        WIDTH_OFFSET = ESTART + 12*0 + 8,
        HEIGHT_OFFSET = ESTART + 12*1 + 8,
        ROWS_OFFSET = ESTART + 12*8 + 8,
        BYTES_OFFSET = ESTART + 12*9 + 8,
    };
    static guchar tiff_head[] = {
        0x49, 0x49,   /* magic (LSB) */
        W(42),        /* more magic */
        Q(8),         /* 0th directory offset */
        W(N_ENTRIES), /* number of entries */
        W(GWY_TIFFTAG_IMAGE_WIDTH), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_IMAGE_LENGTH), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_BITS_PER_SAMPLE), W(GWY_TIFF_SHORT), Q(3), Q(HEAD_SIZE),
        W(GWY_TIFFTAG_COMPRESSION), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_COMPRESSION_NONE),
        W(GWY_TIFFTAG_PHOTOMETRIC), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_PHOTOMETRIC_RGB),
        W(GWY_TIFFTAG_STRIP_OFFSETS), W(GWY_TIFF_LONG), Q(1),
            Q(HEAD_SIZE + 22),
        W(GWY_TIFFTAG_ORIENTATION), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_ORIENTATION_TOPLEFT),
        W(GWY_TIFFTAG_SAMPLES_PER_PIXEL), W(GWY_TIFF_SHORT), Q(1), Q(3),
        W(GWY_TIFFTAG_ROWS_PER_STRIP), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_STRIP_BYTE_COUNTS), W(GWY_TIFF_LONG), Q(1), Q(0),
        W(GWY_TIFFTAG_X_RESOLUTION), W(GWY_TIFF_RATIONAL), Q(1),
            Q(HEAD_SIZE + 6),
        W(GWY_TIFFTAG_Y_RESOLUTION), W(GWY_TIFF_RATIONAL), Q(1),
            Q(HEAD_SIZE + 14),
        W(GWY_TIFFTAG_PLANAR_CONFIG), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_PLANAR_CONFIG_CONTIGNUOUS),
        W(GWY_TIFFTAG_RESOLUTION_UNIT), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_RESOLUTION_UNIT_INCH),
        Q(0),              /* next directory (0 = none) */
        /* header data */
        W(0), W(0), W(0),  /* value of bits per sample */
        Q(72), Q(1),       /* x-resolution */
        Q(72), Q(1),       /* y-resolution */
        /* here starts the image data */
    };
    guint nbytes = 3*(bit_depth/8)*width*height;

    *(guint32*)(tiff_head + WIDTH_OFFSET) = GUINT32_TO_LE(width);
    *(guint32*)(tiff_head + HEIGHT_OFFSET) = GUINT32_TO_LE(height);
    *(guint32*)(tiff_head + ROWS_OFFSET) = GUINT32_TO_LE(height);
    *(guint32*)(tiff_head + BYTES_OFFSET) = GUINT32_TO_LE(nbytes);
    *(guint16*)(tiff_head + HEAD_SIZE) = GUINT32_TO_LE(bit_depth);
    *(guint16*)(tiff_head + HEAD_SIZE + 2) = GUINT32_TO_LE(bit_depth);
    *(guint16*)(tiff_head + HEAD_SIZE + 4) = GUINT32_TO_LE(bit_depth);

    return fwrite(tiff_head, sizeof(tiff_head), 1, fh) == 1;
}

static gboolean
write_tiff_head_gray(FILE *fh,
                     guint width,
                     guint height,
                     guint bit_depth)
{
    enum {
        N_ENTRIES = 11,
        ESTART = 4 + 4 + 2,
        HEAD_SIZE = ESTART + 12*N_ENTRIES + 4,  /* head + 0th directory */
        /* offsets of things we have to fill run-time */
        WIDTH_OFFSET = ESTART + 12*0 + 8,
        HEIGHT_OFFSET = ESTART + 12*1 + 8,
        BPS_OFFSET = ESTART + 12*2 + 8,
        ROWS_OFFSET = ESTART + 12*8 + 8,
        BYTES_OFFSET = ESTART + 12*9 + 8,
    };
    static guchar tiff_head[] = {
        0x49, 0x49,   /* magic (LSB) */
        W(42),        /* more magic */
        Q(8),         /* 0th directory offset */
        W(N_ENTRIES), /* number of entries */
        W(GWY_TIFFTAG_IMAGE_WIDTH), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_IMAGE_LENGTH), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_BITS_PER_SAMPLE), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_COMPRESSION), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_COMPRESSION_NONE),
        W(GWY_TIFFTAG_PHOTOMETRIC), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_PHOTOMETRIC_MIN_IS_BLACK),
        W(GWY_TIFFTAG_STRIP_OFFSETS), W(GWY_TIFF_LONG), Q(1), Q(HEAD_SIZE),
        W(GWY_TIFFTAG_ORIENTATION), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_ORIENTATION_TOPLEFT),
        W(GWY_TIFFTAG_SAMPLES_PER_PIXEL), W(GWY_TIFF_SHORT), Q(1), Q(1),
        W(GWY_TIFFTAG_ROWS_PER_STRIP), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_STRIP_BYTE_COUNTS), W(GWY_TIFF_LONG), Q(1), Q(0),
        W(GWY_TIFFTAG_PLANAR_CONFIG), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_PLANAR_CONFIG_CONTIGNUOUS),
        Q(0),              /* next directory (0 = none) */
        /* here starts the image data */
    };
    guint nbytes = (bit_depth/8)*width*height;

    *(guint32*)(tiff_head + WIDTH_OFFSET) = GUINT32_TO_LE(width);
    *(guint32*)(tiff_head + HEIGHT_OFFSET) = GUINT32_TO_LE(height);
    *(guint32*)(tiff_head + ROWS_OFFSET) = GUINT32_TO_LE(height);
    *(guint32*)(tiff_head + BYTES_OFFSET) = GUINT32_TO_LE(nbytes);
    *(guint16*)(tiff_head + BPS_OFFSET) = GUINT32_TO_LE(bit_depth);

    return fwrite(tiff_head, sizeof(tiff_head), 1, fh) == 1;
}

static gboolean
pixmap_save_tiff(GwyContainer *data,
                 const gchar *filename,
                 GwyRunType mode,
                 GError **error)
{
    GwyPixbuf *pixbuf = NULL;
    guint i, rowsize;
    FILE *fh = NULL;
    gboolean ok = FALSE;

    if (!(pixbuf = pixmap_draw_pixbuf(data, "TIFF", TRUE, mode, error)))
        goto fail;

    fh = g_fopen(filename, "wb");
    if (!fh) {
        err_OPEN_WRITE(error);
        goto fail;
    }

    if (pixbuf->colorspace == GWY_COLORSPACE_GRAY) {
        g_assert(pixbuf->nchannels == 1);
        if (!write_tiff_head_gray(fh, pixbuf->width, pixbuf->height,
                                  pixbuf->bit_depth)) {
            err_WRITE(error);
            goto fail;
        }
    }
    else if (pixbuf->colorspace == GWY_COLORSPACE_RGB) {
        g_assert(pixbuf->nchannels == 3);
        if (!write_tiff_head_rgb(fh, pixbuf->width, pixbuf->height,
                                 pixbuf->bit_depth)) {
            err_WRITE(error);
            goto fail;
        }
    }

    rowsize = pixbuf->nchannels * (pixbuf->bit_depth/8) * pixbuf->width;
    for (i = 0; i < pixbuf->height; i++) {
        if (!fwrite((guchar*)pixbuf->pixels + i*pixbuf->rowstride,
                    rowsize, 1, fh)) {
            err_WRITE(error);
            goto fail;
        }
    }
    ok = TRUE;

    gwy_pixbuf_free(pixbuf);;
    if (fh)
        fclose(fh);

fail:
    return ok;
}

#undef W
#undef Q

static void
add_ppm_comment_string(GString *str,
                       const gchar *key,
                       const gchar *value,
                       gboolean take)
{
    g_string_append_printf(str, "# %s %s\n", key, value);
    if (take)
        g_free((gpointer)value);
}

static void
add_ppm_comment_float(GString *str,
                      const gchar *key,
                      gdouble value)
{
    gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];

    g_ascii_dtostr(buffer, sizeof(buffer), value);
    g_string_append_printf(str, "# %s %s\n", key, buffer);
}

static gboolean
pixmap_save_ppm(GwyContainer *data,
                const gchar *filename,
                GwyRunType mode,
                GError **error)
{
    static const gchar ppm_header[] = "P6\n%u\n%u\n255\n";
    static const gchar pgm_header[] = "P5\n%s%u\n%u\n65535\n";
    GwyDataField *dfield;
    GwyPixbuf *pixbuf;
    guint i, rowsize;
    gboolean ok = FALSE;
    gchar *ppmh = NULL;
    FILE *fh;
    gint id;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);

    pixbuf = pixmap_draw_pixbuf(data, "PPM", TRUE, mode, error);
    if (!pixbuf)
        return FALSE;

    fh = g_fopen(filename, "wb");
    if (!fh) {
        err_OPEN_WRITE(error);
        gwy_pixbuf_free(pixbuf);
        return FALSE;
    }

    if (pixbuf->colorspace == GWY_COLORSPACE_GRAY) {
        const guchar *title = "Data";
        gchar *s = g_strdup_printf("/%d/data/title", id);
        GString *str = g_string_new(NULL);
        gdouble min, max;

        gwy_container_gis_string_by_name(data, s, &title);
        g_free(s);

        /* Gwyddion GSF keys */
        gwy_data_field_get_min_max(dfield, &min, &max);
        add_ppm_comment_float(str, GWY_IMGKEY_XREAL,
                              gwy_data_field_get_xreal(dfield));
        add_ppm_comment_float(str, GWY_IMGKEY_YREAL,
                              gwy_data_field_get_yreal(dfield));
        add_ppm_comment_float(str, GWY_IMGKEY_XOFFSET,
                              gwy_data_field_get_xoffset(dfield));
        add_ppm_comment_float(str, GWY_IMGKEY_YOFFSET,
                              gwy_data_field_get_yoffset(dfield));
        add_ppm_comment_float(str, GWY_IMGKEY_ZMIN, min);
        add_ppm_comment_float(str, GWY_IMGKEY_ZMAX, max);
        add_ppm_comment_string(str, GWY_IMGKEY_XYUNIT,
                               gwy_si_unit_get_string
                                      (gwy_data_field_get_si_unit_xy(dfield),
                                       GWY_SI_UNIT_FORMAT_PLAIN),
                               TRUE);
        add_ppm_comment_string(str, GWY_IMGKEY_ZUNIT,
                               gwy_si_unit_get_string
                                      (gwy_data_field_get_si_unit_z(dfield),
                                       GWY_SI_UNIT_FORMAT_PLAIN),
                               TRUE);
        add_ppm_comment_string(str, GWY_IMGKEY_TITLE, title, FALSE);

        ppmh = g_strdup_printf(pgm_header,
                               str->str, pixbuf->width, pixbuf->height);
        g_string_free(str, TRUE);
    }
    else {
        ppmh = g_strdup_printf(ppm_header, pixbuf->width, pixbuf->height);
    }
    if (fwrite(ppmh, 1, strlen(ppmh), fh) != strlen(ppmh)) {
        err_WRITE(error);
        goto end;
    }

    rowsize = pixbuf->nchannels * (pixbuf->bit_depth/8) * pixbuf->width;
    for (i = 0; i < pixbuf->height; i++) {
        if (pixbuf->bit_depth == 16) {
            guint16 *row16 = (guint16*)((guchar*)pixbuf->pixels
                                        + i*pixbuf->rowstride);
            guint j;

            for (j = 0; j < pixbuf->width * pixbuf->nchannels; j++)
                row16[j] = GUINT16_TO_BE(row16[j]);
        }
        if (!fwrite((guchar*)pixbuf->pixels + i*pixbuf->rowstride,
                    rowsize, 1, fh)) {
            err_WRITE(error);
            goto end;
        }
    }

    ok = TRUE;
end:
    gwy_pixbuf_free(pixbuf);
    g_free(ppmh);
    fclose(fh);

    return ok;
}

static gboolean
pixmap_save_bmp(GwyContainer *data,
                const gchar *filename,
                GwyRunType mode,
                GError **error)
{
    static guchar bmp_head[] = {
        'B', 'M',    /* magic */
        0, 0, 0, 0,  /* file size */
        0, 0, 0, 0,  /* reserved */
        54, 0, 0, 0, /* offset */
        40, 0, 0, 0, /* header size */
        0, 0, 0, 0,  /* width */
        0, 0, 0, 0,  /* height */
        1, 0,        /* bit planes */
        24, 0,       /* bpp */
        0, 0, 0, 0,  /* compression type */
        0, 0, 0, 0,  /* (compressed) image size */
        0, 0, 0, 0,  /* x resolution */
        0, 0, 0, 0,  /* y resolution */
        0, 0, 0, 0,  /* ncl */
        0, 0, 0, 0,  /* nic */
    };
    GwyPixbuf *pixbuf;
    guchar *buffer = NULL;
    guint i, j;
    guint bmplen, rowlen, bmprowstride;
    gboolean ok = FALSE;
    FILE *fh;

    pixbuf = pixmap_draw_pixbuf(data, "BMP", FALSE, mode, error);
    if (!pixbuf)
        return FALSE;

    rowlen = 3*pixbuf->width;
    bmprowstride = ((rowlen + 3)/4)*4;
    bmplen = pixbuf->height*bmprowstride + sizeof(bmp_head);

    fh = g_fopen(filename, "wb");
    if (!fh) {
        err_OPEN_WRITE(error);
        gwy_pixbuf_free(pixbuf);
        return FALSE;
    }

    *(guint32*)(bmp_head + 2) = GUINT32_TO_LE(bmplen);
    *(guint32*)(bmp_head + 18) = GUINT32_TO_LE(pixbuf->width);
    *(guint32*)(bmp_head + 22) = GUINT32_TO_LE(pixbuf->height);
    *(guint32*)(bmp_head + 34) = GUINT32_TO_LE(pixbuf->height*bmprowstride);
    if (fwrite(bmp_head, 1, sizeof(bmp_head), fh) != sizeof(bmp_head)) {
        err_WRITE(error);
        goto end;
    }

    /* The ugly part: BMP uses BGR instead of RGB and is written upside down,
     * this silliness may originate nowhere else than in MS... */
    buffer = g_new(guchar, bmprowstride);
    memset(buffer, 0xff, sizeof(bmprowstride));
    for (i = 0; i < pixbuf->height; i++) {
        const guchar *p = ((const guchar *)pixbuf->pixels
                           + (pixbuf->height - 1 - i)*pixbuf->rowstride);
        guchar *q = buffer;

        for (j = pixbuf->width; j; j--, p += 3, q += 3) {
            *q = *(p + 2);
            *(q + 1) = *(p + 1);
            *(q + 2) = *p;
        }
        if (!fwrite(buffer, bmprowstride, 1, fh)) {
            err_WRITE(error);
            goto end;
        }
    }

    ok = TRUE;
end:
    gwy_pixbuf_free(pixbuf);
    g_free(buffer);
    fclose(fh);

    return ok;
}

static gboolean
pixmap_save_targa(GwyContainer *data,
                  const gchar *filename,
                  GwyRunType mode,
                  GError **error)
{
   static guchar targa_head[] = {
     0,           /* idlength */
     0,           /* colourmaptype */
     2,           /* datatypecode: uncompressed RGB */
     0, 0, 0, 0,  /* colourmaporigin, colourmaplength */
     0,           /* colourmapdepth */
     0, 0, 0, 0,  /* x-origin, y-origin */
     0, 0,        /* width */
     0, 0,        /* height */
     24,          /* bits per pixel */
     0x20,        /* image descriptor flags: origin upper */
    };
    GwyPixbuf *pixbuf;
    guchar *buffer = NULL;
    guint i, j;
    gboolean ok = FALSE;
    FILE *fh;

    pixbuf = pixmap_draw_pixbuf(data, "TARGA", FALSE, mode, error);
    if (!pixbuf)
        return FALSE;

    if (pixbuf->height >= 65535 || pixbuf->width >= 65535) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Image is too large to be stored as TARGA."));
        return FALSE;
    }

    *(guint16*)(targa_head + 12) = GUINT16_TO_LE((guint16)pixbuf->width);
    *(guint16*)(targa_head + 14) = GUINT16_TO_LE((guint16)pixbuf->height);

    fh = g_fopen(filename, "wb");
    if (!fh) {
        err_OPEN_WRITE(error);
        gwy_pixbuf_free(pixbuf);
        return FALSE;
    }

    if (fwrite(targa_head, 1, sizeof(targa_head), fh) != sizeof(targa_head)) {
        err_WRITE(error);
        goto end;
    }

    /* The ugly part: TARGA uses BGR instead of RGB */
    buffer = g_new(guchar, pixbuf->rowstride);
    memset(buffer, 0xff, pixbuf->rowstride);
    for (i = 0; i < pixbuf->height; i++) {
        const guchar *p = (const guchar*)pixbuf->pixels + i*pixbuf->rowstride;
        guchar *q = buffer;

        for (j = pixbuf->width; j; j--, p += 3, q += 3) {
            *q = *(p + 2);
            *(q + 1) = *(p + 1);
            *(q + 2) = *p;
        }
        if (!fwrite(buffer, pixbuf->rowstride, 1, fh)) {
            err_WRITE(error);
            goto end;
        }
    }

    ok = TRUE;
end:
    gwy_pixbuf_free(pixbuf);
    g_free(buffer);
    fclose(fh);

    return ok;
}

/***************************************************************************
 *
 *  save - common
 *
 ***************************************************************************/

static GwyPixbuf*
pixmap_draw_pixbuf(GwyContainer *data,
                   const gchar *format_name,
                   gboolean supports_16bit,
                   GwyRunType mode,
                   GError **error)
{
    GwyPixbuf *pixbuf;
    GwyContainer *settings;
    PixmapSaveArgs args;
    const gchar *key;
    gchar *buf;

    settings = gwy_app_settings_get();
    pixmap_save_load_args(settings, &args);
    args.supports_16bit = supports_16bit;
    gwy_app_data_browser_get_current(GWY_APP_DATA_VIEW, &args.data_view,
                                     GWY_APP_DATA_FIELD, &args.dfield,
                                     0);
    if (!args.dfield) {
        err_NO_CHANNEL_EXPORT(error);
        pixmap_save_free_args(&args);
        return FALSE;
    }
    if (!args.data_view) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("Data must be displayed in a window for pixmap export."));
        pixmap_save_free_args(&args);
        return FALSE;
    }
    key = gwy_data_view_get_data_prefix(args.data_view);
    buf = g_strconcat(key, "/realsquare", NULL);
    gwy_container_gis_boolean_by_name(data, buf, &args.realsquare);
    g_free(buf);

    if (!inset_length_ok(args.dfield, args.inset_length)) {
        g_free(args.inset_length);
        args.inset_length = scalebar_auto_length(args.dfield, NULL);
    }

    if (mode == GWY_RUN_INTERACTIVE
        && !pixmap_save_dialog(data, &args, format_name)) {
        pixmap_save_save_args(settings, &args);
        err_CANCELLED(error);
        pixmap_save_free_args(&args);
        return FALSE;
    }
    pixbuf = pixmap_draw(data, &args);
    pixmap_save_save_args(settings, &args);
    pixmap_save_free_args(&args);

    return pixbuf;
}

static GwyPixbuf*
pixmap_draw(GwyContainer *data,
            PixmapSaveArgs *args)
{
    if (args->supports_16bit && args->grayscale)
        return pixmap_draw_heightfield(args);
    else {
        GwyPixbuf *gwypixbuf = g_new(GwyPixbuf, 1);

        gwypixbuf->owner = pixmap_draw_presentational(data, args);
        gwypixbuf->colorspace = gdk_pixbuf_get_colorspace(gwypixbuf->owner);
        gwypixbuf->bit_depth = gdk_pixbuf_get_bits_per_sample(gwypixbuf->owner);
        gwypixbuf->nchannels = gdk_pixbuf_get_n_channels(gwypixbuf->owner);
        gwypixbuf->width = gdk_pixbuf_get_width(gwypixbuf->owner);
        gwypixbuf->height = gdk_pixbuf_get_height(gwypixbuf->owner);
        gwypixbuf->rowstride = gdk_pixbuf_get_rowstride(gwypixbuf->owner);
        gwypixbuf->pixels = gdk_pixbuf_get_pixels(gwypixbuf->owner);
        return gwypixbuf;
    }
}

static void
gwy_pixbuf_free(GwyPixbuf *gwypixbuf)
{
    if (gwypixbuf->owner)
        g_object_unref(gwypixbuf->owner);
    else
        g_free(gwypixbuf->pixels);

    g_free(gwypixbuf);
}

static GdkPixbuf*
pixmap_draw_presentational(GwyContainer *data,
                           PixmapSaveArgs *args)
{
    GwyPixmapLayer *layer;
    GwyLayerBasicRangeType rangetype;
    GdkPixbuf *pixbuf, *datapixbuf, *tmpixbuf;
    GdkPixbuf *hrpixbuf = NULL, *vrpixbuf = NULL, *scalepixbuf = NULL;
    GwySIUnit *siunit_xy, *siunit_z;
    GtkWidget *widget;
    guchar *pixels;
    gint zwidth, zheight, hrh, vrw, scw, y, lw;
    gboolean has_presentation, inverted = FALSE;
    gdouble fontzoom, min, max;
    gint border = 20;
    gint fmw = 18, fmscale_gap = fmw, inset_gap = fmw;

    if (args->supports_16bit && args->grayscale) {
        GwyGradient *gradient;
        gint width = args->zoom * gwy_data_field_get_xres(args->dfield);
        gint height = args->zoom * gwy_data_field_get_yres(args->dfield);

        width = MAX(width, 2);
        height = MAX(height, 2);
        datapixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                    gwy_data_field_get_xres(args->dfield),
                                    gwy_data_field_get_yres(args->dfield));

        gradient = gwy_gradients_get_gradient("Gray");
        gwy_resource_use(GWY_RESOURCE(gradient));
        gwy_pixbuf_draw_data_field(datapixbuf, args->dfield, gradient);
        gwy_resource_release(GWY_RESOURCE(gradient));

        pixbuf = gdk_pixbuf_scale_simple(datapixbuf, width, height,
                                         GDK_INTERP_TILES);
        g_object_unref(datapixbuf);

        return pixbuf;
    }

    widget = GTK_WIDGET(args->data_view);
    g_return_val_if_fail(gwy_data_view_get_data(args->data_view) == data, NULL);
    layer = gwy_data_view_get_base_layer(args->data_view);
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), NULL);
    /* This is not just the scaling of text, but of ticks and other line
     * graphics too. */
    fontzoom = args->font_size/FONT_SIZE;

    siunit_xy = gwy_data_field_get_si_unit_xy(args->dfield);
    has_presentation
        = gwy_layer_basic_get_has_presentation(GWY_LAYER_BASIC(layer));

    datapixbuf = gwy_data_view_export_pixbuf(args->data_view,
                                             args->zoom,
                                             args->draw_mask,
                                             args->draw_selection);
    gwy_debug_objects_creation(G_OBJECT(datapixbuf));
    zwidth = gdk_pixbuf_get_width(datapixbuf);
    zheight = gdk_pixbuf_get_height(datapixbuf);

    fmscale_gap *= fontzoom * args->fmscale_gap;
    inset_gap *= fontzoom * args->inset_gap;
    fmw *= fontzoom;
    lw = ZOOM2LW(fontzoom);

    if (args->xytype == PIXMAP_SCALEBAR) {
        GdkPixbuf *sbpixbuf;
        gint sbw, sbh, sbx, sby;

        sbpixbuf = scalebar(widget, zwidth, args->inset_length,
                            gwy_data_field_get_xreal(args->dfield),
                            fontzoom, &args->inset_color, args->font,
                            args->inset_draw_ticks, args->inset_draw_label);
        sbw = gdk_pixbuf_get_width(sbpixbuf);
        sbh = gdk_pixbuf_get_height(sbpixbuf);

        if (args->inset_pos == INSET_POS_TOP_LEFT
            || args->inset_pos == INSET_POS_TOP_CENTER
            || args->inset_pos == INSET_POS_TOP_RIGHT)
            sby = inset_gap/2;
        else
            sby = zheight - sbh - inset_gap/2;

        if (args->inset_pos == INSET_POS_TOP_LEFT
            || args->inset_pos == INSET_POS_BOTTOM_LEFT)
            sbx = inset_gap/2;
        else if (args->inset_pos == INSET_POS_TOP_CENTER
                 || args->inset_pos == INSET_POS_BOTTOM_CENTER)
            sbx = (zwidth - sbw)/2;
        else
            sbx = zwidth - sbw - inset_gap/2;

        gdk_pixbuf_composite(sbpixbuf, datapixbuf, sbx, sby,
                             sbw, sbh, sbx, sby, 1.0, 1.0,
                             GDK_INTERP_TILES, 0xff);
        g_object_unref(sbpixbuf);
    }

    if (args->xytype != PIXMAP_RULERS && args->ztype == PIXMAP_NONE)
        return datapixbuf;

    if (args->xytype == PIXMAP_RULERS) {
        GwySIValueFormat *format;

        hrpixbuf = hruler(widget, zwidth + 2*lw, border,
                          gwy_data_field_get_xreal(args->dfield),
                          fontzoom, gwy_data_field_get_xoffset(args->dfield),
                          args->font, siunit_xy, &format);
        hrh = gdk_pixbuf_get_height(hrpixbuf);
        if (!args->text_antialias)
            unantialias(hrpixbuf);

        vrpixbuf = vruler(widget, zheight + 2*lw, border,
                          gwy_data_field_get_yreal(args->dfield),
                          fontzoom, gwy_data_field_get_yoffset(args->dfield),
                          args->font, format);
        vrw = gdk_pixbuf_get_width(vrpixbuf);
        if (!args->text_antialias)
            unantialias(vrpixbuf);

        gwy_si_unit_value_format_free(format);
    }
    else {
        vrw = 0;
        hrh = 0;
    }

    if (args->ztype == PIXMAP_FMSCALE) {
        if (has_presentation)
            siunit_z = gwy_si_unit_new(NULL);
        else
            siunit_z = gwy_data_field_get_si_unit_z(args->dfield);

        rangetype = gwy_layer_basic_get_range_type(GWY_LAYER_BASIC(layer));
        gwy_layer_basic_get_range(GWY_LAYER_BASIC(layer), &min, &max);
        scalepixbuf = fmscale(widget, zheight + 2*lw, min, max,
                              fontzoom, args->font, siunit_z,
                              rangetype != GWY_LAYER_BASIC_RANGE_ADAPT);
        inverted = min > max;
        scw = gdk_pixbuf_get_width(scalepixbuf);
        if (has_presentation)
            g_object_unref(siunit_z);
        if (!args->text_antialias)
            unantialias(scalepixbuf);
    }
    else {
        fmscale_gap = 0;
        inset_gap = 0;
        fmw = 0;
        scw = 0;
    }

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, BITS_PER_SAMPLE,
                            vrw + zwidth + 2*lw + 2*border
                            + fmscale_gap + fmw + 2*lw + scw,
                            hrh + zheight + 2*lw + 2*border + border/3);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    gdk_pixbuf_fill(pixbuf, 0xffffffff);
    gdk_pixbuf_copy_area(datapixbuf,
                         0, 0, zwidth, zheight,
                         pixbuf,
                         vrw + lw + border, hrh + lw + border);
    g_object_unref(datapixbuf);
    if (args->xytype == PIXMAP_RULERS) {
        gdk_pixbuf_copy_area(hrpixbuf,
                             0, 0, zwidth + 2*lw + border, hrh,
                             pixbuf,
                             vrw + border, border);
        g_object_unref(hrpixbuf);
        gdk_pixbuf_copy_area(vrpixbuf,
                             0, 0, vrw, zheight + 2*lw + border,
                             pixbuf,
                             border, hrh + border);
        g_object_unref(vrpixbuf);
    }
    if (args->ztype == PIXMAP_FMSCALE) {
        GwyGradient *gradient;
        const guchar *name = NULL, *key, *samples;
        gint nsamples;

        gdk_pixbuf_copy_area(scalepixbuf,
                             0, 0, scw, zheight + 2*lw,
                             pixbuf,
                             border + vrw + zwidth + lw + fmscale_gap + fmw + 2*lw,
                             hrh + border);
        g_object_unref(scalepixbuf);

        key = gwy_layer_basic_get_gradient_key(GWY_LAYER_BASIC(layer));
        if (key)
            gwy_container_gis_string_by_name(data, key, &name);
        gradient = gwy_gradients_get_gradient(name);
        gwy_resource_use(GWY_RESOURCE(gradient));
        samples = gwy_gradient_get_samples(gradient, &nsamples);

        pixels = gdk_pixbuf_get_pixels(pixbuf);
        for (y = 0; y < zheight; y++) {
            gint j, k, yi;
            guchar *row;

            yi = inverted ? zheight-1 - y : y;
            row = pixels
                + gdk_pixbuf_get_rowstride(pixbuf)*(border + hrh + lw + yi)
                + 3*(int)(border + vrw + zwidth + lw + fmscale_gap + lw);
            k = nsamples-1 - floor(nsamples*y/zheight);
            for (j = 0; j < fmw; j++) {
                row[3*j] = samples[4*k];
                row[3*j + 1] = samples[4*k + 1];
                row[3*j + 2] = samples[4*k + 2];
            }
        }

        gwy_resource_release(GWY_RESOURCE(gradient));
    }

    if (args->xytype == PIXMAP_SCALEBAR && args->ztype == PIXMAP_NONE)
        return pixbuf;

    /* outline */
    tmpixbuf = gdk_pixbuf_new_subpixbuf(pixbuf,
                                        vrw + border, hrh + border,
                                        lw, zheight + 2*lw);
    gwy_debug_objects_creation(G_OBJECT(tmpixbuf));
    gdk_pixbuf_fill(tmpixbuf, 0x00000000);
    gdk_pixbuf_copy_area(tmpixbuf, 0, 0, lw, zheight + 2*lw,
                         pixbuf, vrw + border + zwidth + lw, hrh + border);
    if (args->ztype == PIXMAP_FMSCALE) {
        gdk_pixbuf_copy_area(tmpixbuf, 0, 0, lw, zheight + 2*lw,
                             pixbuf,
                             vrw + border + zwidth + lw + fmscale_gap,
                             hrh + border);
        gdk_pixbuf_copy_area(tmpixbuf, 0, 0, lw, zheight + 2*lw,
                             pixbuf,
                             vrw + border + zwidth + lw + fmscale_gap + fmw + lw,
                             hrh + border);
    }
    g_object_unref(tmpixbuf);

    tmpixbuf = gdk_pixbuf_new_subpixbuf(pixbuf,
                                        vrw + border, hrh + border,
                                        zwidth + 2*lw, lw);
    gwy_debug_objects_creation(G_OBJECT(tmpixbuf));
    gdk_pixbuf_fill(tmpixbuf, 0x00000000);
    gdk_pixbuf_copy_area(tmpixbuf, 0, 0, zwidth + 2*lw, lw,
                         pixbuf, vrw + border, hrh + border + zheight + lw);
    if (args->ztype == PIXMAP_FMSCALE) {
        gdk_pixbuf_copy_area(tmpixbuf, 0, 0, fmw + 2*lw, lw,
                             pixbuf,
                             vrw + border + zwidth + 2*lw + fmscale_gap,
                             hrh + border);
        gdk_pixbuf_copy_area(tmpixbuf, 0, 0, fmw + 2*lw, lw,
                             pixbuf,
                             vrw + border + zwidth + 2*lw + fmscale_gap,
                             hrh + border + lw + zheight);
    }
    g_object_unref(tmpixbuf);

    return pixbuf;
}

static GwyPixbuf*
pixmap_draw_heightfield(PixmapSaveArgs *args)
{
    GwyPixbuf *gwypixbuf;
    gdouble min, max;

    g_return_val_if_fail(args->supports_16bit && args->grayscale, NULL);

    gwypixbuf = g_new(GwyPixbuf, 1);
    gwypixbuf->colorspace = GWY_COLORSPACE_GRAY;
    gwypixbuf->bit_depth = args->grayscale;
    gwypixbuf->nchannels = 1;
    gwypixbuf->width = gwy_data_field_get_xres(args->dfield);
    gwypixbuf->height = gwy_data_field_get_yres(args->dfield);
    gwypixbuf->rowstride = gwypixbuf->width * (gwypixbuf->bit_depth/8);
    gwypixbuf->pixels = g_malloc(gwypixbuf->rowstride * gwypixbuf->height);
    gwypixbuf->owner = NULL;

    gwy_data_field_get_min_max(args->dfield, &min, &max);
    if (min == max)
        memset(gwypixbuf->pixels, 0, gwypixbuf->rowstride * gwypixbuf->height);
    else {
        guint16 *pixels = (guint16*)gwypixbuf->pixels;
        gdouble q = 65535.999/(max - min);
        const gdouble *d;
        guint i;

        d = gwy_data_field_get_data_const(args->dfield);
        for (i = 0; i < gwypixbuf->height * gwypixbuf->width; i++)
            pixels[i] = (guint16)(q*(d[i] - min));
    }

    return gwypixbuf;
}

static void
save_calculate_resolutions(PixmapSaveArgs *args)
{
    gdouble xreal, yreal, scale;

    args->xres = gwy_data_field_get_xres(args->dfield);
    args->yres = gwy_data_field_get_yres(args->dfield);
    if (!args->realsquare)
        return;

    xreal = gwy_data_field_get_xreal(args->dfield);
    yreal = gwy_data_field_get_yreal(args->dfield);
    scale = MAX(args->xres/xreal, args->yres/yreal);
    args->xres = GWY_ROUND(xreal*scale);
    args->yres = GWY_ROUND(yreal*scale);
}

static void
save_update_preview(PixmapSaveControls *controls)
{
    GdkPixbuf *pixbuf;
    gdouble zoom;

    zoom = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zoom));

    controls->args->font_size *= controls->args->zoom/zoom;
    if (controls->args->ztype == PIXMAP_NONE
        && controls->args->xytype != PIXMAP_RULERS)
        controls->args->zoom *= 1.4;

    pixbuf = pixmap_draw_presentational(controls->data, controls->args);
    gtk_image_set_from_pixbuf(GTK_IMAGE(controls->image), pixbuf);
    g_object_unref(pixbuf);

    if (controls->args->ztype == PIXMAP_NONE
        && controls->args->xytype != PIXMAP_RULERS)
        controls->args->zoom /= 1.4;
    controls->args->font_size /= controls->args->zoom/zoom;
}

static void
save_update_sensitivity(PixmapSaveControls *controls)
{
    gboolean sens = controls->args->xytype == PIXMAP_SCALEBAR;
    gboolean pmode = !(controls->args->grayscale
                       && controls->args->supports_16bit);
    gboolean fmscale = controls->args->ztype == PIXMAP_FMSCALE;
    guint i;
    GSList *l;

    gtk_widget_set_sensitive(controls->right_column, pmode);
    gtk_widget_set_sensitive(controls->left_column, pmode);

    /* Don't update individual widgets when they are insensitive anyway */
    if (!pmode)
        return;

    gtk_widget_set_sensitive(controls->inset_color_label, sens);
    gtk_widget_set_sensitive(controls->inset_color, sens);
    for (i = 0; i < G_N_ELEMENTS(controls->inset_pos_label); i++)
        gtk_widget_set_sensitive(controls->inset_pos_label[i], sens);
    for (l = controls->inset_pos; l; l = g_slist_next(l))
        gtk_widget_set_sensitive(GTK_WIDGET(l->data), sens);
    gtk_widget_set_sensitive(controls->inset_length_label, sens);
    gtk_widget_set_sensitive(controls->inset_length, sens);
    gtk_widget_set_sensitive(controls->inset_length_auto, sens);
    gtk_widget_set_sensitive(controls->inset_draw_ticks, sens);
    gtk_widget_set_sensitive(controls->inset_draw_label, sens);
    gwy_table_hscale_set_sensitive(controls->inset_gap, sens);
    gwy_table_hscale_set_sensitive(controls->fmscale_gap, fmscale);
}

static void
save_xytype_changed(GtkWidget *button,
                    PixmapSaveControls *controls)
{
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    controls->args->xytype = gwy_radio_buttons_get_current(controls->xytypes);
    save_update_sensitivity(controls);
    save_update_preview(controls);
}

static void
save_ztype_changed(GtkWidget *button,
                   PixmapSaveControls *controls)
{
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    controls->args->ztype = gwy_radio_buttons_get_current(controls->ztypes);
    save_update_sensitivity(controls);
    save_update_preview(controls);
}

static void
save_fmscale_gap_changed(GtkAdjustment *adj,
                         PixmapSaveControls *controls)
{
    if (controls->in_update)
        return;

    controls->args->fmscale_gap = gtk_adjustment_get_value(adj);
    if (controls->args->ztype == PIXMAP_FMSCALE)
        save_update_preview(controls);
}

static void
save_inset_gap_changed(GtkAdjustment *adj,
                       PixmapSaveControls *controls)
{
    if (controls->in_update)
        return;

    controls->args->inset_gap = gtk_adjustment_get_value(adj);
    if (controls->args->xytype == PIXMAP_SCALEBAR)
        save_update_preview(controls);
}

static void
draw_mask_changed(GtkToggleButton *check,
                  PixmapSaveControls *controls)
{
    controls->args->draw_mask = gtk_toggle_button_get_active(check);
    save_update_preview(controls);
}

static void
draw_selection_changed(GtkToggleButton *check,
                       PixmapSaveControls *controls)
{
    controls->args->draw_selection = gtk_toggle_button_get_active(check);
    save_update_preview(controls);
}

static void
update_font_size_to_zoom(PixmapSaveControls *controls)
{
    gdouble zoom;
    gdouble s, v;
    gdouble lower, upper;
    GtkSpinButton *spin;
    GtkAdjustment *adj;

    zoom = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zoom));
    s = FONT_SIZE * zoom;
    lower = 0.25 * s;
    upper = 5.0 * s;
    spin = GTK_SPIN_BUTTON(controls->font_size);
    adj = gtk_spin_button_get_adjustment(spin);
    g_object_set(adj, "lower", lower, "upper", upper, NULL);
    // This does not clamp the value.  Fix it manually.
    v = gtk_adjustment_get_value(adj);
    if (v < lower || v > upper)
        gtk_adjustment_set_value(adj, CLAMP(v, lower, upper));
}

static void
zoom_changed(GtkAdjustment *adj,
             PixmapSaveControls *controls)
{
    gdouble zoom;

    if (controls->in_update)
        return;

    /* Do not update args->zoom, it holds the preview zoom */
    zoom = gtk_adjustment_get_value(adj);
    controls->in_update = TRUE;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->width),
                             zoom*controls->args->xres);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height),
                             zoom*controls->args->yres);
    if (controls->args->scale_font)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(controls->font_size),
                                  zoom*FONT_SIZE);
    else {
        update_font_size_to_zoom(controls);
        if (controls->args->xytype != PIXMAP_NONE
             || controls->args->ztype != PIXMAP_NONE)
            save_update_preview(controls);
    }
    controls->in_update = FALSE;
}

static void
width_changed(GtkAdjustment *adj,
              PixmapSaveControls *controls)
{
    gdouble width;

    if (controls->in_update)
        return;

    width = gtk_adjustment_get_value(adj);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zoom),
                             width/controls->args->xres);
}

static void
height_changed(GtkAdjustment *adj,
               PixmapSaveControls *controls)
{
    gdouble height;

    if (controls->in_update)
        return;

    height = gtk_adjustment_get_value(adj);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zoom),
                             height/controls->args->yres);
}

static void
font_changed(GtkFontButton *button,
             PixmapSaveControls *controls)
{
    PixmapSaveArgs *args = controls->args;
    GtkAdjustment *adj;
    const gchar *full_font = gtk_font_button_get_font_name(button);
    const gchar *size_pos = strrchr(full_font, ' ');
    gchar *end;
    gdouble size;

    if (!size_pos) {
        g_warning("Cannot parse font description `%s' into name and size.",
                  full_font);
        return;
    }
    size = g_ascii_strtod(size_pos+1, &end);
    if (end == size_pos+1) {
        g_warning("Cannot parse font description `%s' into name and size.",
                  full_font);
        return;
    }

    g_free(args->font);
    args->font = g_strndup(full_font, size_pos-full_font);

    if (!args->scale_font) {
        adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->font_size));
        controls->in_update = TRUE;
        gtk_adjustment_set_value(adj, size);
        update_font_size_to_zoom(controls);
        controls->in_update = FALSE;
    }

    if (args->xytype != PIXMAP_NONE || args->ztype != PIXMAP_NONE)
        save_update_preview(controls);
}

static void
update_selected_font(PixmapSaveControls *controls)
{
    PixmapSaveArgs *args = controls->args;
    gchar *full_font;
    gdouble font_size;

    if (args->scale_font)
        font_size = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zoom))
                    * FONT_SIZE;
    else
        font_size = args->font_size;

    full_font = g_strdup_printf("%s %g", controls->args->font, font_size);
    gtk_font_button_set_font_name(GTK_FONT_BUTTON(controls->font), full_font);
    g_free(full_font);
}

static void
scale_font_changed(GtkToggleButton *check,
                   PixmapSaveControls *controls)
{
    controls->args->scale_font = gtk_toggle_button_get_active(check);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->font_size),
                                   !controls->args->scale_font);

    if (controls->args->scale_font) {
        gdouble zoom;

        zoom = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zoom));
        controls->in_update = TRUE;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(controls->font_size),
                                  zoom*FONT_SIZE);
        controls->in_update = FALSE;
    }
    update_selected_font(controls);

    if (controls->args->xytype != PIXMAP_NONE
        || controls->args->ztype != PIXMAP_NONE)
        save_update_preview(controls);
}

static void
text_antialias_changed(GtkToggleButton *check,
                       PixmapSaveControls *controls)
{
    controls->args->text_antialias = gtk_toggle_button_get_active(check);
    if (controls->args->xytype != PIXMAP_NONE
        || controls->args->ztype != PIXMAP_NONE)
        save_update_preview(controls);
}

static void
font_size_changed(GtkAdjustment *adj,
                  PixmapSaveControls *controls)
{
    controls->args->font_size = gtk_adjustment_get_value(adj);
    if (controls->in_update
        || controls->args->scale_font
        || (controls->args->xytype == PIXMAP_NONE
            && controls->args->ztype == PIXMAP_NONE))
        return;

    update_selected_font(controls);
    save_update_preview(controls);
}

static void
save_inset_pos_changed(GtkWidget *button,
                       PixmapSaveControls *controls)
{
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    controls->args->inset_pos
        = gwy_radio_buttons_get_current(controls->inset_pos);

    if (controls->in_update || controls->args->xytype != PIXMAP_SCALEBAR)
        return;

    save_update_preview(controls);
}

static void
save_inset_pos_add(PixmapSaveControls *controls,
                   GtkTable *table,
                   InsetPosType pos,
                   gint col, gint row)
{
    GtkWidget *button;

    button = gtk_radio_button_new_with_label(controls->inset_pos, NULL);
    if (pos == controls->args->inset_pos)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    controls->inset_pos = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
    g_object_set_qdata(G_OBJECT(button), controls->rb_quark,
                       GUINT_TO_POINTER(pos));
    gtk_table_attach(table, button, col, col+1, row, row+1, 0, 0, 0, 0);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(save_inset_pos_changed), controls);
}

static gchar*
scalebar_auto_length(GwyDataField *dfield,
                     gdouble *p)
{
    static const double sizes[] = {
        1.0, 2.0, 3.0, 4.0, 5.0,
        10.0, 20.0, 30.0, 40.0, 50.0,
        100.0, 200.0, 300.0, 400.0, 500.0,
    };
    GwySIValueFormat *format;
    GwySIUnit *siunit;
    gdouble base, x, vmax, real;
    gchar *s;
    gint power10;
    guint i;

    real = gwy_data_field_get_xreal(dfield);
    siunit = gwy_data_field_get_si_unit_xy(dfield);
    vmax = 0.42*real;
    power10 = 3*(gint)(floor(log10(vmax)/3.0));
    base = pow10(power10 + 1e-14);
    x = vmax/base;
    for (i = 1; i < G_N_ELEMENTS(sizes); i++) {
        if (x < sizes[i])
            break;
    }
    x = sizes[i-1] * base;

    format = gwy_si_unit_get_format_for_power10(siunit,
                                                GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                power10, NULL);
    s = g_strdup_printf("%.*f %s",
                        format->precision, x/format->magnitude, format->units);
    gwy_si_unit_value_format_free(format);

    if (p)
        *p = x/real;

    return s;
}

static void
inset_length_changed(GtkEntry *entry,
                     PixmapSaveControls *controls)
{
    PixmapSaveArgs *args = controls->args;
    GwyDataField *dfield;
    const gchar *text;

    dfield = args->dfield;
    text = gtk_entry_get_text(entry);
    g_free(args->inset_length);
    if (inset_length_ok(dfield, text)) {
        /* XXX: We should check markup validity here. */
        args->inset_length = g_strdup(text);
    }
    else {
        args->inset_length = scalebar_auto_length(dfield, NULL);
        gtk_entry_set_text(entry, args->inset_length);
    }

    if (controls->in_update || args->xytype != PIXMAP_SCALEBAR)
        return;

    save_update_preview(controls);
}

static gboolean
inset_length_ok(GwyDataField *dfield,
                const gchar *inset_length)
{
    gdouble xreal, length;
    gint power10;
    GwySIUnit *siunit, *siunitxy;
    gchar *end;
    gboolean ok;

    if (!inset_length || !*inset_length)
        return FALSE;

    length = g_strtod(inset_length, &end);
    siunit = gwy_si_unit_new_parse(end, &power10);
    length *= pow10(power10);
    xreal = gwy_data_field_get_xreal(dfield);
    siunitxy = gwy_data_field_get_si_unit_xy(dfield);
    ok = (gwy_si_unit_equal(siunit, siunitxy)
          && length > 0.1*xreal
          && length < 0.85*xreal);
    g_object_unref(siunit);

    return ok;
}

static void
inset_length_set_auto(PixmapSaveControls *controls)
{
    /* Setting an invalid value causes reset to default. */
    gtk_entry_set_text(GTK_ENTRY(controls->inset_length), "");
    gtk_widget_activate(controls->inset_length);
}

static void
select_inset_color(GwyColorButton *button,
                   PixmapSaveControls *controls)
{
    GtkWindow *parent;
    GtkWidget *dialog, *selector;
    GdkColor gdkcolor;
    gint response;

    gwy_rgba_to_gdk_color(&controls->args->inset_color, &gdkcolor);

    dialog = gtk_color_selection_dialog_new(_("Change Inset Color"));
    selector = GTK_COLOR_SELECTION_DIALOG(dialog)->colorsel;
    gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(selector),
                                          &gdkcolor);
    gtk_color_selection_set_has_palette(GTK_COLOR_SELECTION(selector), FALSE);
    gtk_color_selection_set_has_opacity_control(GTK_COLOR_SELECTION(selector),
                                                FALSE);

    parent = GTK_WINDOW(controls->dialog);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(parent, FALSE);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(selector),
                                          &gdkcolor);
    gtk_widget_destroy(dialog);
    gtk_window_set_modal(parent, TRUE);

    if (response != GTK_RESPONSE_OK)
        return;

    gwy_rgba_from_gdk_color(&controls->args->inset_color, &gdkcolor);
    gwy_color_button_set_color(button, &controls->args->inset_color);
    save_update_preview(controls);
}

static void
grayscale_changed(GtkToggleButton *check,
                  PixmapSaveControls *controls)
{
    controls->args->grayscale = gtk_toggle_button_get_active(check) ? 16 : 0;
    save_update_sensitivity(controls);
    save_update_preview(controls);
}

static void
inset_draw_ticks_toggled(GtkToggleButton *button,
                         PixmapSaveControls *controls)
{
    PixmapSaveArgs *args = controls->args;

    args->inset_draw_ticks = gtk_toggle_button_get_active(button);
    if (controls->in_update || args->xytype != PIXMAP_SCALEBAR)
        return;

    save_update_preview(controls);
}

static void
inset_draw_label_toggled(GtkToggleButton *button,
                         PixmapSaveControls *controls)
{
    PixmapSaveArgs *args = controls->args;

    args->inset_draw_label = gtk_toggle_button_get_active(button);
    if (controls->in_update || args->xytype != PIXMAP_SCALEBAR)
        return;

    save_update_preview(controls);
}

static GtkObject*
attach_gap(GtkTable *table,
           gint row,
           gdouble value)
{
    GtkWidget *widget;
    GtkObject *adj;

    adj = gtk_adjustment_new(value, 0.0, 2.0, 0.01, 0.1, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("Gap:"), NULL, adj,
                            GWY_HSCALE_DEFAULT);

    /* Make the hscale take more columns to accommodate for inset position
     * controls. */
    widget = gwy_table_get_child_widget(GTK_WIDGET(table), row, 2);
    g_object_ref(widget);
    gtk_container_remove(GTK_CONTAINER(table), widget);
    gtk_table_attach(table, widget,
                     3, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_object_unref(widget);

    widget = gwy_table_get_child_widget(GTK_WIDGET(table), row, 1);
    g_object_ref(widget);
    gtk_container_remove(GTK_CONTAINER(table), widget);
    gtk_table_attach(table, widget,
                     1, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_object_unref(widget);

    return adj;
}

static gboolean
pixmap_save_dialog(GwyContainer *data,
                   PixmapSaveArgs *args,
                   const gchar *name)
{
    enum { RESPONSE_RESET = 1 };

    PixmapSaveControls controls;
    GtkWidget *dialog, *spin, *vbox, *hbox, *align, *check, *label;
    GtkTable *table;
    GtkObject *adj;
    gdouble minzoom, maxzoom;
    gint response;
    gchar *s, *title;
    gint row;

    controls.data = data;
    controls.args = args;
    save_calculate_resolutions(args);
    controls.in_update = TRUE;

    s = g_ascii_strup(name, -1);
    title = g_strdup_printf(_("Export %s"), s);
    g_free(s);
    dialog = gtk_dialog_new_with_buttons(title, NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    g_free(title);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    /********************************************************************
     * Left column
     ********************************************************************/

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    table = GTK_TABLE(gtk_table_new(15, 3, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    controls.left_column = GTK_WIDGET(table);
    row = 0;

    gtk_table_attach(table, gwy_label_new_header("Scaling"),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    minzoom = 2.0/MIN(args->xres, args->yres);
    maxzoom = 4096.0/MAX(args->xres, args->yres);
    controls.zoom = gtk_adjustment_new(args->zoom,
                                       minzoom, maxzoom, 0.001, 0.5, 0);
    spin = gwy_table_attach_spinbutton(GTK_WIDGET(table),
                                       row, _("_Zoom:"), NULL, controls.zoom);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    g_signal_connect(controls.zoom, "value-changed",
                     G_CALLBACK(zoom_changed), &controls);
    row++;

    controls.width = gtk_adjustment_new(args->zoom*args->xres,
                                        2, 4096, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(GTK_WIDGET(table),
                                       row, _("_Width:"), "px", controls.width);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    g_signal_connect(controls.width, "value-changed",
                     G_CALLBACK(width_changed), &controls);
    row++;

    controls.height = gtk_adjustment_new(args->zoom*args->yres,
                                         2, 4096, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(GTK_WIDGET(table),
                                       row, _("_Height:"), "px",
                                       controls.height);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    g_signal_connect(controls.height, "value-changed",
                     G_CALLBACK(height_changed), &controls);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    gtk_table_attach(table, gwy_label_new_header(_("Labels")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new(_("Font:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.font = gtk_font_button_new();
    gtk_font_button_set_show_size(GTK_FONT_BUTTON(controls.font), FALSE);
    gtk_font_button_set_use_font(GTK_FONT_BUTTON(controls.font), TRUE);
    gtk_table_attach(table, controls.font,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.font, "font-set",
                     G_CALLBACK(font_changed), &controls);
    row++;

    controls.scale_font
        = gtk_check_button_new_with_mnemonic(_("Scale text _proportionally"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.scale_font),
                                 args->scale_font);
    gtk_table_attach(table, controls.scale_font,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.scale_font, "toggled",
                     G_CALLBACK(scale_font_changed), &controls);
    row++;

    if (args->scale_font)
        args->font_size = FONT_SIZE*args->zoom;
    adj = gtk_adjustment_new(args->font_size,
                             0.5*FONT_SIZE*minzoom, 32.0*FONT_SIZE*maxzoom,
                             1.0, 10.0, 0);
    controls.font_size = gwy_table_attach_spinbutton(GTK_WIDGET(table), row,
                                                     _("_Font size:"), NULL,
                                                     adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls.font_size), 2);
    update_font_size_to_zoom(&controls);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls.font_size),
                                   !args->scale_font);
    g_signal_connect(adj, "value-changed",
                     G_CALLBACK(font_size_changed), &controls);
    row++;

    controls.text_antialias
        = gtk_check_button_new_with_mnemonic(_("Use antialiasing"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.text_antialias),
                                 args->text_antialias);
    gtk_table_attach(table, controls.text_antialias,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.text_antialias, "toggled",
                     G_CALLBACK(text_antialias_changed), &controls);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    gtk_table_attach(table, gwy_label_new_header(_("Options")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("Draw _mask"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), args->draw_mask);
    gtk_table_attach(table, check, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(draw_mask_changed), &controls);
    controls.draw_mask = check;
    row++;

    check = gtk_check_button_new_with_mnemonic(_("Draw _selection"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 args->draw_selection);
    gtk_table_attach(table, check, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(draw_selection_changed), &controls);
    controls.draw_selection = check;
    row++;

    table = GTK_TABLE(gtk_table_new(12, 3, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    row = 0;

    gtk_table_attach(table, gwy_label_new_header(_("Grayscale Mode")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("Export as 1_6 bit "
                                                 "grayscale"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 args->supports_16bit && args->grayscale);
    gtk_widget_set_sensitive(check, args->supports_16bit);
    gtk_table_attach(table, check, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(grayscale_changed), &controls);
    controls.grayscale = check;
    row++;

    /********************************************************************
     * Right column
     ********************************************************************/

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, FALSE, 0);

    table = GTK_TABLE(gtk_table_new(16, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(table));
    controls.right_column = GTK_WIDGET(table);
    row = 0;

    gtk_table_attach(table, gwy_label_new_header(_("Lateral Scale")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.xytypes
        = gwy_radio_buttons_createl(G_CALLBACK(save_xytype_changed), &controls,
                                    args->xytype,
                                    gwy_sgettext("ruler|_None"), PIXMAP_NONE,
                                    _("_Rulers"), PIXMAP_RULERS,
                                    _("_Inset scale bar"), PIXMAP_SCALEBAR,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.xytypes, table,
                                            3, row);

    controls.inset_gap = attach_gap(table, row, args->inset_gap);
    g_signal_connect(controls.inset_gap, "value-changed",
                     G_CALLBACK(save_inset_gap_changed), &controls);
    row++;

    controls.inset_color_label = gtk_label_new(_("Color:"));
    gtk_misc_set_alignment(GTK_MISC(controls.inset_color_label), 0.0, 0.5);
    gtk_table_attach(table, controls.inset_color_label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.inset_color = gwy_color_button_new_with_color(&args->inset_color);
    gwy_color_button_set_use_alpha(GWY_COLOR_BUTTON(controls.inset_color),
                                   FALSE);
    gtk_table_attach(table, controls.inset_color, 1, 2, row, row+1, 0, 0, 0, 0);
    g_signal_connect(controls.inset_color, "clicked",
                     G_CALLBACK(select_inset_color), &controls);
    gtk_table_set_row_spacing(table, row-1, 8);
    row++;

    controls.inset_pos = NULL;

    controls.inset_pos_label[0] = label = gtk_label_new(_("Position:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.inset_pos_label[1] = label = gtk_label_new(_("left"));
    gtk_table_attach(table, label, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.inset_pos_label[2] = label = gtk_label_new(_("center"));
    gtk_table_attach(table, label, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.inset_pos_label[3] = label = gtk_label_new(_("right"));
    gtk_table_attach(table, label, 3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.inset_pos_label[4] = label = gtk_label_new(_("top"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.rb_quark = g_quark_from_string("gwy-radiobuttons-key");

    save_inset_pos_add(&controls, table, INSET_POS_TOP_LEFT, 1, row);
    save_inset_pos_add(&controls, table, INSET_POS_TOP_CENTER, 2, row);
    save_inset_pos_add(&controls, table, INSET_POS_TOP_RIGHT, 3, row);
    row++;

    controls.inset_pos_label[5] = label = gtk_label_new(_("bottom"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    save_inset_pos_add(&controls, table, INSET_POS_BOTTOM_LEFT, 1, row);
    save_inset_pos_add(&controls, table, INSET_POS_BOTTOM_CENTER, 2, row);
    save_inset_pos_add(&controls, table, INSET_POS_BOTTOM_RIGHT, 3, row);
    row++;

    controls.inset_length_label = label
        = gtk_label_new_with_mnemonic(_("_Length:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.inset_length = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(controls.inset_length), 8);
    gtk_entry_set_text(GTK_ENTRY(controls.inset_length),
                       controls.args->inset_length);
    gwy_widget_set_activate_on_unfocus(controls.inset_length, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.inset_length);
    g_signal_connect(controls.inset_length, "activate",
                     G_CALLBACK(inset_length_changed), &controls);
    gtk_table_attach(table, controls.inset_length,
                     1, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.inset_length_auto = gtk_button_new_with_mnemonic(_("_Auto"));
    g_signal_connect_swapped(controls.inset_length_auto, "clicked",
                             G_CALLBACK(inset_length_set_auto), &controls);
    gtk_table_attach(table, controls.inset_length_auto,
                     3, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.inset_draw_ticks
        = gtk_check_button_new_with_mnemonic(_("Draw _ticks"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.inset_draw_ticks),
                                 args->inset_draw_ticks);
    gtk_table_attach(table, controls.inset_draw_ticks,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.inset_draw_ticks, "toggled",
                     G_CALLBACK(inset_draw_ticks_toggled), &controls);
    row++;

    controls.inset_draw_label
        = gtk_check_button_new_with_mnemonic(_("Draw _label"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.inset_draw_label),
                                 args->inset_draw_label);
    gtk_table_attach(table, controls.inset_draw_label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.inset_draw_label, "toggled",
                     G_CALLBACK(inset_draw_label_toggled), &controls);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    gtk_table_attach(table, gwy_label_new_header(_("Value Scale")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.ztypes
        = gwy_radio_buttons_createl(G_CALLBACK(save_ztype_changed), &controls,
                                    args->ztype,
                                    gwy_sgettext("ruler|_None"), PIXMAP_NONE,
                                    _("_False color ruler"), PIXMAP_FMSCALE,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.ztypes, table, 3, row);

    controls.fmscale_gap = attach_gap(table, row, args->fmscale_gap);
    g_signal_connect(controls.fmscale_gap, "value-changed",
                     G_CALLBACK(save_fmscale_gap_changed), &controls);
    row++;

    save_update_sensitivity(&controls);

    /********************************************************************
     * Preview
     ********************************************************************/

    align = gtk_alignment_new(0.5, 0.5, 1.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    controls.image = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(align), controls.image);

    /* XXX: We use this for *preview* zoom during the dialog, the real zoom
     * is only in the adjustment and it's loaded on close. */
    args->zoom = PREVIEW_SIZE/(gdouble)MAX(args->xres, args->yres);
    gtk_widget_activate(controls.inset_length);
    save_update_preview(&controls);
    update_selected_font(&controls);
    controls.in_update = FALSE;

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            args->zoom
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.zoom));
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            controls.in_update = TRUE;
            args->zoom = pixmap_save_defaults.zoom;
            args->xytype = pixmap_save_defaults.xytype;
            args->ztype = pixmap_save_defaults.ztype;
            args->inset_pos = pixmap_save_defaults.inset_pos;
            args->fmscale_gap = pixmap_save_defaults.fmscale_gap;
            args->inset_gap = pixmap_save_defaults.inset_gap;
            inset_length_set_auto(&controls);
            args->draw_mask = pixmap_save_defaults.draw_mask;
            args->draw_selection = pixmap_save_defaults.draw_selection;
            args->text_antialias = pixmap_save_defaults.text_antialias;
            args->inset_draw_ticks = pixmap_save_defaults.inset_draw_ticks;
            args->inset_draw_label = pixmap_save_defaults.inset_draw_label;
            gtk_adjustment_set_value(GTK_ADJUSTMENT(controls.zoom), args->zoom);
            args->zoom = PREVIEW_SIZE/(gdouble)MAX(args->xres, args->yres);
            gwy_radio_buttons_set_current(controls.xytypes, args->xytype);
            gwy_radio_buttons_set_current(controls.ztypes, args->ztype);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.draw_mask),
                                         args->draw_mask);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.draw_selection),
                                         args->draw_selection);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.text_antialias),
                                         args->text_antialias);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.inset_draw_ticks),
                                         args->inset_draw_ticks);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.inset_draw_label),
                                         args->inset_draw_label);
            gtk_adjustment_set_value(GTK_ADJUSTMENT(controls.fmscale_gap),
                                     args->fmscale_gap);
            gtk_adjustment_set_value(GTK_ADJUSTMENT(controls.inset_gap),
                                     args->inset_gap);
            controls.in_update = FALSE;
            save_update_sensitivity(&controls);
            save_update_preview(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->zoom = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.zoom));
    gtk_widget_destroy(dialog);

    return TRUE;
}

/***************************************************************************
 *
 *  renderers
 *
 ***************************************************************************/

static void
format_layout(PangoLayout *layout,
              PangoRectangle *logical,
              GString *string,
              const gchar *format,
              ...)
{
    gchar *buffer;
    gint length;
    va_list args;

    g_string_truncate(string, 0);
    va_start(args, format);
    length = g_vasprintf(&buffer, format, args);
    va_end(args);
    g_string_append_len(string, buffer, length);
    g_free(buffer);

    /* Replace ASCII with proper minus */
    if (string->str[0] == '-') {
        g_string_erase(string, 0, 1);
        g_string_prepend_unichar(string, 0x2212);
    }

    pango_layout_set_markup(layout, string->str, string->len);
    pango_layout_get_extents(layout, NULL, logical);
}

static GdkPixbuf*
hruler(GtkWidget *widget,
       gint size,
       gint extra,
       gdouble real,
       gdouble zoom,
       gdouble offset,
       const gchar *font,
       GwySIUnit *siunit,
       GwySIValueFormat **pformat)
{
    PangoRectangle logical1, logical2;
    PangoLayout *layout;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GdkGC *gc;
    gdouble base, step, x, from, to;
    GString *s;
    GwySIValueFormat *format;
    gint l, n, ix;
    gint tick, height, lw;
    gboolean units_placed;

    s = g_string_new(NULL);
    layout = prepare_layout(zoom, font);

    format = gwy_si_unit_get_format_with_resolution(siunit,
                                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                    real, real/12,
                                                    NULL);
    *pformat = format;
    offset /= format->magnitude;
    real /= format->magnitude;
    format_layout(layout, &logical1, s, "%.*f",
                  format->precision, -real);
    format_layout(layout, &logical2, s, "%.*f %s",
                  format->precision, offset, format->units);

    l = MAX(PANGO_PIXELS(logical1.width), PANGO_PIXELS(logical2.width));
    n = CLAMP(size/l, 1, 10);
    step = real/n;
    base = pow10(floor(log10(step)));
    step = step/base;
    if (step <= 2.0)
        step = 2.0;
    else if (step <= 5.0)
        step = 5.0;
    else {
        base *= 10;
        step = 1.0;
        format->precision = MAX(format->precision - 1, 0);
    }

    tick = zoom*TICK_LENGTH;
    lw = ZOOM2LW(zoom);
    l = MAX(PANGO_PIXELS(logical1.height), PANGO_PIXELS(logical2.height));
    height = l + 2*zoom + tick + 2;
    drawable = prepare_drawable(widget, size + extra, height, lw, &gc);

    from = offset;
    from = ceil(from/(base*step) - 1e-15)*(base*step);
    to = real + offset;
    to = floor(to/(base*step) + 1e-15)*(base*step);

    units_placed = FALSE;
    for (x = from; x <= to; x += base*step) {
        if (fabs(x) < 1e-15*base*step)
            x = 0.0;
        format_layout(layout, &logical1, s, "%.*f%s%s",
                      format->precision, x,
                      units_placed ? "" : " ",
                      units_placed ? "" : format->units);
        units_placed = TRUE;
        ix = (x - offset)/real*size + lw/2;
        if (ix + PANGO_PIXELS(logical1.width) <= size + extra/4)
            gdk_draw_layout(drawable, gc,
                            ix+1, 1 + l - PANGO_PIXELS(logical1.height),
                            layout);
        gdk_draw_line(drawable, gc, ix, height-1, ix, height-1-tick);
    }

    pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable, NULL,
                                          0, 0, 0, 0, size + extra, height);

    g_object_unref(gc);
    g_object_unref(drawable);
    g_object_unref(layout);
    g_string_free(s, TRUE);

    return pixbuf;
}

/* Take a particular format and use that.  This is necessary to keep
 * consistency with the horizontal axis because the vertical one does not
 * bear any units itself. */
static GdkPixbuf*
vruler(GtkWidget *widget, gint size,
       gint extra,
       gdouble real,
       gdouble zoom,
       gdouble offset,
       const gchar *font,
       GwySIValueFormat *format)
{
    PangoRectangle logical1, logical2;
    PangoLayout *layout;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GdkGC *gc;
    gdouble base, step, x, from, to;
    GString *s;
    gint l, n, ix;
    gint tick, width, lw;

    s = g_string_new(NULL);
    layout = prepare_layout(zoom, font);

    /* note the algorithm is the same to force consistency between axes,
     * even though the vertical one could be filled with ticks more densely */
    offset /= format->magnitude;
    real /= format->magnitude;
    format_layout(layout, &logical1, s, "%.*f",
                  format->precision, -real);
    format_layout(layout, &logical2, s, "%.*f %s",
                  format->precision, offset, format->units);

    l = MAX(PANGO_PIXELS(logical1.width), PANGO_PIXELS(logical2.width));
    n = CLAMP(size/l, 1, 10);
    step = real/n;
    base = pow10(floor(log10(step)));
    step = step/base;
    if (step <= 2.0)
        step = 2.0;
    else if (step <= 5.0)
        step = 5.0;
    else {
        base *= 10;
        step = 1.0;
        format->precision = MAX(format->precision - 1, 0);
    }

    format_layout(layout, &logical1, s, "%.*f", format->precision, -real);
    l = PANGO_PIXELS(logical1.width);

    tick = zoom*TICK_LENGTH;
    lw = ZOOM2LW(zoom);
    width = l + 2*zoom + tick + 2;
    drawable = prepare_drawable(widget, width, size + extra, lw, &gc);

    from = offset;
    from = ceil(from/(base*step) - 1e-15)*(base*step);
    to = real + offset;
    to = floor(to/(base*step) + 1e-15)*(base*step);

    for (x = from; x <= to; x += base*step) {
        if (fabs(x) < 1e-15*base*step)
            x = 0.0;
        format_layout(layout, &logical1, s, "%.*f", format->precision, x);
        ix = (x - offset)/real*size + lw/2;
        if (ix + PANGO_PIXELS(logical1.height) <= size + extra/4)
            gdk_draw_layout(drawable, gc,
                            l - PANGO_PIXELS(logical1.width) + 1, ix+1, layout);
        gdk_draw_line(drawable, gc, width-1, ix, width-1-tick, ix);
    }

    pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable, NULL,
                                          0, 0, 0, 0, width, size + extra);

    g_object_unref(gc);
    g_object_unref(drawable);
    g_object_unref(layout);
    g_string_free(s, TRUE);

    return pixbuf;
}

/* auxilliary function to compute decimal points from tickdist */
static gint
gwy_pixmap_step_to_prec(gdouble d)
{
    gdouble resd = log10(7.5)-log10(d);
    if (resd != resd)
        return 1;
    if (resd > 1e20)
        return 1;
    if (resd < 1.0)
        resd = 1.0;
    return (gint) floor(resd);
}

static void
unantialias(GdkPixbuf *pixbuf)
{
    guint width = gdk_pixbuf_get_width(pixbuf);
    guint height = gdk_pixbuf_get_height(pixbuf);
    guint rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guint bpp = gdk_pixbuf_get_has_alpha(pixbuf) ? 4 : 3;
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    guint i, j;

    for (i = 0; i < height; i++) {
        guchar *row = pixels + i*rowstride;
        /* Threshold all channels independently.  This is fine for grey levels.
         * We should not be run on a full-colour pixbuf. */
        for (j = 0; j < width*bpp; j++)
            row[j] = (row[j] < 143) ? 0x00 : 0xff;
    }
}

static GdkPixbuf*
fmscale(GtkWidget *widget, gint size,
        gdouble bot,
        gdouble top,
        gdouble zoom,
        const gchar *font,
        GwySIUnit *siunit,
        gboolean inner_ticks)
{
    PangoRectangle logical1, logical2;
    PangoLayout *layout;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GdkGC *gc;
    gdouble x_max;
    gdouble scale, x, m, tickdist, max;
    GwySIValueFormat *format;
    GString *s;
    gint l, tick, width, lw;
    gint units_width, label_height, mintickdist, prec = 1, pos, bool_draw = 1;

    s = g_string_new(NULL);
    layout = prepare_layout(zoom, font);

    x_max = MAX(fabs(bot), fabs(top));
    format = gwy_si_unit_get_format(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                    x_max, NULL);
    format_layout(layout, &logical1, s, " %s", format->units);
    units_width = PANGO_PIXELS(logical1.width);
    label_height = PANGO_PIXELS(logical1.height);
    mintickdist = label_height*1.5; /* mintickdist is in pixels; tickdist is in
                                     * meters or whichever basic unit */
    /* prec computation starts here */
    /* Don't attempt to draw anything if rounding errors are too large or
     * scale calculation can overflow */
    x = top - bot;
    max = MAX(fabs(bot), fabs(top));
    if (x < 1e-15*max || x <= 1e4*G_MINDOUBLE || max >= 1e-4*G_MAXDOUBLE)
        bool_draw = 0;
    scale = size/(top - bot);
    x = mintickdist/scale;
    m = pow10(floor(log10(x)));
    x /= m;
    if (x == 1.0)
        x = 1.0;
    else if (x <= 2.0)
        x = 2.0;
    else if (x <= 5.0)
        x = 5.0;
    else
        x = 10.0;
    tickdist = x*m;
    x = floor(bot/tickdist)*tickdist;
    max = ceil(top/tickdist)*tickdist;
    prec = gwy_pixmap_step_to_prec(tickdist/format->magnitude);
    /* prec computation ends here */

    format_layout(layout, &logical1, s, "%.*f %s",
                  prec, top/format->magnitude, format->units);
    format_layout(layout, &logical2, s, "%.*f %s",
                  prec, bot/format->magnitude, format->units);

    l = MAX(PANGO_PIXELS(logical1.width), PANGO_PIXELS(logical2.width));
    tick = zoom*TICK_LENGTH; /* physical tick length */
    lw = ZOOM2LW(zoom); /* line width */
    width = l + 2*zoom + tick + 2;
    drawable = prepare_drawable(widget, width, size, lw, &gc);

    /* bottom text and line */
    format_layout(layout, &logical1, s, "%.*f",
                  prec, bot/format->magnitude);
    gdk_draw_layout(drawable, gc,
                    width - PANGO_PIXELS(logical1.width) - 2 - units_width,
                    size - 1 - PANGO_PIXELS(logical1.height),
                    layout);
    gdk_draw_line(drawable, gc, 0, size - (lw + 1)/2, tick, size - (lw + 1)/2);
    /* end of bottom text and line */

    /* top text start */
    format_layout(layout, &logical1, s, "%.*f %s",
                  prec, top/format->magnitude, format->units);
    gdk_draw_layout(drawable, gc,
                    width - PANGO_PIXELS(logical1.width) - 2, 1,
                    layout); /* top text end */
    gdk_draw_line(drawable, gc, 0, lw/2, tick, lw/2); /* top line */

    /* the rest of the ticks is drawn here (other than bot, top) */
    if (inner_ticks) {
        while (bool_draw && x <= max) {
            pos = size-1 - GWY_ROUND((x - bot)*scale);
            if (pos > label_height && pos < size-1-label_height) {
                gdk_draw_line(drawable, gc, 0, pos, tick/2, pos);
                format_layout(layout, &logical1, s, "%.*f",
                              prec, x/format->magnitude);
                if (pos - PANGO_PIXELS(logical1.height) > label_height) {
                    gdk_draw_layout(drawable, gc,
                                    width - PANGO_PIXELS(logical1.width)
                                    - 2 - units_width,
                                    pos - PANGO_PIXELS(logical1.height),
                                    layout);
                }
            }
            x += tickdist;
        }
    }

    /* all drawing is finished here */
    pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable, NULL,
                                          0, 0, 0, 0, width, size);

    gwy_si_unit_value_format_free(format);
    g_object_unref(gc);
    g_object_unref(drawable);
    g_object_unref(layout);
    g_string_free(s, TRUE);

    return pixbuf;
}

static GdkPixbuf*
scalebar(GtkWidget *widget,
         gint size,
         const gchar *length,
         gdouble real,
         gdouble zoom,
         const GwyRGBA *color,
         const gchar *font,
         gboolean draw_ticks,
         gboolean draw_label)
{
    guchar color_bytes[3];
    PangoRectangle logical;
    PangoLayout *layout = NULL;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GwySIUnit *siunit;
    GdkGC *gc;
    GString *s;
    gchar *end;
    gint i, tick, height, width, lw, power10;
    guint j, rowstride;
    guchar *pixels;
    gdouble p;

    if (draw_label)
        layout = prepare_layout(zoom, font);

    s = g_string_new(NULL);
    p = g_strtod(length, &end);
    siunit = gwy_si_unit_new_parse(end, &power10);
    p *= pow10(power10)/real;
    g_object_unref(siunit);

    if (draw_label)
        format_layout(layout, &logical, s, "%s", length);
    else
        gwy_clear(&logical, 1);

    lw = ZOOM2LW(zoom);
    tick = draw_ticks ? zoom*TICK_LENGTH : lw;
    width = GWY_ROUND(size*p);
    height = PANGO_PIXELS(logical.height) + tick;

    if (((height < 2 || width < 2) && (draw_label))
        || height > 16384 || width > 16384) {
        g_warning("Will not create scalebar of size %dx%d, we got the "
                  "scaling terribly wrong somewhere.", width, height);
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, BITS_PER_SAMPLE,
                                1, 1);
        gdk_pixbuf_fill(pixbuf, 0x00000000);
        return pixbuf;
    }

    drawable = prepare_drawable(widget, width, height, lw, &gc);

    if (draw_ticks) {
        gdk_draw_line(drawable, gc, lw/2, 0, lw/2, tick);
        gdk_draw_line(drawable, gc, (gint)(width - lw/2.0),
                      0, (gint)(width  - lw/2.0), tick);
    }
    gdk_draw_line(drawable, gc, 0, tick/2, width, tick/2);

    if (draw_label)
        gdk_draw_layout(drawable, gc,
                        (width - PANGO_PIXELS(logical.width))/2, tick,
                        layout);

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, BITS_PER_SAMPLE,
                            width, height);
    gdk_pixbuf_get_from_drawable(pixbuf, drawable, NULL,
                                 0, 0, 0, 0, width, height);

    g_object_unref(gc);
    g_object_unref(drawable);
    gwy_object_unref(layout);
    g_string_free(s, TRUE);

    color_bytes[0] = floor(255.999999*color->r);
    color_bytes[1] = floor(255.999999*color->g);
    color_bytes[2] = floor(255.999999*color->b);

    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    for (i = 0; i < height; i++) {
        guchar *row = pixels + rowstride*i;

        for (j = 0; j < (guint)width; j++) {
            row[4*j + 3] = 255 - row[4*j];
            row[4*j] = color_bytes[0];
            row[4*j + 1] = color_bytes[1];
            row[4*j + 2] = color_bytes[2];
        }
    }

    return pixbuf;
}

static GdkDrawable*
prepare_drawable(GtkWidget *widget,
                 gint width,
                 gint height,
                 gint lw,
                 GdkGC **gc)
{
    GdkWindow *window;
    GdkDrawable *drawable;
    GdkColormap *cmap;
    GdkColor fg;

    /* FIXME: this creates a drawable with *SCREEN* bit depth
     * We should render a pixmap with Pango FT2 and use that */
    window = widget->window;
    drawable = GDK_DRAWABLE(gdk_pixmap_new(GDK_DRAWABLE(window),
                                           width, height, -1));
    cmap = gdk_drawable_get_colormap(drawable);
    *gc = gdk_gc_new(drawable);

    fg.red = 0xffff;
    fg.green = 0xffff;
    fg.blue = 0xffff;
    gdk_colormap_alloc_color(cmap, &fg, FALSE, TRUE);
    gdk_gc_set_foreground(*gc, &fg);
    gdk_draw_rectangle(drawable, *gc, TRUE, 0, 0, width, height);

    fg.red = 0x0000;
    fg.green = 0x0000;
    fg.blue = 0x0000;
    gdk_colormap_alloc_color(cmap, &fg, FALSE, TRUE);
    gdk_gc_set_foreground(*gc, &fg);
    gdk_gc_set_line_attributes(*gc, lw, GDK_LINE_SOLID,
                               GDK_CAP_PROJECTING, GDK_JOIN_BEVEL);

    return drawable;
}

static PangoLayout*
prepare_layout(gdouble zoom, const gchar *font)
{
    PangoContext *context;
    PangoFontDescription *fontdesc;
    PangoLayout *layout;
    gchar *full_font;

    context = gdk_pango_context_get();
    full_font = g_strconcat(font, " 12", NULL);
    fontdesc = pango_font_description_from_string(full_font);
    g_free(full_font);
    pango_font_description_set_size(fontdesc, 12*PANGO_SCALE*zoom);
    pango_context_set_font_description(context, fontdesc);
    layout = pango_layout_new(context);
    g_object_unref(context);
    pango_font_description_free(fontdesc);

    return layout;
}

static PangoLayout*
create_layout(const gchar *fontname, gdouble fontsize, cairo_t *cr)
{
    PangoContext *context;
    PangoFontDescription *fontdesc;
    PangoLayout *layout;
    gchar *full_font;

    /* This creates a layout with private context so we can modify the
     * context at will. */
    layout = pango_cairo_create_layout(cr);

    full_font = g_strdup_printf("%s %.2f", fontname, fontsize);
    fontdesc = pango_font_description_from_string(full_font);
    g_free(full_font);
    pango_font_description_set_size(fontdesc, PANGO_SCALE*fontsize);
    context = pango_layout_get_context(layout);
    pango_context_set_font_description(context, fontdesc);
    pango_font_description_free(fontdesc);
    pango_layout_context_changed(layout);
    /* XXX: Must call pango_cairo_update_layout() if we change the
     * transformation afterwards. */

    return layout;
}

static cairo_surface_t*
create_surface(const PixmapSaveArgs *args,
               const gchar *name,
               const gchar *filename,
               gdouble width, gdouble height)
{
    cairo_surface_t *surface = NULL;
    cairo_format_t imageformat = CAIRO_FORMAT_RGB24;
    gint iwidth, iheight;

    if (width <= 0.0)
        width = 100.0;
    if (height <= 0.0)
        height = 100.0;

    iwidth = (gint)ceil(width);
    iheight = (gint)ceil(height);

    if (gwy_stramong(name, "png", "jpeg2000", NULL)) {
        if (args->transparent)
            imageformat = CAIRO_FORMAT_ARGB32;
        surface = cairo_image_surface_create(imageformat, iwidth, iheight);
    }
    else if (gwy_stramong(name, "jpeg", "tiff", "pnm", "bmp", "tga", NULL))
        surface = cairo_image_surface_create(imageformat, iwidth, iheight);
#ifdef CAIRO_HAS_PDF_SURFACE
    else if (gwy_strequal(name, "pdf"))
        surface = cairo_pdf_surface_create(filename, width, height);
#endif
#ifdef CAIRO_HAS_PS_SURFACE
    else if (gwy_strequal(name, "eps")) {
        surface = cairo_ps_surface_create(filename, width, height);
        /* Requires cairo 1.6. */
        cairo_ps_surface_set_eps(surface, TRUE);
    }
#endif
#ifdef CAIRO_HAS_SVG_SURFACE
    else if (gwy_strequal(name, "svg")) {
        surface = cairo_svg_surface_create(filename, width, height);
    }
#endif
    else {
        g_assert_not_reached();
    }

    return surface;
}

typedef struct {
    gdouble x, y;
    gdouble w, h;
} PixmapSaveRect;

typedef struct {
    gdouble from, to, step, base;
} RulerTicks;

typedef struct {
    /* Only the width and height are meaningful here */
    PixmapSaveRect inset_label;
    PixmapSaveRect fmscale_label;
    PixmapSaveRect fmscale_unit_label;
    PixmapSaveRect title_label;

    GwySIValueFormat *vf_ruler;
    RulerTicks hruler_ticks;
    gdouble hruler_label_height;

    /* Actual rectangles, including positions, of the image parts. */
    PixmapSaveRect image;
    PixmapSaveRect hruler;
    PixmapSaveRect vruler;
    PixmapSaveRect inset_ruler;
    PixmapSaveRect fmscale;
    PixmapSaveRect title;

    /* Union of all above (plus maybe borders). */
    PixmapSaveRect canvas;
} PixmapSaveSizes;

static GwySIValueFormat*
find_hruler_ticks(GwySIUnit *unit, gdouble size, gdouble real, gdouble offset,
                  RulerTicks *ticks, gdouble *height,
                  PangoLayout *layout, GString *s)
{
    PangoRectangle logical1, logical2;
    GwySIValueFormat *vf;
    gdouble len, bs;
    guint n;

    vf = gwy_si_unit_get_format_with_resolution(unit,
                                                GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                real, real/12,
                                                NULL);
    gwy_debug("unit '%s'", vf->units);
    offset /= vf->magnitude;
    real /= vf->magnitude;
    format_layout(layout, &logical1, s, "%.*f",
                  vf->precision, -real);
    gwy_debug("right '%s'", s->str);
    format_layout(layout, &logical2, s, "%.*f %s",
                  vf->precision, offset, vf->units);
    gwy_debug("first '%s'", s->str);

    *height = MAX(logical1.height/pangoscale, logical2.height/pangoscale);
    len = MAX(logical1.width/pangoscale, logical2.width/pangoscale);
    gwy_debug("label len %g, height %g", len, *height);
    n = CLAMP(GWY_ROUND(size/len), 1, 10);
    gwy_debug("nticks %u", n);
    ticks->step = real/n;
    ticks->base = pow10(floor(log10(ticks->step)));
    ticks->step /= ticks->base;
    if (ticks->step <= 2.0)
        ticks->step = 2.0;
    else if (ticks->step <= 5.0)
        ticks->step = 5.0;
    else {
        ticks->base *= 10;
        ticks->step = 1.0;
        if (vf->precision)
            vf->precision--;
    }
    gwy_debug("base %g, step %g", ticks->base, ticks->step);

    bs = ticks->base * ticks->step;
    ticks->from = ceil(offset/bs - 1e-14)*bs;
    ticks->to = floor((real + offset)/bs + 1e-14)*bs;
    gwy_debug("from %g, to %g", ticks->from, ticks->to);

    return vf;
}

static PixmapSaveSizes*
calculate_sizes(const PixmapSaveArgs *args,
                const gchar *name)
{
    cairo_surface_t *surface = create_surface(args, name, NULL, 0.0, 0.0);
    cairo_t *cr = cairo_create(surface);
    PangoLayout *layout = create_layout(args->font, 12.0, cr);
    PixmapSaveSizes *sizes = g_new0(PixmapSaveSizes, 1);
    guint xres = gwy_data_field_get_xres(args->dfield);
    guint yres = gwy_data_field_get_yres(args->dfield);
    gdouble xreal = gwy_data_field_get_xreal(args->dfield);
    gdouble yreal = gwy_data_field_get_yreal(args->dfield);
    gdouble xoffset = gwy_data_field_get_xoffset(args->dfield);
    gdouble yoffset = gwy_data_field_get_yoffset(args->dfield);
    GwySIUnit *xyunit = gwy_data_field_get_si_unit_xy(args->dfield);
    GString *s = g_string_new(NULL);
    gdouble lw = args->lw;

    if (args->realsquare) {
        gdouble scale = MAX(xres/xreal, yres/yreal);
        /* This is how GwyDataView rounds it so we should get a pixmap of
         * this size. */
        sizes->image.w = GWY_ROUND(xreal*scale);
        sizes->image.h = GWY_ROUND(yreal*scale);
    }
    else {
        sizes->image.w = xres;
        sizes->image.h = yres;
    }
    sizes->image.w += 2.0*lw;
    sizes->image.h += 2.0*lw;

    sizes->vf_ruler = find_hruler_ticks(xyunit, xres, xreal, xoffset,
                                        &sizes->hruler_ticks,
                                        &sizes->hruler_label_height,
                                        layout, s);
    if (args->xytype == PIXMAP_RULERS) {
        sizes->hruler.h = sizes->hruler_label_height
                          + 1.1*TICK_LENGTH;
        sizes->hruler.w = sizes->image.w;
    }

    sizes->image.y += sizes->hruler.h;

    sizes->canvas.w = sizes->image.w;
    sizes->canvas.h = sizes->image.h + sizes->hruler.h;

    g_string_free(s, TRUE);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return sizes;
}

static void
destroy_sizes(PixmapSaveSizes *sizes)
{
    if (sizes->vf_ruler)
        gwy_si_unit_value_format_free(sizes->vf_ruler);
    g_free(sizes);
}

static void
pixmap_draw_data(const PixmapSaveArgs *args,
                 const PixmapSaveSizes *sizes,
                 cairo_t *cr)
{
    GwyPixmapLayer *layer;
    const PixmapSaveRect *rect = &sizes->image;
    GdkPixbuf *pixbuf;
    gdouble lw = args->lw;
    gint w, h;

    layer = gwy_data_view_get_base_layer(args->data_view);
    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
    pixbuf = gwy_data_view_export_pixbuf(args->data_view, 1.0,
                                         args->draw_mask, args->draw_selection);
    w = gdk_pixbuf_get_width(pixbuf);
    h = gdk_pixbuf_get_height(pixbuf);

    cairo_save(cr);
    cairo_rectangle(cr, rect->x + lw, rect->y + lw, w, h);
    cairo_clip(cr);
    gdk_cairo_set_source_pixbuf(cr, pixbuf, rect->x + lw, rect->y + lw);
    /* Pixelated zoom, this is what we want for data! */
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_restore(cr);
    g_object_unref(pixbuf);

    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, lw);
    cairo_rectangle(cr, rect->x + 0.5*lw, rect->y + 0.5*lw, w + lw, h + lw);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void
pixmap_draw_hruler(const PixmapSaveArgs *args,
                   const PixmapSaveSizes *sizes,
                   PangoLayout *layout,
                   cairo_t *cr)
{
    gdouble xreal = gwy_data_field_get_xreal(args->dfield);
    gdouble xoffset = gwy_data_field_get_xoffset(args->dfield);
    const PixmapSaveRect *rect = &sizes->hruler;
    const RulerTicks *ticks = &sizes->hruler_ticks;
    GwySIValueFormat *vf = sizes->vf_ruler;
    GString *s = g_string_new(NULL);
    gdouble lw = args->lw;
    gdouble x, bs, scale, ximg;
    gboolean units_placed = FALSE;

    scale = (rect->w - 2.0*lw)/(xreal/sizes->vf_ruler->magnitude);
    bs = ticks->step*ticks->base;

    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, lw);
    for (x = ticks->from; x <= ticks->to + 1e-14*bs; x += bs) {
        ximg = (x - xoffset)*scale + lw;
        gwy_debug("x %g -> %g", x, ximg);
        cairo_move_to(cr, ximg, rect->h);
        cairo_line_to(cr, ximg, rect->h - TICK_LENGTH);
    };
    cairo_stroke(cr);
    cairo_restore(cr);

    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    for (x = ticks->from; x <= ticks->to + 1e-14*bs; x += bs) {
        PangoRectangle logical;

        if (fabs(x) < 1e-14)
            x = 0.0;
        ximg = (x - xoffset)*scale + lw;
        if (!units_placed && (x >= 0.0 || ticks->to <= -1e-14)) {
            format_layout(layout, &logical, s, "%.*f %s",
                          vf->precision, x, vf->units);
            units_placed = TRUE;
        }
        else
            format_layout(layout, &logical, s, "%.*f", vf->precision, x);

        if (ximg + logical.width/pangoscale <= rect->w) {
            cairo_move_to(cr, ximg, rect->h - 1.1*TICK_LENGTH);
            cairo_rel_move_to(cr, 0.0, -logical.height/pangoscale);
            pango_cairo_show_layout(cr, layout);
        }
    };
    cairo_restore(cr);

    g_string_free(s, TRUE);
}

/* We assume cr is already created for the layout with the correct scale(!). */
static void
pixmap_draw_cairo(GwyContainer *data,
                  const PixmapSaveArgs *args,
                  const PixmapSaveSizes *sizes,
                  cairo_t *cr)
{
    PangoLayout *layout;

    layout = create_layout(args->font, 12.0, cr);

    pixmap_draw_data(args, sizes, cr);
    pixmap_draw_hruler(args, sizes, layout, cr);

    g_object_unref(layout);
}

/*
 * How to determine the size?
 * - Axes and scales dimensions depend on the text dimensions.
 * - We can measure text in Pango units (convertible with PANGO_PIXELS to
 *   device units: pixels or points) and only need a layout.
 * - For PangoCairo rendering the layout must be created with
 *   pango_cairo_create_layout(cr), i.e. we need a Cairo context.
 * - Cairo context can be only created for a Cairo surface.
 * - This is a cyclic dependence.
 *
 * So we create text-measuring Cairo surface of more or less random dimensions.
 * It is of the same type but not used for any rendering.  We measure labels
 * using this surface and calculate positions and sizes of all components of
 * the exported image.  Then we actually render them.
 *
 * To avoid doing everything twice, the measurement phase should provide
 * directly interpretable information about positions and sizes.
 **/
static gboolean
pixmap_save_cairo(GwyContainer *data,
                  const gchar *filename,
                  GwyRunType mode,
                  GError **error,
                  const gchar *name)
{
    GwyContainer *settings;
    PixmapSaveArgs args;
    PixmapSaveSizes *sizes;
    cairo_surface_t *surface;
    cairo_t *cr;

    settings = gwy_app_settings_get();
    pixmap_save_load_args(settings, &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_VIEW, &args.data_view,
                                     GWY_APP_DATA_FIELD, &args.dfield,
                                     0);
    sizes = calculate_sizes(&args, name);

    /* FIXME: This is good for pixmap images; we need a different size
     * determination method for vector graphics. */
    surface = create_surface(&args, name, filename,
                             args.zoom*sizes->canvas.w,
                             args.zoom*sizes->canvas.h);
    cr = cairo_create(surface);
    cairo_scale(cr, args.zoom, args.zoom);

    pixmap_draw_cairo(data, &args, sizes, cr);

    cairo_surface_flush(surface);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    destroy_sizes(sizes);

    return TRUE;
}

/***************************************************************************
 *
 *  sub
 *
 ***************************************************************************/

static PixmapFormatInfo*
find_format(const gchar *name)
{
    GSList *l;

    for (l = pixmap_formats; l; l = g_slist_next(l)) {
        PixmapFormatInfo *format_info = (PixmapFormatInfo*)l->data;
        if (gwy_strequal(format_info->name, name))
            return format_info;
    }

    return NULL;
}

static const gchar draw_mask_key[]        = "/module/pixmap/draw_mask";
static const gchar draw_selection_key[]   = "/module/pixmap/draw_selection";
static const gchar text_antialias_key[]   = "/module/pixmap/text_antialias";
static const gchar font_key[]             = "/module/pixmap/font";
static const gchar font_size_key[]        = "/module/pixmap/font_size";
static const gchar grayscale_key[]        = "/module/pixmap/grayscale";
static const gchar inset_color_key[]      = "/module/pixmap/inset_color";
static const gchar inset_pos_key[]        = "/module/pixmap/inset_pos";
static const gchar inset_draw_ticks_key[] = "/module/pixmap/inset_draw_ticks";
static const gchar inset_draw_label_key[] = "/module/pixmap/inset_draw_label";
static const gchar inset_length_key[]     = "/module/pixmap/inset_length";
static const gchar scale_font_key[]       = "/module/pixmap/scale_font";
static const gchar fmscale_gap_key[]      = "/module/pixmap/fmscale_gap";
static const gchar inset_gap_key[]        = "/module/pixmap/inset_gap";
static const gchar xytype_key[]           = "/module/pixmap/xytype";
static const gchar zoom_key[]             = "/module/pixmap/zoom";
static const gchar ztype_key[]            = "/module/pixmap/ztype";

static void
pixmap_save_sanitize_args(PixmapSaveArgs *args)
{
    args->xytype = MIN(args->xytype, PIXMAP_SCALEBAR);
    args->ztype = MIN(args->ztype, PIXMAP_FMSCALE);
    args->inset_color.a = 1.0;
    args->inset_pos = MIN(args->inset_pos, INSET_NPOS - 1);
    /* handle inset_length later, its usability depends on the data field. */
    args->zoom = CLAMP(args->zoom, 0.06, 16.0);
    args->draw_mask = !!args->draw_mask;
    args->draw_selection = !!args->draw_selection;
    args->text_antialias = !!args->text_antialias;
    args->scale_font = !!args->scale_font;
    args->inset_draw_ticks = !!args->inset_draw_ticks;
    args->inset_draw_label = !!args->inset_draw_label;
    args->font_size = CLAMP(args->font_size, 1.2, 120.0);
    args->fmscale_gap = CLAMP(args->fmscale_gap, 0.0, 2.0);
    args->inset_gap = CLAMP(args->inset_gap, 0.0, 2.0);
    args->grayscale = (args->grayscale == 16) ? 16 : 0;
}

static void
pixmap_save_free_args(PixmapSaveArgs *args)
{
    g_free(args->font);
    g_free(args->inset_length);
}

static void
pixmap_save_load_args(GwyContainer *container,
                      PixmapSaveArgs *args)
{
    *args = pixmap_save_defaults;

    gwy_container_gis_double_by_name(container, zoom_key, &args->zoom);
    gwy_container_gis_enum_by_name(container, xytype_key, &args->xytype);
    gwy_container_gis_enum_by_name(container, ztype_key, &args->ztype);
    gwy_rgba_get_from_container(&args->inset_color, container, inset_color_key);
    gwy_container_gis_enum_by_name(container, inset_pos_key, &args->inset_pos);
    gwy_container_gis_string_by_name(container, inset_length_key,
                                     (const guchar**)&args->inset_length);
    gwy_container_gis_boolean_by_name(container, draw_mask_key,
                                      &args->draw_mask);
    gwy_container_gis_boolean_by_name(container, draw_selection_key,
                                      &args->draw_selection);
    gwy_container_gis_boolean_by_name(container, text_antialias_key,
                                      &args->text_antialias);
    gwy_container_gis_string_by_name(container, font_key,
                                     (const guchar**)&args->font);
    gwy_container_gis_boolean_by_name(container, scale_font_key,
                                      &args->scale_font);
    gwy_container_gis_double_by_name(container, font_size_key,
                                     &args->font_size);
    gwy_container_gis_double_by_name(container, fmscale_gap_key,
                                     &args->fmscale_gap);
    gwy_container_gis_double_by_name(container, inset_gap_key,
                                     &args->inset_gap);
    gwy_container_gis_int32_by_name(container, grayscale_key, &args->grayscale);
    gwy_container_gis_boolean_by_name(container, inset_draw_ticks_key,
                                      &args->inset_draw_ticks);
    gwy_container_gis_boolean_by_name(container, inset_draw_label_key,
                                      &args->inset_draw_label);

    args->font = g_strdup(args->font);
    args->inset_length = g_strdup(args->inset_length);

    pixmap_save_sanitize_args(args);
}

static void
pixmap_save_save_args(GwyContainer *container,
                      PixmapSaveArgs *args)
{
    gwy_container_set_double_by_name(container, zoom_key, args->zoom);
    gwy_container_set_enum_by_name(container, xytype_key, args->xytype);
    gwy_container_set_enum_by_name(container, ztype_key, args->ztype);
    gwy_rgba_store_to_container(&args->inset_color, container, inset_color_key);
    gwy_container_set_enum_by_name(container, inset_pos_key, args->inset_pos);
    gwy_container_set_string_by_name(container, inset_length_key,
                                     g_strdup(args->inset_length));
    gwy_container_set_boolean_by_name(container, draw_mask_key,
                                      args->draw_mask);
    gwy_container_set_boolean_by_name(container, draw_selection_key,
                                      args->draw_selection);
    gwy_container_set_boolean_by_name(container, text_antialias_key,
                                      args->text_antialias);
    gwy_container_set_string_by_name(container, font_key,
                                     g_strdup(args->font));
    gwy_container_set_boolean_by_name(container, scale_font_key,
                                      args->scale_font);
    gwy_container_set_double_by_name(container, font_size_key,
                                     args->font_size);
    gwy_container_set_double_by_name(container, fmscale_gap_key,
                                     args->fmscale_gap);
    gwy_container_set_double_by_name(container, inset_gap_key,
                                     args->inset_gap);
    gwy_container_set_int32_by_name(container, grayscale_key, args->grayscale);
    gwy_container_set_boolean_by_name(container, inset_draw_ticks_key,
                                      args->inset_draw_ticks);
    gwy_container_set_boolean_by_name(container, inset_draw_label_key,
                                      args->inset_draw_label);
}

static const gchar xreal_key[]       = "/module/pixmap/xreal";
static const gchar yreal_key[]       = "/module/pixmap/yreal";
static const gchar xyexponent_key[]  = "/module/pixmap/xyexponent";
static const gchar xymeasureeq_key[] = "/module/pixmap/xymeasureeq";
static const gchar xyunit_key[]      = "/module/pixmap/xyunit";
static const gchar zreal_key[]       = "/module/pixmap/zreal";
static const gchar zexponent_key[]   = "/module/pixmap/zexponent";
static const gchar zunit_key[]       = "/module/pixmap/zunit";
static const gchar maptype_key[]     = "/module/pixmap/maptype";

static void
pixmap_load_sanitize_args(PixmapLoadArgs *args)
{
    args->maptype = MIN(args->maptype, PIXMAP_MAP_LAST-1);
    args->xreal = CLAMP(args->xreal, 0.01, 10000.0);
    args->yreal = CLAMP(args->yreal, 0.01, 10000.0);
    args->zreal = CLAMP(args->zreal, 0.01, 10000.0);
    args->xyexponent = CLAMP(args->xyexponent, -12, 3);
    args->zexponent = CLAMP(args->zexponent, -12, 3);
    args->xymeasureeq = !!args->xymeasureeq;
}

static void
pixmap_load_load_args(GwyContainer *container,
                      PixmapLoadArgs *args)
{
    *args = pixmap_load_defaults;

    gwy_container_gis_double_by_name(container, xreal_key, &args->xreal);
    gwy_container_gis_double_by_name(container, yreal_key, &args->yreal);
    gwy_container_gis_int32_by_name(container, xyexponent_key,
                                    &args->xyexponent);
    gwy_container_gis_double_by_name(container, zreal_key, &args->zreal);
    gwy_container_gis_int32_by_name(container, zexponent_key,
                                    &args->zexponent);
    gwy_container_gis_enum_by_name(container, maptype_key, &args->maptype);
    gwy_container_gis_boolean_by_name(container, xymeasureeq_key,
                                      &args->xymeasureeq);
    gwy_container_gis_string_by_name(container, xyunit_key,
                                     (const guchar**)&args->xyunit);
    gwy_container_gis_string_by_name(container, zunit_key,
                                     (const guchar**)&args->zunit);

    args->xyunit = g_strdup(args->xyunit);
    args->zunit = g_strdup(args->zunit);

    pixmap_load_sanitize_args(args);
}

static void
pixmap_load_save_args(GwyContainer *container,
                      PixmapLoadArgs *args)
{
    gwy_container_set_double_by_name(container, xreal_key, args->xreal);
    gwy_container_set_double_by_name(container, yreal_key, args->yreal);
    gwy_container_set_int32_by_name(container, xyexponent_key,
                                    args->xyexponent);
    gwy_container_set_double_by_name(container, zreal_key, args->zreal);
    gwy_container_set_int32_by_name(container, zexponent_key,
                                    args->zexponent);
    gwy_container_set_enum_by_name(container, maptype_key, args->maptype);
    gwy_container_set_boolean_by_name(container, xymeasureeq_key,
                                      args->xymeasureeq);
    gwy_container_set_string_by_name(container, xyunit_key,
                                     g_strdup(args->xyunit));
    gwy_container_set_string_by_name(container, zunit_key,
                                     g_strdup(args->zunit));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
