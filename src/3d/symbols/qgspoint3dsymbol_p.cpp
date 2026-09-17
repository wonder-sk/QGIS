/***************************************************************************
  qgspoint3dsymbol_p.cpp
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

#include "qgspoint3dsymbol_p.h"

#include <memory>

#include "qgs3drendercontext.h"
#include "qgs3dutils.h"
#include "qgsbillboardgeometry3d.h"
#include "qgsbillboardmaterial3d.h"
#include "qgsfeature3dhandler_p.h"
#include "qgsgputexture.h"
#include "qgslogger.h"
#include "qgsmarkersymbol.h"
#include "qgsmaterial3dfactory.h"
#include "qgsmaterialcontext.h"
#include "qgspoint3dsymbol.h"
#include "qgsprimitivegeometry3d.h"
#include "qgssceneobject3d.h"
#include "qgsvectorlayer.h"

#include <QPainter>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

/// @cond PRIVATE


//* INSTANCED RENDERING *//


class QgsInstancedPoint3DSymbolHandler : public QgsFeature3DHandler
{
  public:
    QgsInstancedPoint3DSymbolHandler( const QgsPoint3DSymbol *symbol, const QgsFeatureIds &selectedIds )
      : mSymbol( static_cast<QgsPoint3DSymbol *>( symbol->clone() ) )
      , mSelectedIds( selectedIds )
    {}

    bool prepare( const Qgs3DRenderContext &context, QSet<QString> &attributeNames, const QgsBox3D &chunkExtent ) override;
    void processFeature( const QgsFeature &feature, const Qgs3DRenderContext &context ) override;
    std::unique_ptr<QgsSceneObject3D> createSceneObject( const Qgs3DRenderContext &context ) override;

  private:
    //! temporary data we collect per feature
    struct PointData
    {
        QVector<QVector3D> positions; // contains triplets of float x,y,z for each point
        QVector<QVector3D> scales;
        QVector<QVector4D> rotations;
    };

    static std::unique_ptr<QgsPrimitiveGeometry3D> symbolGeometry( const QgsPoint3DSymbol *symbol );

    void addPart( QgsSceneObject3D *object, const Qgs3DRenderContext &context, PointData &out, bool selected );
    void updateZRangeFromPointData( const PointData &pointData );

    // input specific for this class
    std::unique_ptr<QgsPoint3DSymbol> mSymbol;
    QVector3D mSymbolScale;
    QQuaternion mSymbolRotation;
    QVector3D mPointTranslation;
    // inputs - generic
    QgsFeatureIds mSelectedIds;
    // outputs
    PointData outNormal;   //!< Features that are not selected
    PointData outSelected; //!< Features that are selected
};


bool QgsInstancedPoint3DSymbolHandler::prepare( const Qgs3DRenderContext &context, QSet<QString> &attributeNames, const QgsBox3D &chunkExtent )
{
  mChunkOrigin = chunkExtent.center();
  mChunkExtent = chunkExtent;

  QSet<QString> attrs = mSymbol->dataDefinedProperties().referencedFields( context.expressionContext() );
  attributeNames.unite( attrs );
  attrs = mSymbol->materialSettings()->dataDefinedProperties().referencedFields( context.expressionContext() );
  attributeNames.unite( attrs );

  Qgs3DUtils::decomposeTransformMatrix( mSymbol->transform(), mPointTranslation, mSymbolRotation, mSymbolScale );

  return true;
}

