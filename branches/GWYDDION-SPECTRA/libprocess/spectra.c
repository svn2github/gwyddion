/*
 *  $Id: spectra.c 6957 2006-11-08 14:44:11Z owaind $
 *  Copyright (C) 2003 Owain Davies, David Necas (Yeti), Petr Klapetek.
 *  E-mail: owain.davies@blueyonder.co.uk
 *          yeti@gwyddion.net, klapetek@gwyddion.net.
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
#define DEBUG

#include "config.h"
#include <string.h>
#include <glib.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/spectra.h>
#include <libprocess/linestats.h>
#include <libprocess/interpolation.h>



#define GWY_SPECTRA_TYPE_NAME "GwySpectra"

enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

static void        gwy_spectra_finalize           (GObject *object);
static void        gwy_spectra_serializable_init  (GwySerializableIface *iface);
static GByteArray* gwy_spectra_serialize          (GObject *obj,
                                                   GByteArray *buffer);
static gsize       gwy_spectra_get_size           (GObject *obj);
static GObject*    gwy_spectra_deserialize        (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject*    gwy_spectra_duplicate_real     (GObject *object);
static void        gwy_spectra_clone_real         (GObject *source,
                                                   GObject *copy);

static guint spectra_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwySpectra, gwy_spectra, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_spectra_serializable_init))

static void
gwy_spectra_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    iface->serialize = gwy_spectra_serialize;
    iface->deserialize = gwy_spectra_deserialize;
    iface->get_size = gwy_spectra_get_size;
    iface->duplicate = gwy_spectra_duplicate_real;
    iface->clone = gwy_spectra_clone_real;
}

static void
gwy_spectra_class_init(GwySpectraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_spectra_finalize;

/**
 * GwySpectra::data-changed:
 * @gwydataline: The #GwySpectra which received the signal.
 *
 * The ::data-changed signal is never emitted by the spectra itself.  It
 * is intended as a means to notify other spectra users they should
 * update themselves.
 */
    spectra_signals[DATA_CHANGED]
        = g_signal_new("data-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwySpectraClass, data_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_spectra_init(GwySpectra *spectra)
{
    gwy_debug_objects_creation(G_OBJECT(spectra));
}

static void
gwy_spectra_finalize(GObject *object)
{
    gint i;
    GwySpectra *spectra = (GwySpectra*)object;
    GwyDataLine *spec;

    gwy_object_unref(spectra->si_unit_xy);
    gwy_debug("");

    for (i = 0; i < spectra->spectrum->len; i++) {
        spec = g_ptr_array_index(spectra->spectrum, i);
        gwy_object_unref(spec);
    }
    g_ptr_array_free(spectra->spectrum, TRUE);
    g_array_free(spectra->x_pos, TRUE);
    g_array_free(spectra->y_pos, TRUE);
    G_OBJECT_CLASS(gwy_spectra_parent_class)->finalize(object);
}

/**
 * gwy_spectra_new:
 *
 * Creates a new Spectra object containing zero spectra.
 *
 * Returns: A newly created spectra.
 **/

GwySpectra*
gwy_spectra_new() {

    GwySpectra *spectra;

    gwy_debug("");
    spectra = g_object_new(GWY_TYPE_SPECTRA, NULL);

    spectra->spectrum = g_ptr_array_new();
    spectra->x_pos = g_array_new(FALSE, TRUE, sizeof(gdouble));
    spectra->y_pos = g_array_new(FALSE, TRUE, sizeof(gdouble));

    return spectra;
}

/**
 * gwy_spectra_new_alike:
 * @model: A Spectra object to take units from.
 *
 * Creates a new Spectra object similar to an existing one, but containing zero
 * spectra. The same amount of memory preallocated to the arrays, but they
 * contain zero.
 *
 * Use gwy_spectra_duplicate() if you want to copy a spectra object including
 * the spectra in it.
 *
 * Returns: A newly created Spectra object.
 **/
GwySpectra*
gwy_spectra_new_alike(GwySpectra *model)
{
    GwySpectra *spectra;

    g_return_val_if_fail(GWY_IS_SPECTRA(model), NULL);
    spectra = g_object_new(GWY_TYPE_SPECTRA, NULL);

    spectra->spectrum = g_ptr_array_new();
    spectra->x_pos = g_array_sized_new(FALSE,
                                       TRUE,
                                       sizeof(gdouble),
                                       model->x_pos->len);
    spectra->y_pos = g_array_sized_new(FALSE,
                                       TRUE,
                                       sizeof(gdouble),
                                       model->y_pos->len);

    if (model->si_unit_xy)
        spectra->si_unit_xy = gwy_si_unit_duplicate(model->si_unit_xy);

    return spectra;
}


static GByteArray*
gwy_spectra_serialize(GObject *obj,
                        GByteArray *buffer)
{
    GwySpectra *spectra;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SPECTRA(obj), NULL);

    spectra = GWY_SPECTRA(obj);
    if (!spectra->si_unit_xy)
        spectra->si_unit_xy = gwy_si_unit_new("");
    {
        GwySerializeSpec spec[] = {
            { 'D', "x_pos", &spectra->x_pos->data, &spectra->x_pos->len, },
            { 'D', "y_pos", &spectra->y_pos->data, &spectra->y_pos->len, },
            { 'O', "spectrum", &spectra->spectrum->pdata,
                               &spectra->spectrum->len, },
            { 'o', "si_unit_xy", &spectra->si_unit_xy, NULL, },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_SPECTRA_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_spectra_get_size(GObject *obj)
{
    GwySpectra *spectra;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SPECTRA(obj), 0);

    spectra = GWY_SPECTRA(obj);
    if (!spectra->si_unit_xy)
        spectra->si_unit_xy = gwy_si_unit_new("");
    {
        GPtrArray* spc = spectra->spectrum;
        GwySerializeSpec spec[] = {
            { 'D', "x_pos", &spectra->x_pos->data, &spectra->x_pos->len, },
            { 'D', "y_pos", &spectra->y_pos->data, &spectra->y_pos->len, },
            { 'O', "spectrum", &spc->pdata, &spc->len, },
            { 'o', "si_unit_xy", &spectra->si_unit_xy, NULL, },
        };

        return gwy_serialize_get_struct_size(GWY_SPECTRA_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_spectra_deserialize(const guchar *buffer,
                        gsize size,
                        gsize *position)
{
    guint32 n_x_pos, n_y_pos, n_spectrum;
    gdouble *x_pos = NULL, *y_pos = NULL;
    GwySIUnit *si_unit_xy = NULL;
    GwyDataLine *spectrum = NULL;
    GwySpectra *spectra;

    GwySerializeSpec spec[] = {
      { 'D', "x_pos", &x_pos, &n_x_pos, },
      { 'D', "y_pos", &y_pos, &n_y_pos, },
      { 'O', "spectrum", &spectrum, &n_spectrum, },
      { 'o', "si_unit_xy", &si_unit_xy, NULL, },
    };

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_SPECTRA_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(x_pos);
        g_free(y_pos);
        g_free(spectrum);     /* FIXME: Is this safe enough? */
        gwy_object_unref(si_unit_xy);
        return NULL;
    }
    if (!((n_x_pos == n_y_pos) && (n_x_pos == n_spectrum))) {
        g_critical("Serialized %s size mismatch (%u,%u,%u) not equal.",
              GWY_SPECTRA_TYPE_NAME, n_x_pos, n_y_pos, n_spectrum);
        g_free(x_pos);
        g_free(y_pos);
        g_free(spectrum);     /* FIXME: Is this safe enough? */
        gwy_object_unref(si_unit_xy);
        return NULL;
    }

    spectra = gwy_spectra_new();

    g_free(spectra->spectrum->pdata);
    spectra->spectrum->pdata = (gpointer*)spectrum;
    spectra->spectrum->len = n_spectrum;

    g_free(spectra->x_pos->data);
    spectra->x_pos->data = (gchar*)x_pos;
    spectra->x_pos->len = n_x_pos;

    g_free(spectra->y_pos->data);
    spectra->y_pos->data = (gchar*)y_pos;
    spectra->y_pos->len = n_y_pos;

    if (si_unit_xy) {
        if (spectra->si_unit_xy != NULL)
            gwy_object_unref(spectra->si_unit_xy);
        spectra->si_unit_xy = si_unit_xy;
    }

    return (GObject*)spectra;
}

static GObject*
gwy_spectra_duplicate_real(GObject *object)
{
    gint i;
    GwySpectra *spectra, *duplicate;
    GwyDataLine *data_line;

    g_return_val_if_fail(GWY_IS_DATA_LINE(object), NULL);
    spectra = GWY_SPECTRA(object);
    duplicate = gwy_spectra_new_alike(spectra);
    /* duplicate the co-ordinates of the spectra */
    memcpy(duplicate->x_pos->data,
           spectra->x_pos->data,
           spectra->x_pos->len*sizeof(gdouble));
    memcpy(duplicate->y_pos->data,
           spectra->y_pos->data,
           spectra->y_pos->len*sizeof(gdouble));
    duplicate->x_pos->len = spectra->x_pos->len;
    duplicate->y_pos->len = spectra->y_pos->len;

    /* Duplicate the spectra themselves */
    for (i = 0; i < spectra->spectrum->len; i++) {
        data_line = (GwyDataLine*)g_ptr_array_index(spectra->spectrum, i);
        g_ptr_array_add(duplicate->spectrum,
        gwy_data_line_duplicate(data_line));
    }
    return (GObject*)duplicate;
}

static void
gwy_spectra_clone_real(GObject *source, GObject *copy)
{
    GwySpectra *spectra, *clone;

    g_return_if_fail(GWY_IS_SPECTRA(source));
    g_return_if_fail(GWY_IS_SPECTRA(copy));

    spectra = GWY_SPECTRA(source);
    clone = GWY_SPECTRA(copy);

    if (spectra->x_pos->len != clone->x_pos->len) {
        g_array_set_size(clone->x_pos, spectra->x_pos->len);
    }
    if (spectra->y_pos->len != clone->y_pos->len) {
        g_array_set_size(clone->y_pos, spectra->y_pos->len);
    }

    memcpy(clone->x_pos->data,
           spectra->x_pos->data,
           spectra->x_pos->len*sizeof(gdouble));
    memcpy(clone->y_pos->data,
           spectra->y_pos->data,
           spectra->y_pos->len*sizeof(gdouble));

    /* SI Units can be NULL */
    if (spectra->si_unit_xy && clone->si_unit_xy)
        gwy_serializable_clone(G_OBJECT(spectra->si_unit_xy),
                               G_OBJECT(clone->si_unit_xy));
    else if (spectra->si_unit_xy && !clone->si_unit_xy)
        clone->si_unit_xy = gwy_si_unit_duplicate(spectra->si_unit_xy);
    else if (!spectra->si_unit_xy && clone->si_unit_xy)
        gwy_object_unref(clone->si_unit_xy);
}

/**
 * gwy_spectra_data_changed:
 * @spectra: A spectra object.
 *
 * Emits signal "data_changed" on a spectra object.
 **/
void
gwy_spectra_data_changed(GwySpectra *spectra)
{
    g_signal_emit(spectra, spectra_signals[DATA_CHANGED], 0);
}

/**
 * gwy_spectra_copy:
 * @a: Source Spectra object.
 * @b: Destination Spectra object.
 *
 * Copies the contents of a spectra object a to another already allocated
 * spectra object b. It employs the clone method, and any data in b is lost.
 *
 **/
void
gwy_spectra_copy(GwySpectra *a, GwySpectra *b)
{
    g_return_if_fail(GWY_IS_SPECTRA(a));
    g_return_if_fail(GWY_IS_SPECTRA(a));

    gwy_spectra_clone_real((GObject*)a, (GObject*)b);
}

/**
 * gwy_spectra_get_si_unit_x:
 * @spectra: A spectra.
 *
 * Returns SI unit used for the location co-ordinates of spectra.
 *
 * Returns: SI unit corresponding to the  the location co-ordinates of spectra
 * object. Its reference count is not incremented.
 **/
GwySIUnit*
gwy_spectra_get_si_unit_xy(GwySpectra *spectra)
{
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), NULL);

    if (!spectra->si_unit_xy)
        spectra->si_unit_xy = gwy_si_unit_new("");

    return spectra->si_unit_xy;
}

/**
 * gwy_spectra_set_si_unit_xy:
 * @spectra: A Spectra object.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the location co-ordinates of the spectra
 * object.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 **/
void
gwy_spectra_set_si_unit_xy(GwySpectra *spectra,
                           GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (spectra->si_unit_xy == si_unit)
        return;

    gwy_object_unref(spectra->si_unit_xy);
    g_object_ref(si_unit);
    spectra->si_unit_xy = si_unit;
}

/**
 * function_name:
 * @param1: Description of param1.
 * @param2: Description of param2.
 *
 * Description of function opperation.
 *
 * Returns: What is returned, and how should it be treated?
 **/

/**
 * gwy_spectra_itox:
 * @spectra: A Spectra Object
 * @i: index of a spectrum
 *
 * Returns the x-coordinate of the spectrum i in the Spectra object.
 *
 * Returns: A gdouble corresponding to the real x co-ordinate of the spectrum.
 **/
gdouble
gwy_spectra_itox (GwySpectra *spectra, guint i)
{
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), 0.0);

    if (i >= spectra->x_pos->len) {
        return 0.0;
    }
    return g_array_index(spectra->x_pos, gdouble, i);
}

