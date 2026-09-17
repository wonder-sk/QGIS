/***************************************************************************
  qgsline3dsymbol_p.cpp
  --------------------------------------
  Date                 : July 2017
  Copyright            : (C) 2017 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsline3dsymbol_p.h"

#include "qgs3drendercontext.h"
#include "qgs3dutils.h"
#include "qgsfeature3dhandler_p.h"
#include "qgsgeos.h"
#include "qgsline3dsymbol.h"
#include "qgslogger.h"
#include "qgsmaterial3dfactory.h"
#include "qgsmaterialcontext.h"
#include "qgsmessagelog.h"
#include "qgsmultilinestring.h"
#include "qgsmultipolygon.h"
#include "qgspolygon.h"
#include "qgssceneobject3d.h"
#include "qgstessellatedpolygongeometry3d.h"
#include "qgstessellator.h"
#include "qgsvectorlayer.h"

#include <QString>

using namespace Qt::StringLiterals;

/// @cond PRIVATE

// -----------


class QgsBufferedLine3DSymbolHandler : public QgsFeature3DHandler
{
  public:
    QgsBufferedLine3DSymbolHandler( const QgsLine3DSymbol *symbol, const QgsFeatureIds &selectedIds )
      : mSymbol( static_cast<QgsLine3DSymbol *>( symbol->clone() ) )
      , mSelectedIds( selectedIds )
    {}

    bool prepare( const Qgs3DRenderContext &context, QSet<QString> &attributeNames, const QgsBox3D &chunkExtent ) override;
    void processFeature( const QgsFeature &feature, const Qgs3DRenderContext &context ) override;
    std::unique_ptr<QgsSceneObject3D> createSceneObject( const Qgs3DRenderContext &context ) override;

  private:
    //! temporary data we will pass to the tessellator
    struct LineData
    {
        std::unique_ptr<QgsTessellator> tessellator;
        QVector<QgsFeatureId> triangleIndexFids;
        QVector<uint> triangleIndexStartingIndices;
    };

    void processPolygon( QgsPolygon *polyBuffered, QgsFeatureId fid, float height, float extrusionHeight, const Qgs3DRenderContext &context, LineData &lineData );

    void addPart( QgsSceneObject3D *object, const Qgs3DRenderContext &context, LineData &lineData, bool selected );

    // input specific for this class
    std::unique_ptr<QgsLine3DSymbol> mSymbol;
    // inputs - generic
    QgsFeatureIds mSelectedIds;
    // outputs
    LineData mLineDataNormal;   //!< Features that are not selected
    LineData mLineDataSelected; //!< Features that are selected
};


bool QgsBufferedLine3DSymbolHandler::prepare( const Qgs3DRenderContext &, QSet<QString> &attributeNames, const QgsBox3D &chunkExtent )
{
  Q_UNUSED( attributeNames )

  mChunkOrigin = chunkExtent.center();
  mChunkOrigin.setZ( 0. ); // set the chunk origin to the bottom of the box, as the tessellator currently always considers origin z to be zero
  mChunkExtent = chunkExtent;

  const bool requiresTextureCoordinates = mSymbol->materialSettings() && mSymbol->materialSettings()->requiresTextureCoordinates();
  const bool requiresTangents = mSymbol->materialSettings() && mSymbol->materialSettings()->requiresTangents();

  const auto makeTessellator = [this, requiresTextureCoordinates, requiresTangents] {
    auto tessellator = std::make_unique<QgsTessellator>();
    tessellator->setOrigin( mChunkOrigin );
    tessellator->setAddNormals( true );
    tessellator->setAddTextureUVs( requiresTextureCoordinates );
    tessellator->setAddTangents( requiresTangents );
    tessellator->setExtrusionFaces( Qgis::ExtrusionFace::Walls | Qgis::ExtrusionFace::Roof );
    tessellator->setTriangulationAlgorithm( Qgis::TriangulationAlgorithm::Earcut );
    return tessellator;
  };

  mLineDataNormal.tessellator = makeTessellator();
  mLineDataSelected.tessellator = makeTessellator();

  return true;
}

void QgsBufferedLine3DSymbolHandler::processFeature( const QgsFeature &feature, const Qgs3DRenderContext &context )
{
  if ( feature.geometry().isNull() )
    return;

  LineData &lineData = mSelectedIds.contains( feature.id() ) ? mLineDataSelected : mLineDataNormal;

  QgsGeometry geom = feature.geometry();
  ( void ) clipGeometryIfTooLarge( geom );

  if ( geom.isEmpty() )
    return;

  const QgsAbstractGeometry *abstractGeom = geom.constGet()->simplifiedTypeRef();

  // segmentize curved geometries if necessary
  if ( QgsWkbTypes::isCurvedType( abstractGeom->wkbType() ) )
  {
    geom = QgsGeometry( abstractGeom->segmentize() );
    abstractGeom = geom.constGet()->simplifiedTypeRef();
  }

  // TODO: configurable
  const int nSegments = 4;
  const Qgis::EndCapStyle endCapStyle = Qgis::EndCapStyle::Round;
  const Qgis::JoinStyle joinStyle = Qgis::JoinStyle::Round;
  const double mitreLimit = 0;

  const QgsGeos engine( abstractGeom );

  double width = mSymbol->width();
  if ( qgsDoubleNear( width, 0 ) )
  {
    // a zero-width buffered line should be treated like a "wall" or "fence" -- we fake this by bumping the width to a very tiny amount,
    // so that we get a very narrow polygon shape to work with...
    width = 0.001;
  }

  QgsAbstractGeometry *buffered = engine.buffer( width / 2., nSegments, endCapStyle, joinStyle, mitreLimit ); // factory
  if ( !buffered )
    return;

  if ( QgsWkbTypes::flatType( buffered->wkbType() ) == Qgis::WkbType::Polygon )
  {
    QgsPolygon *polyBuffered = qgsgeometry_cast<QgsPolygon *>( buffered );
    processPolygon( polyBuffered, feature.id(), mSymbol->offset(), mSymbol->extrusionHeight(), context, lineData );
  }
  else if ( QgsWkbTypes::flatType( buffered->wkbType() ) == Qgis::WkbType::MultiPolygon )
  {
    QgsMultiPolygon *mpolyBuffered = qgsgeometry_cast<QgsMultiPolygon *>( buffered );
    for ( int i = 0; i < mpolyBuffered->numGeometries(); ++i )
    {
      QgsPolygon *polyBuffered = qgsgeometry_cast<QgsPolygon *>( mpolyBuffered->polygonN( i ) )->clone(); // need to clone individual geometry parts
      processPolygon( polyBuffered, feature.id(), mSymbol->offset(), mSymbol->extrusionHeight(), context, lineData );
    }
    delete buffered;
  }
  mFeatureCount++;
}

void QgsBufferedLine3DSymbolHandler::processPolygon( QgsPolygon *polyBuffered, QgsFeatureId fid, float height, float extrusionHeight, const Qgs3DRenderContext &context, LineData &lineData )
{
  Qgs3DUtils::clampAltitudes( polyBuffered, mSymbol->altitudeClamping(), mSymbol->altitudeBinding(), height, context );

  Q_ASSERT( lineData.tessellator->dataVerticesCount() % 3 == 0 );
  const uint startingTriangleIndex = static_cast<uint>( lineData.tessellator->dataVerticesCount() / 3 );
  lineData.triangleIndexStartingIndices.append( startingTriangleIndex );
  lineData.triangleIndexFids.append( fid );
  lineData.tessellator->addPolygon( *polyBuffered, extrusionHeight );
  if ( !lineData.tessellator->error().isEmpty() )
  {
    QgsMessageLog::logMessage( lineData.tessellator->error(), QObject::tr( "3D" ) );
  }

  delete polyBuffered;
}

std::unique_ptr<QgsSceneObject3D> QgsBufferedLine3DSymbolHandler::createSceneObject( const Qgs3DRenderContext &context )
{
  auto object = std::make_unique<QgsSceneObject3D>();

  addPart( object.get(), context, mLineDataNormal, false );
  addPart( object.get(), context, mLineDataSelected, true );

  mZMin = std::min( mLineDataNormal.tessellator->zMinimum(), mLineDataSelected.tessellator->zMinimum() );
  mZMax = std::max( mLineDataNormal.tessellator->zMaximum(), mLineDataSelected.tessellator->zMaximum() );

  if ( object->parts().empty() )
    return nullptr;

  object->setGeoTranslation( mChunkOrigin );
  return object;
}


void QgsBufferedLine3DSymbolHandler::addPart( QgsSceneObject3D *object, const Qgs3DRenderContext &context, LineData &lineData, bool selected )
{
  if ( lineData.tessellator->dataVerticesCount() == 0 )
    return; // nothing to show

  QgsMaterialContext materialContext = QgsMaterialContext::fromRenderContext( context );
  materialContext.setIsSelected( selected );
  materialContext.setIsHighlighted( mHighlightingEnabled );

  const QgsAbstractMaterialSettings *materialSettings = mSymbol->materialSettings();
  std::unique_ptr<QgsMaterial3D> material = QgsMaterial3DFactory::createMaterial( materialSettings, Qgis::MaterialRenderingTechnique::Triangles, materialContext, object );
  if ( !material )
    return;

  const bool requiresTextureCoordinates = materialSettings && materialSettings->requiresTextureCoordinates();
  const bool requiresTangents = materialSettings && materialSettings->requiresTangents();

  auto geometry = std::make_unique<QgsTessellatedPolygonGeometry3D>( true, requiresTextureCoordinates, requiresTangents );
  geometry->setVertexBufferData( lineData.tessellator->vertexBuffer(), lineData.tessellator->uniqueVertexCount() );
  geometry->setIndexBufferData( lineData.tessellator->indexBuffer(), lineData.tessellator->dataVerticesCount() );
  geometry->setTriangleIndex( lineData.triangleIndexFids, lineData.triangleIndexStartingIndices );

  object->addPart( std::move( geometry ), std::move( material ) );
}


// --------------


namespace Qgs3DSymbolImpl
{

  QgsFeature3DHandler *handlerForLine3DSymbol( const QgsVectorLayer *layer, const QgsAbstract3DSymbol *symbol )
  {
    const QgsLine3DSymbol *lineSymbol = dynamic_cast<const QgsLine3DSymbol *>( symbol );
    if ( !lineSymbol )
      return nullptr;

    if ( lineSymbol->renderAsSimpleLines() )
    {
      // TODO: simple lines are drawn as screen space thick lines, which P6 of the QRhi
      // migration owns along with the line material and the rest of the LineStripAdjacency
      // construction sites
      QgsDebugError( u"3D line symbols rendered as simple lines are not supported by the QRhi engine yet"_s );
      return nullptr;
    }

    return new QgsBufferedLine3DSymbolHandler( lineSymbol, layer->selectedFeatureIds() );
  }
} // namespace Qgs3DSymbolImpl

/// @endcond
