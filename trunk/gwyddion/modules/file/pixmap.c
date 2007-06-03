/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2007 David Necas (Yeti), Petr Klapetek.
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
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <glib/gstdio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef HAVE_TIFF
#include <tiffio.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#include "err.h"

#define GWY_PNG_EXTENSIONS   ".png"
#define GWY_JPEG_EXTENSIONS  ".jpeg,.jpg,.jpe"
#define GWY_TIFF_EXTENSIONS  ".tiff,.tif"
#define GWY_PPM_EXTENSIONS   ".ppm,.pnm"
#define GWY_BMP_EXTENSIONS   ".bmp"
#define GWY_TARGA_EXTENSIONS ".tga,.targa"

#define ZOOM2LW(x) ((x) > 1 ? ((x) + 0.4) : 1)

enum {
    BITS_PER_SAMPLE = 8,
    TICK_LENGTH     = 10,
    PREVIEW_SIZE    = 240,
    FONT_SIZE       = 12
};

/* What is present on the exported image */
typedef enum {
    PIXMAP_RAW_DATA,
    PIXMAP_RULERS,
    PIXMAP_EVERYTHING,
    PIXMAP_LAST
} PixmapOutput;

/* What value is used when importing from image */
typedef enum {
    PIXMAP_MAP_NONE = 0,
    PIXMAP_MAP_RED,
    PIXMAP_MAP_GREEN,
    PIXMAP_MAP_BLUE,
    PIXMAP_MAP_VALUE,
    PIXMAP_MAP_SUM,
    PIXMAP_MAP_ALPHA,
    PIXMAP_MAP_LAST
} PixmapMapType;

typedef struct {
    gdouble zoom;
    PixmapOutput otype;
    gboolean draw_mask;
    gboolean draw_selection;
    gboolean scale_font;
    gdouble font_size;
    /* Interface only */
    GwyDataView *data_view;
    GwyDataField *dfield;
    /* These two are `1:1' sizes, i.e. they differ from data field sizes when
     * realsquare is TRUE. */
    gint xres;
    gint yres;
    gboolean realsquare;
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
    GSList *otypes;
    GtkObject *zoom;
    GtkObject *width;
    GtkObject *height;
    GtkWidget *font_size;
    GtkWidget *image;
    GtkWidget *draw_mask;
    GtkWidget *draw_selection;
    GtkWidget *scale_font;
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

static gboolean          module_register           (void);
static gint              pixmap_detect       (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name,
                                              const gchar *name);
static GwyContainer*     pixmap_load               (const gchar *filename,
                                                    GwyRunType mode,
                                                    GError **error,
                                                    const gchar *name);
static void             pixmap_load_pixbuf_to_data_field(GdkPixbuf *pixbuf,
                                                         GwyDataField *dfield,
                                                         PixmapMapType maptype);
static gboolean          pixmap_load_dialog        (PixmapLoadArgs *args,
                                                    const gchar *name,
                                                    gint xres,
                                                    gint yres,
                                                    const gboolean mapknown);
static void              pixmap_load_create_preview(PixmapLoadArgs *args,
                                                    PixmapLoadControls *controls);
static void              pixmap_load_map_type_update(GtkWidget *combo,
                                                     PixmapLoadControls *controls);
static void              xyreal_changed_cb         (GtkAdjustment *adj,
                                                    PixmapLoadControls *controls);
static void              xymeasureeq_changed_cb    (PixmapLoadControls *controls);
static void              set_combo_from_unit       (GtkWidget *combo,
                                                    const gchar *str);
static void              units_change_cb           (GtkWidget *button,
                                                    PixmapLoadControls *controls);
static void              pixmap_load_update_controls(PixmapLoadControls *controls,
                                                    PixmapLoadArgs *args);
static void              pixmap_load_update_values (PixmapLoadControls *controls,
                                                    PixmapLoadArgs *args);
static GdkPixbuf*        pixmap_draw_pixbuf        (GwyContainer *data,
                                                    const gchar *format_name,
                                                    GwyRunType mode,
                                                    GError **error);
static GdkPixbuf*        pixmap_real_draw_pixbuf   (GwyContainer *data,
                                                    PixmapSaveArgs *args);
static gboolean          pixmap_save_dialog        (GwyContainer *data,
                                                    PixmapSaveArgs *args,
                                                    const gchar *name);
static gboolean          pixmap_save_png           (GwyContainer *data,
                                                    const gchar *filename,
                                                    GwyRunType mode,
                                                    GError **error);
static gboolean          pixmap_save_jpeg          (GwyContainer *data,
                                                    const gchar *filename,
                                                    GwyRunType mode,
                                                    GError **error);
#ifdef HAVE_TIFF
static gboolean          pixmap_save_tiff          (GwyContainer *data,
                                                    const gchar *filename,
                                                    GwyRunType mode,
                                                    GError **error);
#endif
static gboolean          pixmap_save_ppm           (GwyContainer *data,
                                                    const gchar *filename,
                                                    GwyRunType mode,
                                                    GError **error);
static gboolean          pixmap_save_bmp           (GwyContainer *data,
                                                    const gchar *filename,
                                                    GwyRunType mode,
                                                    GError **error);
static gboolean          pixmap_save_targa         (GwyContainer *data,
                                                    const gchar *filename,
                                                    GwyRunType mode,
                                                    GError **error);
static GdkPixbuf*        hruler                    (gint size,
                                                    gint extra,
                                                    gdouble real,
                                                    gdouble zoom,
                                                    gdouble offset,
                                                    GwySIUnit *siunit);
static GdkPixbuf*        vruler                    (gint size,
                                                    gint extra,
                                                    gdouble real,
                                                    gdouble zoom,
                                                    gdouble offset,
                                                    GwySIUnit *siunit);
static GdkPixbuf*        fmscale                   (gint size,
                                                    gdouble bot,
                                                    gdouble top,
                                                    gdouble zoom,
                                                    GwySIUnit *siunit);
static GdkDrawable*      prepare_drawable          (gint width,
                                                    gint height,
                                                    gint lw,
                                                    GdkGC **gc);
static PangoLayout*      prepare_layout            (gdouble zoom);
static PixmapFormatInfo* find_format               (const gchar *name);
static void              pixmap_save_load_args     (GwyContainer *container,
                                                    PixmapSaveArgs *args);
static void              pixmap_save_save_args     (GwyContainer *container,
                                                    PixmapSaveArgs *args);
static void              pixmap_save_sanitize_args (PixmapSaveArgs *args);
static void              pixmap_load_load_args     (GwyContainer *container,
                                                    PixmapLoadArgs *args);
static void              pixmap_load_save_args     (GwyContainer *container,
                                                    PixmapLoadArgs *args);
static void              pixmap_load_sanitize_args (PixmapLoadArgs *args);

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
#ifdef HAVE_TIFF
    {
        "tiff",
        N_("TIFF (.tiff,.tif)"),
        GWY_TIFF_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_tiff,
    },
#endif
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
};

