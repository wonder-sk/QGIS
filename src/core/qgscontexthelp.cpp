/***************************************************************************
                          qgscontexthelp.cpp
                    Display context help for a dialog
                             -------------------
    begin                : 2005-06-19
    copyright            : (C) 2005 by Gary E.Sherman
    email                : sherman at mrcc.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QDesktopServices>
#include <QLocale>
#include <QSettings>
#include <QUrl>

#include "qgscontexthelp.h"
#include "qgsmessagelog.h"
#include "qgsapplication.h"
#include "qgslogger.h"

QString QgsContextHelp::gHelpUrlBase;


void QgsContextHelp::run( QString context )
{
  init();

  if ( gHelpUrlBase.isEmpty() )
  {
    // TODO: be able to switch to something else
    // TODO: be able to use locally installed documentation

    QSettings settings;
    bool overrideLocale = settings.value( "locale/overrideFlag", false ).toBool();
    QString userLocale = settings.value( "locale/userLocale", QString() ).toString();
    QString localeName = QLocale::system().name();  // e.g. "en_US"
    if ( overrideLocale && !userLocale.isEmpty() )
      localeName = userLocale;

    QString helpLanguage = localeName.split( '_' ).first();
    QString helpVersion = QString( "%1.%2" )
        .arg( QGis::QGIS_VERSION_INT / 10000 )
        .arg( QGis::QGIS_VERSION_INT / 100 % 100 );
    helpVersion = "2.6";

    gHelpUrlBase = QString( "http://docs.qgis.org/%1/%2/docs/user_manual/" )
           .arg( helpVersion )
           .arg( helpLanguage );
  }


  if ( !gContextHelpTexts.contains( context ) )
  {
    // TODO: say sorry
    return;
  }

  QUrl url( gHelpUrlBase + gContextHelpTexts[context] );
  QDesktopServices::openUrl( url );
}

QgsContextHelp::QgsContextHelp()
{
}

QgsContextHelp::~QgsContextHelp()
{
}