/**
 * gwy_spectra_itoy:
 * @spectra: A Spectra Object
 * @i: index of a spectrum
 *
 * Returns the y-coordinate of the spectrum i in the Spectra object.
 *
 * Returns: A gdouble corresponding to the real x coordinate of the spectrum.
 **/
gdouble
gwy_spectra_itoy (GwySpectra *spectra, guint i)
{
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), 0.0);

    if (i >= spectra->y_pos->len) {
        return 0.0;
    }
    return g_array_index(spectra->y_pos, gdouble, i);
}

/**
 * gwy_spectra_xytoi:
 * @spectra: A Spectra Object
 * @real_x: The x coordinate of the location of the spectrum.
 * @real_y: The y coordinate of the location of the spectrum.
 *
 * Finds the index of the spectrum closest to the location specified by
 * the coordinated x and y.
 *
 * Returns: The index of the nearest spectrum.
 **/
guint
gwy_spectra_xytoi (GwySpectra *spectra, gdouble real_x, gdouble real_y)
{
    gdouble r, x, y, rmin = 0;
    guint i = 0, n, imin = 0;

    g_return_val_if_fail(GWY_IS_SPECTRA(spectra),  0);

    if (spectra->x_pos->len != spectra->y_pos->len) {
        g_critical("Numer of elements in x_pos and y_pos different.");
        return 0;
    }
    if (spectra->x_pos->len == 0) {
        g_critical("Spectra Object contains no spectra.");
        return 0;
    }

    n = spectra->x_pos->len;

    r = 0;
    for (i = 0; i < n; i++) {
        x = g_array_index(spectra->x_pos, gdouble, i);
        y = g_array_index(spectra->y_pos, gdouble, i);
        r = sqrt((x-real_x)*(x-real_x)+(y-real_y)*(y-real_y));
        if ((r < rmin) || (i = 0)) {
            rmin = r;
            imin = i;
        }
    }
    return imin;
}