/* List of PixmapFormatInfo for all formats.
 * FIXME: this is never freed */
static GSList *pixmap_formats = NULL;

static const PixmapSaveArgs pixmap_save_defaults = {
    1.0, PIXMAP_EVERYTHING, TRUE, TRUE, FONT_SIZE, TRUE,
    /* Interface only */
    NULL, NULL, 0, 0, FALSE,
};

static const PixmapLoadArgs pixmap_load_defaults = {
    100.0, 100.0, -6, TRUE, "m", 1.0, -6, "m", PIXMAP_MAP_VALUE, NULL
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Exports data as pixmap images and imports data from pixmap images. "
       "Supports following image formats for export: "
       "PNG, "
       "JPEG, "
       "TIFF (if available), "
       "PPM, "
       "BMP, "
       "TARGA. "
       "Import support relies on GDK and thus may be installation-dependent."),
    "Yeti <yeti@gwyddion.net>",
    "6.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    PixmapFormatInfo *format_info;
    GSList *formats, *l;
    gboolean registered[G_N_ELEMENTS(saveable_formats)];
    guint i;

    memset(registered, 0, G_N_ELEMENTS(saveable_formats)*sizeof(gboolean));
    formats = gdk_pixbuf_get_formats();
    for (l = formats; l; l = g_slist_next(l)) {
        GdkPixbufFormat *pixbuf_format = (GdkPixbufFormat*)l->data;
        GwyFileSaveFunc save = NULL;
        gchar *fmtname;

        /* Ignore all vector formats */
        if (gdk_pixbuf_format_is_scalable(pixbuf_format))
            continue;

        fmtname = gdk_pixbuf_format_get_name(pixbuf_format);
        /* Ignore some really silly formats explicitly */
        if (gwy_strequal(fmtname, "ico")
            || gwy_strequal(fmtname, "ani")
            || gwy_strequal(fmtname, "wbmp")
            /* libwmf loader seems to try to claim ownership of almost
             * arbitrary binary data, prints error messages, and it's silly
             * to load WMF to Gwyddion anyway */
            || gwy_strequal(fmtname, "wmf")
            /* swfdec causes strange errors and how mad one has to be to try
             * to import Flash to Gwyddion? */
            || gwy_strequal(fmtname, "swf")
            || gwy_strequal(fmtname, "xbm")
            || gwy_strequal(fmtname, "svg")) {
            g_free(fmtname);
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

    /* FIXME: GdkPixbuf doesn't good a good job regarding detection
     * we do some sanity check ourselves */
    if (gwy_strequal(name, "png")
        && memcmp(fileinfo->head, "\x89PNG\r\n\x1a\n", 8) != 0)
        return 0;
    if (gwy_strequal(name, "bmp")
        && strncmp(fileinfo->head, "BM", 2) != 0)
        return 0;
    if (gwy_strequal(name, "pnm")
        && (fileinfo->head[0] != 'P' || !g_ascii_isdigit(fileinfo->head[1])))
        return 0;
    if (gwy_strequal(name, "xpm")
        && strncmp(fileinfo->head, "/* XPM */", 9) != 0)
        return 0;
    if (gwy_strequal(name, "tiff")
        && memcmp(fileinfo->head, "MM\x00\x2a", 4) != 0
        && memcmp(fileinfo->head, "II\x2a\x00", 4) != 0)
        return 0;
    if (gwy_strequal(name, "jpeg")
        && memcmp(fileinfo->head, "\xff\xd8", 2) != 0)
        return 0;
    if (gwy_strequal(name, "pcx")
        && (fileinfo->head[0] != '\x0a' || fileinfo->head[1] > 0x05))
        return 0;
    if (gwy_strequal(name, "gif")
        && strncmp(fileinfo->head, "GIF8", 4) != 0)
        return 0;
    if (gwy_strequal(name, "svg")
        && strncmp(fileinfo->head, "<?xml", 5) != 0)
        return 0;
    if (gwy_strequal(name, "ras")
        && memcmp(fileinfo->head, "\x59\xa6\x6a\x95", 4) != 0)
        return 0;
    /* FIXME: cannot detect targa, must try loader */

    loader = gdk_pixbuf_loader_new_with_type(name, NULL);
    if (!loader)
        return 0;

    if (gdk_pixbuf_loader_write(loader,
                                fileinfo->head, fileinfo->buffer_len, &err))
        score = 80;
    else {
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
    enum { buffer_length = 4096 };
    guchar pixmap_buf[buffer_length];
    PixmapFormatInfo *format_info;
    GdkPixbufLoader *loader;
    GwyDataField *dfield;
    GwySIUnit *siunit;
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

    /* Someday we can load pixmaps with default settings */
    if (mode != GWY_RUN_INTERACTIVE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_INTERACTIVE,
                    _("Pixmap image import must be run as interactive."));
        return NULL;
    }

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

    loader = gdk_pixbuf_loader_new_with_type(name, &err);
    if (!loader) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("Cannot get pixbuf loader: %s."), err->message);
        g_clear_error(&err);
        fclose(fh);
        return NULL;
    }

    do {
        n = fread(pixmap_buf, 1, buffer_length, fh);
        gwy_debug("loaded %u bytes", n);
        if (!gdk_pixbuf_loader_write(loader, pixmap_buf, n, &err)) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Pixbuf loader refused data: %s."), err->message);
            g_clear_error(&err);
            g_object_unref(loader);
            return NULL;
        }
    } while (n == buffer_length);

    if (!gdk_pixbuf_loader_close(loader, &err)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_DATA,
                    _("Pixbuf loader refused data: %s."), err->message);
        g_clear_error(&err);
        g_object_unref(loader);
        return NULL;
    }

    pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    g_assert(pixbuf);
    g_object_ref(pixbuf);
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
    ok = pixmap_load_dialog(&args, name, width, height, maptype_known);
    pixmap_load_save_args(settings, &args);
    if (!ok) {
        err_CANCELLED(error);
        g_object_unref(pixbuf);
        g_free(args.xyunit);
        g_free(args.zunit);
        return NULL;
    }

    dfield = gwy_data_field_new(width, height, args.xreal, args.yreal, FALSE);
    pixmap_load_pixbuf_to_data_field(pixbuf, dfield, args.maptype);
    g_object_unref(pixbuf);

    gwy_data_field_set_xreal(dfield, args.xreal*pow10(args.xyexponent));
    gwy_data_field_set_yreal(dfield, args.yreal*pow10(args.xyexponent));
    gwy_data_field_multiply(dfield, args.zreal*pow10(args.zexponent));
    siunit = gwy_si_unit_new(args.xyunit);
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);
    siunit = gwy_si_unit_new(args.zunit);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    g_free(args.xyunit);
    g_free(args.zunit);

    data = gwy_container_new();
    gwy_container_set_object_by_name(data, "/0/data", dfield);
    g_object_unref(dfield);

    return data;
}

