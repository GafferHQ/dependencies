/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qvfb.h"

#include <QApplication>
#include <QRegExp>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#ifdef Q_WS_X11
#include <QX11Info>
#endif

QT_BEGIN_NAMESPACE

void fn_quit_qvfb(int)
{
    // pretend that we have quit normally
    qApp->quit();
}


void usage( const char *app )
{
    printf( "Usage: %s [-width width] [-height height] [-depth depth] [-zoom zoom]"
	    "[-mmap] [-nocursor] [-qwsdisplay :id] [-x11display :id] [-skin skindirectory]\n"
	    "Supported depths: 1, 4, 8, 12, 15, 16, 18, 24, 32\n", app );
}
int qvfb_protocol = 0;

int runQVfb( int argc, char *argv[] )
{
    Q_INIT_RESOURCE(qvfb);

    QApplication app( argc, argv );

    QTranslator translator;
    QTranslator qtTranslator;
    QString sysLocale = QLocale::system().name();
    QString resourceDir = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
    if (translator.load(QLatin1String("qvfb_") + sysLocale, resourceDir)
        && qtTranslator.load(QLatin1String("qt_") + sysLocale, resourceDir)) {
        app.installTranslator(&translator);
        app.installTranslator(&qtTranslator);
    }

    int width = 0;
    int height = 0;
    int depth = -32; // default, but overridable by skin
    bool depthSet = false;
    int rotation = 0;
    bool cursor = true;
    QVFb::DisplayType displayType = QVFb::QWS;
    double zoom = 1.0;
    QString displaySpec( ":0" );
    QString skin;

    for ( int i = 1; i < argc; i++ ){
	QString arg = argv[i];
	if ( arg == "-width" ) {
	    width = atoi( argv[++i] );
	} else if ( arg == "-height" ) {
	    height = atoi( argv[++i] );
	} else if ( arg == "-skin" ) {
	    skin = argv[++i];
	} else if ( arg == "-depth" ) {
	    depth = atoi( argv[++i] );
	    depthSet = true;
	} else if ( arg == "-nocursor" ) {
	    cursor = false;
	} else if ( arg == "-mmap" ) {
	    qvfb_protocol = 1;
	} else if ( arg == "-zoom" ) {
	    zoom = atof( argv[++i] );
	} else if ( arg == "-qwsdisplay" ) {
	    displaySpec = argv[++i];
	    displayType = QVFb::QWS;
#ifdef Q_WS_X11
	} else if ( arg == "-x11display" ) {
	    displaySpec = argv[++i];
	    displayType = QVFb::X11;

	    // Usually only the default X11 depth will work with Xnest,
	    // so override the default of 32 with the actual X11 depth.
	    if (!depthSet)
		depth = -QX11Info::appDepth(); // default, but overridable by skin
#endif
	} else {
	    printf( "Unknown parameter %s\n", arg.toLatin1().constData() );
	    usage( argv[0] );
	    exit(1);
	}
    }

    int displayId = 0;
    QRegExp r( ":[0-9]+" );
    int m = r.indexIn( displaySpec, 0 );
    int len = r.matchedLength();
    if ( m >= 0 ) {
	displayId = displaySpec.mid( m+1, len-1 ).toInt();
    }
    QRegExp rotRegExp( "Rot[0-9]+" );
    m = rotRegExp.indexIn( displaySpec, 0 );
    len = rotRegExp.matchedLength();
    if ( m >= 0 ) {
	rotation = displaySpec.mid( m+3, len-3 ).toInt();
    }

    signal(SIGINT, fn_quit_qvfb);
    signal(SIGTERM, fn_quit_qvfb);

    QVFb mw( displayId, width, height, depth, rotation, skin, displayType );
    mw.setZoom(zoom);
    //app.setMainWidget( &mw );
    mw.enableCursor(cursor);
    mw.show();

    return app.exec();
}

QT_END_NAMESPACE

int main( int argc, char *argv[] )
{
    return QT_PREPEND_NAMESPACE(runQVfb)(argc, argv);
}