void QgsInstancedPoint3DSymbolHandler::processFeature( const QgsFeature &feature, const Qgs3DRenderContext &context )
{
  PointData &out = mSelectedIds.contains( feature.id() ) ? outSelected : outNormal;

  if ( feature.geometry().isNull() )
    return;

  const QgsPropertyCollection &ddp = mSymbol->dataDefinedProperties();

  QgsVector3D translation = mPointTranslation;
  const bool hasDDTranslation = ddp.isActive( QgsAbstract3DSymbol::Property::TranslationX )
                                || ddp.isActive( QgsAbstract3DSymbol::Property::TranslationY )
                                || ddp.isActive( QgsAbstract3DSymbol::Property::TranslationZ );
  if ( hasDDTranslation )
  {
    const double translationX = ddp.valueAsDouble( QgsAbstract3DSymbol::Property::TranslationX, context.expressionContext(), translation.x() );
    const double translationY = ddp.valueAsDouble( QgsAbstract3DSymbol::Property::TranslationY, context.expressionContext(), translation.y() );
    const double translationZ = ddp.valueAsDouble( QgsAbstract3DSymbol::Property::TranslationZ, context.expressionContext(), translation.z() );
    translation = QgsVector3D( translationX, translationY, translationZ );
  }

  const std::size_t oldSize = out.positions.size();
  Qgs3DUtils::extractPointPositions( feature, context, mChunkOrigin, mSymbol->altitudeClamping(), out.positions, translation );

  const std::size_t added = out.positions.size() - oldSize;

  const bool hasDDScale = ddp.isActive( QgsAbstract3DSymbol::Property::ScaleX ) || ddp.isActive( QgsAbstract3DSymbol::Property::ScaleY ) || ddp.isActive( QgsAbstract3DSymbol::Property::ScaleZ );

  if ( hasDDScale )
  {
    out.scales.resize( out.positions.size() );
    QVector3D *outScale = out.scales.data() + oldSize;

    const double scaleX = ddp.valueAsDouble( QgsAbstract3DSymbol::Property::ScaleX, context.expressionContext(), mSymbolScale.x() );
    const double scaleY = ddp.valueAsDouble( QgsAbstract3DSymbol::Property::ScaleY, context.expressionContext(), mSymbolScale.y() );
    const double scaleZ = ddp.valueAsDouble( QgsAbstract3DSymbol::Property::ScaleZ, context.expressionContext(), mSymbolScale.z() );

    for ( std::size_t i = 0; i < added; ++i )
    {
      ( *outScale++ ) = QVector3D( static_cast<float>( scaleX ), static_cast<float>( scaleY ), static_cast<float>( scaleZ ) );
    }
  }

  const bool hasDDRotation = ddp.isActive( QgsAbstract3DSymbol::Property::RotationX )
                             || ddp.isActive( QgsAbstract3DSymbol::Property::RotationY )
                             || ddp.isActive( QgsAbstract3DSymbol::Property::RotationZ );
  if ( hasDDRotation )
  {
    out.rotations.resize( out.positions.size() );
    QVector4D *outRotation = out.rotations.data() + oldSize;

    // extract default rotation components from symbol rotation
    const QVector3D baseEuler = mSymbolRotation.toEulerAngles();

    const double rotationX = ddp.valueAsDouble( QgsAbstract3DSymbol::Property::RotationX, context.expressionContext(), baseEuler.x() );
    const double rotationY = ddp.valueAsDouble( QgsAbstract3DSymbol::Property::RotationY, context.expressionContext(), baseEuler.y() );
    const double rotationZ = ddp.valueAsDouble( QgsAbstract3DSymbol::Property::RotationZ, context.expressionContext(), baseEuler.z() );

    //... and then re-calculate the rotation vector for this feature
    const QQuaternion finalQuat = QQuaternion::fromEulerAngles( static_cast<float>( rotationX ), static_cast<float>( rotationY ), static_cast<float>( rotationZ ) );
    const QVector4D finalVec4 = finalQuat.toVector4D();

    for ( std::size_t i = 0; i < added; ++i )
    {
      ( *outRotation++ ) = finalVec4;
    }
  }

  mFeatureCount++;
}

