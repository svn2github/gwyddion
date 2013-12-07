/*
 *  $Id$
 *  Copyright (C) 2009-2013 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LIBGWY_PACK_H__
#define __LIBGWY_PACK_H__

#include <glib.h>

G_BEGIN_DECLS

#define GWY_PACK_ERROR gwy_pack_error_quark()

typedef enum {
    GWY_PACK_ERROR_FORMAT = 1,
    GWY_PACK_ERROR_SIZE,
    GWY_PACK_ERROR_ARGUMENTS,
    GWY_PACK_ERROR_DATA,
} GwyPackError;

GQuark gwy_pack_error_quark(void)                 G_GNUC_CONST;
gsize  gwy_pack_size       (const gchar *format,
                            GError **error);
gsize  gwy_pack            (const gchar *format,
                            guchar *buffer,
                            gsize size,
                            GError **error,
                            ...);
gsize  gwy_unpack          (const gchar *format,
                            const guchar *buffer,
                            gsize size,
                            GError **error,
                            ...);
void   gwy_unpack_data     (const gchar *format,
                            gconstpointer buffer,
                            gsize len,
                            gdouble *data,
                            gint data_stride,
                            gdouble factor,
                            gdouble shift);

/* This is necessary to fool gtk-doc that ignores static inline functions */
#define _GWY_STATIC_INLINE static inline

_GWY_STATIC_INLINE gboolean gwy_read_gboolean8     (const guchar **ppv);
_GWY_STATIC_INLINE gint16   gwy_read_gint16_le     (const guchar **ppv);
_GWY_STATIC_INLINE gint16   gwy_read_gint16_be     (const guchar **ppv);
_GWY_STATIC_INLINE guint16  gwy_read_guint16_le    (const guchar **ppv);
_GWY_STATIC_INLINE guint16  gwy_read_guint16_be    (const guchar **ppv);
_GWY_STATIC_INLINE gint32   gwy_read_gint32_le     (const guchar **ppv);
_GWY_STATIC_INLINE gint32   gwy_read_gint32_be     (const guchar **ppv);
_GWY_STATIC_INLINE guint32  gwy_read_guint32_le    (const guchar **ppv);
_GWY_STATIC_INLINE guint32  gwy_read_guint32_be    (const guchar **ppv);
_GWY_STATIC_INLINE gint64   gwy_read_gint64_le     (const guchar **ppv);
_GWY_STATIC_INLINE gint64   gwy_read_gint64_be     (const guchar **ppv);
_GWY_STATIC_INLINE guint64  gwy_read_guint64_le    (const guchar **ppv);
_GWY_STATIC_INLINE guint64  gwy_read_guint64_be    (const guchar **ppv);
_GWY_STATIC_INLINE gfloat   gwy_read_gfloat_le     (const guchar **ppv);
_GWY_STATIC_INLINE gfloat   gwy_read_gfloat_be     (const guchar **ppv);
_GWY_STATIC_INLINE gdouble  gwy_read_gdouble_le    (const guchar **ppv);
_GWY_STATIC_INLINE gdouble  gwy_read_gdouble_be    (const guchar **ppv);

#undef _GWY_STATIC_INLINE

static inline gboolean
gwy_read_gboolean8(const guchar **ppv)
{
    const guint8 *pv = (const guint8*)(*ppv);
    guint8 v = *pv;
    *ppv += sizeof(guint8);
    return !!v;
}

static inline gint16
gwy_read_gint16_le(const guchar **ppv)
{
    const gint16 *pv = (const gint16*)(*ppv);
    gint16 v = GINT16_FROM_LE(*pv);
    *ppv += sizeof(gint16);
    return v;
}

static inline gint16
gwy_read_gint16_be(const guchar **ppv)
{
    const gint16 *pv = (const gint16*)(*ppv);
    gint16 v = GINT16_FROM_BE(*pv);
    *ppv += sizeof(gint16);
    return v;
}

