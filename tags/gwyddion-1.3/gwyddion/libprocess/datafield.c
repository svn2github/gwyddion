/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include <string.h>
#include <stdlib.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "datafield.h"

#define GWY_DATA_FIELD_TYPE_NAME "GwyDataField"

/* Private DataLine functions */
void            _gwy_data_line_initialize        (GwyDataLine *a,
                                                  gint res, gdouble real,
                                                  gboolean nullme);
void            _gwy_data_line_free              (GwyDataLine *a);

static void     gwy_data_field_class_init        (GwyDataFieldClass *klass);
static void     gwy_data_field_init              (GwyDataField *data_field);
static void     gwy_data_field_finalize          (GwyDataField *data_field);
static void     gwy_data_field_serializable_init (GwySerializableIface *iface);
static void     gwy_data_field_watchable_init    (GwyWatchableIface *iface);
static GByteArray* gwy_data_field_serialize      (GObject *obj,
                                                  GByteArray *buffer);
static GObject* gwy_data_field_deserialize       (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
static GObject* gwy_data_field_duplicate         (GObject *object);
/*static void     gwy_data_field_value_changed     (GObject *object);*/

/*local functions*/
static void     gwy_data_field_alloc             (GwyDataField *a,
                                                  gint xres,
                                                  gint yres);
/* exported for other datafield function
 * XXX: this should rather not exist at all, use gwy_data_field_new()...  */
void           _gwy_data_field_initialize        (GwyDataField *a,
                                                  gint xres,
                                                  gint yres,
                                                  gdouble xreal,
                                                  gdouble yreal,
                                                  gboolean nullme);
void           _gwy_data_field_free              (GwyDataField *a);

static gdouble  square_area                      (GwyDataField *data_field,
                                                  gint ulcol, gint ulrow,
                                                  gint brcol, gint brrow);


GType
gwy_data_field_get_type(void)
{
    static GType gwy_data_field_type = 0;

    if (!gwy_data_field_type) {
        static const GTypeInfo gwy_data_field_info = {
            sizeof(GwyDataFieldClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_data_field_class_init,
            NULL,
            NULL,
            sizeof(GwyDataField),
            0,
            (GInstanceInitFunc)gwy_data_field_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_data_field_serializable_init,
            NULL,
            NULL
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_data_field_watchable_init,
            NULL,
            NULL
        };

        gwy_debug("");
        gwy_data_field_type = g_type_register_static(G_TYPE_OBJECT,
                                                   GWY_DATA_FIELD_TYPE_NAME,
                                                   &gwy_data_field_info,
                                                   0);
        g_type_add_interface_static(gwy_data_field_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_data_field_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_data_field_type;
}

static void
gwy_data_field_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->serialize = gwy_data_field_serialize;
    iface->deserialize = gwy_data_field_deserialize;
    iface->duplicate = gwy_data_field_duplicate;
}

static void
gwy_data_field_watchable_init(GwyWatchableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_data_field_class_init(GwyDataFieldClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    gobject_class->finalize = (GObjectFinalizeFunc)gwy_data_field_finalize;
}

static void
gwy_data_field_init(GwyDataField *data_field)
{
    gwy_debug("");
    data_field->data = NULL;
    data_field->xres = 0;
    data_field->yres = 0;
    data_field->xreal = 0.0;
    data_field->yreal = 0.0;
}

static void
gwy_data_field_finalize(GwyDataField *data_field)
{
    gwy_debug("%p is dying!", data_field);
    g_object_unref(data_field->si_unit_xy);
    g_object_unref(data_field->si_unit_z);
    _gwy_data_field_free(data_field);
}

GObject*
gwy_data_field_new(gint xres, gint yres,
                   gdouble xreal, gdouble yreal,
                   gboolean nullme)
{
    GwyDataField *data_field;

    data_field = g_object_new(GWY_TYPE_DATA_FIELD, NULL);

    _gwy_data_field_initialize(data_field, xres, yres, xreal, yreal, nullme);

    return (GObject*)(data_field);
}

static GByteArray*
gwy_data_field_serialize(GObject *obj,
                         GByteArray *buffer)
{
    GwyDataField *data_field;
    gsize datasize;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_DATA_FIELD(obj), NULL);

    data_field = GWY_DATA_FIELD(obj);
    datasize = data_field->xres*data_field->yres;
    {
        GwySerializeSpec spec[] = {
            { 'i', "xres", &data_field->xres, NULL, },
            { 'i', "yres", &data_field->yres, NULL, },
            { 'd', "xreal", &data_field->xreal, NULL, },
            { 'd', "yreal", &data_field->yreal, NULL, },
            { 'o', "si_unit_xy", &data_field->si_unit_xy, NULL, },
            { 'o', "si_unit_z", &data_field->si_unit_z, NULL, },
            { 'D', "data", &data_field->data, &datasize, },
        };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_DATA_FIELD_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_data_field_deserialize(const guchar *buffer,
                           gsize size,
                           gsize *position)
{
    gsize fsize;
    gint xres, yres;
    gdouble xreal, yreal, *data = NULL;
    GwySIUnit *si_unit_xy = NULL, *si_unit_z = NULL;
    GwyDataField *data_field;
    GwySerializeSpec spec[] = {
        { 'i', "xres", &xres, NULL, },
        { 'i', "yres", &yres, NULL, },
        { 'd', "xreal", &xreal, NULL, },
        { 'd', "yreal", &yreal, NULL, },
        { 'o', "si_unit_xy", &si_unit_xy, NULL, },
        { 'o', "si_unit_z", &si_unit_z, NULL, },
        { 'D', "data", &data, &fsize, },
    };

    gwy_debug("");

    si_unit_z = NULL;
    si_unit_xy = NULL;

    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_DATA_FIELD_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(data);
        g_object_unref(si_unit_xy);
        g_object_unref(si_unit_z);
        return NULL;
    }
    if (fsize != (gsize)(xres*yres)) {
        g_critical("Serialized %s size mismatch %u != %u",
              GWY_DATA_FIELD_TYPE_NAME, fsize, xres*yres);
        g_free(data);
        g_object_unref(si_unit_xy);
        g_object_unref(si_unit_z);
        return NULL;
    }

    /* don't allocate large amount of memory just to immediately free it */
    data_field = (GwyDataField*)gwy_data_field_new(1, 1, xreal, yreal, FALSE);
    g_free(data_field->data);
    data_field->data = data;
    data_field->xres = xres;
    data_field->yres = yres;
    if (si_unit_z != NULL) data_field->si_unit_z = si_unit_z;
    if (si_unit_xy != NULL) data_field->si_unit_xy = si_unit_xy;


    return (GObject*)data_field;
}

static GObject*
gwy_data_field_duplicate(GObject *object)
{
    GwyDataField *data_field;
    GObject *duplicate;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(object), NULL);
    data_field = GWY_DATA_FIELD(object);
    duplicate = gwy_data_field_new(data_field->xres, data_field->yres,
                                   data_field->xreal, data_field->yreal,
                                   FALSE);
    gwy_data_field_copy(data_field, GWY_DATA_FIELD(duplicate));

    return duplicate;
}

/*
static void
gwy_data_field_value_changed(GObject *object)
{
    gwy_debug("signal: GwyGwyDataLine changed");
    g_signal_emit_by_name(GWY_DATA_FIELD(object), "value_changed", NULL);
}
*/


/**
 * gwy_data_field_alloc:
 * @a: pointer to data field structure to be allocated.
 * @xres: X resolution
 * @yres: Y resolution
 *
 * Allocates GwyDataField.
 *
 * Does NOT create an object!
 **/
static void
gwy_data_field_alloc(GwyDataField *a, gint xres, gint yres)
{
    gwy_debug("");

    a->xres = xres;
    a->yres = yres;
    a->data = g_new(gdouble, a->xres*a->yres);
    a->si_unit_xy = NULL;
    a->si_unit_z = NULL;
}

/**
 * _gwy_data_field_initialize:
 * @a: A data field structure to be initialized
 * @xres: X resolution
 * @yres: Y resolution
 * @xreal: X real dimension of the field
 * @yreal: Y real dimension of the field
 * @nullme: true if field should be filled with zeros
 *
 * Allocates and initializes GwyDataField.
 *
 * Does NOT create an object!
 **/
void
_gwy_data_field_initialize(GwyDataField *a,
                          gint xres, gint yres,
                          gdouble xreal, gdouble yreal,
                          gboolean nullme)
{
    int i;

    gwy_debug("(%dx%d)", xres, yres);

    gwy_data_field_alloc(a, xres, yres);
    a->xreal = xreal;
    a->yreal = yreal;
    if (nullme) {
        for (i = 0; i < (a->xres*a->yres); i++)
            a->data[i] = 0;
    }
    a->si_unit_xy = (GwySIUnit*)gwy_si_unit_new("m");
    a->si_unit_z = (GwySIUnit*)gwy_si_unit_new("m");
}

void
_gwy_data_field_free(GwyDataField *a)
{
    gwy_debug("");
    g_free(a->data);
}

/**
 * gwy_data_field_copy:
 * @a: source data field.
 * @b: destination data field.
 *
 * Copies the contents of an already allocated data field to a data field
 * of the same size.
 *
 * Returns:
 **/
gboolean
gwy_data_field_copy(GwyDataField *a, GwyDataField *b)
{
    g_return_val_if_fail(a->xres == b->xres && a->yres == b->yres, FALSE);

    b->xreal = a->xreal;
    b->yreal = a->yreal;
    gwy_object_unref(b->si_unit_xy);
    gwy_object_unref(b->si_unit_z);
    b->si_unit_xy = (GwySIUnit *)gwy_serializable_duplicate(G_OBJECT(a->si_unit_xy));
    b->si_unit_z = (GwySIUnit *)gwy_serializable_duplicate(G_OBJECT(a->si_unit_z));

    memcpy(b->data, a->data, a->xres*a->yres*sizeof(gdouble));

    return TRUE;
}

/**
 * gwy_data_field_area_copy:
 * @src: Source data field.
 * @dest: Destination data field.
 * @ulcol: Starting column.
 * @ulrow: Starting row.
 * @brcol: Ending column (noninclusive).
 * @brrow: Ending row (noninclusive).
 * @destcol: Destination column.
 * @destrow: Destination row.
 *
 * Copies a rectangular area from @src to @dest.
 *
 * The area starts at (@ulcol, @ulrow) in @src and ends at (@brcol, @brrow)
 * (noninclusive).  It is copied to @dest starting from (@destcol, @destrow).
 *
 * There must be enough room in the destination data field, areas sticking
 * out are rejected.  If @src is equal to @dest, the areas may not overlap.
 *
 * Returns: Whether it succeeded (area sizes OK).
 **/
gboolean
gwy_data_field_area_copy(GwyDataField *src,
                         GwyDataField *dest,
                         gint ulcol, gint ulrow,
                         gint brcol, gint brrow,
                         gint destcol, gint destrow)
{
    gint i;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(src), FALSE);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dest), FALSE);

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    if (ulcol < 0
        || ulrow < 0
        || brcol > src->xres
        || brrow > src->yres
        || destcol < 0
        || destrow < 0
        || destcol + brcol - ulcol > dest->xres
        || destrow + brrow - ulrow > dest->yres)
        return FALSE;

    dest->si_unit_xy = (GwySIUnit *)gwy_serializable_duplicate(G_OBJECT(src->si_unit_xy));
    dest->si_unit_z = (GwySIUnit *)gwy_serializable_duplicate(G_OBJECT(src->si_unit_z));

    for (i = 0; i < brrow - ulrow; i++)
        memcpy(dest->data + dest->xres*(destrow + i) + destcol,
               src->data + src->xres*(ulrow + i) + ulcol,
               (brcol - ulcol)*sizeof(gdouble));

    return TRUE;
}

