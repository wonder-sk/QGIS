/***************************************************************************
  qgsquickutils.cpp
  --------------------------------------
  Date                 : Nov 2017
  Copyright            : (C) 2017 by Peter Petrik
  Email                : zilolv at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QApplication>
#include <QDesktopWidget>
#include <QString>

#include "qgis.h"
#include "qgscoordinatereferencesystem.h"
#include "qgscoordinatetransform.h"
#include "qgsdistancearea.h"
#include "qgslogger.h"
#include "qgsvectorlayer.h"
#include "qgsfeature.h"
#include "qgsapplication.h"
#include "qgsvaluerelationfieldformatter.h"

#include "qgsquickfeaturelayerpair.h"
#include "qgsquickmapsettings.h"
#include "qgsquickutils.h"
#include "qgsunittypes.h"


QgsQuickUtils::QgsQuickUtils( QObject *parent )
  : QObject( parent )
  , mScreenDensity( calculateScreenDensity() )
{
}

/**
 * Makes QgsCoordinateReferenceSystem::fromEpsgId accessible for QML components
 */
QgsCoordinateReferenceSystem QgsQuickUtils::coordinateReferenceSystemFromEpsgId( long epsg )
{
  return QgsCoordinateReferenceSystem::fromEpsgId( epsg );
}

QgsPointXY QgsQuickUtils::pointXY( double x, double y )
{
  return QgsPointXY( x, y );
}

QgsPoint QgsQuickUtils::point( double x, double y, double z, double m )
{
  return QgsPoint( x, y, z, m );
}

QgsPoint QgsQuickUtils::coordinateToPoint( const QGeoCoordinate &coor )
{
  return QgsPoint( coor.longitude(), coor.latitude(), coor.altitude() );
}

QgsPointXY QgsQuickUtils::transformPoint( const QgsCoordinateReferenceSystem &srcCrs,
    const QgsCoordinateReferenceSystem &destCrs,
    const QgsCoordinateTransformContext &context,
    const QgsPointXY &srcPoint )
{
  QgsCoordinateTransform mTransform( srcCrs, destCrs, context );
  QgsPointXY pt = mTransform.transform( srcPoint );
  return pt;
}

double QgsQuickUtils::screenUnitsToMeters( QgsQuickMapSettings *mapSettings, int baseLengthPixels )
{
  if ( mapSettings == nullptr ) return 0.0;

  QgsDistanceArea mDistanceArea;
  mDistanceArea.setEllipsoid( QStringLiteral( "WGS84" ) );
  mDistanceArea.setSourceCrs( mapSettings->destinationCrs(), mapSettings->transformContext() );

  // calculate the geographic distance from the central point of extent
  // to the specified number of points on the right side
  QSize s = mapSettings->outputSize();
  QPoint pointCenter( s.width() / 2, s.height() / 2 );
  QgsPointXY p1 = mapSettings->screenToCoordinate( pointCenter );
  QgsPointXY p2 = mapSettings->screenToCoordinate( pointCenter + QPoint( baseLengthPixels, 0 ) );
  return mDistanceArea.measureLine( p1, p2 );
}

bool QgsQuickUtils::fileExists( const QString &path )
{
  QFileInfo check_file( path );
  // check if file exists and if yes: Is it really a file and no directory?
  return ( check_file.exists() && check_file.isFile() );
}

QString QgsQuickUtils::getRelativePath( const QString &path, const QString &prefixPath )
{
  QString modPath = path;
  QString filePrefix( "file://" );

  if ( path.startsWith( filePrefix ) )
  {
    modPath = modPath.replace( filePrefix, QString() );
  }

  if ( prefixPath.isEmpty() ) return modPath;

  // Do not use a canonical path for non-existing path
  if ( !QFileInfo( path ).exists() )
  {
    if ( !prefixPath.isEmpty() && modPath.startsWith( prefixPath ) )
    {
      return modPath.replace( prefixPath, QString() );
    }
  }
  else
  {
    QDir absoluteDir( modPath );
    QDir prefixDir( prefixPath );
    QString canonicalPath = absoluteDir.canonicalPath();
    QString prefixCanonicalPath = prefixDir.canonicalPath() + "/";

    if ( prefixCanonicalPath.length() > 1 && canonicalPath.startsWith( prefixCanonicalPath ) )
    {
      return canonicalPath.replace( prefixCanonicalPath, QString() );
    }
  }

  return QString();
}