static inline guint16
gwy_read_guint16_le(const guchar **ppv)
{
    const guint16 *pv = (const guint16*)(*ppv);
    guint16 v = GUINT16_FROM_LE(*pv);
    *ppv += sizeof(guint16);
    return v;
}

static inline guint16
gwy_read_guint16_be(const guchar **ppv)
{
    const guint16 *pv = (const guint16*)(*ppv);
    guint16 v = GUINT16_FROM_BE(*pv);
    *ppv += sizeof(guint16);
    return v;
}

static inline gint32
gwy_read_gint32_le(const guchar **ppv)
{
    const gint32 *pv = (const gint32*)(*ppv);
    gint32 v = GINT32_FROM_LE(*pv);
    *ppv += sizeof(gint32);
    return v;
}

static inline gint32
gwy_read_gint32_be(const guchar **ppv)
{
    const gint32 *pv = (const gint32*)(*ppv);
    gint32 v = GINT32_FROM_BE(*pv);
    *ppv += sizeof(gint32);
    return v;
}

static inline guint32
gwy_read_guint32_le(const guchar **ppv)
{
    const guint32 *pv = (const guint32*)(*ppv);
    guint32 v = GUINT32_FROM_LE(*pv);
    *ppv += sizeof(guint32);
    return v;
}

static inline guint32
gwy_read_guint32_be(const guchar **ppv)
{
    const guint32 *pv = (const guint32*)(*ppv);
    guint32 v = GUINT32_FROM_BE(*pv);
    *ppv += sizeof(guint32);
    return v;
}

static inline gint64
gwy_read_gint64_le(const guchar **ppv)
{
    const gint64 *pv = (const gint64*)(*ppv);
    gint64 v = GINT64_FROM_LE(*pv);
    *ppv += sizeof(gint64);
    return v;
}

static inline gint64
gwy_read_gint64_be(const guchar **ppv)
{
    const gint64 *pv = (const gint64*)(*ppv);
    gint64 v = GINT64_FROM_BE(*pv);
    *ppv += sizeof(gint64);
    return v;
}

static inline guint64
gwy_read_guint64_le(const guchar **ppv)
{
    const guint64 *pv = (const guint64*)(*ppv);
    guint64 v = GUINT64_FROM_LE(*pv);
    *ppv += sizeof(guint64);
    return v;
}

static inline guint64
gwy_read_guint64_be(const guchar **ppv)
{
    const guint64 *pv = (const guint64*)(*ppv);
    guint64 v = GUINT64_FROM_BE(*pv);
    *ppv += sizeof(guint64);
    return v;
}

static inline gfloat
gwy_read_gfloat_le(const guchar **ppv)
{
    union { guint32 i; gfloat f; } v;
    const guint32 *pv = (const guint32*)(*ppv);
    v.i = GUINT32_FROM_BE(*pv);
    *ppv += sizeof(gfloat);
    return v.f;
}

static inline gfloat
gwy_read_gfloat_be(const guchar **ppv)
{
    union { guint32 i; gfloat f; } v;
    const guint32 *pv = (const guint32*)(*ppv);
    v.i = GUINT32_FROM_LE(*pv);
    *ppv += sizeof(gfloat);
    return v.f;
}

static inline gdouble
gwy_read_gdouble_le(const guchar **ppv)
{
    union { guint64 i; gdouble f; } v;
    const guint64 *pv = (const guint64*)(*ppv);
    v.i = GUINT64_FROM_LE(*pv);
    *ppv += sizeof(gdouble);
    return v.f;
}

static inline gdouble
gwy_read_gdouble_be(const guchar **ppv)
{
    union { guint64 i; gdouble f; } v;
    const guint64 *pv = (const guint64*)(*ppv);
    v.i = GUINT64_FROM_BE(*pv);
    *ppv += sizeof(gdouble);
    return v.f;
}

gdouble gwy_read_pascal_real_le(const guchar **ppv);
gdouble gwy_read_pascal_real_be(const guchar **ppv);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