/**
 * gwy_data_field_resample:
 * @a: A data field to be resampled
 * @xres: desired X resolution
 * @yres: desired Y resolution
 * @interpolation: interpolation method
 *
 * Resamples GwyDataField using given interpolation method
 **/
void
gwy_data_field_resample(GwyDataField *a,
                        gint xres, gint yres,
                        GwyInterpolationType interpolation)
{
    GwyDataField b;
    gdouble xratio, yratio, xpos, ypos;
    gint i,j;

    if (a->xres == xres && a->yres == yres)
        return;

    if (interpolation != GWY_INTERPOLATION_NONE) {
        gwy_data_field_alloc(&b, a->xres, a->yres);
        gwy_data_field_copy(a, &b);
    }

    a->xres = xres;
    a->yres = yres;
    a->data = g_renew(gdouble, a->data, a->xres*a->yres);

    if (interpolation == GWY_INTERPOLATION_NONE)
        return;

    xratio = (gdouble)(b.xres-1)/(gdouble)(a->xres-1);
    yratio = (gdouble)(b.yres-1)/(gdouble)(a->yres-1);

    for (i = 0; i < a->yres; i++) {
        gdouble *row = a->data + i*a->xres;

        ypos = (gdouble)i*yratio;
        if (ypos > (b.yres-1))
            ypos = (b.yres-1);

        for (j = 0; j < a->xres; j++, row++) {
            xpos = (gdouble)j*xratio;
            if (xpos > (b.xres-1))
                xpos = (b.xres-1);
            *row = gwy_data_field_get_dval(&b, xpos, ypos, interpolation);
        }
    }
    _gwy_data_field_free(&b);
}

void
gwy_data_field_confirmsize(GwyDataField *a, gint xres, gint yres)
{
    if (a->data == NULL)
        _gwy_data_field_initialize(a, xres, yres, xres, yres, FALSE);
    else if (a->xres != xres)
        gwy_data_field_resample(a, xres, yres, GWY_INTERPOLATION_NONE);
}

/**
 * gwy_data_field_resize:
 * @a: A data field to be resized
 * @ulcol: upper-left column coordinate
 * @ulrow: upper-left row coordinate
 * @brcol: bottom-right column coordinate (exclusive)
 * @brrow: bottom-right row coordinate (exclusive)
 *
 * Resizes (crops) the GwyDataField.
 *
 * Extracts part of the GwyDataField.between
 * upper-left and bottom-right points.
 *
 * Returns: %TRUE on success.
 **/
gboolean
gwy_data_field_resize(GwyDataField *a,
                      gint ulcol, gint ulrow, gint brcol, gint brrow)
{
    GwyDataField b;
    gint i, xres, yres;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0
                         && brcol <= a->xres && brrow <= a->yres,
                         FALSE);

    yres = brrow - ulrow;
    xres = brcol - ulcol;
    gwy_data_field_alloc(&b, xres, yres);

    for (i = ulrow; i < brrow; i++) {
        memcpy(b.data + (i-ulrow)*xres,
               a->data + i*a->xres + ulcol,
               xres*sizeof(gdouble));
    }
    a->xres = xres;
    a->yres = yres;
    GWY_SWAP(gdouble*, a->data, b.data);

    _gwy_data_field_free(&b);
    return TRUE;
}

/**
 * gwy_data_field_get_dval:
 * @a: A data field
 * @x: x position
 * @y: y postition
 * @interpolation: interpolation method to be used
 *
 * Interpolates to extract a value of the field in arbitrary position.
 *
 * Returns: value at the position (x,y).
 **/
gdouble
gwy_data_field_get_dval(GwyDataField *a, gdouble x, gdouble y,
                        GwyInterpolationType interpolation)
{
    gint ix, iy, i;
    gint floorx, floory;
    gdouble restx, resty, valpx, valxp, valpp, va, vb, vc, vd;
    gdouble intline[4];

    if (x < 0 && x > -0.1)
        x = 0;
    if (y < 0 && x > -0.1)
        y = 0;

    if (!(x >= 0 && y >= 0 && y < a->yres && x < a->xres))
        g_warning("Bad dval request: %f %f", x, y);
    g_return_val_if_fail(x >= 0 && y >= 0 && y < a->yres && x < a->xres, 0.0);
    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        return 0.0;

        case GWY_INTERPOLATION_ROUND:
        ix = (gint)(x + 0.5);
        iy = (gint)(y + 0.5);
        return a->data[ix + a->xres*iy];

        case GWY_INTERPOLATION_BILINEAR:
        floorx = (gint)floor(x);
        floory = (gint)floor(y);
        restx = x - (gdouble)floorx;
        resty = y - (gdouble)floory;

        if (restx != 0)
            valpx = restx*(1 - resty)*a->data[floorx + 1 + a->xres*floory];
        else
            valpx = 0;

        if (resty != 0)
            valxp = resty*(1 - restx)*a->data[floorx + a->xres*(floory + 1)];
        else
            valxp = 0;

        if (restx != 0 && resty != 0)
            valpp = restx*resty*a->data[floorx + 1 + a->xres*(floory + 1)];
        else
            valpp = 0;

        return valpx + valxp + valpp
               + (1 - restx)*(1 - resty)*a->data[floorx + a->xres*floory];


        default:
        floorx = (gint)floor(x);
        floory = (gint)floor(y);
        restx = x - (gdouble)floorx;
        resty = y - (gdouble)floory;

        /*return ROUND result if we have no space for interpolations*/
        if (floorx < 1 || floory < 1
            || floorx >= (a->xres-2) || floory >= (a->yres-2)) {
            ix = (gint)(x + 0.5);
            iy = (gint)(y + 0.5);
            return a->data[ix + a->xres*iy];
        }

        /*interpolation in x direction*/
        for (i = 0; i < 4; i++)
            intline[i] = a->data[floorx - 1 + i + a->xres * (floory - 1)];
        va = gwy_interpolation_get_dval_of_equidists (restx, intline,
                                                      interpolation);
        for (i = 0; i < 4; i++)
            intline[i] = a->data[floorx - 1 + i + a->xres * (floory)];
        vb = gwy_interpolation_get_dval_of_equidists (restx, intline,
                                                      interpolation);
        for (i = 0; i < 4; i++)
            intline[i] = a->data[floorx - 1 + i + a->xres * (floory + 1)];
        vc = gwy_interpolation_get_dval_of_equidists (restx, intline,
                                                      interpolation);
        for (i = 0; i < 4; i++)
            intline[i] = a->data[floorx - 1 + i + a->xres * (floory + 2)];
        vd = gwy_interpolation_get_dval_of_equidists (restx, intline,
                                                      interpolation);

        /*interpolation in y direction*/
        intline[0] = va;
        intline[1] = vb;
        intline[2] = vc;
        intline[3] = vd;
        return gwy_interpolation_get_dval_of_equidists(resty, intline,
                                                       interpolation);
    }
}


/**
 * gwy_data_field_get_data:
 * @a: A data field
 *
 * Get the data of the field.
 *
 * Returns: The data field as a pointer to an array of
 *          gwy_data_field_get_xres()*gwy_data_field_get_yres() #gdouble's,
 *          ordered by lines.  I.e., they are to be accessed as
 *          data[row*xres + column].
 **/
gdouble*
gwy_data_field_get_data(GwyDataField *a)
{
    return a->data;
}

/**
 * gwy_data_field_get_xres:
 * @a: A data field
 *
 * Get X resolution of the field.
 *
 * Returns:X resolution
 **/
gint
gwy_data_field_get_xres(GwyDataField *a)
{
    return a->xres;
}

