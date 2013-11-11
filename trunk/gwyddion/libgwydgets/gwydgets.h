/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_GWYDGETS_H__
#define __GWY_GWYDGETS_H__

#include <gwyconfig.h>

#include <libgwydgets/gwydgetenums.h>
#include <libgwydgets/gwydgettypes.h>

#include <libgwydgets/gwy3dlabel.h>
#include <libgwydgets/gwy3dsetup.h>
#include <libgwydgets/gwy3dview.h>
#include <libgwydgets/gwy3dwindow.h>
#include <libgwydgets/gwyaxis.h>
#include <libgwydgets/gwycoloraxis.h>
#include <libgwydgets/gwycolorbutton.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwycurve.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydataviewlayer.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwygrainvaluemenu.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwygrapharea.h>
#include <libgwydgets/gwygraphcorner.h>
#include <libgwydgets/gwygraphdata.h>
#include <libgwydgets/gwygrapharea.h>
#include <libgwydgets/gwygraphcorner.h>
#include <libgwydgets/gwygraphcurves.h>
#include <libgwydgets/gwygraphlabel.h>
#include <libgwydgets/gwygraphcurvemodel.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphlabel.h>
#include <libgwydgets/gwygraphwindow.h>
#include <libgwydgets/gwyhmarkerbox.h>
#include <libgwydgets/gwyhruler.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwymarkerbox.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwyoptionmenus.h>
#include <libgwydgets/gwypixmaplayer.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwyruler.h>
#include <libgwydgets/gwyselectiongraph1darea.h>
#include <libgwydgets/gwyselectiongrapharea.h>
#include <libgwydgets/gwyselectiongraphline.h>
#include <libgwydgets/gwyselectiongraphpoint.h>
#include <libgwydgets/gwyselectiongraphzoom.h>
#include <libgwydgets/gwysensitivitygroup.h>
#include <libgwydgets/gwyscitext.h>
#include <libgwydgets/gwyshader.h>
#include <libgwydgets/gwystatusbar.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyvectorlayer.h>
#include <libgwydgets/gwyvruler.h>

#ifdef GWYDDION_HAS_OPENGL
#include <gdk/gdkgl.h>
#endif

G_BEGIN_DECLS

#ifndef GWYDDION_HAS_OPENGL
typedef void GdkGLConfig;
#endif

void         gwy_widgets_type_init          (void);
gboolean     gwy_widgets_gl_init            (void);
GdkGLConfig* gwy_widgets_get_gl_config      (void);

G_END_DECLS

#endif /* __GWY_GWYDGETS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