gint CompFunc_r(gconstpointer a, gconstpointer b) {
    coord_pos *A, *B;
    A = (coord_pos*)a;
    B = (coord_pos*)b;

    if (A->r < B->r)
        return -1;
    if (A->r > B->r)
        return 1;
    return 0; /* Equal */
}

/**
 * gwy_spectra_nearest:
 * @spectra: A Spectra Object
 * @plist:  pointer to a pointer where the list will be allocated.
 * @real_x: The x coordinate.
 * @real_y: The y coordinate.
 *
 * Gets a list of the indices to spectra ordered by their
 * distance from x_real, y_real. A newly created array is allocated and the
 * list of indicies is stored there.The calling function must ensure
 * the memory is freed once the list is finished with.
 *
 * Returns: The number of elements in the @plist array.
 **/
gint
gwy_spectra_nearest (GwySpectra *spectra,
                     guint** plist,
                     gdouble real_x,
                     gdouble real_y)
{
    GArray* points;
    guint i = 0;
    coord_pos item;
    gdouble x, y;

    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), 0);

    if (spectra->x_pos->len != spectra->y_pos->len) {
        g_critical("Numer of elements in x_pos and y_pos different.");
        return 0;
    }
    if (spectra->x_pos->len == 0) {
        g_critical("Spectra Object contains no spectra.");
        return 0;
    }
    if (plist == NULL) {
        g_critical("Nowhere to create array");
        return 0;
    }

    points = g_array_sized_new(FALSE,
                               TRUE,
                               sizeof(coord_pos),
                               spectra->x_pos->len);
    for (i = 0; i < spectra->x_pos->len; i++) {
        x = g_array_index(spectra->x_pos, gdouble, i);
        y = g_array_index(spectra->y_pos, gdouble, i);
        item.index = i;
        item.r = sqrt((x-real_x)*(x-real_x)+(y-real_y)*(y-real_y));
        g_array_append_val(points, item);
    }
    g_array_sort(points, CompFunc_r);
    *(plist) = g_new(guint, points->len);
    for (i = 0; i < points->len; i++)
        *(*(plist)+i) = (g_array_index(points, coord_pos, i)).r;

    g_array_free(points, TRUE);

    return points->len;
}