/**
 * gwy_data_field_get_yres:
 * @a: A data field
 *
 * Get Y resolution of the field.
 *
 * Returns: Y resolution
 **/
gint
gwy_data_field_get_yres(GwyDataField *a)
{
    return a->yres;
}

/**
 * gwy_data_field_get_xreal:
 * @a: A data field
 *
 * Get the X real size value
 *
 * Returns:X real size value
 **/
gdouble
gwy_data_field_get_xreal(GwyDataField *a)
{
    return a->xreal;
}

/**
 * gwy_data_field_get_yreal:
 * @a: A data field
 *
 * Get the Y real size value
 *
 * Returns: Y real size value
 **/
gdouble
gwy_data_field_get_yreal(GwyDataField *a)
{
    return a->yreal;
}

/**
 * gwy_data_field_set_xreal:
 * @a: A data field
 * @xreal: new X real size value
 *
 * Set the X real size value
 **/
void
gwy_data_field_set_xreal(GwyDataField *a, gdouble xreal)
{
    a->xreal = xreal;
}

/**
 * gwy_data_field_set_yreal:
 * @a: A data field
 * @yreal: new Y real size value
 *
 * Set the Y real size value
 **/
void
gwy_data_field_set_yreal(GwyDataField *a, gdouble yreal)
{
    a->yreal = yreal;
}


/**
 * gwy_data_field_get_si_unit_xy:
 * @a: A data field
 *
 *
 *
 * Returns: SI unit corresponding to the lateral (XY) dimensions of the field
 **/
GwySIUnit*
gwy_data_field_get_si_unit_xy(GwyDataField *a)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(a), NULL);

    gwy_debug("xy unit = <%s>", gwy_si_unit_get_unit_string(a->si_unit_xy));
    return a->si_unit_xy;
}

/**
 * gwy_data_field_get_si_unit_z:
 * @a: A data field
 *
 *
 *
 * Returns: SI unit corresponding to the "height" (Z) dimension of the field
 **/
GwySIUnit*
gwy_data_field_get_si_unit_z(GwyDataField *a)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(a), NULL);

    gwy_debug("z unit = <%s>", gwy_si_unit_get_unit_string(a->si_unit_z));
    return a->si_unit_z;
}

/**
 * gwy_data_field_set_si_unit_xy:
 * @a: A data field
 * @si_unit: SI unit to be set
 *
 * Sets the SI unit corresponding to the lateral (XY) dimensions of the field.
 **/
void
gwy_data_field_set_si_unit_xy(GwyDataField *a, GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(a));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    gwy_object_unref(a->si_unit_xy);
    g_object_ref(si_unit);
    a->si_unit_xy = si_unit;
    gwy_debug("xy unit = <%s>", gwy_si_unit_get_unit_string(a->si_unit_xy));
}

/**
 * gwy_data_field_set_si_unit_z:
 * @a: A data field
 * @si_unit: SI unit to be set
 *
 * Sets the SI unit corresponding to the "height" (Z) dimension of the field.
 **/
void
gwy_data_field_set_si_unit_z(GwyDataField *a, GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(a));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    gwy_object_unref(a->si_unit_z);
    g_object_ref(si_unit);
    a->si_unit_z = si_unit;
    gwy_debug("z unit = <%s>", gwy_si_unit_get_unit_string(a->si_unit_z));
}

/**
 * gwy_data_field_get_value_format_xy:
 * @data_field: A data field.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying coordinates of @data_field.
 *
 * Returns: The value format.  If @format was %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwySIValueFormat*
gwy_data_field_get_value_format_xy(GwyDataField *data_field,
                                   GwySIValueFormat *format)
{
    gdouble max, unit;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);

    max = MAX(data_field->xreal, data_field->yreal);
    unit = MIN(data_field->xreal/data_field->xres,
               data_field->yreal/data_field->yres);
    return gwy_si_unit_get_format_with_resolution(data_field->si_unit_xy,
                                                  max, unit, format);
}

/**
 * gwy_data_field_get_value_format_z:
 * @data_field: A data field.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying values of @data_field.
 *
 * Returns: The value format.  If @format was %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwySIValueFormat*
gwy_data_field_get_value_format_z(GwyDataField *data_field,
                                  GwySIValueFormat *format)
{
    gdouble max, min;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);

    max = fabs(gwy_data_field_get_max(data_field));
    min = fabs(gwy_data_field_get_min(data_field));
    max = MAX(min, max);

    return gwy_si_unit_get_format(data_field->si_unit_z, max, format);
}

/**
 * gwy_data_field_itor:
 * @a: A data field
 * @pixval: value at data (pixel) coordinates
 *
 * recomputes row pixel coordinate to real coordinate
 *
 * Returns: recomputed value
 **/
gdouble
gwy_data_field_itor(GwyDataField *a, gdouble pixval)
{
    return (gdouble)pixval*a->yreal/a->yres;
}

/**
 * gwy_data_field_jtor:
 * @a: A data field
 * @pixval:  value at real coordinates
 *
 * recomputes column real coordinate to pixel coordinate
 *
 * Note: for field represented by square grid (same distance
 * between adjacent pixels in X and Y dimension, the
 * functions gwy_data_field_itor() and gwy_data_field_jtor()
 * are  identical.
 * Returns: recomputed value
 **/
gdouble
gwy_data_field_jtor(GwyDataField *a, gdouble pixval)
{
    return (gdouble)pixval*a->xreal/a->xres;
}


/**
 * gwy_data_field_rtoi:
 * @a: A data field
 * @realval:  value at real coordinates
 *
 * recomputes row real coordinate to pixel coordinate
 *
 * Returns: recomputed value
 **/
gdouble
gwy_data_field_rtoi(GwyDataField *a, gdouble realval)
{
    return realval*a->yres/a->yreal;
}


/**
 * gwy_data_field_rtoj:
 * @a: A data field
 * @realval:  value at real coordinates
 *
 * recomputes column real coordinate to pixel coordinate
 *
 * Returns: recomputed value
 **/
gdouble
gwy_data_field_rtoj(GwyDataField *a, gdouble realval)
{
    return realval*a->xres/a->xreal;
}

gboolean
gwy_data_field_inside(GwyDataField *a, gint i, gint j)
{
    if (i >= 0 && j >= 0 && i < a->xres && j < a->yres)
        return TRUE;
    else
        return FALSE;
}


/**
 * gwy_data_field_get_val:
 * @a: A data field
 * @col: column position
 * @row: row position
 *
 * Get value at given pixel
 *
 * Returns: value at (i, j)
 **/
gdouble
gwy_data_field_get_val(GwyDataField *a, gint col, gint row)
{
    g_return_val_if_fail(gwy_data_field_inside(a, col, row), 0.0);
    return a->data[col + a->xres*row];
}

/**
 * gwy_data_field_set_val:
 * @a: A data field
 * @col: column position
 * @row: row position
 * @value: value to set
 *
 * Set @value at given pixel
 **/
void
gwy_data_field_set_val(GwyDataField *a, gint col, gint row, gdouble value)
{
    g_return_if_fail(gwy_data_field_inside(a, col, row));
    a->data[col + a->xres*row] = value;
}

/**
 * gwy_data_field_get_dval_real:
 * @a: A data field
 * @x: x postion in real coordinates
 * @y: y postition in real coordinates
 * @interpolation: interpolation method
 *
 * Get value at arbitrary point given by real values.
 *
 * See also gwy_data_field_get_dval() that does the same for arbitrary point
 * given by data (pixel) coordinate values.
 *
 * Returns: value at point x, y
 **/
gdouble
gwy_data_field_get_dval_real(GwyDataField *a, gdouble x, gdouble y,
                             GwyInterpolationType interpolation)
{
    return  gwy_data_field_get_dval(a,
                                    gwy_data_field_rtoj(a, x),/*swapped ij*/
                                    gwy_data_field_rtoi(a, y),
                                    interpolation);
}

/**
 * gwy_data_field_rotate:
 * @a: A data field
 * @angle: angle (in degrees)
 * @interpolation: interpolation method
 *
 * Rotates field by a given angle.
 *
 * The values that will be outside of square after rotation will
 * be lost. The new unknown values will be set to field minimum value.
 **/
void
gwy_data_field_rotate(GwyDataField *a, gdouble angle,
                      GwyInterpolationType interpolation)
{
    GwyDataField b;
    gdouble inew, jnew, ir, jr, ang, icor, jcor, sn, cs, val;
    gint i,j;

    angle = fmod(angle, 360.0);
    if (angle < 0.0)
        angle += 360.0;

    if (angle == 0.0)
        return;

    gwy_data_field_alloc(&b, a->xres, a->yres);
    gwy_data_field_copy(a, &b);

    val = gwy_data_field_get_min(a);
    ang = 3*G_PI/4 + angle*G_PI/180;
    sn = sin(angle*G_PI/180);
    cs = cos(angle*G_PI/180);
    icor = (gdouble)a->yres/2
            + G_SQRT2*(gdouble)a->yres/2*sin(ang)
            - sn*(a->xres-a->yres)/2;
    jcor = (gdouble)a->xres/2
           + G_SQRT2*(gdouble)a->xres/2*cos(ang)
           + sn*(a->xres-a->yres)/2;;
    if (angle == 90.0) {
        sn = 1.0;
        cs = 0.0;
        icor = 1.0;
        jcor = 0.0;
    }
    if (angle == 180.0) {
        sn = 0.0;
        cs = -1.0;
        icor = 1.0;
        jcor = a->xres-1;
    }
    if (angle == 270.0) {
        sn = -1.0;
        cs = 0.0;
        icor = a->yres;
        jcor = a->xres-1;
    }

    for (i = 0; i < a->yres; i++) { /*row*/
        for (j = 0; j < a->xres; j++) { /*column*/
            ir = a->yres-i-icor;
            jr = j-jcor;
            inew = -ir*cs + jr*sn;
            jnew = ir*sn + jr*cs;
            if (inew > a->yres || jnew > a->xres || inew < -1 || jnew < -1)
                a->data[j + a->xres*i] = val;
            else {
                if (inew > (a->yres - 1))
                    inew = a->yres-1;
                if (jnew > (a->xres - 1))
                    jnew = a->xres-1;
                if (inew < 0) inew = 0;
                if (jnew < 0) jnew = 0;
                a->data[j + a->xres*i] = gwy_data_field_get_dval(&b, jnew, inew,
                                                                 interpolation);
            }
        }
    }

    _gwy_data_field_free(&b);
}