static void
pixmap_load_pixbuf_to_data_field(GdkPixbuf *pixbuf,
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

            default:
            g_assert_not_reached();
            break;
        }
    }
}

static gboolean
pixmap_load_dialog(PixmapLoadArgs *args,
                   const gchar *name,
                   gint xres,
                   gint yres,
                   const gboolean mapknown)
{
    enum { RESPONSE_RESET = 1 };
    static const GwyEnum value_map_types[] = {
        { "Red",         PIXMAP_MAP_RED,   },
        { "Green",       PIXMAP_MAP_GREEN, },
        { "Blue",        PIXMAP_MAP_BLUE,  },
        { "Value (max)", PIXMAP_MAP_VALUE, },
        { "RGB sum",     PIXMAP_MAP_SUM,   },
        { "Alpha",       PIXMAP_MAP_ALPHA, },
    };

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
    title = g_strconcat(_("Import "), s, NULL);
    g_free(s);
    dialog = gtk_dialog_new_with_buttons(title, NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    g_free(title);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    table = gtk_table_new(3, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_add(GTK_CONTAINER(align), table);
    row = 0;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Resolution")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    g_snprintf(buf, sizeof(buf), "%u", xres);
    label = gtk_label_new(buf);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gwy_table_attach_row(table, row++, _("_Horizontal size:"), _("px"),
                         label);

    g_snprintf(buf, sizeof(buf), "%u", yres);
    label = gtk_label_new(buf);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    gwy_table_attach_row(table, row++, _("_Vertical size:"), _("px"),
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

    adj = gtk_adjustment_new(args->xreal, 0.01, 10000, 1, 100, 100);
    controls.xreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls.xreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.xreal,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("_Width"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.xreal);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    align = gtk_alignment_new(0.0, 0.5, 1.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), align, 2, 3, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_size_group_add_widget(sizegroup, hbox2);
    gtk_container_add(GTK_CONTAINER(align), hbox2);

    unit = gwy_si_unit_new(args->xyunit);
    controls.xyexponent = gwy_combo_box_metric_unit_new(NULL, NULL,
                                                        args->xyexponent - 6,
                                                        args->xyexponent + 6,
                                                        unit,
                                                        args->xyexponent);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.xyexponent, FALSE, FALSE, 0);

    controls.xyunits = gtk_button_new_with_label(_("Change"));
    g_object_set_data(G_OBJECT(controls.xyunits), "id", (gpointer)"xy");
    g_signal_connect(controls.xyunits, "clicked",
                     G_CALLBACK(units_change_cb), &controls);
    gtk_box_pack_end(GTK_BOX(hbox2), controls.xyunits, FALSE, FALSE, 0);
    row++;

    adj = gtk_adjustment_new(args->yreal, 0.01, 10000, 1, 100, 100);
    controls.yreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls.yreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.yreal,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("H_eight"));
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

    adj = gtk_adjustment_new(args->zreal, 0.01, 10000, 1, 100, 100);
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
    gtk_size_group_add_widget(sizegroup, hbox2);
    gtk_container_add(GTK_CONTAINER(align), hbox2);

    gwy_si_unit_set_from_string(unit, args->zunit);
    controls.zexponent = gwy_combo_box_metric_unit_new(NULL, NULL,
                                                        args->zexponent - 6,
                                                        args->zexponent + 6,
                                                        unit,
                                                        args->zexponent);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.zexponent, FALSE, FALSE, 0);
    g_object_unref(unit);

    controls.zunits = gtk_button_new_with_label(_("Change"));
    g_object_set_data(G_OBJECT(controls.zunits), "id", (gpointer)"z");
    g_signal_connect(controls.zunits, "clicked",
                     G_CALLBACK(units_change_cb), &controls);
    gtk_box_pack_end(GTK_BOX(hbox2), controls.zunits, FALSE, FALSE, 0);

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
        gwy_table_attach_row(table, row++, _("Use"), _("as data"),
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
    GwyContainer *data;
    GwyDataField *dfield;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    if (!gwy_container_gis_object_by_name(data, "/0/data", &dfield)) {
        dfield = gwy_data_field_new(1, 1, 1.0, 1.0, FALSE);
        gwy_container_set_object_by_name(data, "/0/data", dfield);
        g_object_unref(dfield);
    }
    pixmap_load_pixbuf_to_data_field(controls->small_pixbuf, dfield,
                                     args->maptype);
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

/***************************************************************************
 *
 *  writers
 *
 ***************************************************************************/

static gboolean
pixmap_save_png(GwyContainer *data,
                const gchar *filename,
                GwyRunType mode,
                GError **error)
{
    GdkPixbuf *pixbuf;
    GError *err = NULL;
    gboolean ok;

    pixbuf = pixmap_draw_pixbuf(data, "PNG", mode, error);
    if (!pixbuf)
        return FALSE;

    ok = gdk_pixbuf_save(pixbuf, filename, "png", &err, NULL);
    if (!ok) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Pixbuf save failed: %s."), err->message);
        g_clear_error(&err);
    }
    g_object_unref(pixbuf);

    return ok;
}

static gboolean
pixmap_save_jpeg(GwyContainer *data,
                 const gchar *filename,
                 GwyRunType mode,
                 GError **error)
{
    GdkPixbuf *pixbuf;
    GError *err = NULL;
    gboolean ok;

    pixbuf = pixmap_draw_pixbuf(data, "JPEG", mode, error);
    if (!pixbuf)
        return FALSE;

    ok = gdk_pixbuf_save(pixbuf, filename, "jpeg", &err, "quality", "98", NULL);
    if (!ok) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Pixbuf save failed: %s."), err->message);
        g_clear_error(&err);
    }
    g_object_unref(pixbuf);

    return ok;
}

