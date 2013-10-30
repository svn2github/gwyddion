/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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

#include "libgwyapp/libgwyapp.h"

typedef struct {
    GObject gobject;
} GwyExtTest;

typedef struct {
    GObjectClass gobject_class;
} GwyExtTestClass;

G_DEFINE_TYPE(GwyExtTest, gwy_ext_test, G_TYPE_OBJECT);

static const GwyModuleProvidedType module_types[] = {
    { "GwyExtTestType", gwy_ext_test_get_type },
};

static const GwyModuleInfo module_info = {
   GWY_MODULE_ABI_VERSION,
   G_N_ELEMENTS(module_types),
   "A test module.  It does not do anything.",
   "Yeti <yeti@physics.muni.cz>",
   "0.0",
   "David Nečas (Yeti)",
   "2013",
   module_types,
};

GWY_DEFINE_MODULE(&module_info, test);

static void
gwy_ext_test_init(G_GNUC_UNUSED GwyExtTest *test)
{
}

static void
gwy_ext_test_class_init(G_GNUC_UNUSED GwyExtTestClass *klass)
{
    g_printerr("GwyExtTestClass initialised.\n");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
