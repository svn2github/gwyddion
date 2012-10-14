/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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

#ifndef __LIBGWY_H__
#define __LIBGWY_H__

#include <libgwy/array.h>
#include <libgwy/brick.h>
#include <libgwy/brick-arithmetic.h>
#include <libgwy/brick-part.h>
#include <libgwy/brick-statistics.h>
#include <libgwy/calc.h>
#include <libgwy/container.h>
#include <libgwy/coords.h>
#include <libgwy/coords-line.h>
#include <libgwy/coords-point.h>
#include <libgwy/coords-rectangle.h>
#include <libgwy/curve.h>
#include <libgwy/curve-statistics.h>
#include <libgwy/error-list.h>
#include <libgwy/expr.h>
#include <libgwy/fft.h>
#include <libgwy/field.h>
#include <libgwy/field-arithmetic.h>
#include <libgwy/field-correlate.h>
#include <libgwy/field-distributions.h>
#include <libgwy/field-filter.h>
#include <libgwy/field-level.h>
#include <libgwy/field-part.h>
#include <libgwy/field-read.h>
#include <libgwy/field-statistics.h>
#include <libgwy/field-transform.h>
#include <libgwy/fitter.h>
#include <libgwy/fit-func.h>
#include <libgwy/fit-param.h>
#include <libgwy/fit-task.h>
#include <libgwy/gl-material.h>
#include <libgwy/gradient.h>
#include <libgwy/grain-value.h>
#include <libgwy/int-set.h>
#include <libgwy/interpolation.h>
#include <libgwy/inventory.h>
#include <libgwy/line.h>
#include <libgwy/line-arithmetic.h>
#include <libgwy/line-distributions.h>
#include <libgwy/line-part.h>
#include <libgwy/line-statistics.h>
#include <libgwy/macros.h>
#include <libgwy/main.h>
#include <libgwy/mask-field.h>
#include <libgwy/mask-field-arithmetic.h>
#include <libgwy/mask-field-grains.h>
#include <libgwy/mask-field-transform.h>
#include <libgwy/mask-iter.h>
#include <libgwy/mask-line.h>
#include <libgwy/master.h>
#include <libgwy/math.h>
#include <libgwy/object-utils.h>
#include <libgwy/pack.h>
#include <libgwy/rand.h>
#include <libgwy/resource.h>
#include <libgwy/rgba.h>
#include <libgwy/serializable.h>
#include <libgwy/serializable-boxed.h>
#include <libgwy/serialize.h>
#include <libgwy/strfuncs.h>
#include <libgwy/surface.h>
#include <libgwy/surface-statistics.h>
#include <libgwy/types.h>
#include <libgwy/unit.h>
#include <libgwy/user-fit-func.h>
#include <libgwy/user-grain-value.h>
#include <libgwy/value-format.h>
#include <libgwy/version.h>

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