/**
 * gwy_data_field_invert:
 * @a: pointer fo field
 * @x: invert in X direction?
 * @y: invert in Y direction?
 * @z: invert in Z direction?
 *
 * Make requested inversion(s).
 **/
void
gwy_data_field_invert(GwyDataField *a,
                      gboolean x,
                      gboolean y,
                      gboolean z)
{
    gint i,j;
    gdouble avg;
    gdouble *line, *ap, *ap2;
    gsize linelen;
    gdouble *data;

    g_return_if_fail(GWY_IS_DATA_FIELD(a));
    data = a->data;

    if (z) {
        avg = gwy_data_field_get_avg(a);
        ap = data;
        for (i = a->yres*a->xres; i; i--) {
            *ap = 2*avg - *ap;
            ap++;
        }
    }

    if (!x && !y)
        return;

    line = g_new(gdouble, a->xres);
    linelen = a->xres*sizeof(gdouble);
    if (y) {
        for (i = 0; i < a->yres; i++) {
            ap = data + i*a->xres;
            memcpy(line, ap, linelen);
            for (j = 0; j < a->xres; j++)
                ap[j] = line[a->xres-j-1];
        }
    }
    if (x) {
        /* What is lesser evil?
         * allocating one extra datafield or doing 50% extra memcpy()s */
        for (i = 0; i < a->yres/2; i++) {
            ap = data + i*a->xres;
            ap2 = data + (a->yres-i-1)*a->xres;
            memcpy(line, ap, linelen);
            memcpy(ap, ap2, linelen);
            memcpy(ap2, line, linelen);
        }
    }
    g_free(line);
}

/**
 * gwy_data_field_fill:
 * @a: A data field
 * @value: value to be entered
 *
 * Fill GwyDataField with given value
 **/
void
gwy_data_field_fill(GwyDataField *a, gdouble value)
{
    gint i;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++)
        *p = value;
}

/**
 * gwy_data_field_area_fill:
 * @a: A data field
 * @ulcol: upper-left column coordinate
 * @ulrow: upper-left row coordinate
 * @brcol: bottom-right column coordinate + 1
 * @brrow: bottom-right row coordinate + 1
 * @value: value to be entered
 *
 * Fill a specified part of the field witha given  value
 **/
void
gwy_data_field_area_fill(GwyDataField *a,
                         gint ulcol, gint ulrow, gint brcol, gint brrow,
                         gdouble value)
{
    gint i, j;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_if_fail(ulcol >= 0 && ulrow >= 0
                     && brcol <= a->xres && brrow <= a->yres);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            *(row++) = value;
    }
}

/**
 * gwy_data_field_multiply:
 * @a: A data field
 * @value: value to be used for multiplication
 *
 * Multiply GwyDataField by given value.
 **/
void
gwy_data_field_multiply(GwyDataField *a, gdouble value)
{
    gint i;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++)
        *p *= value;
}

/**
 * gwy_data_field_area_multiply:
 * @a: A data field
 * @ulcol: upper-left column coordinate
 * @ulrow: upper-left row coordinate
 * @brcol: bottom-right column coordinate + 1
 * @brrow: bottom-right row coordinate + 1
 * @value: value to be used
 *
 * Multiply a specified part of the field by the given value
 **/
void
gwy_data_field_area_multiply(GwyDataField *a,
                             gint ulcol, gint ulrow, gint brcol, gint brrow,
                             gdouble value)
{
    gint i, j;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_if_fail(ulcol >= 0 && ulrow >= 0
                     && brcol <= a->xres && brrow <= a->yres);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            *(row++) *= value;
    }
}

/**
 * gwy_data_field_add:
 * @a: A data field
 * @value: value to be added
 *
 * Add given value to GwyDataField
 **/
void
gwy_data_field_add(GwyDataField *a, gdouble value)
{
    gint i;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++)
        *p += value;
}

/**
 * gwy_data_field_area_add:
 * @a: A data field
 * @ulcol: upper-left column coordinate
 * @ulrow: upper-left row coordinate
 * @brcol: bottom-right column coordinate + 1
 * @brrow: bottom-right row coordinate + 1
 * @value: value to be used
 *
 * Add the given value to a specified part of the field
 **/
void
gwy_data_field_area_add(GwyDataField *a,
                        gint ulcol, gint ulrow, gint brcol, gint brrow,
                        gdouble value)
{
    gint i, j;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_if_fail(ulcol >= 0 && ulrow >= 0
                     && brcol <= a->xres && brrow <= a->yres);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            *(row++) += value;
    }
}

/**
 * gwy_data_field_get_max:
 * @a: A data field
 *
 * Get maximum value of the GwyDataField.
 *
 * Returns:maximum value of the GwyDataField
 **/
gdouble
gwy_data_field_get_max(GwyDataField *a)
{
    gint i;
    gdouble max = a->data[0];
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++) {
        if (max < *p)
            max = *p;
    }
    return max;
}


/**
 * gwy_data_field_area_get_max:
 * @dfield: A data field
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Finds maximum value in a rectangular part of a data field.
 *
 * Returns: The maximum value, and undefined value (currently -%G_MAXDOUBLE)
 *          for zero @width or @height.
 *
 * Since: 1.2:
 **/
gdouble
gwy_data_field_area_get_max(GwyDataField *dfield,
                            gint col, gint row, gint width, gint height)
{
    gint i, j;
    gdouble max = -G_MAXDOUBLE;
    gdouble *datapos;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), max);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         max);
    if (!width || !height)
        return max;

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++) {
            if (max < *drow)
                max = *drow;
            drow++;
        }
    }

    return max;
}

gdouble
gwy_data_field_get_area_max(GwyDataField *a,
                            gint ulcol,
                            gint ulrow,
                            gint brcol,
                            gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    return gwy_data_field_area_get_max(a, ulcol, ulrow,
                                       brcol - ulcol, brrow - ulrow);
}

/**
 * gwy_data_field_get_min:
 * @a: A data field
 *
 * Get minimum value of the GwyDataField
 *
 * Returns: minimum value of the GwyDataField
 **/
gdouble
gwy_data_field_get_min(GwyDataField *a)
{
    gint i;
    gdouble min = a->data[0];
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++) {
        if (min > *p)
            min = *p;
    }
    return min;
}


/**
 * gwy_data_field_area_get_min:
 * @dfield: A data field
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Finds minimum value in a rectangular part of a data field.
 *
 * Returns: The minimum value, and undefined value (currently %G_MAXDOUBLE)
 *          for zero @width or @height.
 *
 * Since 1.2.
 **/
gdouble
gwy_data_field_area_get_min(GwyDataField *dfield,
                            gint col, gint row, gint width, gint height)
{
    gint i, j;
    gdouble min = G_MAXDOUBLE;
    gdouble *datapos;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), min);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         min);
    if (!width || !height)
        return min;

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++) {
            if (min > *drow)
                min = *drow;
            drow++;
        }
    }

    return min;
}

gdouble
gwy_data_field_get_area_min(GwyDataField *a,
                            gint ulcol,
                            gint ulrow,
                            gint brcol,
                            gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    return gwy_data_field_area_get_min(a, ulcol, ulrow,
                                       brcol-ulcol, brrow-ulrow);
}

/**
 * gwy_data_field_get_sum:
 * @a: A data field
 *
 * Sum all the values in GwyDataField
 *
 * Returns: sum of GwyDataField.
 **/
gdouble
gwy_data_field_get_sum(GwyDataField *a)
{
    gint i;
    gdouble sum = 0;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++)
        sum += *p;

    return sum;
}

/**
 * gwy_data_field_area_get_sum:
 * @dfield: A data field
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Sums values of a rectangular part of a data field.
 *
 * Returns: The value sum.
 *
 * Since 1.2.
 **/
gdouble
gwy_data_field_area_get_sum(GwyDataField *dfield,
                            gint col, gint row, gint width, gint height)
{
    gint i, j;
    gdouble sum = 0;
    gdouble *datapos;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), sum);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         sum);

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++)
            sum += *(drow++);
    }

    return sum;
}

gdouble
gwy_data_field_get_area_sum(GwyDataField *a,
                            gint ulcol,
                            gint ulrow,
                            gint brcol,
                            gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    return gwy_data_field_area_get_sum(a, ulcol, ulrow,
                                       brcol-ulcol, brrow-ulrow);
}

/**
 * gwy_data_field_get_avg:
 * @a: A data field
 *
 * Averages values of GwyDataField
 *
 * Returns: Average value of GwyDataField
 **/
gdouble
gwy_data_field_get_avg(GwyDataField *a)
{
    return gwy_data_field_get_sum(a)/((gdouble)(a->xres * a->yres));
}

/**
 * gwy_data_field_area_get_avg:
 * @dfield: A data field
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Averages values of a rectangular part of a data field.
 *
 * Returns: The average value.
 *
 * Since 1.2.
 **/
gdouble
gwy_data_field_area_get_avg(GwyDataField *dfield,
                            gint col, gint row, gint width, gint height)
{
    return gwy_data_field_area_get_sum(dfield, col, row,
                                       width, height)/(width*height);
}