/**
 * gwy_spectra_setpos:
 * @spectra: A Spectra Object
 * @real_x: The new x coordinate of the location of the spectrum.
 * @real_y: The new y coordinate of the location of the spectrum.
 * @i: The index of a spectrum.
 *
 * Sets the location coordinates of a spectrum.
 *
 **/
void
gwy_spectra_setpos (GwySpectra *spectra, gdouble real_x,
                    gdouble real_y, guint i)
{
    g_return_if_fail(GWY_IS_SPECTRA(spectra));

    if (spectra->x_pos->len != spectra->y_pos->len) {
        g_critical("Numer of elements in x_pos and y_pos different.");
        return;
    }

    if (i >= spectra->y_pos->len) {
        return;
    }

    g_array_index(spectra->x_pos, gdouble, i) = real_x;
    g_array_index(spectra->y_pos, gdouble, i) = real_y;
    return;
}

/**
 * gwy_spectra_get_spectrum:
 * @spectra: A Spectra object
 * @i: Index of a spectrum
 *
 * Gets a dataline that contains the spectrum at index i.
 *
 * Returns: A #GwyDataLine containing the spectrum, and increases
 * reference count.
 **/
GwyDataLine*
gwy_spectra_get_spectrum (GwySpectra *spectra, gint i)
{
    GwyDataLine* data_line;
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), NULL);

    if (i >= spectra->y_pos->len) {
        g_critical("Invalid spectrum index");
        return NULL;
    }
    data_line = g_ptr_array_index(spectra->spectrum, i);
    g_object_unref(data_line);
    return data_line;
}

