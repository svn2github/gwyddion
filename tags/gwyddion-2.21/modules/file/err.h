/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
#ifndef __GWY_FILE_ERR_H__
#define __GWY_FILE_ERR_H__

#include <errno.h>
#include <glib/gutils.h>
#include <libgwymodule/gwymodule-file.h>

/* I/O Errors */
static inline void
err_GET_FILE_CONTENTS(GError **error, GError **err)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                _("Cannot read file contents: %s"), (*err)->message);
    g_clear_error(err);
}

static inline void
err_OPEN_WRITE(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                _("Cannot open file for writing: %s."), g_strerror(errno));
}

static inline void
err_WRITE(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                _("Cannot write to file: %s."), g_strerror(errno));
}

/* Multipart errors */
static inline void
err_DATA_PART(GError **error, const gchar *name)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_SPECIFIC,
                _("No data file corresponding to `%s' was found."), name);
}

/* Data format errors */
static inline void
err_TOO_SHORT(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("File is too short to be of the assumed file type."));
}

static inline void
err_FILE_TYPE(GError **error, const gchar *name)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("File is not a %s file, it is seriously damaged, "
                  "or it is of an unknown format version."), name);
}

static inline gboolean
err_SIZE_MISMATCH(GError **error, guint expected, guint real, gboolean strict)
{
    if (expected == real || (!strict && expected < real))
        return FALSE;

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Expected data size calculated from file headers "
                  "is %u bytes, but the real size is %u bytes."),
                expected, real);
    return TRUE;
}

static inline gboolean
err_DIMENSION(GError **error, gint dim)
{
    if (dim >= 1 && dim <= 1 << 15)
        return FALSE;

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Invalid field dimension: %d."), dim);
    return TRUE;
}

static inline void
err_BPP(GError **error, gint bpp)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("The number of bits per sample %d is invalid or "
                  "unsupported for this file type."),
                bpp);
}

static inline void
err_DATA_TYPE(GError **error,
              gint type)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Data type %d is invalid or unsupported."), type);
}

static inline void
err_MISSING_FIELD(GError **error, const gchar *name)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Header field `%s' is missing."),
                name);
}

static inline void
err_UNSUPPORTED(GError **error, const gchar *name)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("The value of parameter `%s' is invalid or unsupported."),
                name);
}

static inline void
err_INVALID(GError **error, const gchar *name)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Parameter `%s' is missing or invalid."),
                name);
}

static inline void
err_NO_DATA(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("File contains no (importable) data."));
}

static inline void
err_NO_CHANNEL_EXPORT(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("File contains no exportable channel."));
}

/* Cancelled */
static inline void
err_CANCELLED(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_CANCELLED,
                _("File import was cancelled by user."));
}

static inline gboolean
require_keys(GHashTable *hash,
             GError **error,
             ...)
{
    va_list ap;
    const gchar *key;

    if (!hash) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Missing header."));
        return FALSE;
    }

    va_start(ap, error);
    while ((key = va_arg(ap, const gchar *))) {
        if (!g_hash_table_lookup(hash, key)) {
            err_MISSING_FIELD(error, key);
            va_end(ap);
            return FALSE;
        }
    }
    va_end(ap);

    return TRUE;
}

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