#ifdef HAVE_TIFF
static gboolean
pixmap_save_tiff(GwyContainer *data,
                 const gchar *filename,
                 GwyRunType mode,
                 GError **error)
{
    GdkPixbuf *pixbuf;
    TIFF *out;
    guchar *pixels = NULL;
    guint rowstride, i, width, height;
    /* TODO: error handling (ugly, requires global variables for
     * communication) */
    gboolean ok = TRUE;

    pixbuf = pixmap_draw_pixbuf(data, "TIFF", mode, error);
    if (!pixbuf)
        return FALSE;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    out = TIFFOpen(filename, "w");
    if (!out) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("TIFFOpen() function failed."));
        g_object_unref(pixbuf);
        return FALSE;
    }

    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

    g_return_val_if_fail(TIFFScanlineSize(out) <= (glong)rowstride, FALSE);
    TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, 3*width));
    for (i = 0; i < height; i++) {
        if (TIFFWriteScanline(out, pixels + i*rowstride, i, 0) < 0) {
            g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                        _("TIFFWriteScanline() function failed."));
            ok = FALSE;
            break;
        }
    }
    TIFFClose(out);
    g_object_unref(pixbuf);

    return ok;
}
#endif

static gboolean
pixmap_save_ppm(GwyContainer *data,
                const gchar *filename,
                GwyRunType mode,
                GError **error)
{
    static const gchar *ppm_header = "P6\n%u\n%u\n255\n";
    GdkPixbuf *pixbuf;
    guchar *pixels = NULL;
    guint rowstride, i, width, height;
    gboolean ok = FALSE;
    gchar *ppmh = NULL;
    FILE *fh;

    pixbuf = pixmap_draw_pixbuf(data, "PPM", mode, error);
    if (!pixbuf)
        return FALSE;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    fh = g_fopen(filename, "wb");
    if (!fh) {
        err_OPEN_WRITE(error);
        g_object_unref(pixbuf);
        return FALSE;
    }

    ppmh = g_strdup_printf(ppm_header, width, height);
    if (fwrite(ppmh, 1, strlen(ppmh), fh) != strlen(ppmh)) {
        err_WRITE(error);
        goto end;
    }

    for (i = 0; i < height; i++) {
        if (fwrite(pixels + i*rowstride, 1, 3*width, fh) != 3*width) {
            err_WRITE(error);
            goto end;
        }
    }

    ok = TRUE;
end:
    g_object_unref(pixbuf);
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
    GdkPixbuf *pixbuf;
    guchar *pixels = NULL, *buffer = NULL;
    guint rowstride, i, j, width, height;
    guint bmplen, bmprowstride;
    gboolean ok = FALSE;
    FILE *fh;

    pixbuf = pixmap_draw_pixbuf(data, "BMP", mode, error);
    if (!pixbuf)
        return FALSE;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    bmprowstride = 12*((width + 3)/4);
    bmplen = height*bmprowstride + sizeof(bmp_head);

    fh = g_fopen(filename, "wb");
    if (!fh) {
        err_OPEN_WRITE(error);
        g_object_unref(pixbuf);
        return FALSE;
    }

    *(guint32*)(bmp_head + 2) = GUINT32_TO_LE(bmplen);
    *(guint32*)(bmp_head + 18) = GUINT32_TO_LE(bmprowstride/3);
    *(guint32*)(bmp_head + 22) = GUINT32_TO_LE(height);
    *(guint32*)(bmp_head + 34) = GUINT32_TO_LE(height*bmprowstride);
    if (fwrite(bmp_head, 1, sizeof(bmp_head), fh) != sizeof(bmp_head)) {
        err_WRITE(error);
        goto end;
    }

    /* The ugly part: BMP uses BGR instead of RGB and is written upside down,
     * this silliness may originate nowhere else than in MS... */
    buffer = g_new(guchar, bmprowstride);
    memset(buffer, 0xff, sizeof(bmprowstride));
    for (i = 0; i < height; i++) {
        guchar *p = pixels + (height - 1 - i)*rowstride;
        guchar *q = buffer;

        for (j = width; j; j--, p += 3, q += 3) {
            *q = *(p + 2);
            *(q + 1) = *(p + 1);
            *(q + 2) = *p;
        }
        if (fwrite(buffer, 1, bmprowstride, fh) != bmprowstride) {
            err_WRITE(error);
            goto end;
        }
    }

    ok = TRUE;
end:
    g_object_unref(pixbuf);
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
    GdkPixbuf *pixbuf;
    guchar *pixels, *buffer = NULL;
    guint targarowstride, rowstride, i, j, width, height;
    gboolean ok = FALSE;
    FILE *fh;

    pixbuf = pixmap_draw_pixbuf(data, "TARGA", mode, error);
    if (!pixbuf)
        return FALSE;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    targarowstride = 12*((width + 3)/4);

    if (height > 65535 || width > 65535) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Image is too large to be stored as TARGA."));
        return FALSE;
    }
    targa_head[12] = (targarowstride/3) & 0xff;
    targa_head[13] = (targarowstride/3 >> 8) & 0xff;
    targa_head[14] = (height) & 0xff;
    targa_head[15] = (height >> 8) & 0xff;

    fh = g_fopen(filename, "wb");
    if (!fh) {
        err_OPEN_WRITE(error);
        g_object_unref(pixbuf);
        return FALSE;
    }

    if (fwrite(targa_head, 1, sizeof(targa_head), fh) != sizeof(targa_head)) {
        err_WRITE(error);
        goto end;
    }

    /* The ugly part: TARGA uses BGR instead of RGB */
    buffer = g_new(guchar, targarowstride);
    memset(buffer, 0xff, sizeof(targarowstride));
    for (i = 0; i < height; i++) {
        guchar *p = pixels + i*rowstride;
        guchar *q = buffer;

        for (j = width; j; j--, p += 3, q += 3) {
            *q = *(p + 2);
            *(q + 1) = *(p + 1);
            *(q + 2) = *p;
        }
        if (fwrite(buffer, 1, targarowstride, fh) != targarowstride) {
            err_WRITE(error);
            goto end;
        }
    }

    ok = TRUE;