gdouble
gwy_data_field_get_area_avg(GwyDataField *a,
                            gint ulcol,
                            gint ulrow,
                            gint brcol,
                            gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    return gwy_data_field_area_get_avg(a, ulcol, ulrow,
                                       brcol-ulcol, brrow-ulrow);
}

/**
 * gwy_data_field_get_surface_area:
 * @a: data field
 * @interpolation: interpolation method
 *
 * Computes surface area.
 *
 * Returns: surface area
 **/
gdouble
gwy_data_field_get_surface_area(GwyDataField *a,
                                G_GNUC_UNUSED GwyInterpolationType interpolation)
{
    gint i, j;
    gdouble sum = 0;

    for (i = 0; i < (a->xres - 1); i++) {
        for (j = 0; j < (a->yres - 1); j++)
            sum += square_area(a, i, j, i + 1, j + 1);
    }
    return sum;
}

/**
 * gwy_data_field_area_get_surface_area:
 * @dfield: A data field
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @interpolation: Interpolation method.
 *
 * Computes surface area of a rectangular part of a data field.
 *
 * Returns: The surface area.
 *
 * Since 1.2.
 **/
gdouble
gwy_data_field_area_get_surface_area(GwyDataField *dfield,
                                     gint col, gint row,
                                     gint width, gint height,
                                     G_GNUC_UNUSED GwyInterpolationType interpolation)
{
    gint i, j;
    gdouble sum = 0.0;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), sum);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         sum);
    if (!width || !height)
        return sum;

    for (i = 0; i < height-1; i++) {
        for (j = 0; j < width-1; j++)
            sum += square_area(dfield, col+j, row+i, col+j+1, row+i+1);
    }

    return sum;
}

gdouble
gwy_data_field_get_area_surface_area(GwyDataField *a,
                                     gint ulcol,
                                     gint ulrow,
                                     gint brcol,
                                     gint brrow,
                                     GwyInterpolationType interpolation)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    return gwy_data_field_area_get_surface_area(a, ulcol, ulrow,
                                                brcol-ulcol, brrow-ulrow,
                                                interpolation);
}

/**
 * gwy_data_field_get_rms:
 * @a: A data field
 *
 * Evaluates Root mean square value of GwyDataField
 *
 * Returns: RMS of GwyDataField
 **/

gdouble
gwy_data_field_get_rms(GwyDataField *a)
{
    gint i, n;
    gdouble rms, sum2 = 0;
    gdouble sum = gwy_data_field_get_sum(a);
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++)
        sum2 += (*p)*(*p);

    n = a->xres * a->yres;
    rms = sqrt(fabs(sum2 - sum*sum/n)/n);

    return rms;
}


/**
 * gwy_data_field_area_get_rms:
 * @dfield: A data field
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes root mean square value of a rectangular part of a data field.
 *
 * Returns: The root mean square value.
 *
 * Since: 1.2.
 **/
gdouble
gwy_data_field_area_get_rms(GwyDataField *dfield,
                            gint col, gint row, gint width, gint height)
{
    gint i, j, n;
    gdouble rms = 0.0, sum2 = 0.0;
    gdouble sum;
    gdouble *datapos;

    if (width == 0 || height == 0)
        return rms;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), rms);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         rms);
    if (!width || !height)
        return rms;

    sum = gwy_data_field_area_get_sum(dfield, col, row, width, height);
    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++) {
            sum2 += (*drow) * (*drow);
            *drow++;
        }
    }

    n = width*height;
    rms = sqrt(fabs(sum2 - sum*sum/n)/n);

    return rms;
}

gdouble
gwy_data_field_get_area_rms(GwyDataField *a,
                            gint ulcol,
                            gint ulrow,
                            gint brcol,
                            gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    return gwy_data_field_area_get_rms(a, ulcol, ulrow,
                                       brcol-ulcol, brrow-ulrow);
}


/**
 * gwy_data_field_threshold:
 * @a: A data field
 * @threshval: threshold value
 * @bottom: lower value
 * @top: upper value
 *
 * Tresholds values of GwyDataField. Values
 * smaller than @threshold will be set to value
 * @bottom, values higher than @threshold will be set to value @top
 *
 * Returns: total number of values above threshold.
 **/
gint
gwy_data_field_threshold(GwyDataField *a,
                         gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, tot = 0;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++) {
        if (*p < threshval)
            *p = bottom;
        else {
            *p = top;
            tot++;
        }
    }

    return tot;
}


gint
gwy_data_field_area_threshold(GwyDataField *a,
                              gint ulcol, gint ulrow, gint brcol, gint brrow,
                              gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, j, tot = 0;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0
                         && brcol <= a->xres && brrow <= a->yres, 0);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
        {
            if (*row < threshval)
                *row = bottom;
            else {
                *row = top;
                tot++;
            }
        }
    }

    return tot;
}



/**
 * gwy_data_field_clamp:
 * @a: A data field
 * @bottom: Lower limit value.
 * @top: Upper limit value.
 *
 * Limits data field values to the range [@bottom, @top].
 *
 * Returns: The number of changed values.
 **/
gint
gwy_data_field_clamp(GwyDataField *a,
                     gdouble bottom, gdouble top)
{
    gint i, tot = 0;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++) {
        if (*p < bottom) {
            *p = bottom;
            tot++;
        }
        else if (*p > top) {
            *p = top;
            tot++;
        }
    }

    return tot;
}


/**
 * gwy_data_field_area_clamp:
 * @a: A data field
 * @ulcol: Upper-left column coordinate (inclusive).
 * @ulrow: Upper-left row coordinate (inclusive).
 * @brcol: Bottom-right column coordinate (exclusive).
 * @brrow: Bottom-right row coordinate (exclusive).
 * @bottom: Lower limit value.
 * @top: Upper limit value.
 *
 * Limits values in a rectangular part of a data field to the range
 * [@bottom, @top].
 *
 * Returns: The number of changed values.
 **/
gint
gwy_data_field_area_clamp(GwyDataField *a,
                          gint ulcol,
                          gint ulrow,
                          gint brcol,
                          gint brrow,
                          gdouble bottom,
                          gdouble top)
{
    gint i, j, tot = 0;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0
                         && brcol < a->xres && brrow < a->yres, 0);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++) {
            if (*row < bottom) {
                *row = bottom;
                tot++;
            }
            else if (*row > top) {
                *row = top;
                tot++;
            }
        }

    }
    return tot;
}


/**
 * gwy_data_field_get_row:
 * @a: A data field
 * @b: A data line
 * @row: index of row
 *
 * Extracts row into data line. Data line should be allocated allready.
 **/
void
gwy_data_field_get_row(GwyDataField *a, GwyDataLine* b, gint row)
{
    g_return_if_fail(row >= 0 && row < a->yres);

    gwy_data_line_resample(b, a->xres, GWY_INTERPOLATION_NONE);
    memcpy(b->data, a->data + row*a->xres, a->xres*sizeof(gdouble));
}


/**
 * gwy_data_field_get_column:
 * @a: A data field
 * @b: A data line
 * @col: index of column
 *
 *  Extracts column into data line. Data line should be allocated allready.
 **/
void
gwy_data_field_get_column(GwyDataField *a, GwyDataLine* b, gint col)
{
    gint k;
    gdouble *p;

    g_return_if_fail(col >= 0 && col < a->xres);

    gwy_data_line_resample(b, a->yres, GWY_INTERPOLATION_NONE);
    p = a->data + col;
    for (k = 0; k < a->yres; k++)
        b->data[k] = p[k*a->xres];
}

/**
 * gwy_data_field_get_row_part:
 * @a: A data field
 * @b: A data line
 * @row: index of row
 * @from: beginning index
 * @to: end index
 *
 * Extracts row part into data line. Data line should be allocated allready.
 **/
void
gwy_data_field_get_row_part(GwyDataField *a,
                            GwyDataLine* b,
                            gint row,
                            gint from,
                            gint to)
{
    g_return_if_fail(row >= 0 && row < a->yres);
    if (to < from)
        GWY_SWAP(gint, from, to);

    if (b->res != (to-from))
        gwy_data_line_resample(b, to-from, GWY_INTERPOLATION_NONE);

    memcpy(b->data, a->data + row*a->xres + from, (to-from)*sizeof(gdouble));
}


/**
 * gwy_data_field_get_column_part:
 * @a: A data field
 * @b: A data line
 * @col: index of column
 * @from: beginning index
 * @to: end index
 *
 * Extracts column part into data line. Data line should be allocated allready.
 **/
void
gwy_data_field_get_column_part(GwyDataField *a,
                               GwyDataLine* b,
                               gint col,
                               gint from,
                               gint to)
{
    gint k;

    g_return_if_fail(col >= 0 && col < a->xres);
    if (to < from)
        GWY_SWAP(gint, from, to);

    if (b->res != (to-from))
        gwy_data_line_resample(b, to-from, GWY_INTERPOLATION_NONE);

    for (k = 0; k < to-from; k++)
        b->data[k]=a->data[(k+from)*a->xres + col];
}

/**
 * gwy_data_field_set_row:
 * @a: A data field
 * @b: A data line
 * @row: index of row
 *
 * Sets the row in the data field to values of data line.
 **/
void
gwy_data_field_set_row(GwyDataField *a, GwyDataLine* b, gint row)
{
    g_return_if_fail(row >= 0 && row < a->yres);
    g_return_if_fail(a->xres == b->res);

    memcpy(a->data + row*a->xres, b->data, a->xres*sizeof(gdouble));
}


/**
 * gwy_data_field_set_column:
 * @a: A data field
 * @b: A data line
 * @col: index of column
 *
 * Sets the column in the data field to values of data line.
 **/