void QgsQuickUtils::logMessage( const QString &message, const QString &tag, Qgis::MessageLevel level )
{
  QgsMessageLog::logMessage( message, tag, level );
}

QgsQuickFeatureLayerPair QgsQuickUtils::featureFactory( const QgsFeature &feature, QgsVectorLayer *layer )
{
  return QgsQuickFeatureLayerPair( feature, layer );
}

const QUrl QgsQuickUtils::getThemeIcon( const QString &name )
{
  QString path = QStringLiteral( "qrc:/%1.svg" ).arg( name );
  QgsDebugMsg( QStringLiteral( "Using icon %1 from %2" ).arg( name, path ) );
  return QUrl( path );
}

const QUrl QgsQuickUtils::getEditorComponentSource( const QString &widgetName )
{
  QString path( "qgsquick%1.qml" );
  QStringList supportedWidgets = { QStringLiteral( "textedit" ),
                                   QStringLiteral( "valuemap" ),
                                   QStringLiteral( "valuerelation" ),
                                   QStringLiteral( "checkbox" ),
                                   QStringLiteral( "externalresource" ),
                                   QStringLiteral( "datetime" ),
                                   QStringLiteral( "range" )
                                 };
  if ( supportedWidgets.contains( widgetName ) )
  {
    return QUrl( path.arg( widgetName ) );
  }
  else
  {
    return QUrl( path.arg( QStringLiteral( "textedit" ) ) );
  }
}

QString QgsQuickUtils::formatPoint(
  const QgsPoint &point,
  QgsCoordinateFormatter::Format format,
  int decimals,
  QgsCoordinateFormatter::FormatFlags flags )
{
  return QgsCoordinateFormatter::format( point, format, decimals, flags );
}

QString QgsQuickUtils::formatDistance( double distance,
                                       QgsUnitTypes::DistanceUnit units,
                                       int decimals,
                                       QgsUnitTypes::SystemOfMeasurement destSystem )
{
  double destDistance;
  QgsUnitTypes::DistanceUnit destUnits;

  humanReadableDistance( distance, units, destSystem, destDistance, destUnits );

  return QStringLiteral( "%1 %2" )
         .arg( QString::number( destDistance, 'f', decimals ) )
         .arg( QgsUnitTypes::toAbbreviatedString( destUnits ) );
}

bool QgsQuickUtils::removeFile( const QString &filePath )
{
  QFile file( filePath );
  return file.remove( filePath );
}


void QgsQuickUtils::humanReadableDistance( double srcDistance, QgsUnitTypes::DistanceUnit srcUnits,
    QgsUnitTypes::SystemOfMeasurement destSystem,
    double &destDistance, QgsUnitTypes::DistanceUnit &destUnits )
{
  if ( ( destSystem == QgsUnitTypes::MetricSystem ) || ( destSystem == QgsUnitTypes::UnknownSystem ) )
  {
    return formatToMetricDistance( srcDistance, srcUnits, destDistance, destUnits );
  }
  else if ( destSystem == QgsUnitTypes::ImperialSystem )
  {
    return formatToImperialDistance( srcDistance, srcUnits, destDistance, destUnits );
  }
  else if ( destSystem == QgsUnitTypes::USCSSystem )
  {
    return formatToUSCSDistance( srcDistance, srcUnits, destDistance, destUnits );
  }
  else
  {
    Q_ASSERT( false ); //should never happen
  }
}