end:
    g_object_unref(pixbuf);
    g_free(buffer);
    fclose(fh);

    return ok;
}

/***************************************************************************
 *
 *  save - common
 *
 ***************************************************************************/

static GdkPixbuf*
pixmap_draw_pixbuf(GwyContainer *data,
                   const gchar *format_name,
                   GwyRunType mode,
                   GError **error)
{
    GdkPixbuf *pixbuf;
    GwyContainer *settings;
    PixmapSaveArgs args;
    const gchar *key;
    gchar *buf;

    if (mode != GWY_RUN_INTERACTIVE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_INTERACTIVE,
                    _("Pixmap image export must be run as interactive."));
        return FALSE;
    }

    settings = gwy_app_settings_get();
    pixmap_save_load_args(settings, &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_VIEW, &args.data_view,
                                     GWY_APP_DATA_FIELD, &args.dfield,
                                     0);
    if (!args.dfield || !args.data_view) {
        err_NO_CHANNEL_EXPORT(error);
        return FALSE;
    }
    key = gwy_data_view_get_data_prefix(args.data_view);
    buf = g_strconcat(key, "/realsquare", NULL);
    gwy_container_gis_boolean_by_name(data, buf, &args.realsquare);
    g_free(buf);

    if (!pixmap_save_dialog(data, &args, format_name)) {
        err_CANCELLED(error);
        return NULL;
    }
    pixbuf = pixmap_real_draw_pixbuf(data, &args);
    pixmap_save_save_args(settings, &args);

    return pixbuf;
}

