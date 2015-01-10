/***************************************************************************
                          qgscontexthelp.h
               Display context help for a dialog by invoking the
                                 QgsHelpViewer
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
#ifndef QGSCONTEXTHELP_H
#define QGSCONTEXTHELP_H

#include <QObject>
#include <QHash>
#include <QProcess>

/** \ingroup core
 * Provides a context based help browser for a dialog.
 */
class CORE_EXPORT QgsContextHelp : public QObject
{
    Q_OBJECT
  public:
    static void run( QString context );

  private:
    //! Constructor
    QgsContextHelp();
    //! Destructor
    ~QgsContextHelp();

    static QHash<QString, QString> gContextHelpTexts;
    static QString gHelpUrlBase;

    static void init();
};

#endif //QGSCONTEXTHELP_H