void
gwy_data_field_set_column(GwyDataField *a, GwyDataLine* b, gint col)
{
    gint k;
    gdouble *p;

    g_return_if_fail(col >= 0 && col < a->xres);
    g_return_if_fail(a->yres == b->res);

    p = a->data + col;
    for (k = 0; k < a->yres; k++)
        p[k*a->xres] = b->data[k];
}

/**
 * gwy_data_field_get_data_line:
 * @a: A data field
 * @b: A data line
 * @ulcol: upper-left column coordinate
 * @ulrow: upper-left row coordinate
 * @brcol: bottom-right column coordinate + 1
 * @brrow: bottom-right row coordinate + 1
 * @res: requested resolution of data line
 * @interpolation: interpolation type
 *
 * Extracts a profile from data field and
 * puts it into data line. It is expected that the data
 * line is allready allocated.
 *
 * Returns: true at success
 **/
gboolean
gwy_data_field_get_data_line(GwyDataField *a, GwyDataLine* b,
                             gint ulcol, gint ulrow, gint brcol, gint brrow,
                             gint res, GwyInterpolationType interpolation)
{
    gint k;
    gdouble cosa, sina, size;

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0
                         && brcol >= 0 && brrow >= 0
                         && ulrow <= a->yres && ulcol <= a->xres
                         && brrow <= a->yres && brcol <= a->xres,
                         FALSE);

 /*   brcol -= 1;
    brrow -= 1;
*/
    size = sqrt((ulcol - brcol)*(ulcol - brcol)
                + (ulrow - brrow)*(ulrow - brrow));
    if (res <= 0)
        res = (gint)size;

    cosa = (gdouble)(brcol - ulcol)/(res - 1);
    sina = (gdouble)(brrow - ulrow)/(res - 1);

    gwy_data_line_resample(b, res, GWY_INTERPOLATION_NONE);
    for (k = 0; k < res; k++)
        b->data[k] = gwy_data_field_get_dval(a, ulcol + k*cosa, ulrow + k*sina,
                                             interpolation);

    b->real = size*a->xreal/a->xres;

    return TRUE;
}

/**
 * gwy_data_field_get_data_line_averaged:
 * @a: A data field
 * @b: A data line
 * @ulcol: upper-left column coordinate
 * @ulrow: upper-left row coordinate
 * @brcol: bottom-right column coordinate + 1
 * @brrow: bottom-right row coordinate + 1
 * @res: requested resolution of data line
 * @thickness: thickness of line to be averaged
 * @interpolation: interpolation type
 *
 * Extracts an averaged  profile from data field and
 * puts it into data line. It is expected that the data
 * line is allready allocated.
 *
 * Returns: true at success
 * Since 1.2 
 **/
gboolean
gwy_data_field_get_data_line_averaged(GwyDataField *a, GwyDataLine* b,
                             gint ulcol, gint ulrow, gint brcol, gint brrow,
                             gint res, gint thickness, GwyInterpolationType interpolation)
{
    gint k, j;
    gdouble cosa, sina, size, mid, sum;
    gdouble col, row, srcol, srrow;

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0
                         && brcol >= 0 && brrow >= 0
                         && ulrow <= a->yres && ulcol <= a->xres
                         && brrow <= a->yres && brcol <= a->xres,
                         FALSE);

    size = sqrt((ulcol - brcol)*(ulcol - brcol)
                + (ulrow - brrow)*(ulrow - brrow));
    if (res <= 0)
        res = (gint)size;

    cosa = (gdouble)(brcol - ulcol)/(res - 1);
    sina = (gdouble)(brrow - ulrow)/(res - 1);

    /*extract regular one-pixel line*/
    gwy_data_line_resample(b, res, GWY_INTERPOLATION_NONE);
    for (k = 0; k < res; k++)
        b->data[k] = gwy_data_field_get_dval(a, ulcol + k*cosa, ulrow + k*sina,
                                             interpolation);
    b->real = size*a->xreal/a->xres;

    if (thickness <= 1) return TRUE;
    
    /*add neighbour values to the line*/
    for (k = 0; k < res; k++)
    {
        mid = b->data[k];
        sum = 0;
        for (j=(-thickness/2); j<(thickness - thickness/2); j++)
        {
            srcol = ulcol + k*cosa;
            srrow = ulrow + k*sina;
            col = (srcol + j*sina);
            row = (srrow + j*cosa);
            if (col >= 0 && col < (a->xres-1) && row >= 0 && row < (a->yres-1))
            {
                sum += gwy_data_field_get_dval(a, col, row, interpolation);
            }
            else
            {
                sum += mid;
            }
        }
        b->data[k] = sum/(gdouble)thickness;
    }

    return TRUE;
}

/**
 * gwy_data_field_plane_coeffs:
 * @a: A data field
 * @ap: coefficient
 * @bp: coefficient
 * @cp: coefficient
 *
 * Evaluates coefficients of plane fit of data field.
 **/
void
gwy_data_field_plane_coeffs(GwyDataField *a,
                            gdouble *ap, gdouble *bp, gdouble *cp)
{
    gdouble sumxi, sumxixi, sumyi, sumyiyi;
    gdouble sumsi = 0.0;
    gdouble sumsixi = 0.0;
    gdouble sumsiyi = 0.0;
    gdouble nx = a->xres;
    gdouble ny = a->yres;
    gdouble bx, by;
    gdouble *pdata;
    gint i;

    g_return_if_fail(GWY_IS_DATA_FIELD(a));

    sumxi = (nx-1)/2;
    sumxixi = (2*nx-1)*(nx-1)/6;
    sumyi = (ny-1)/2;
    sumyiyi = (2*ny-1)*(ny-1)/6;

    pdata = a->data;
    for (i = 0; i < a->xres*a->yres; i++) {
        sumsi += *pdata;
        sumsixi += *pdata * (i%a->xres);
        sumsiyi += *pdata * (i/a->xres);
        *pdata++;
    }
    sumsi /= nx*ny;
    sumsixi /= nx*ny;
    sumsiyi /= nx*ny;

    bx = (sumsixi - sumsi*sumxi) / (sumxixi - sumxi*sumxi);
    by = (sumsiyi - sumsi*sumyi) / (sumyiyi - sumyi*sumyi);
    if (bp)
        *bp = bx*nx/a->xreal;
    if (cp)
        *cp = by*ny/a->yreal;
    if (ap)
        *ap = sumsi - bx*sumxi - by*sumyi;
}

/**
 * gwy_data_field_area_fit_plane:
 * @dfield: A data field
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @pa: Where constant coefficient should be stored (or %NULL).
 * @pbx: Where x plane coefficient should be stored (or %NULL).
 * @pby: Where y plane coefficient should be stored (or %NULL).
 *
 * Fits a plane through a rectangular part of a data field.
 *
 * Since: 1.2.
 **/
void
gwy_data_field_area_fit_plane(GwyDataField *dfield,
                              gint col, gint row, gint width, gint height,
                              gdouble *pa, gdouble *pbx, gdouble *pby)
{
    gdouble sumxi, sumxixi, sumyi, sumyiyi;
    gdouble sumsi = 0.0;
    gdouble sumsixi = 0.0;
    gdouble sumsiyi = 0.0;
    gdouble a, bx, by;
    gdouble *datapos;
    gint i, j;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= dfield->xres
                     && row + height <= dfield->yres);

    /* try to return something reasonable even in degenerate cases */
    if (!width || !height)
        a = bx = by = 0.0;
    else if (height == 1 && width == 1) {
        a = dfield->data[row*dfield->xres + col];
        bx = by = 0.0;
    }
    else {
        sumxi = (width - 1.0)/2;
        sumyi = (height - 1.0)/2;
        sumxixi = (2.0*width - 1.0)*(width - 1.0)/6;
        sumyiyi = (2.0*height - 1.0)*(height - 1.0)/6;

        datapos = dfield->data + row*dfield->xres + col;
        for (i = 0; i < height; i++) {
            gdouble *drow = datapos + i*dfield->xres;

            for (j = 0; j < width; j++) {
                sumsi += drow[j];
                sumsixi += drow[j]*j;
                sumsiyi += drow[j]*i;
            }
        }
        sumsi /= width*height;
        sumsixi /= width*height;
        sumsiyi /= width*height;

        if (width == 1)
            bx = 0.0;
        else
            bx = (sumsixi - sumsi*sumxi) / (sumxixi - sumxi*sumxi);

        if (height == 1)
            by = 0.0;
        else
            by = (sumsiyi - sumsi*sumyi) / (sumyiyi - sumyi*sumyi);

        a = sumsi - bx*sumxi - by*sumyi;
        bx *= width/dfield->xreal;
        by *= height/dfield->yreal;
    }

    if (pa)
        *pa = a;
    if (pbx)
        *pbx = bx;
    if (pby)
        *pby = by;
}

/**
 * gwy_data_field_plane_level:
 * @a: A data field
 * @ap: coefficient
 * @bp: coefficient
 * @cp: coefficient
 *
 * Plane leveling.
 **/
void
gwy_data_field_plane_level(GwyDataField *a, gdouble ap, gdouble bp, gdouble cp)
{
    gint i, j;
    gdouble bpix = bp/a->xres*a->xreal;
    gdouble cpix = cp/a->yres*a->yreal;

    for (i = 0; i < a->yres; i++) {
        gdouble *row = a->data + i*a->xres;
        gdouble rb = ap + cpix*i;

        for (j = 0; j < a->xres; j++, row++)
            *row -= rb + bpix*j;
    }
}

/**
 * gwy_data_field_plane_rotate:
 * @a: A data field
 * @xangle: rotation angle in x direction (rotation along y axis)
 * @yangle: rotation angle in y direction (rotation along x axis)
 * @interpolation: interpolation type
 *
 * Performs rotation of plane along x and y axis.
 **/
