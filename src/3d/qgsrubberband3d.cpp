/***************************************************************************
  qgsrubberband3d.cpp
  --------------------------------------
  Date                 : June 2021
  Copyright            : (C) 2021 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsrubberband3d.h"

#include "qgs3d.h"
#include "qgs3dmapscene.h"
#include "qgs3dmapsettings.h"
#include "qgs3dutils.h"
#include "qgslinestring.h"
#include "qgsmarkersymbol.h"
#include "qgsmessagelog.h"
#include "qgspolygon.h"
#include "qgsrhi3dengine.h"
#include "qgssymbollayer.h"
#include "qgssymbollayerutils.h"
#include "qgsvertexid.h"

#include <QColor>
#include <QString>

using namespace Qt::StringLiterals;

/// @cond PRIVATE


QgsRubberBand3D::QgsRubberBand3D( Qgs3DMapScene *scene, const Qgis::GeometryType geometryType )
  : mMapSettings( scene->mapSettings() )
  , mEngine( scene->engine() )
  , mGeometryType( geometryType )
{
  switch ( mGeometryType )
  {
    case Qgis::GeometryType::Point:
    case Qgis::GeometryType::Line:
    case Qgis::GeometryType::Polygon:
      setMarkerType( mMarkerType );
      break;
    case Qgis::GeometryType::Null:
    case Qgis::GeometryType::Unknown:
      QgsDebugError( "Unknown GeometryType used in QgsRubberband3D" );
      break;
  }
}

void QgsRubberBand3D::removePoint( int index )
{
  if ( QgsPolygon *polygon = qgsgeometry_cast<QgsPolygon *>( mGeometry.get() ) )
  {
    QgsLineString *lineString = qgsgeometry_cast<QgsLineString *>( polygon->exteriorRing() );
    const int vertexIndex = index < 0 ? lineString->numPoints() - 1 + index : index;
    lineString->deleteVertex( QgsVertexId( 0, 0, vertexIndex ) );

    if ( lineString->numPoints() < 3 )
    {
      mGeometry.set( new QgsLineString( *lineString ) );
    }
  }
  else if ( QgsLineString *lineString = qgsgeometry_cast<QgsLineString *>( mGeometry.get() ) )
  {
    const int vertexIndex = index < 0 ? lineString->numPoints() + index : index;
    lineString->deleteVertex( QgsVertexId( 0, 0, vertexIndex ) );
  }
  else
  {
    return;
  }

  updateGeometry();
}

QgsRubberBand3D::~QgsRubberBand3D() = default;


float QgsRubberBand3D::width() const
{
  return mWidth;
}

void QgsRubberBand3D::setWidth( float width )
{
  const bool isLineOrPolygon = mGeometryType == Qgis::GeometryType::Line || mGeometryType == Qgis::GeometryType::Polygon;
  mWidth = width;

  if ( isLineOrPolygon && mEdgesEnabled )
  {
    // when highlighting lines, the vertex markers should be wider
    width *= 3;
  }

  mMarkerSymbol->setSize( width );
}

QColor QgsRubberBand3D::color() const
{
  return mColor;
}

void QgsRubberBand3D::setColor( const QColor color )
{
  const bool isLineOrPolygon = mGeometryType == Qgis::GeometryType::Line || mGeometryType == Qgis::GeometryType::Polygon;
  mColor = color;

  if ( isLineOrPolygon )
  {
    mMarkerSymbol->setColor( color.lighter( 130 ) );
  }
  else
  {
    mMarkerSymbol->setColor( color );
  }

  if ( mMarkerSymbol->symbolLayerCount() > 0 && mMarkerSymbol->symbolLayer( 0 )->layerType() == "SimpleMarker"_L1 && !mOutlineColor.value() )
  {
    mMarkerSymbol->symbolLayer( 0 )->setStrokeColor( color );
  }
}

QColor QgsRubberBand3D::outlineColor() const
{
  return mOutlineColor;
}

void QgsRubberBand3D::setOutlineColor( const QColor color )
{
  mOutlineColor = color;

  if ( mMarkerSymbol->symbolLayerCount() > 0 && mMarkerSymbol->symbolLayer( 0 )->layerType() == "SimpleMarker"_L1 )
  {
    mMarkerSymbol->symbolLayer( 0 )->setStrokeColor( color );
  }
}

void QgsRubberBand3D::setMarkerType( const MarkerType marker )
{
  mMarkerType = marker;

  const bool lineOrPolygon = mGeometryType == Qgis::GeometryType::Line || mGeometryType == Qgis::GeometryType::Polygon;

  const QVariantMap props {
    { u"color"_s, lineOrPolygon ? mColor.lighter( 130 ).name() : mColor.name() },
    { u"size_unit"_s, u"pixel"_s },
    { u"size"_s, QString::number( lineOrPolygon ? mWidth * 3.f : mWidth ) },
    { u"outline_color"_s, mOutlineColor.value() ? mOutlineColor.name() : mColor.name() },
    { u"outline_style"_s, QgsSymbolLayerUtils::encodePenStyle( mMarkerOutlineStyle ) },
    { u"outline_width"_s, QString::number( lineOrPolygon ? 0.5 : 1 ) },
    { u"name"_s, mMarkerType == Square ? u"square"_s : u"circle"_s }
  };

  mMarkerSymbol = QgsMarkerSymbol::createSimple( props );
}

QgsRubberBand3D::MarkerType QgsRubberBand3D::markerType() const
{
  return mMarkerType;
}

void QgsRubberBand3D::setMarkerOutlineStyle( const Qt::PenStyle style )
{
  mMarkerOutlineStyle = style;
  setMarkerType( markerType() );
}

Qt::PenStyle QgsRubberBand3D::markerOutlineStyle() const
{
  return mMarkerOutlineStyle;
}

void QgsRubberBand3D::setMarkersEnabled( const bool enable )
{
  mMarkerEnabled = enable;
}

bool QgsRubberBand3D::hasMarkersEnabled() const
{
  return mMarkerEnabled;
}

void QgsRubberBand3D::setEdgesEnabled( const bool enable )
{
  mEdgesEnabled = enable;
  setColor( mColor );
}

bool QgsRubberBand3D::hasEdgesEnabled() const
{
  return mEdgesEnabled;
}

void QgsRubberBand3D::setFillEnabled( const bool enable )
{
  mPolygonFillEnabled = enable;
  setColor( mColor );
}

bool QgsRubberBand3D::hasFillEnabled() const
{
  return mPolygonFillEnabled;
}

void QgsRubberBand3D::reset()
{
  mGeometry.set( nullptr );
  updateGeometry();
}

void QgsRubberBand3D::addPoint( const QgsPoint &pt )
{
  if ( QgsPolygon *polygon = qgsgeometry_cast<QgsPolygon *>( mGeometry.get() ) )
  {
    QgsLineString *exteriorRing = qgsgeometry_cast<QgsLineString *>( polygon->exteriorRing() );
    const int lastVertexIndex = exteriorRing->numPoints() - 1;
    exteriorRing->insertVertex( QgsVertexId( 0, 0, lastVertexIndex ), pt );
  }
  else if ( QgsLineString *lineString = qgsgeometry_cast<QgsLineString *>( mGeometry.get() ) )
  {
    lineString->addVertex( pt );
    // transform linestring to polygon if we have enough vertices
    if ( mGeometryType == Qgis::GeometryType::Polygon && lineString->numPoints() >= 3 )
    {
      mGeometry.set( new QgsPolygon( lineString->clone() ) );
    }
  }
  else if ( !mGeometry.constGet() )
  {
    mGeometry.set( new QgsLineString( QVector<QgsPoint> { pt } ) );
  }

  updateGeometry();
}

void QgsRubberBand3D::setGeometry( const QgsGeometry &geometry )
{
  mGeometry = geometry;
  mGeometryType = geometry.type();

  updateGeometry();
}

void QgsRubberBand3D::removeLastPoint()
{
  removePoint( -1 );
}

void QgsRubberBand3D::removePenultimatePoint()
{
  removePoint( -2 );
}

void QgsRubberBand3D::moveLastPoint( const QgsPoint &pt )
{
  if ( QgsPolygon *polygon = qgsgeometry_cast<QgsPolygon *>( mGeometry.get() ) )
  {
    QgsLineString *lineString = qgsgeometry_cast<QgsLineString *>( polygon->exteriorRing() );
    const int lastVertexIndex = lineString->numPoints() - 2;
    lineString->moveVertex( QgsVertexId( 0, 0, lastVertexIndex ), pt );
  }
  else if ( QgsLineString *lineString = qgsgeometry_cast<QgsLineString *>( mGeometry.get() ) )
  {
    const int lastVertexIndex = lineString->numPoints() - 1;
    lineString->moveVertex( QgsVertexId( 0, 0, lastVertexIndex ), pt );
  }
  else
  {
    return;
  }

  updateGeometry();
}

void QgsRubberBand3D::updateGeometry()
{
  // TODO: draw the rubber band. It needs thick lines and billboarded markers, which are
  // the two primitives the QRhi engine does not have yet - see P6 of the port. The
  // geometry is still kept up to date by the callers above, so the map tools that drive
  // a rubber band work; they simply draw nothing.
}

/// @endcond