void QgsQuickUtils::formatToMetricDistance( double srcDistance,
    QgsUnitTypes::DistanceUnit srcUnits,
    double &destDistance,
    QgsUnitTypes::DistanceUnit &destUnits )
{
  double dist = srcDistance * QgsUnitTypes::fromUnitToUnitFactor( srcUnits, QgsUnitTypes::DistanceMillimeters );
  if ( dist < 0 )
  {
    destDistance = 0;
    destUnits = QgsUnitTypes::DistanceMillimeters;
    return;
  }

  double mmToKm = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceKilometers, QgsUnitTypes::DistanceMillimeters );
  if ( dist > mmToKm )
  {
    destDistance = dist / mmToKm;
    destUnits = QgsUnitTypes::DistanceKilometers;
    return;
  }

  double mmToM = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceMeters, QgsUnitTypes::DistanceMillimeters );
  if ( dist > mmToM )
  {
    destDistance = dist / mmToM;
    destUnits = QgsUnitTypes::DistanceMeters;
    return;
  }

  double mmToCm = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceCentimeters, QgsUnitTypes::DistanceMillimeters );
  if ( dist > mmToCm )
  {
    destDistance = dist / mmToCm;
    destUnits = QgsUnitTypes::DistanceCentimeters;
    return;
  }

  destDistance = dist;
  destUnits = QgsUnitTypes::DistanceMillimeters;
}

void QgsQuickUtils::formatToImperialDistance( double srcDistance,
    QgsUnitTypes::DistanceUnit srcUnits,
    double &destDistance,
    QgsUnitTypes::DistanceUnit &destUnits )
{
  double dist = srcDistance * QgsUnitTypes::fromUnitToUnitFactor( srcUnits, QgsUnitTypes::DistanceFeet );
  if ( dist < 0 )
  {
    destDistance = 0;
    destUnits = QgsUnitTypes::DistanceFeet;
    return;
  }

  double feetToMile = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceMiles, QgsUnitTypes::DistanceFeet );
  if ( dist > feetToMile )
  {
    destDistance = dist / feetToMile;
    destUnits = QgsUnitTypes::DistanceMiles;
    return;
  }

  double feetToYard = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceYards, QgsUnitTypes::DistanceFeet );
  if ( dist > feetToYard )
  {
    destDistance = dist / feetToYard;
    destUnits = QgsUnitTypes::DistanceYards;
    return;
  }

  destDistance = dist;
  destUnits = QgsUnitTypes::DistanceFeet;
  return;
}

void QgsQuickUtils::formatToUSCSDistance( double srcDistance,
    QgsUnitTypes::DistanceUnit srcUnits,
    double &destDistance,
    QgsUnitTypes::DistanceUnit &destUnits )
{
  double dist = srcDistance * QgsUnitTypes::fromUnitToUnitFactor( srcUnits, QgsUnitTypes::DistanceFeet );
  if ( dist < 0 )
  {
    destDistance = 0;
    destUnits = QgsUnitTypes::DistanceFeet;
    return;
  }

  double feetToMile = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceNauticalMiles, QgsUnitTypes::DistanceFeet );
  if ( dist > feetToMile )
  {
    destDistance = dist / feetToMile;
    destUnits = QgsUnitTypes::DistanceNauticalMiles;
    return;
  }

  double feetToYard = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceYards, QgsUnitTypes::DistanceFeet );
  if ( dist > feetToYard )
  {
    destDistance = dist / feetToYard;
    destUnits = QgsUnitTypes::DistanceYards;
    return;
  }

  destDistance = dist;
  destUnits = QgsUnitTypes::DistanceFeet;
  return;
}