void QgsInstancedPoint3DSymbolHandler::updateZRangeFromPointData( const PointData &pointData )
{
  const QVector3D *scales = pointData.scales.empty() ? nullptr : pointData.scales.constData();
  for ( const QVector3D &pos : std::as_const( pointData.positions ) )
  {
    double minZ = 0;
    double maxZ = 0;

    // also account for the actual height of the objects themselves
    // NOTE -- these calculations are naive, and assume no rotation or scaling of the symbol!
    switch ( mSymbol->shape() )
    {
      case Qgis::Point3DShape::Cylinder:
      {
        const float length = mSymbol->shapeProperty( u"length"_s ).toFloat();
        minZ -= length * 0.5f;
        maxZ += length * 0.5f;
        break;
      }

      case Qgis::Point3DShape::Sphere:
      {
        const float radius = mSymbol->shapeProperty( u"radius"_s ).toFloat();
        minZ -= radius;
        maxZ += radius;
        break;
      }

      case Qgis::Point3DShape::Cone:
      {
        const float length = mSymbol->shapeProperty( u"length"_s ).toFloat();
        minZ -= length * 0.5f;
        maxZ += length * 0.5f;
        break;
      }

      case Qgis::Point3DShape::Cube:
      {
        const float size = mSymbol->shapeProperty( u"size"_s ).toFloat();
        minZ -= size * 0.5f;
        maxZ += size * 0.5f;
        break;
      }

      case Qgis::Point3DShape::Torus:
      {
        const float radius = mSymbol->shapeProperty( u"radius"_s ).toFloat();
        minZ -= radius;
        maxZ += radius;
        break;
      }

      case Qgis::Point3DShape::Plane:
      {
        // worst case scenario -- even though planes are usually rotated so that they are flat,
        // let's account for possible overridden rotation
        const float size = mSymbol->shapeProperty( u"size"_s ).toFloat();
        minZ -= size * 0.5f;
        maxZ += size * 0.5f;
        break;
      }

      case Qgis::Point3DShape::ExtrudedText:
      case Qgis::Point3DShape::Model:
      case Qgis::Point3DShape::Billboard:
        break;
    }

    if ( scales )
    {
      const double zScale = ( *scales++ )[2];
      minZ *= zScale;
      maxZ *= zScale;
    }

    // as we are relative to chunk center elevation we have to add mChunkOrigin.z()
    minZ += pos.z() + mChunkOrigin.z();
    maxZ += pos.z() + mChunkOrigin.z();

    if ( minZ < mZMin )
      mZMin = static_cast<float>( minZ );
    if ( maxZ > mZMax )
      mZMax = static_cast<float>( maxZ );
  }
}

std::unique_ptr<QgsSceneObject3D> QgsInstancedPoint3DSymbolHandler::createSceneObject( const Qgs3DRenderContext &context )
{
  auto object = std::make_unique<QgsSceneObject3D>();

  addPart( object.get(), context, outNormal, false );
  addPart( object.get(), context, outSelected, true );

  updateZRangeFromPointData( outNormal );
  updateZRangeFromPointData( outSelected );

  if ( object->parts().empty() )
    return nullptr;

  object->setGeoTranslation( mChunkOrigin );
  return object;
}

void QgsInstancedPoint3DSymbolHandler::addPart( QgsSceneObject3D *object, const Qgs3DRenderContext &context, PointData &out, bool selected )
{
  if ( out.positions.isEmpty() )
    return; // nothing to show

  std::unique_ptr<QgsPrimitiveGeometry3D> geometry = symbolGeometry( mSymbol.get() );
  if ( !geometry )
    return;

  Qgis::InstancedMaterialFlags flags;
  if ( !out.scales.empty() )
    flags |= Qgis::InstancedMaterialFlag::DataDefinedScale;
  if ( !out.rotations.empty() )
    flags |= Qgis::InstancedMaterialFlag::DataDefinedRotation;

  const QString upAxis = mSymbol->shapeProperty( u"upAxis"_s ).toString();
  const QString forwardAxis = mSymbol->shapeProperty( u"forwardAxis"_s ).toString();
  const QMatrix4x4 meshTransform = Qgs3DUtils::axisTransformMatrix( !upAxis.isEmpty() ? upAxis : u"y"_s, !forwardAxis.isEmpty() ? forwardAxis : u"-z"_s );

  QgsMaterialContext materialContext = QgsMaterialContext::fromRenderContext( context );
  materialContext.setIsSelected( selected );
  materialContext.setIsHighlighted( mHighlightingEnabled );

  std::unique_ptr<QgsMaterial3D> material = QgsMaterial3DFactory::createInstancedMaterial( mSymbol->materialSettings(), materialContext, flags, meshTransform, mSymbolScale, mSymbolRotation, object );
  if ( !material )
    return;

  geometry->setInstances( out.positions, out.scales, out.rotations );

  const int instanceCount = geometry->instanceCount();
  QgsSceneObject3D::Part *part = object->addPart( std::move( geometry ), std::move( material ) );
  part->instanceCount = instanceCount;
}