static GdkPixbuf*
pixmap_real_draw_pixbuf(GwyContainer *data,
                        PixmapSaveArgs *args)
{
    GtkWidget *data_window;
    GwyPixmapLayer *layer;
    GwyGradient *gradient;
    GtkWidget *coloraxis;
    GdkPixbuf *pixbuf, *hrpixbuf, *vrpixbuf, *datapixbuf, *tmpixbuf;
    GdkPixbuf *scalepixbuf = NULL;
    GwySIUnit *siunit_xy, *siunit_z;
    const guchar *samples, *name;
    const gchar *key;
    guchar *pixels;
    gint zwidth, zheight, hrh, vrw, scw, nsamp, y, lw;
    gboolean has_presentation;
    gdouble fontzoom, min, max;
    gint border = 20;
    gint gap = 20;
    gint fmw = 18;

    data_window = gtk_widget_get_toplevel(GTK_WIDGET(args->data_view));
    g_return_val_if_fail(gwy_data_view_get_data(args->data_view) == data, NULL);
    layer = gwy_data_view_get_base_layer(args->data_view);
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), NULL);
    /* This is not just the scaling of text, but of ticks and other line
     * graphics too. */
    fontzoom = args->font_size/FONT_SIZE;

    siunit_xy = gwy_data_field_get_si_unit_xy(args->dfield);
    has_presentation
        = gwy_layer_basic_get_has_presentation(GWY_LAYER_BASIC(layer));
    key = gwy_layer_basic_get_gradient_key(GWY_LAYER_BASIC(layer));
    name = NULL;
    if (key)
        gwy_container_gis_string_by_name(data, key, &name);
    gradient = gwy_gradients_get_gradient(name);
    gwy_resource_use(GWY_RESOURCE(gradient));
    samples = gwy_gradient_get_samples(gradient, &nsamp);
    gwy_resource_release(GWY_RESOURCE(gradient));

    datapixbuf = gwy_data_view_export_pixbuf(args->data_view,
                                             args->zoom,
                                             args->draw_mask,
                                             args->draw_selection);
    gwy_debug_objects_creation(G_OBJECT(datapixbuf));
    zwidth = gdk_pixbuf_get_width(datapixbuf);
    zheight = gdk_pixbuf_get_height(datapixbuf);

    if (args->otype == PIXMAP_RAW_DATA)
        return datapixbuf;

    gap *= fontzoom;
    fmw *= fontzoom;
    lw = ZOOM2LW(fontzoom);

    hrpixbuf = hruler(zwidth + 2*lw, border,
                      gwy_data_field_get_xreal(args->dfield),
                      fontzoom, gwy_data_field_get_xoffset(args->dfield),
                      siunit_xy);
    hrh = gdk_pixbuf_get_height(hrpixbuf);
    vrpixbuf = vruler(zheight + 2*lw, border,
                      gwy_data_field_get_yreal(args->dfield),
                      fontzoom, gwy_data_field_get_yoffset(args->dfield),
                      siunit_xy);
    vrw = gdk_pixbuf_get_width(vrpixbuf);
    if (args->otype == PIXMAP_EVERYTHING) {
        if (has_presentation)
            siunit_z = gwy_si_unit_new(NULL);
        else
            siunit_z = gwy_data_field_get_si_unit_z(args->dfield);
        coloraxis
            = gwy_data_window_get_color_axis(GWY_DATA_WINDOW(data_window));
        gwy_color_axis_get_range(GWY_COLOR_AXIS(coloraxis), &min, &max);
        scalepixbuf = fmscale(zheight + 2*lw, min, max, fontzoom, siunit_z);
        scw = gdk_pixbuf_get_width(scalepixbuf);
        if (has_presentation)
            g_object_unref(siunit_z);
    }
    else {
        gap = 0;
        fmw = 0;
        scw = 0;
    }

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, BITS_PER_SAMPLE,
                            vrw + zwidth + 2*lw + 2*border
                            + gap + fmw + 2*lw + scw,
                            hrh + zheight + 2*lw + 2*border + border/3);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    gdk_pixbuf_fill(pixbuf, 0xffffffff);
    gdk_pixbuf_copy_area(datapixbuf,
                         0, 0, zwidth, zheight,
                         pixbuf,
                         vrw + lw + border, hrh + lw + border);
    g_object_unref(datapixbuf);
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
    if (args->otype == PIXMAP_EVERYTHING) {
        gdk_pixbuf_copy_area(scalepixbuf,
                            0, 0, scw, zheight + 2*lw,
                            pixbuf,
                            border + vrw + zwidth + 2*lw + gap + fmw + 2*lw,
                            hrh + border);
        g_object_unref(scalepixbuf);

        pixels = gdk_pixbuf_get_pixels(pixbuf);
        for (y = 0; y < zheight; y++) {
            gint j, k;
            guchar *row;

            row = pixels
                + gdk_pixbuf_get_rowstride(pixbuf)*(border + hrh + lw + y)
                + 3*(int)(border + vrw + zwidth + 2*lw + gap + lw);
            k = nsamp-1 - floor(nsamp*y/zheight);
            for (j = 0; j < fmw; j++) {
                row[3*j] = samples[4*k];
                row[3*j + 1] = samples[4*k + 1];
                row[3*j + 2] = samples[4*k + 2];
            }
        }
    }

    /* outline */
    tmpixbuf = gdk_pixbuf_new_subpixbuf(pixbuf,
                                        vrw + border, hrh + border,
                                        lw, zheight + 2*lw);
    gwy_debug_objects_creation(G_OBJECT(tmpixbuf));
    gdk_pixbuf_fill(tmpixbuf, 0x000000);
    gdk_pixbuf_copy_area(tmpixbuf, 0, 0, lw, zheight + 2*lw,
                         pixbuf, vrw + border + zwidth + lw, hrh + border);
    if (args->otype == PIXMAP_EVERYTHING) {
        gdk_pixbuf_copy_area(tmpixbuf, 0, 0, lw, zheight + lw,
                            pixbuf,
                            vrw + border + zwidth + 2*lw + gap,
                            hrh + border);
        gdk_pixbuf_copy_area(tmpixbuf, 0, 0, lw, zheight + 2*lw,
                            pixbuf,
                            vrw + border + zwidth + 2*lw + gap + fmw + lw,
                            hrh + border);
    }
    g_object_unref(tmpixbuf);

    tmpixbuf = gdk_pixbuf_new_subpixbuf(pixbuf,
                                        vrw + border, hrh + border,
                                        zwidth + 2*lw, lw);
    gwy_debug_objects_creation(G_OBJECT(tmpixbuf));
    gdk_pixbuf_fill(tmpixbuf, 0x000000);
    gdk_pixbuf_copy_area(tmpixbuf, 0, 0, zwidth + 2*lw, lw,
                         pixbuf, vrw + border, hrh + border + zheight + lw);
    if (args->otype == PIXMAP_EVERYTHING) {
        gdk_pixbuf_copy_area(tmpixbuf, 0, 0, fmw + 2*lw, lw,
                            pixbuf,
                            vrw + border + zwidth + 2*lw + gap,
                            hrh + border);
        gdk_pixbuf_copy_area(tmpixbuf, 0, 0, fmw + 2*lw, lw,
                            pixbuf,
                            vrw + border + zwidth + 2*lw + gap,
                            hrh + border + lw + zheight);
    }
    g_object_unref(tmpixbuf);

    return pixbuf;
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
update_preview(PixmapSaveControls *controls)
{
    GdkPixbuf *pixbuf;
    gdouble zoom;

    zoom = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zoom));

    controls->args->font_size *= controls->args->zoom/zoom;
    if (controls->args->otype == PIXMAP_RAW_DATA)
        controls->args->zoom *= 1.4;

    pixbuf = pixmap_real_draw_pixbuf(controls->data, controls->args);
    gtk_image_set_from_pixbuf(GTK_IMAGE(controls->image), pixbuf);
    g_object_unref(pixbuf);

    if (controls->args->otype == PIXMAP_RAW_DATA)
        controls->args->zoom /= 1.4;
    controls->args->font_size /= controls->args->zoom/zoom;
}

static void
save_type_changed(GtkWidget *button,
                  PixmapSaveControls *controls)
{
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    controls->args->otype = gwy_radio_buttons_get_current(controls->otypes);
    update_preview(controls);
}

static void
draw_mask_changed(GtkToggleButton *check,
                  PixmapSaveControls *controls)
{
    controls->args->draw_mask = gtk_toggle_button_get_active(check);
    update_preview(controls);
}

static void
draw_selection_changed(GtkToggleButton *check,
                       PixmapSaveControls *controls)
{
    controls->args->draw_selection = gtk_toggle_button_get_active(check);
    update_preview(controls);
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
    else if (controls->args->otype != PIXMAP_RAW_DATA)
        update_preview(controls);
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
    if (controls->args->otype != PIXMAP_RAW_DATA)
        update_preview(controls);
}

static void
font_size_changed(GtkAdjustment *adj,
                  PixmapSaveControls *controls)
{
    controls->args->font_size = gtk_adjustment_get_value(adj);
    if (controls->in_update
        || controls->args->scale_font
        || controls->args->otype == PIXMAP_RAW_DATA)
        return;

    update_preview(controls);
}

