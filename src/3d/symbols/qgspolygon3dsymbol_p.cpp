/***************************************************************************
  qgspolygon3dsymbol_p.cpp
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

#include "qgspolygon3dsymbol_p.h"

#include "qgs3drendercontext.h"
#include "qgs3dutils.h"
#include "qgsfeature3dhandler_p.h"
#include "qgsmaterial3dfactory.h"
#include "qgsmaterial3dutils.h"
#include "qgsmaterialcontext.h"
#include "qgsmessagelog.h"
#include "qgsmetalroughtexturedmaterialsettings.h"
#include "qgsmultipolygon.h"
#include "qgsphongtexturedmaterialsettings.h"
#include "qgspolygon.h"
#include "qgspolygon3dsymbol.h"
#include "qgspolyhedralsurface.h"
#include "qgssceneobject3d.h"
#include "qgssymbollayerutils.h"
#include "qgstessellatedpolygongeometry3d.h"
#include "qgstessellator.h"
#include "qgsvectorlayer.h"

#include <QString>

using namespace Qt::StringLiterals;

/// @cond PRIVATE


class QgsPolygon3DSymbolHandler : public QgsFeature3DHandler
{
  public:
    QgsPolygon3DSymbolHandler( const QgsPolygon3DSymbol *symbol, const QgsFeatureIds &selectedIds )
      : mSymbol( static_cast<QgsPolygon3DSymbol *>( symbol->clone() ) )
      , mSelectedIds( selectedIds )
    {}

    bool prepare( const Qgs3DRenderContext &context, QSet<QString> &attributeNames, const QgsBox3D &chunkExtent ) override;
    void processFeature( const QgsFeature &f, const Qgs3DRenderContext &context ) override;
    std::unique_ptr<QgsSceneObject3D> createSceneObject( const Qgs3DRenderContext &context ) override;

  private:
    //! temporary data we will pass to the tessellator
    struct PolygonData
    {
        std::unique_ptr<QgsTessellator> tessellator;
        QVector<QgsFeatureId> triangleIndexFids;
        QVector<uint> triangleIndexStartingIndices;
        QByteArray materialDataDefined;
        QVector<float> ddTextureTransformData;
    };

    void processPolygon(
      const QgsPolygon *poly,
      QgsFeatureId fid,
      float offset,
      float extrusionHeight,
      float dataDefinedTextureScale,
      float dataDefinedTextureRotation,
      float dataDefinedTextureOffsetX,
      float dataDefinedTextureOffsetY,
      const Qgs3DRenderContext &context,
      PolygonData &out
    );
    void processMaterialDatadefined( uint verticesCount, const QgsExpressionContext &context, PolygonData &out );
    void addPart( QgsSceneObject3D *object, const Qgs3DRenderContext &context, PolygonData &out, bool selected );

    // input specific for this class
    std::unique_ptr<QgsPolygon3DSymbol> mSymbol;
    // inputs - generic
    QgsFeatureIds mSelectedIds;
    // outputs
    PolygonData outNormal;   //!< Features that are not selected
    PolygonData outSelected; //!< Features that are selected
};


bool QgsPolygon3DSymbolHandler::prepare( const Qgs3DRenderContext &context, QSet<QString> &attributeNames, const QgsBox3D &chunkExtent )
{
  mChunkOrigin = chunkExtent.center();
  mChunkOrigin.setZ( 0. ); // set the chunk origin to the bottom of the box, as the tessellator currently always considers origin z to be zero
  mChunkExtent = chunkExtent;

  const QgsAbstractMaterialSettings *materialSettings = mSymbol->materialSettings();
  const bool requiresTextureCoordinates = materialSettings->requiresTextureCoordinates();
  const bool requiresTangents = materialSettings->requiresTangents();

  const auto makeTessellator = [this, requiresTextureCoordinates, requiresTangents] {
    auto tessellator = std::make_unique<QgsTessellator>();
    tessellator->setOrigin( mChunkOrigin );
    tessellator->setAddNormals( true );
    tessellator->setInvertNormals( mSymbol->invertNormals() );
    tessellator->setBackFacesEnabled( mSymbol->addBackFaces() );
    tessellator->setExtrusionFaces( mSymbol->extrusionFaces() );
    tessellator->setAddTextureUVs( requiresTextureCoordinates );
    tessellator->setAddTangents( requiresTangents );
    tessellator->setTriangulationAlgorithm( Qgis::TriangulationAlgorithm::Earcut );
    return tessellator;
  };

  outNormal.tessellator = makeTessellator();
  outSelected.tessellator = makeTessellator();

  QSet<QString> attrs = mSymbol->dataDefinedProperties().referencedFields( context.expressionContext() );
  attributeNames.unite( attrs );
  attrs = materialSettings->dataDefinedProperties().referencedFields( context.expressionContext() );
  attributeNames.unite( attrs );
  return true;
}

void QgsPolygon3DSymbolHandler::processPolygon(
  const QgsPolygon *poly,
  QgsFeatureId fid,
  float offset,
  float extrusionHeight,
  float dataDefinedTextureScale,
  float dataDefinedTextureRotation,
  float dataDefinedTextureOffsetX,
  float dataDefinedTextureOffsetY,
  const Qgs3DRenderContext &context,
  PolygonData &out
)
{
  std::unique_ptr<QgsPolygon> polyClone( poly->clone() );

  // TODO: polygon edges are drawn as thick lines, which P6 of the QRhi migration owns -
  // mSymbol->edgesEnabled() has no effect until the line material is ported

  Qgs3DUtils::clampAltitudes( polyClone.get(), mSymbol->altitudeClamping(), mSymbol->altitudeBinding(), offset, context );

  Q_ASSERT( out.tessellator->dataVerticesCount() % 3 == 0 );
  const uint startingTriangleIndex = static_cast<uint>( out.tessellator->dataVerticesCount() / 3 );
  out.triangleIndexStartingIndices.append( startingTriangleIndex );
  out.triangleIndexFids.append( fid );

  const size_t oldVertexCount = out.tessellator->uniqueVertexCount();
  out.tessellator->addPolygon( *polyClone, extrusionHeight );
  if ( !out.tessellator->error().isEmpty() )
  {
    QgsMessageLog::logMessage( out.tessellator->error(), QObject::tr( "3D" ) );
  }

  const QgsPropertyCollection &dataDefinedProperties = mSymbol->materialSettings()->dataDefinedProperties();
  if ( dataDefinedProperties.hasActiveProperties() )
  {
    const uint newUniqueVertices = out.tessellator->uniqueVertexCount() - oldVertexCount;
    processMaterialDatadefined( newUniqueVertices, context.expressionContext(), out );

    if ( dataDefinedProperties.isActive( QgsAbstractMaterialSettings::Property::TextureScale )
         || dataDefinedProperties.isActive( QgsAbstractMaterialSettings::Property::TextureRotation )
         || dataDefinedProperties.isActive( QgsAbstractMaterialSettings::Property::TextureOffset ) )
    {
      for ( uint i = 0; i < newUniqueVertices; ++i )
      {
        out.ddTextureTransformData << dataDefinedTextureOffsetX << dataDefinedTextureOffsetY << dataDefinedTextureScale << dataDefinedTextureRotation;
      }
    }
  }
}

void QgsPolygon3DSymbolHandler::processMaterialDatadefined( uint verticesCount, const QgsExpressionContext &context, QgsPolygon3DSymbolHandler::PolygonData &out )
{
  const QByteArray bytes = QgsMaterial3DFactory::dataDefinedColors( mSymbol->materialSettings(), context );
  out.materialDataDefined.append( bytes.repeated( static_cast<int>( verticesCount ) ) );
}

void QgsPolygon3DSymbolHandler::processFeature( const QgsFeature &f, const Qgs3DRenderContext &context )
{
  if ( f.geometry().isNull() )
    return;

  PolygonData &out = mSelectedIds.contains( f.id() ) ? outSelected : outNormal;

  QgsGeometry geom = f.geometry();
  ( void ) clipGeometryIfTooLarge( geom );

  if ( geom.isEmpty() )
    return;

  const QgsAbstractGeometry *g = geom.constGet()->simplifiedTypeRef();

  // segmentize curved geometries if necessary
  if ( QgsWkbTypes::isCurvedType( g->wkbType() ) )
  {
    geom = QgsGeometry( g->segmentize() );
    g = geom.constGet()->simplifiedTypeRef();
  }

  const QgsPropertyCollection &ddp = mSymbol->dataDefinedProperties();
  const bool hasDDHeight = ddp.isActive( QgsAbstract3DSymbol::Property::Height );
  const bool hasDDExtrusion = ddp.isActive( QgsAbstract3DSymbol::Property::ExtrusionHeight );

  float offset = mSymbol->offset();
  float extrusionHeight = mSymbol->extrusionHeight();
  if ( hasDDHeight )
    offset = static_cast<float>( ddp.valueAsDouble( QgsAbstract3DSymbol::Property::Height, context.expressionContext(), offset ) );
  if ( hasDDExtrusion )
    extrusionHeight = ddp.valueAsDouble( QgsAbstract3DSymbol::Property::ExtrusionHeight, context.expressionContext(), extrusionHeight );

  const QgsAbstractMaterialSettings *materialSettings = mSymbol->materialSettings();
  const bool hasTextureScale = materialSettings
                               && materialSettings->dataDefinedProperties().isActive( QgsAbstractMaterialSettings::Property::TextureScale )
                               && materialSettings->supportedProperties().contains( QgsAbstractMaterialSettings::Property::TextureScale );
  const bool hasTextureRotation = materialSettings
                                  && materialSettings->dataDefinedProperties().isActive( QgsAbstractMaterialSettings::Property::TextureRotation )
                                  && materialSettings->supportedProperties().contains( QgsAbstractMaterialSettings::Property::TextureRotation );
  const bool hasTextureOffset = materialSettings
                                && materialSettings->dataDefinedProperties().isActive( QgsAbstractMaterialSettings::Property::TextureOffset )
                                && materialSettings->supportedProperties().contains( QgsAbstractMaterialSettings::Property::TextureOffset );
  float dataDefinedTextureRotation = 0;
  float dataDefinedTextureScale = 1;
  float dataDefinedTextureOffsetX = 0;
  float dataDefinedTextureOffsetY = 0;
  if ( auto phongTexturedMaterial = dynamic_cast<const QgsPhongTexturedMaterialSettings *>( materialSettings ) )
  {
    dataDefinedTextureRotation = static_cast<float>( phongTexturedMaterial->textureRotation() );
    dataDefinedTextureScale = static_cast<float>( phongTexturedMaterial->textureScale() );
    dataDefinedTextureOffsetX = static_cast<float>( phongTexturedMaterial->textureOffset().x() );
    dataDefinedTextureOffsetY = static_cast<float>( phongTexturedMaterial->textureOffset().y() );
  }
  else if ( auto metalRoughTexturedMaterial = dynamic_cast<const QgsMetalRoughTexturedMaterialSettings *>( materialSettings ) )
  {
    dataDefinedTextureRotation = static_cast<float>( metalRoughTexturedMaterial->textureRotation() );
    dataDefinedTextureScale = static_cast<float>( metalRoughTexturedMaterial->textureScale() );
    dataDefinedTextureOffsetX = static_cast<float>( metalRoughTexturedMaterial->textureOffset().x() );
    dataDefinedTextureOffsetY = static_cast<float>( metalRoughTexturedMaterial->textureOffset().y() );
  }

  if ( hasTextureScale || hasTextureRotation || hasTextureOffset )
  {
    dataDefinedTextureRotation = static_cast<float>(
      materialSettings->dataDefinedProperties().valueAsDouble( QgsAbstractMaterialSettings::Property::TextureRotation, context.expressionContext(), dataDefinedTextureRotation )
    );
    dataDefinedTextureScale = static_cast<float>(
      100 / materialSettings->dataDefinedProperties().valueAsDouble( QgsAbstractMaterialSettings::Property::TextureScale, context.expressionContext(), 100 / dataDefinedTextureScale )
    );

    const QVariant offsetValue = materialSettings->dataDefinedProperties().value( QgsAbstractMaterialSettings::Property::TextureOffset, context.expressionContext() );
    bool ok = false;
    const QPointF ddOffset = QgsSymbolLayerUtils::toPoint( offsetValue, &ok );
    if ( ok )
    {
      dataDefinedTextureOffsetX = static_cast<float>( ddOffset.x() );
      dataDefinedTextureOffsetY = static_cast<float>( ddOffset.y() );
    }
  }

  if ( const QgsPolygon *poly = qgsgeometry_cast<const QgsPolygon *>( g ) )
  {
    processPolygon( poly, f.id(), offset, extrusionHeight, dataDefinedTextureScale, dataDefinedTextureRotation, dataDefinedTextureOffsetX, dataDefinedTextureOffsetY, context, out );
  }
  else if ( const QgsMultiPolygon *mpoly = qgsgeometry_cast<const QgsMultiPolygon *>( g ) )
  {
    for ( int i = 0; i < mpoly->numGeometries(); ++i )
    {
      const QgsPolygon *poly = mpoly->polygonN( i );
      processPolygon( poly, f.id(), offset, extrusionHeight, dataDefinedTextureScale, dataDefinedTextureRotation, dataDefinedTextureOffsetX, dataDefinedTextureOffsetY, context, out );
    }
  }
  else if ( const QgsGeometryCollection *gc = qgsgeometry_cast<const QgsGeometryCollection *>( g ) )
  {
    for ( int i = 0; i < gc->numGeometries(); ++i )
    {
      const QgsAbstractGeometry *g2 = gc->geometryN( i );
      if ( QgsWkbTypes::flatType( g2->wkbType() ) == Qgis::WkbType::Polygon )
      {
        const QgsPolygon *poly = static_cast<const QgsPolygon *>( g2 );
        processPolygon( poly, f.id(), offset, extrusionHeight, dataDefinedTextureScale, dataDefinedTextureRotation, dataDefinedTextureOffsetX, dataDefinedTextureOffsetY, context, out );
      }
    }
  }
  else if ( const QgsPolyhedralSurface *polySurface = qgsgeometry_cast<const QgsPolyhedralSurface *>( g ) )
  {
    for ( int i = 0; i < polySurface->numPatches(); ++i )
    {
      const QgsPolygon *poly = polySurface->patchN( i );
      processPolygon( poly, f.id(), offset, extrusionHeight, dataDefinedTextureScale, dataDefinedTextureRotation, dataDefinedTextureOffsetX, dataDefinedTextureOffsetY, context, out );
    }
  }
  else
    QgsDebugError( u"not a polygon"_s );

  mFeatureCount++;
}

std::unique_ptr<QgsSceneObject3D> QgsPolygon3DSymbolHandler::createSceneObject( const Qgs3DRenderContext &context )
{
  auto object = std::make_unique<QgsSceneObject3D>();

  addPart( object.get(), context, outNormal, false );
  addPart( object.get(), context, outSelected, true );

  mZMin = std::min( outNormal.tessellator->zMinimum(), outSelected.tessellator->zMinimum() );
  mZMax = std::max( outNormal.tessellator->zMaximum(), outSelected.tessellator->zMaximum() );

  if ( object->parts().empty() )
    return nullptr;

  // the vertices are relative to the chunk's own origin, which is a map coordinate: where
  // that lands in world coordinates depends on the scene origin of the moment
  object->setGeoTranslation( mChunkOrigin );
  return object;
}

void QgsPolygon3DSymbolHandler::addPart( QgsSceneObject3D *object, const Qgs3DRenderContext &context, PolygonData &polyData, bool selected )
{
  if ( polyData.tessellator->dataVerticesCount() == 0 )
    return; // nothing to show

  QgsMaterialContext materialContext = QgsMaterialContext::fromRenderContext( context );
  materialContext.setIsSelected( selected );
  materialContext.setIsHighlighted( mHighlightingEnabled );

  const QgsAbstractMaterialSettings *materialSettings = mSymbol->materialSettings();
  const bool dataDefined = materialSettings->dataDefinedProperties().hasActiveProperties();
  std::unique_ptr<QgsMaterial3D> material
    = QgsMaterial3DFactory::createMaterial( materialSettings, dataDefined ? Qgis::MaterialRenderingTechnique::TrianglesDataDefined : Qgis::MaterialRenderingTechnique::Triangles, materialContext, object );
  if ( !material )
    return;

  switch ( mSymbol->cullingMode() )
  {
    case Qgs3DTypes::NoCulling:
      material->setCullMode( QRhiGraphicsPipeline::None );
      break;
    case Qgs3DTypes::Front:
      material->setCullMode( QRhiGraphicsPipeline::Front );
      break;
    case Qgs3DTypes::Back:
      material->setCullMode( QRhiGraphicsPipeline::Back );
      break;
    case Qgs3DTypes::FrontAndBack:
      // QRhi has no mode that culls both, and the documented meaning of this one is that
      // nothing is drawn, so nothing is what the chunk gets
      return;
  }

  auto geometry = std::make_unique<QgsTessellatedPolygonGeometry3D>( true, materialSettings->requiresTextureCoordinates(), materialSettings->requiresTangents() );
  geometry->setVertexBufferData( polyData.tessellator->vertexBuffer(), polyData.tessellator->uniqueVertexCount() );
  geometry->setIndexBufferData( polyData.tessellator->indexBuffer(), polyData.tessellator->dataVerticesCount() );
  geometry->setTriangleIndex( polyData.triangleIndexFids, polyData.triangleIndexStartingIndices );

  if ( dataDefined )
  {
    geometry->setDataDefinedColorData( polyData.materialDataDefined, QgsMaterial3DFactory::dataDefinedColorAttributes( materialSettings ), QgsMaterial3DFactory::dataDefinedColorsUseFloats( materialSettings ) );

    if ( !polyData.ddTextureTransformData.isEmpty() )
    {
      const QByteArray transforms( reinterpret_cast<const char *>( polyData.ddTextureTransformData.constData() ), static_cast<qsizetype>( polyData.ddTextureTransformData.size() * sizeof( float ) ) );
      geometry->setTextureTransformData( transforms );
    }
  }

  object->addPart( std::move( geometry ), std::move( material ) );
}


// --------------


namespace Qgs3DSymbolImpl
{

  QgsFeature3DHandler *handlerForPolygon3DSymbol( const QgsVectorLayer *layer, const QgsAbstract3DSymbol *symbol )
  {
    const QgsPolygon3DSymbol *polygonSymbol = dynamic_cast<const QgsPolygon3DSymbol *>( symbol );
    if ( !polygonSymbol )
      return nullptr;

    return new QgsPolygon3DSymbolHandler( polygonSymbol, layer->selectedFeatureIds() );
  }
} // namespace Qgs3DSymbolImpl

/// @endcond