/**
 * gwy_spectra_set_spectrum:
 * @spectra: A Spectra Object
 * @i: Index of a spectrum to replace
 * @new_spectrum: A #GwyDataLine Object containing the new spectrum.
 *
 * Replaces the ith spectrum in the spectra object with a the
 * supplied spectrum, new_spectrum. It takes its own reference
 * to the New_Spectrum dataline.
 *
 **/
void
gwy_spectra_set_spectrum (GwySpectra *spectra,
                          guint i,
                          GwyDataLine *new_spectrum)
{
    GwyDataLine* data_line;
    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_return_if_fail(GWY_IS_DATA_LINE(new_spectrum));

    if (i >= spectra->y_pos->len) {
        g_critical("Invalid spectrum index");
        return;
    }
    
    g_object_ref(new_spectrum);    
    data_line = g_ptr_array_index(spectra->spectrum, i);
    g_object_unref(data_line);
    g_ptr_array_index(spectra->spectrum, i) = new_spectrum;
}

/**
 * gwy_spectra_add_spectrum:
 * @spectra: A Spectra Object
 * @new_spectrum: A GwyDataLine containing the spectrum to append.
 * @real_x: The x coordinate of the location of the spectrum.
 * @real_y: The y coordinate of the location of the spectrum.
 *
 * Appends a new_spectrum to the spectra collection with a position of x, y.
 * gwy_spectra_add takes a refference to the supplied spectrum.
 **/
void
gwy_spectra_add_spectrum (GwySpectra *spectra, GwyDataLine *new_spectrum,
                          gdouble x, gdouble y)
{
    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_return_if_fail(GWY_IS_DATA_LINE(new_spectrum));

    g_ptr_array_add(spectra->spectrum, new_spectrum);
    g_array_append_val(spectra->x_pos, x);
    g_array_append_val(spectra->y_pos, y);

    g_object_ref(new_spectrum);

}

/**
 * gwy_spectra_remove:
 * @spectra: A Spectra Object
 * @i: Index of spectrum to remove.
 *
 * Removes the ith spectrum from the Spectra collection.
 **/