QString QgsQuickUtils::dumpScreenInfo() const
{
  QRect rec = QApplication::desktop()->screenGeometry();
  int dpiX = QApplication::desktop()->physicalDpiX();
  int dpiY = QApplication::desktop()->physicalDpiY();
  int height = rec.height();
  int width = rec.width();
  double sizeX = static_cast<double>( width ) / dpiX * 25.4;
  double sizeY = static_cast<double>( height ) / dpiY * 25.4;

  QString msg;
  msg += tr( "screen resolution: %1x%2 px\n" ).arg( width ).arg( height );
  msg += tr( "screen DPI: %1x%2\n" ).arg( dpiX ).arg( dpiY );
  msg += tr( "screen size: %1x%2 mm\n" ).arg( QString::number( sizeX, 'f', 0 ), QString::number( sizeY, 'f', 0 ) );
  msg += tr( "screen density: %1" ).arg( mScreenDensity );
  return msg;
}

QVariantMap QgsQuickUtils::createValueRelationCache( const QVariantMap &config, const QgsFeature &formFeature )
{
  QVariantMap valueMap;
  QgsValueRelationFieldFormatter::ValueRelationCache cache = QgsValueRelationFieldFormatter::createCache( config, formFeature );

  for ( const QgsValueRelationFieldFormatter::ValueRelationItem &item : qgis::as_const( cache ) )
  {
    valueMap.insert( item.key.toString(), item.value );
  }
  return valueMap;
}

QString QgsQuickUtils::evaluateExpression( const QgsQuickFeatureLayerPair &pair, QgsProject *activeProject, const QString &expression )
{
  QList<QgsExpressionContextScope *> scopes;
  scopes << QgsExpressionContextUtils::globalScope();
  scopes << QgsExpressionContextUtils::projectScope( activeProject );
  scopes << QgsExpressionContextUtils::layerScope( pair.layer() );

  QgsExpressionContext context( scopes );
  context.setFeature( pair.feature() );
  QgsExpression expr( expression );
  return expr.evaluate( &context ).toString();
}

void QgsQuickUtils::selectFeaturesInLayer( QgsVectorLayer *layer, const QList<int> &fids, QgsVectorLayer::SelectBehavior behavior )
{
  QgsFeatureIds qgsFids;
  for ( const int &fid : fids )
    qgsFids << fid;
  layer->selectByIds( qgsFids, behavior );
}

qreal QgsQuickUtils::screenDensity() const
{
  return mScreenDensity;
}

qreal QgsQuickUtils::calculateScreenDensity()
{
  // calculate screen density for calculation of real pixel sizes from density-independent pixels
  int dpiX = QApplication::desktop()->physicalDpiX();
  int dpiY = QApplication::desktop()->physicalDpiY();
  int dpi = dpiX < dpiY ? dpiX : dpiY; // In case of asymmetrical DPI. Improbable
  return dpi / 160.;  // 160 DPI is baseline for density-independent pixels in Android
}

// ------------

QgsQuickValueRelationListModel::QgsQuickValueRelationListModel( QObject *parent )
  : QAbstractListModel( parent )
{
}

void QgsQuickValueRelationListModel::populate( const QVariantMap &config, const QgsFeature &formFeature )
{
  beginResetModel();
  mCache = QgsValueRelationFieldFormatter::createCache( config, formFeature );
  endResetModel();
}

QVariant QgsQuickValueRelationListModel::keyForRow( int row ) const
{
  if ( row < 0 || row >= mCache.count() )
  {
    QgsDebugMsg( "keyForRow: access outside of range " + QString::number( row ) );
    return QVariant();
  }
  return mCache[row].key;
}

int QgsQuickValueRelationListModel::rowForKey( const QVariant &key ) const
{
  for ( int i = 0; i < mCache.count(); ++i )
  {
    if ( mCache[i].key == key )
      return i;
  }
  QgsDebugMsg( "rowForKey: key not found: " + key.toString() );
  return -1;
}

int QgsQuickValueRelationListModel::rowCount( const QModelIndex & ) const
{
  return mCache.count();
}

QVariant QgsQuickValueRelationListModel::data( const QModelIndex &index, int role ) const
{
  if ( !index.isValid() )
    return QVariant();

  if ( role == Qt::DisplayRole )
    return mCache[index.row()].value;

  return QVariant();
}