std::unique_ptr<QgsPrimitiveGeometry3D> QgsInstancedPoint3DSymbolHandler::symbolGeometry( const QgsPoint3DSymbol *symbol )
{
  const bool tangents = symbol->materialSettings() && symbol->materialSettings()->requiresTangents();

  switch ( symbol->shape() )
  {
    case Qgis::Point3DShape::Cylinder:
    {
      const float radius = symbol->shapeProperty( u"radius"_s ).toFloat();
      const float length = symbol->shapeProperty( u"length"_s ).toFloat();
      return QgsPrimitiveGeometry3D::cylinder( radius, length, 16, 16, tangents );
    }

    case Qgis::Point3DShape::Sphere:
    {
      const float radius = symbol->shapeProperty( u"radius"_s ).toFloat();
      const int rings = symbol->shapeProperty( u"rings"_s ).toInt();
      const int slices = symbol->shapeProperty( u"slices"_s ).toInt();
      return QgsPrimitiveGeometry3D::sphere( radius, rings, slices, tangents );
    }

    case Qgis::Point3DShape::Cone:
    {
      const float length = symbol->shapeProperty( u"length"_s ).toFloat();
      const float bottomRadius = symbol->shapeProperty( u"bottomRadius"_s ).toFloat();
      const float topRadius = symbol->shapeProperty( u"topRadius"_s ).toFloat();
      return QgsPrimitiveGeometry3D::cone( bottomRadius, topRadius, length, true, true, 16, 16, tangents );
    }

    case Qgis::Point3DShape::Cube:
    {
      const float size = symbol->shapeProperty( u"size"_s ).toFloat();
      return QgsPrimitiveGeometry3D::cuboid( size, size, size, tangents );
    }

    case Qgis::Point3DShape::Torus:
    {
      const float radius = symbol->shapeProperty( u"radius"_s ).toFloat();
      const float minorRadius = symbol->shapeProperty( u"minorRadius"_s ).toFloat();
      return QgsPrimitiveGeometry3D::torus( radius, minorRadius, 16, 16, tangents );
    }

    case Qgis::Point3DShape::Plane:
    {
      const float size = symbol->shapeProperty( u"size"_s ).toFloat();
      return QgsPrimitiveGeometry3D::plane( size, size, QSize( 2, 2 ), tangents );
    }

    case Qgis::Point3DShape::ExtrudedText:
    {
      const float depth = symbol->shapeProperty( u"depth"_s ).toFloat();
      const QString text = symbol->shapeProperty( u"text"_s ).toString();
      return QgsPrimitiveGeometry3D::extrudedText( text, QFont(), depth );
    }

    case Qgis::Point3DShape::Model:
    case Qgis::Point3DShape::Billboard:
      break;
  }

  Q_ASSERT( false );
  return nullptr;
}


//* BILLBOARD RENDERING *//


class QgsPoint3DBillboardSymbolHandler : public QgsFeature3DHandler
{
  public:
    QgsPoint3DBillboardSymbolHandler( const QgsPoint3DSymbol *symbol, const QgsFeatureIds &selectedIds )
      : mSymbol( static_cast<QgsPoint3DSymbol *>( symbol->clone() ) )
      , mSelectedIds( selectedIds )
    {}