void gwy_spectra_remove_spectrum (GwySpectra *spectra, guint i)
{
    GwyDataLine* data_line;

    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    if (i >= spectra->y_pos->len) {
        g_critical("Invalid spectrum index");
        return;
    }

    data_line = g_ptr_array_index(spectra->spectrum, i);
    g_object_unref(data_line);
    g_ptr_array_remove_index(spectra->spectrum, i);
    g_array_remove_index(spectra->x_pos, i);
    g_array_remove_index(spectra->y_pos, i);
}


/* function to calculate the angle A ^B^ C
 * points is an array in the format:
 * Ax, Ay, Bx, By, Cx, Cy
 */
gdouble calc_angle(const gdouble* points)
{
    gdouble Ax, Ay, Bx, By, Cx, Cy;
    gdouble aa, bb, cc, beta;

    Ax = points[0];
    Ay = points[1];
    Bx = points[2];
    By = points[3];
    Cx = points[4];
    Cy = points[5];

    aa = (Bx-Cx)*(Bx-Cx)+(By-Cy)*(By-Cy);
    bb = (Ax-Cx)*(Ax-Cx)+(Ay-Cy)*(Ay-Cy);
    cc = (Ax-Bx)*(Ax-Bx)+(Ay-By)*(Ay-By);

    beta = acos((aa+cc-bb)/(sqrt(4*aa*cc)));
    return beta;
}

gint is_in_triangle(gdouble x, gdouble y,
                    const gdouble* triangle)
{
    guint i;
    gdouble points[6], ang;
    points[2] = x;
    points[3] = y;
    ang = 0;
    for (i = 0; i < 3; i++) {
        points[0] = triangle[(guint)fmod(i*2, 6)];
        points[1] = triangle[(guint)fmod(i*2+1, 6)];
        points[4] = triangle[(guint)fmod(i*2+2, 6)];
        points[5] = triangle[(guint)fmod(i*2+3, 6)];
        ang += calc_angle(points);
    }
    if (ang == 2*M_PI)
        return 1;
    else
        return 0;
}

gdouble* tri_interp_weight(gdouble x, gdouble y,
                           const gdouble* triangle)
{
    /* TODO: Function to get the interpolation weightings
             between three points*/
    return NULL;
}

/**
 * gwy_spectra_get_spectra_interp:
 * @spectra: A Spectra Object
 * @x: x coordinate for spectrum
 * @y: y coordinate fro spectrum
 *
 * In future will:
 * Interpolates a spectrum at some arbitary point specified by x,y.
 * If the point is within the triangle made by three measured spectra
 * then these are used else it is interpolated from the nearest two or one.
 * But currently:
 * Returns a newly created dataline, corresponding to the nearest spectrum.
 * Returns: A newly created dataline.
 **/
GwyDataLine*
gwy_spectra_get_spectra_interp (GwySpectra *spectra, gdouble x, gdouble y)
{
    guint i;
    /* TODO: Function to do a proper interpolation. */
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), NULL);
    if (spectra->x_pos->len <= 0) {
        g_critical("Contains no spectra");
        return NULL;
    }
    i = gwy_spectra_xytoi(spectra, x, y);
    return gwy_data_line_duplicate(gwy_spectra_get_spectrum(spectra, i));
}

/**
 * gwy_spectra_empty:
 * @spectra: A Spectra Object
 *
 * Removes all spectra from the collection.
 * Returns: A newly created dataline.
 **/
void
gwy_spectra_empty (GwySpectra *spectra)
{
    int i = 0;
    GwyDataLine* spec;

    for (i = 0; i < spectra->spectrum->len; i++) {
        spec = g_ptr_array_index(spectra->spectrum, i);
        gwy_object_unref(spec);
    }
    g_free(spectra->spectrum);
    g_array_free(spectra->x_pos, TRUE);
    g_array_free(spectra->y_pos, TRUE);
    spectra->x_pos = g_array_new(FALSE, TRUE, sizeof(gdouble));
    spectra->y_pos = g_array_new(FALSE, TRUE, sizeof(gdouble));
}

/************************** Documentation ****************************/

/**
 * SECTION:Spectra
 * @title: GwySpectra
 * @short_description: Collection of dataline representing point spectra.
 *
 * #GwySpectra contains an array of GwyDataLines and coordinates representing
 * where in a datafield the spectrum was aquired.
 **/

/**
 * GwySpectra:
 *
 * The #GwySpectra struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * gwy_spectra_duplicate:
 * @spectra: A Spectra object to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
