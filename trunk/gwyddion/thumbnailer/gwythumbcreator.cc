/*
 *  @(#) $Id$
 *  Copyright (C) 2008 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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

#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qprocess.h>
#include <QtGui/qimage.h>
#include <kio/thumbcreator.h>

/* The interface we implement */
class GwyThumbCreator : public ThumbCreator {
    bool create(const QString &path, int width, int height, QImage &image);
    Flags flags() const;
};

/* The symbol the module must export */
extern "C" {
    ThumbCreator *new_creator() { return new GwyThumbCreator(); }
};

/* What should the thumbnail consumer do with the result.  Possible flags are:
 * DrawFrame: Draw a frame around, we probably want this.
 * BlendIcon: Blend preview with MIME icon, we probably do not want this. */
ThumbCreator::Flags
GwyThumbCreator::flags() const
{
    return (Flags)(DrawFrame);
}

/* The real stuff.  To create the thumbnail we simply run gwyddion-thumbnailer.
 * This is inefficient but it:
 * - prevents Qt/Gtk+ impedance matching problems (such as Qt doing `#define
 *   signals private')
 * - prevents all sorts of leaks that can occur in Gwyddion file modules from
 *   hurting long-living GwyThumbCreators
 * - prevents intricate initialization issues if thumbnailers are created from
 *   several threads simultaneously
 */
bool
GwyThumbCreator::create(const QString &path,
                        int width, int height,
                        QImage &image)
{
    QString program = GWYDDION_THUMBNAILER;
    QString s = "%1";
    QStringList arguments;
    arguments << "kde4" << s.arg(qMax(width, height)) << path;

    QProcess *gwythumbnailer = new QProcess();
    gwythumbnailer->start(program, arguments);
    if (!gwythumbnailer->waitForStarted(2000)) {
        delete gwythumbnailer;
        return false;
    }

    QByteArray pngdata;
    while (gwythumbnailer->waitForReadyRead(100))
        pngdata += gwythumbnailer->readAllStandardOutput();
    image.loadFromData(pngdata, "PNG");
    delete gwythumbnailer;
    return !image.isNull();
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