void
gwy_data_field_plane_rotate(GwyDataField *a, gdouble xangle, gdouble yangle,
                            GwyInterpolationType interpolation)
{
    int k;
    GwyDataLine l;

    if (xangle != 0) {
        _gwy_data_line_initialize(&l, a->xres, a->xreal, 0);
        for (k = 0; k < a->yres; k++) {
            gwy_data_field_get_row(a, &l, k);
            gwy_data_line_line_rotate(&l, -xangle, interpolation);
            gwy_data_field_set_row(a, &l, k);
        }
        _gwy_data_line_free(&l);
    }


    if (yangle != 0) {
        _gwy_data_line_initialize(&l, a->yres, a->yreal, 0);
        for (k = 0; k < a->xres; k++) {
            gwy_data_field_get_column(a, &l, k);
            gwy_data_line_line_rotate(&l, -yangle, interpolation);
            gwy_data_field_set_column(a, &l, k);
        }
        _gwy_data_line_free(&l);
    }
}


/**
 * gwy_data_field_get_xder:
 * @a: A data field
 * @col: column coordinate
 * @row: row coordinate
 *
 * Computes derivative in x-direction.
 *
 * Returns: Derivative in x-direction
 **/
gdouble
gwy_data_field_get_xder(GwyDataField *a, gint col, gint row)
{
    gdouble *p;

    g_return_val_if_fail(gwy_data_field_inside(a, col, row), 0.0);

    p = a->data + row*a->xres + col;
    if (col == 0)
        return (*(p+1) - *p) * a->xres/a->xreal;
    if (col == a->xres-1)
        return (*p - *(p-1)) * a->xres/a->xreal;
    return (*(p+1) - *(p-1)) * a->xres/a->xreal/2;
}

/**
 * gwy_data_field_get_yder:
 * @a: A data field
 * @col: column coordinate
 * @row: row coordinate
 *
 *  Computes derivative in y-direction.
 *
 * Returns: Derivative in y-direction
 **/
gdouble
gwy_data_field_get_yder(GwyDataField *a, gint col, gint row)
{
    gdouble *p;
    gint xres;

    g_return_val_if_fail(gwy_data_field_inside(a, col, row), 0.0);

    xres = a->xres;
    p = a->data + row*xres + col;
    if (row == 0)
        return (*p - *(p+xres)) * a->yres/a->yreal;
    if (row == a->yres-1)
        return ( *(p-xres) - *p) * a->yres/a->yreal;
    return (*(p-xres) - *(p+xres)) * a->yres/a->yreal/2;
}

/**
 * gwy_data_field_get_angder:
 * @a: A data field
 * @col: column coordinate
 * @row: row coordinate
 * @theta: angle specifying direction
 *
 * Computes derivative in direction specified by given angle.
 * Angle is given in degrees.
 *
 * Returns: Derivative in direction given by angle @theta
 **/
gdouble
gwy_data_field_get_angder(GwyDataField *a, gint col, gint row, gdouble theta)
{
    g_return_val_if_fail(gwy_data_field_inside(a, col, row), 0.0);
    return gwy_data_field_get_xder(a, col, row)*cos(theta*G_PI/180)
           + gwy_data_field_get_yder(a, col, row)*sin(theta*G_PI/180);
}

/**
 * gwy_data_field_shade:
 * @data_field: A data field
 * @target_field: A shaded data field
 * @theta: shading angle
 * @phi: shading angle
 *
 * Creates a shaded data field. Target field should
 * be allready allocated.
 **/
void
gwy_data_field_shade(GwyDataField *data_field, GwyDataField *target_field,
                                                    gdouble theta, gdouble phi)
{
    gint i, j;
    gdouble max, maxval;

    gwy_data_field_resample(target_field, data_field->xres, data_field->yres, GWY_INTERPOLATION_NONE);

    max = -G_MAXDOUBLE;
    for (i = 0; i < data_field->yres; i++)
    {

        for (j = 0; j < data_field->xres; j++)
        {
            target_field->data[j + data_field->xres*i] = - gwy_data_field_get_angder(data_field, j, i, phi);

            if (max < target_field->data[j + data_field->xres*i]) max = target_field->data[j + data_field->xres*i];
        }
    }

    maxval = G_PI*theta/180.0*max;
    for (i = 0; i < data_field->xres*data_field->yres; i++) target_field->data[i] = max-fabs(maxval-target_field->data[i]);
}


/**
 * gwy_data_field_get_stats:
 * @data_field: A data field
 * @avg: average height value of the surface
 * @ra: average value of the irregularities
 * @rms: root mean square value of the irregularities (Rq)
 * @skew: skew (symmetry of height distribution)
 * @kurtosis: kurtosis (peakedness of height ditribution)
 *
 * Computes basic statistical quantities
 * in one interation over the datafield.
 **/
void
gwy_data_field_get_stats(GwyDataField *data_field,
                         gdouble *avg,
                         gdouble *ra,
                         gdouble *rms,
                         gdouble *skew,
                         gdouble *kurtosis)
{
    gint i;
    gdouble c_sz1, c_sz2, c_sz3, c_sz4, c_abs1;
    gdouble *p = data_field->data;
    gdouble nn = data_field->xres * data_field->yres;
    gdouble dif;

    c_sz1 = c_sz2 = c_sz3 = c_sz4 = c_abs1 = 0;

    *avg = gwy_data_field_get_avg(data_field);

    for (i = nn; i; i--, p++)
    {
        dif = (*p - *avg);
        c_abs1 += fabs(dif);
        c_sz1 += dif;
        c_sz2 += dif*dif;
        c_sz3 += dif*dif*dif;
        c_sz4 += dif*dif*dif*dif;

    }

    *ra = c_abs1/nn;
    *rms = c_sz2/nn;
    *skew = c_sz3/pow(*rms, 1.5)/nn;
    //*(c_sz3/nn - 3*c_sz1*c_sz3/nn2 + 2*c_s3z/nn3)/pow((c_sz2/nn - c_s2z/nn2),1.5);*/
    *kurtosis = c_sz4/(*rms)/(*rms)/nn - 3;
    /*(c_sz4/nn - 4*c_sz1*c_sz3/nn4 + 6*c_s2z*c_sz2/nn3 - 3*c_s4z/nn4 - 3)/pow((c_sz2/nn - c_s2z/nn2),2);*/

    *rms = sqrt(*rms);

}

void
gwy_data_field_area_get_stats(GwyDataField *dfield,
                              gint col, gint row, gint width, gint height,
                              gdouble *avg,
                              gdouble *ra,
                              gdouble *rms,
                              gdouble *skew,
                              gdouble *kurtosis)
{
    gint i, j;
    gdouble /*c_sz1,*/ c_sz2, c_sz3, c_sz4, c_abs1;
    gdouble nn, dif;
    gdouble *datapos;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= dfield->xres
                     && row + height <= dfield->yres);

    nn = width*height;
    /*c_sz1 =*/ c_sz2 = c_sz3 = c_sz4 = c_abs1 = 0;

    *avg = gwy_data_field_area_get_avg(dfield, col, row, width, height);

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++) {
            dif = (*(drow++) - *avg);
            c_abs1 += fabs(dif);
            /*c_sz1 += dif; XXX always zero */
            c_sz2 += dif*dif;
            c_sz3 += dif*dif*dif;
            c_sz4 += dif*dif*dif*dif;
        }
    }

    *ra = c_abs1/nn;
    *rms = c_sz2/nn;
    *kurtosis = c_sz4/(*rms)/(*rms)/nn - 3;
    *rms = sqrt(*rms);
    *skew = c_sz3/(*rms)/(*rms)/(*rms)/nn;
}

void
gwy_data_field_get_area_stats(GwyDataField *data_field,
                              gint ulcol,
                              gint ulrow,
                              gint brcol,
                              gint brrow,
                              gdouble *avg,
                              gdouble *ra,
                              gdouble *rms,
                              gdouble *skew,
                              gdouble *kurtosis)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    /* XXX: brain damage. but this is what the original does.
     * warning added. */
    if (brcol == data_field->xres) {
        brcol = data_field->xres - 1;
        g_warning("gwy_data_field_get_area_stats: brcol off by 1");
    }
    if (brrow == data_field->yres) {
        brrow = data_field->yres - 1;
        g_warning("gwy_data_field_get_area_stats: brrow off by 1");
    }

    gwy_data_field_area_get_stats(data_field, ulcol, ulrow,
                                  brcol-ulcol, brrow-ulrow,
                                  avg, ra, rms, skew, kurtosis);
}