    bool prepare( const Qgs3DRenderContext &context, QSet<QString> &attributeNames, const QgsBox3D &chunkExtent ) override;
    void processFeature( const QgsFeature &feature, const Qgs3DRenderContext &context ) override;
    std::unique_ptr<QgsSceneObject3D> createSceneObject( const Qgs3DRenderContext &context ) override;

  private:
    struct PointData
    {
        QVector<QVector3D> positions; // contains triplets of float x,y,z for each point
    };

    static QImage renderSymbolToImage( const QgsMarkerSymbol *markerSymbol, const Qgs3DRenderContext &context, bool selected );

    void addPart( QgsSceneObject3D *object, const Qgs3DRenderContext &context, PointData &out, bool selected );

    // input specific for this class
    std::unique_ptr<QgsPoint3DSymbol> mSymbol;
    // inputs - generic
    QgsFeatureIds mSelectedIds;
    // outputs
    PointData outNormal;   //!< Features that are not selected
    PointData outSelected; //!< Features that are selected
};

bool QgsPoint3DBillboardSymbolHandler::prepare( const Qgs3DRenderContext &context, QSet<QString> &attributeNames, const QgsBox3D &chunkExtent )
{
  Q_UNUSED( context )
  Q_UNUSED( attributeNames )

  mChunkOrigin = chunkExtent.center();
  mChunkExtent = chunkExtent;

  return true;
}

void QgsPoint3DBillboardSymbolHandler::processFeature( const QgsFeature &feature, const Qgs3DRenderContext &context )
{
  PointData &out = mSelectedIds.contains( feature.id() ) ? outSelected : outNormal;

  if ( feature.geometry().isNull() )
    return;

  Qgs3DUtils::extractPointPositions( feature, context, mChunkOrigin, mSymbol->altitudeClamping(), out.positions );
  mFeatureCount++;
}

std::unique_ptr<QgsSceneObject3D> QgsPoint3DBillboardSymbolHandler::createSceneObject( const Qgs3DRenderContext &context )
{
  auto object = std::make_unique<QgsSceneObject3D>();

  addPart( object.get(), context, outNormal, false );
  addPart( object.get(), context, outSelected, true );

  updateZRangeFromPositions( outNormal.positions );
  updateZRangeFromPositions( outSelected.positions );

  // the billboard height is part of the object's placement rather than of its vertices,
  // so it has to be added back into the range the chunk reports
  const float billboardHeight = mSymbol->billboardHeight();
  // as we are relative to chunk center elevation we have to add mChunkOrigin.z()
  mZMin += static_cast<float>( billboardHeight + mChunkOrigin.z() );
  mZMax += static_cast<float>( billboardHeight + mChunkOrigin.z() );

  if ( object->parts().empty() )
    return nullptr;

  object->setGeoTranslation( mChunkOrigin + QgsVector3D( 0, 0, billboardHeight ) );
  return object;
}

QImage QgsPoint3DBillboardSymbolHandler::renderSymbolToImage( const QgsMarkerSymbol *markerSymbol, const Qgs3DRenderContext &context, bool selected )
{
  QgsRenderContext context2D;
  context2D.setSelectionColor( context.selectionColor() );
  context2D.setScaleFactor( context.outputDpi() / 25.4 );
  context2D.setFlag( Qgis::RenderContextFlag::Antialiasing );
  context2D.setFlag( Qgis::RenderContextFlag::HighQualityImageTransforms );

  std::unique_ptr<QgsMarkerSymbol> clonedSymbol( markerSymbol->clone() );
  clonedSymbol->startRender( context2D );

  constexpr int BUFFER_SIZE_PIXELS = 2;

  const QRectF bounds = markerSymbol->bounds( QPointF( 0, 0 ), context2D );

  QImage image( static_cast<int>( std::ceil( bounds.size().width() ) ) + 2 * BUFFER_SIZE_PIXELS, static_cast<int>( std::ceil( bounds.size().height() ) ) + 2 * BUFFER_SIZE_PIXELS, QImage::Format_ARGB32_Premultiplied );
  image.fill( Qt::transparent );

  QPainter painter( &image );
  context2D.setPainter( &painter );

  clonedSymbol->renderPoint( QPointF( -bounds.left() + BUFFER_SIZE_PIXELS, -bounds.top() + BUFFER_SIZE_PIXELS ), nullptr, context2D, -1, selected );

  painter.end();

  clonedSymbol->stopRender( context2D );
  return image;
}

