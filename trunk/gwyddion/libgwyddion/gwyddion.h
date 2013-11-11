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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef __GWY_GWYDDION_H__
#define __GWY_GWYDDION_H__

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyddionenums.h>
#include <libgwyddion/gwyddiontypes.h>
#include <libgwyddion/gwyenum.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwynlfit.h>
#include <libgwyddion/gwynlfitpreset.h>
#include <libgwyddion/gwyfdcurvepreset.h>
#include <libgwyddion/gwyserializable.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwyddion/gwyinventory.h>
#include <libgwyddion/gwyresource.h>
#include <libgwyddion/gwyentities.h>
#include <libgwyddion/gwysiunit.h>
#include <libgwyddion/gwymd5.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwyexpr.h>
#include <libgwyddion/gwystringlist.h>
#include <libgwyddion/gwyversion.h>

G_BEGIN_DECLS

void gwy_type_init(void);

G_END_DECLS

#endif /* __GWY_GWYDDION_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