static gboolean
pixmap_save_dialog(GwyContainer *data,
                   PixmapSaveArgs *args,
                   const gchar *name)
{
    enum { RESPONSE_RESET = 1 };

    PixmapSaveControls controls;
    GtkWidget *dialog, *table, *spin, *hbox, *align, *check;
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
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    g_free(title);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    table = gtk_table_new(13, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_add(GTK_CONTAINER(align), table);
    row = 0;

    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(gwy_sgettext("Scaling")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    minzoom = 2.0/MIN(args->xres, args->yres);
    maxzoom = 4096.0/MAX(args->xres, args->yres);
    controls.zoom = gtk_adjustment_new(args->zoom, minzoom, maxzoom,
                                       0.001, 0.5, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Zoom:"), NULL,
                                       controls.zoom);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    g_signal_connect(controls.zoom, "value-changed",
                     G_CALLBACK(zoom_changed), &controls);
    row++;

    controls.width = gtk_adjustment_new(args->zoom*args->xres,
                                        2, 4096, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Width:"), "px",
                                       controls.width);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    g_signal_connect(controls.width, "value-changed",
                     G_CALLBACK(width_changed), &controls);
    row++;

    controls.height = gtk_adjustment_new(args->zoom*args->yres,
                                         2, 4096, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Height:"), "px",
                                       controls.height);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    g_signal_connect(controls.height, "value-changed",
                     G_CALLBACK(height_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    controls.scale_font
        = gtk_check_button_new_with_mnemonic(_("Scale text _proportionally"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.scale_font),
                                 args->scale_font);
    gtk_table_attach(GTK_TABLE(table), controls.scale_font,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.scale_font, "toggled",
                     G_CALLBACK(scale_font_changed), &controls);
    row++;

    if (args->scale_font)
        args->font_size = FONT_SIZE*args->zoom;
    adj = gtk_adjustment_new(args->font_size,
                             FONT_SIZE*minzoom, FONT_SIZE*maxzoom,
                             0.01, 1.0, 0);
    controls.font_size = gwy_table_attach_spinbutton(table, row,
                                                     _("_Font size:"), NULL,
                                                     adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls.font_size), 2);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls.font_size),
                                   !args->scale_font);
    g_signal_connect(adj, "value-changed",
                     G_CALLBACK(font_size_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Output")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.otypes
        = gwy_radio_buttons_createl(G_CALLBACK(save_type_changed), &controls,
                                    args->otype,
                                    _("_Data alone"), PIXMAP_RAW_DATA,
                                    _("Data + _rulers"), PIXMAP_RULERS,
                                    _("_Everything"), PIXMAP_EVERYTHING,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.otypes,
                                            GTK_TABLE(table), 3, row);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    check = gtk_check_button_new_with_mnemonic(_("Draw _mask"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 args->draw_mask);
    gtk_table_attach(GTK_TABLE(table), check, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(draw_mask_changed), &controls);
    controls.draw_mask = check;
    row++;

    check = gtk_check_button_new_with_mnemonic(_("Draw _selection"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 args->draw_selection);
    gtk_table_attach(GTK_TABLE(table), check, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(draw_selection_changed), &controls);
    controls.draw_selection = check;
    row++;

    /* preview */
    align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    controls.image = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(align), controls.image);

    /* XXX: We use this for *preview* zoom during the dialog, the real zoom
     * is only in the adjustment and it's loaded on close. */
    args->zoom = PREVIEW_SIZE/(gdouble)MAX(args->xres, args->yres);
    update_preview(&controls);
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
            args->zoom = pixmap_save_defaults.zoom;
            args->otype = pixmap_save_defaults.otype;
            args->draw_mask = pixmap_save_defaults.draw_mask;
            args->draw_selection = pixmap_save_defaults.draw_selection;
            gtk_adjustment_set_value(GTK_ADJUSTMENT(controls.zoom), args->zoom);
            args->zoom = PREVIEW_SIZE/(gdouble)MAX(args->xres, args->yres);
            gwy_radio_buttons_set_current(controls.otypes, args->otype);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.draw_mask),
                                         args->draw_mask);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.draw_selection),
                                         args->draw_selection);
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
hruler(gint size,
       gint extra,
       gdouble real,
       gdouble zoom,
       gdouble offset,
       GwySIUnit *siunit)
{
    PangoRectangle logical1, logical2;
    PangoLayout *layout;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GdkGC *gc;
    GwySIValueFormat *format;
    gdouble base, step, x, from, to;
    GString *s;
    gint l, n, ix;
    gint tick, height, lw;

    s = g_string_new(NULL);
    layout = prepare_layout(zoom);

    format = gwy_si_unit_get_format_with_resolution(siunit,
                                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                    real, real/12,
                                                    NULL);
    format_layout(layout, &logical1, s, "%.*f",
                  format->precision, -real/format->magnitude);
    format_layout(layout, &logical2, s, "%.*f %s",
                  format->precision, 0.0, format->units);

    offset /= format->magnitude;
    real /= format->magnitude;

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
    drawable = prepare_drawable(size + extra, height, lw, &gc);

    from = offset;
    from = ceil(from/(base*step) - 1e-15)*(base*step);
    to = real + offset;
    to = floor(to/(base*step) + 1e-15)*(base*step);

    for (x = from; x <= to; x += base*step) {
        if (fabs(x) < 1e-15*base*step)
            x = 0.0;
        format_layout(layout, &logical1, s, "%.*f%s%s",
                      format->precision, x,
                      x ? "" : " ",
                      x ? "" : format->units);
        ix = (x - offset)/real*size + lw/2;
        if (ix + PANGO_PIXELS(logical1.width) <= size + extra/4)
            gdk_draw_layout(drawable, gc,
                            ix+1, 1 + l - PANGO_PIXELS(logical1.height),
                            layout);
        gdk_draw_line(drawable, gc, ix, height-1, ix, height-1-tick);
    }

    pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable, NULL,
                                          0, 0, 0, 0, size + extra, height);

    gwy_si_unit_value_format_free(format);
    g_object_unref(gc);
    g_object_unref(drawable);
    g_object_unref(layout);
    g_string_free(s, TRUE);

    return pixbuf;
}