gint
gwy_data_field_get_line_stat_function(GwyDataField *data_field,
                                      GwyDataLine *target_line,
                                      gint ulcol, gint ulrow,
                                      gint brcol, gint brrow,
                                      GwySFOutputType type,
                                      GtkOrientation orientation,
                                      GwyInterpolationType interpolation,
                                      GwyWindowingType windowing, gint nstats)
{
    gint i, k, j, size;
    GwyDataLine *hlp_line;
    GwyDataLine *hlp_tarline;
    gdouble min = G_MAXDOUBLE, max = -G_MAXDOUBLE, val;

    gwy_debug("");
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);


    /*precompute settings if necessary*/
    if (type == GWY_SF_OUTPUT_DH || type == GWY_SF_OUTPUT_CDH) {
        min = gwy_data_field_area_get_min(data_field,
                                          ulcol, ulrow,
                                          brcol-ulcol, brrow-ulrow);
        max = gwy_data_field_area_get_max(data_field,
                                          ulcol, ulrow,
                                          brcol-ulcol, brrow-ulrow);
    }
    else if (type==GWY_SF_OUTPUT_DA || type==GWY_SF_OUTPUT_CDA) {
        if (orientation == GTK_ORIENTATION_HORIZONTAL) {
            for (i = ulcol; i < brcol; i++)
                for (j = ulrow; j < brrow; j++) {
                    val = gwy_data_field_get_xder(data_field, i, j);
                    if (min>val)
                        min = val;
                    if (max<val)
                        max = val;
                }
        }
        else if (orientation == GTK_ORIENTATION_VERTICAL) {
            for (i = ulcol; i < brcol; i++) {
                for (j = ulrow; j < brrow; j++) {
                    val = gwy_data_field_get_yder(data_field, i, j);
                    if (min>val)
                        min = val;
                    if (max<val)
                        max = val;
                }
            }
        }
    }

    /*average over profiles*/
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        size = brcol-ulcol;
        if (size < 10) {
            g_warning("Field too small");
            return 0;
        }
        if (type==GWY_SF_OUTPUT_PSDF && size < 64)
        {
            g_warning("Field too small");
            return 0;
        }

        hlp_line = GWY_DATA_LINE(gwy_data_line_new(size,
                                                   gwy_data_field_jtor(data_field, size),
                                                   FALSE));
        hlp_tarline = GWY_DATA_LINE(gwy_data_line_new(size,
                                                      gwy_data_field_jtor(data_field, size),
                                                      FALSE));

        if (nstats <= 0) {
            nstats = size;
        }
        if (type == GWY_SF_OUTPUT_DH || type==GWY_SF_OUTPUT_DA
            || type == GWY_SF_OUTPUT_CDA || type==GWY_SF_OUTPUT_CDH) {
            gwy_data_line_resample(target_line, nstats, interpolation);
            size = nstats;
        }
        else {
            gwy_data_line_resample(target_line, size, interpolation);
        }
        gwy_data_line_fill(target_line, 0.0);

        for (k = ulrow; k < brrow; k++) {
            gwy_data_field_get_row_part(data_field, hlp_line, k, ulcol, brcol);

            switch (type) {
                case GWY_SF_OUTPUT_DH:
                gwy_data_line_dh(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_CDH:
                gwy_data_line_cdh(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_DA:
                gwy_data_line_da(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_CDA:
                gwy_data_line_cda(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_ACF:
                gwy_data_line_acf(hlp_line, hlp_tarline);
                break;

                case GWY_SF_OUTPUT_HHCF:
                gwy_data_line_hhcf(hlp_line, hlp_tarline);
                break;

                case GWY_SF_OUTPUT_PSDF:
                gwy_data_line_psdf(hlp_line, hlp_tarline, windowing, interpolation);
                gwy_data_line_resample(hlp_tarline, size, interpolation);
                break;

                default:
                g_assert_not_reached();
                break;
            }
            for (j = 0; j < size; j++) { /*size*/
                target_line->data[j] += hlp_tarline->data[j]
                                        /((gdouble)(brrow-ulrow));
            }
            target_line->real = hlp_tarline->real;
        }
        _gwy_data_line_free(hlp_line);
        _gwy_data_line_free(hlp_tarline);

    }
    else if (orientation == GTK_ORIENTATION_VERTICAL) {
        size = brrow-ulrow;
        if (size < 10) {
            g_warning("Field too small");
            return 0;
        }

        hlp_line = GWY_DATA_LINE(
                       gwy_data_line_new(size,
                                         gwy_data_field_jtor(data_field, size),
                                         FALSE));
        hlp_tarline = GWY_DATA_LINE(
                          gwy_data_line_new(size,
                                            gwy_data_field_jtor(data_field,
                                                                size),
                                            FALSE));
        if (nstats <= 0) {
            nstats = size;
        }
        if (type == GWY_SF_OUTPUT_DH || type==GWY_SF_OUTPUT_DA
            || type == GWY_SF_OUTPUT_CDA || type==GWY_SF_OUTPUT_CDH) {
            gwy_data_line_resample(target_line, nstats, interpolation);
            size = nstats;
        }
        else {
            gwy_data_line_resample(target_line, size, interpolation);
        }
        gwy_data_line_fill(target_line, 0.0);

        for (k = ulcol; k < brcol; k++) {
            gwy_data_field_get_column_part(data_field,
                                           hlp_line, k, ulrow, brrow);

            switch (type) {
                case GWY_SF_OUTPUT_DH:
                gwy_data_line_dh(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_CDH:
                gwy_data_line_cdh(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_DA:
                gwy_data_line_da(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_CDA:
                gwy_data_line_cda(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_ACF:
                gwy_data_line_acf(hlp_line, hlp_tarline);
                break;

                case GWY_SF_OUTPUT_HHCF:
                gwy_data_line_hhcf(hlp_line, hlp_tarline);
                break;

                case GWY_SF_OUTPUT_PSDF:
                gwy_data_line_psdf(hlp_line, hlp_tarline, windowing,
                                   interpolation);
                gwy_data_line_resample(hlp_tarline, size, interpolation);
                break;

                default:
                g_assert_not_reached();
                break;
            }
            for (j = 0; j < size; j++) {
                target_line->data[j] += hlp_tarline->data[j]
                                        /((gdouble)(brcol-ulcol));
            }
            target_line->real = hlp_tarline->real;
        }
        _gwy_data_line_free(hlp_line);
        _gwy_data_line_free(hlp_tarline);

    }
    return 1;
}

/* XXX: why this function does not have `area' in name? */
void
gwy_data_field_fit_lines(GwyDataField *data_field, gint ulcol, gint ulrow,
                         gint brcol, gint brrow, GwyFitLineType fit_type,
                         gboolean exclude, GtkOrientation orientation)
{

    gint i, xres, yres, n;
    gdouble coefs[4];
    GwyDataLine *hlp;

    gwy_debug("");

    xres = data_field->xres;
    yres = data_field->yres;
    hlp = (GwyDataLine *) gwy_data_line_new(xres, data_field->xreal, 0);

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);

    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    n = (gint)fit_type;

    if (exclude) {
        if (orientation == GTK_ORIENTATION_HORIZONTAL) {
            if ((xres - brcol) > ulcol) {
                ulcol = brcol;
                brcol = xres;
            }
            else {
                brcol = ulcol;
                ulcol = 0;
            }
        }
        else if (orientation == GTK_ORIENTATION_VERTICAL) {
            if ((yres - brrow) > ulrow) {
                ulrow = brrow;
                brrow = yres;
            }
            else {
                brrow = ulrow;
                ulrow = 0;
            }
        }
    }

    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        for (i = 0; i < yres; i++) {
            gwy_data_field_get_row(data_field, hlp, i);
            if (i > ulrow && i <= brrow) {
                gwy_data_line_part_fit_polynom(hlp, n, coefs, ulcol, brcol);
            }
            else {
                gwy_data_line_fit_polynom(hlp, n, coefs);
            }
            gwy_data_line_subtract_polynom(hlp, n, coefs);
            gwy_data_field_set_row(data_field, hlp, i);
        }
    }
    else if (orientation == GTK_ORIENTATION_VERTICAL) {
        for (i = 0; i < xres; i++) {
            gwy_data_field_get_column(data_field, hlp, i);
            if (i > ulcol && i <= brcol) {
                gwy_data_line_part_fit_polynom(hlp, n, coefs, ulrow, brrow);
            }
            else {
                gwy_data_line_fit_polynom(hlp, n, coefs);
            }
            gwy_data_line_subtract_polynom(hlp, n, coefs);
            gwy_data_field_set_column(data_field, hlp, i);
        }
    }
    g_object_unref(hlp);

}

/**
 * square_area:
 * @data_field: data 
 * @ulcol: upper-left coordinate (in pixel units)
 * @ulrow: upper-left coordinate (in pixel units)
 * @brcol: bottom-right coordinate (in pixel units)
 * @brrow: bottom-right coordinate (in pixel units)
 *
 * Computes surface area within given rectangle
 *
 * Returns: surface area (in real units)
 * Since 1.1
 **/
static gdouble
square_area(GwyDataField *data_field, gint ulcol, gint ulrow, gint brcol,
            gint brrow)
{
    gdouble x, z1, z2, z3, z4, a, b, c, d, e, f, s1, s2, sa, sb, res;

    /* FIXME: this does not work when x and y measures are different */
    x = data_field->xreal / data_field->xres;

    z1 = data_field->data[(ulcol) + data_field->xres * (ulrow)];
    z2 = data_field->data[(brcol) + data_field->xres * (ulrow)];
    z3 = data_field->data[(ulcol) + data_field->xres * (brrow)];
    z4 = data_field->data[(brcol) + data_field->xres * (brrow)];

    a = sqrt(x * x + (z1 - z2) * (z1 - z2));
    b = sqrt(x * x + (z1 - z3) * (z1 - z3));
    c = sqrt(x * x + (z3 - z4) * (z3 - z4));
    d = sqrt(x * x + (z2 - z4) * (z2 - z4));
    e = sqrt(2 * x * x + (z3 - z2) * (z3 - z2));
    f = sqrt(2 * x * x + (z4 - z1) * (z4 - z1));

    s1 = (a + b + e)/2;
    s2 = (c + d + e)/2;
    sa = sqrt(s1 * (s1 - a) * (s1 - b) * (s1 - e)) +
        sqrt(s2 * (s2 - c) * (s2 - d) * (s2 - e));

    s1 = (a + d + f)/2;
    s2 = (c + b + f)/2;
    sb = sqrt(s1 * (s1 - a) * (s1 - d) * (s1 - f))
         + sqrt(s2 * (s2 - c) * (s2 - b) * (s2 - f));

    if (sa < sb)
        res = sa;
    else
        res = sb;

    return res;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