void QgsPoint3DBillboardSymbolHandler::addPart( QgsSceneObject3D *object, const Qgs3DRenderContext &context, PointData &out, bool selected )
{
  if ( out.positions.isEmpty() )
    return; // nothing to show

  QgsMarkerSymbol *markerSymbol = mSymbol->billboardSymbol();
  std::unique_ptr<QgsSymbol> defaultSymbol;
  if ( !markerSymbol )
  {
    defaultSymbol = QgsSymbol::defaultSymbol( Qgis::GeometryType::Point );
    markerSymbol = static_cast<QgsMarkerSymbol *>( defaultSymbol.get() );
  }

  const QImage symbolImage = renderSymbolToImage( markerSymbol, context, selected );
  if ( symbolImage.isNull() )
    return;

  auto texture = std::make_unique<QgsGpuTexture>();
  texture->setImage( symbolImage );
  // a rendered symbol is sRGB encoded, and the billboard shader works in linear space
  texture->setSrgb( true );
  QgsGpuTexture *texturePtr = object->addTexture( std::move( texture ) );

  auto material = std::make_unique<QgsBillboardMaterial3D>();
  material->setSize( QSizeF( symbolImage.width(), symbolImage.height() ) );
  material->setSpriteTexture( texturePtr );

  auto geometry = std::make_unique<QgsBillboardGeometry3D>();
  geometry->setPositions( out.positions );

  const int instanceCount = geometry->count();
  QgsSceneObject3D::Part *part = object->addPart( std::move( geometry ), std::move( material ) );
  part->instanceCount = instanceCount;
  part->isTransparent = true;
  // a billboard turns to face the main camera, so what the light camera would see of it
  // bears no relation to what is drawn - which is why these have never cast shadows
  part->castsShadows = false;
}


// --------------


namespace Qgs3DSymbolImpl
{

  QgsFeature3DHandler *handlerForPoint3DSymbol( const QgsVectorLayer *layer, const QgsAbstract3DSymbol *symbol )
  {
    const QgsPoint3DSymbol *pointSymbol = dynamic_cast<const QgsPoint3DSymbol *>( symbol );
    if ( !pointSymbol )
      return nullptr;

    switch ( pointSymbol->shape() )
    {
      case Qgis::Point3DShape::Model:
        // TODO: 3D model point symbols need glTF and OBJ files loaded into instanced QRhi
        // geometry, which went with QgsObj3DUtils and the Qt3D half of QgsGltf3DUtils
        QgsDebugError( u"3D model point symbols are not supported by the QRhi engine yet"_s );
        return nullptr;

      case Qgis::Point3DShape::Billboard:
        return new QgsPoint3DBillboardSymbolHandler( pointSymbol, layer->selectedFeatureIds() );

      case Qgis::Point3DShape::Cylinder:
      case Qgis::Point3DShape::Sphere:
      case Qgis::Point3DShape::Cone:
      case Qgis::Point3DShape::Cube:
      case Qgis::Point3DShape::Torus:
      case Qgis::Point3DShape::Plane:
      case Qgis::Point3DShape::ExtrudedText:
        return new QgsInstancedPoint3DSymbolHandler( pointSymbol, layer->selectedFeatureIds() );
    }

    return nullptr;
  }
} // namespace Qgs3DSymbolImpl

/// @endcond