static GdkPixbuf*
vruler(gint size,
       gint extra,
       gdouble real,
       gdouble zoom,
       gdouble offset,
       GwySIUnit *siunit)
{
    PangoRectangle logical1, logical2;
    PangoLayout *layout;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GdkGC *gc;
    GwySIValueFormat *format;
    gdouble base, step, x, from, to;
    GString *s;
    gint l, n, ix;
    gint tick, width, lw;

    s = g_string_new(NULL);
    layout = prepare_layout(zoom);

    format = gwy_si_unit_get_format_with_resolution(siunit,
                                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                    real, real/12,
                                                    NULL);

    /* note the algorithm is the same to force consistency between axes,
     * even though the vertical one could be filled with tick more densely */
    format_layout(layout, &logical1, s, "%.*f",
                  format->precision, -real/format->magnitude);
    format_layout(layout, &logical2, s, "%.*f %s",
                  format->precision, 0.0, format->units);

    offset /= format->magnitude;
    real /= format->magnitude;

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
    drawable = prepare_drawable(width, size + extra, lw, &gc);

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

    gwy_si_unit_value_format_free(format);
    g_object_unref(gc);
    g_object_unref(drawable);
    g_object_unref(layout);
    g_string_free(s, TRUE);

    return pixbuf;
}


static GdkPixbuf*
fmscale(gint size,
        gdouble bot,
        gdouble top,
        gdouble zoom,
        GwySIUnit *siunit)
{
    PangoRectangle logical1, logical2;
    PangoLayout *layout;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GdkGC *gc;
    gdouble x;
    GwySIValueFormat *format;
    GString *s;
    gint l, tick, width, lw;

    s = g_string_new("");
    layout = prepare_layout(zoom);

    x = MAX(fabs(bot), fabs(top));
    format = gwy_si_unit_get_format(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                    x, NULL);
    format_layout(layout, &logical1, s, "%.*f %s",
                  format->precision, top/format->magnitude, format->units);
    format_layout(layout, &logical2, s, "%.*f %s",
                  format->precision, bot/format->magnitude, format->units);

    l = MAX(PANGO_PIXELS(logical1.width), PANGO_PIXELS(logical2.width));
    tick = zoom*TICK_LENGTH;
    lw = ZOOM2LW(zoom);
    width = l + 2*zoom + tick + 2;
    drawable = prepare_drawable(width, size, lw, &gc);

    format_layout(layout, &logical1, s, "%.*f %s",
                  format->precision, bot/format->magnitude, format->units);
    gdk_draw_layout(drawable, gc,
                    width - PANGO_PIXELS(logical1.width) - 2,
                    size - 1 - PANGO_PIXELS(logical1.height),
                    layout);
    gdk_draw_line(drawable, gc, 0, size - (lw + 1)/2, tick, size - (lw + 1)/2);

    format_layout(layout, &logical1, s, "%.*f %s",
                  format->precision, top/format->magnitude, format->units);
    gdk_draw_layout(drawable, gc,
                    width - PANGO_PIXELS(logical1.width) - 2, 1,
                    layout);
    gdk_draw_line(drawable, gc, 0, lw/2, tick, lw/2);

    gdk_draw_line(drawable, gc, 0, size/2, tick/2, size/2);

    pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable, NULL,
                                          0, 0, 0, 0, width, size);

    gwy_si_unit_value_format_free(format);
    g_object_unref(gc);
    g_object_unref(drawable);
    g_object_unref(layout);
    g_string_free(s, TRUE);

    return pixbuf;
}

static GdkDrawable*
prepare_drawable(gint width,
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
    window = gwy_app_main_window_get()->window;
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
prepare_layout(gdouble zoom)
{
    PangoContext *context;
    PangoFontDescription *fontdesc;
    PangoLayout *layout;

    context = gdk_pango_context_get();
    fontdesc = pango_font_description_from_string("Helvetica 12");
    pango_font_description_set_size(fontdesc, 12*PANGO_SCALE*zoom);
    pango_context_set_font_description(context, fontdesc);
    layout = pango_layout_new(context);
    g_object_unref(context);
    pango_font_description_free(fontdesc);

    return layout;
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

static const gchar draw_mask_key[]      = "/module/pixmap/draw_mask";
static const gchar draw_selection_key[] = "/module/pixmap/draw_selection";
static const gchar font_size_key[]      = "/module/pixmap/font_size";
static const gchar otype_key[]          = "/module/pixmap/otype";
static const gchar scale_font_key[]     = "/module/pixmap/scale_font";
static const gchar zoom_key[]           = "/module/pixmap/zoom";

static void
pixmap_save_sanitize_args(PixmapSaveArgs *args)
{
    args->otype = MIN(args->otype, PIXMAP_LAST-1);
    args->zoom = CLAMP(args->zoom, 0.06, 16.0);
    args->draw_mask = !!args->draw_mask;
    args->draw_selection = !!args->draw_selection;
    args->scale_font = !!args->scale_font;
    args->font_size = CLAMP(args->font_size, 1.2, 120.0);
}

static void
pixmap_save_load_args(GwyContainer *container,
                      PixmapSaveArgs *args)
{
    *args = pixmap_save_defaults;

    gwy_container_gis_double_by_name(container, zoom_key, &args->zoom);
    gwy_container_gis_enum_by_name(container, otype_key, &args->otype);
    gwy_container_gis_boolean_by_name(container, draw_mask_key,
                                      &args->draw_mask);
    gwy_container_gis_boolean_by_name(container, draw_selection_key,
                                      &args->draw_selection);
    gwy_container_gis_boolean_by_name(container, scale_font_key,
                                      &args->scale_font);
    gwy_container_gis_double_by_name(container, font_size_key,
                                     &args->font_size);
    pixmap_save_sanitize_args(args);
}

static void
pixmap_save_save_args(GwyContainer *container,
                      PixmapSaveArgs *args)
{
    gwy_container_set_double_by_name(container, zoom_key, args->zoom);
    gwy_container_set_enum_by_name(container, otype_key, args->otype);
    gwy_container_set_boolean_by_name(container, draw_mask_key,
                                      args->draw_mask);
    gwy_container_set_boolean_by_name(container, draw_selection_key,
                                      args->draw_selection);
    gwy_container_set_boolean_by_name(container, scale_font_key,
                                      args->scale_font);
    gwy_container_set_double_by_name(container, font_size_key, args->font_size);
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
