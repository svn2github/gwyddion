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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gwyserializable.h"
#include "gwycontainer.h"
#include "gwyentities.h"
#include "gwytestser.h"
#include "gwyutils.h"
#include "gwymath.h"
#include "gwysiunit.h"

int
main(void)
{
    GwySIUnit *si;
    gchar prefix[20];
    gdouble div;
    gdouble value;
    
    si = gwy_si_unit_new("Weber");
    
    value = 12e6;
    gwy_si_unit_get_prefixed(si, value, 3, prefix, &div);
    
    printf("unit=%s, power=%f, 12e6 units = %f %s\n",
           gwy_si_unit_get_unit_string(si), div,
           (double)value/(double)div, prefix);
    

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
