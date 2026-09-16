/***************************************************************************
  qgsflatterraingenerator.cpp
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

#include "qgsflatterraingenerator.h"

#include "qgs3dmapsettings.h"
#include "qgs3drendercontext.h"
#include "qgschunknode.h"
#include "qgsgeotransform.h"
#include "qgsprimitivegeometry3d.h"
#include "qgssceneobject3d.h"
#include "qgsterrainentity.h"
#include "qgsterraintileentity_p.h"

#include <Qt3DRender/QGeometryRenderer>

#include "moc_qgsflatterraingenerator.cpp"

/// @cond PRIVATE


//---------------


FlatTerrainChunkLoader::FlatTerrainChunkLoader( QgsTerrainEntity *terrain, QgsChunkNode *node )
  : QgsTerrainTileLoader( terrain, node )
{}

void FlatTerrainChunkLoader::start()
{
  loadTexture();
}


QMatrix4x4 FlatTerrainChunkLoader::tileTransform()
{
  const Qgs3DMapSettings *map = terrain()->mapSettings();

  const QgsBox3D box3D = mNode->box3D();
  const QgsBox3D mapFullBox3D( map->extent(), box3D.zMinimum(), box3D.zMaximum() );

  const QgsBox3D commonExtent(
    std::max( box3D.xMinimum(), mapFullBox3D.xMinimum() ),
    std::max( box3D.yMinimum(), mapFullBox3D.yMinimum() ),
    box3D.zMinimum(),
    std::min( box3D.xMaximum(), mapFullBox3D.xMaximum() ),
    std::min( box3D.yMaximum(), mapFullBox3D.yMaximum() ),
    box3D.zMaximum()
  );

  const double xSide = commonExtent.width();
  const double ySide = commonExtent.height();
  const QgsVector3D centre = QgsVector3D( commonExtent.xMinimum() + xSide / 2, commonExtent.yMinimum() + ySide / 2, 0 ) - map->origin();

  QMatrix4x4 transform;
  transform.translate( centre.toVector3D() );
  // the unit quad lies in the XZ plane, so stand it up before stretching it over the tile
  transform.rotate( 90, QVector3D( 1, 0, 0 ) );
  transform.scale( static_cast<float>( xSide ), 1.0f, static_cast<float>( ySide ) );
  return transform;
}

std::unique_ptr<QgsSceneObject3D> FlatTerrainChunkLoader::createSceneObject()
{
  const Qgs3DMapSettings *map = terrain()->mapSettings();
  if ( !map->rhiEngineEnabled() )
    return nullptr;

  auto object = std::make_unique<QgsSceneObject3D>();

  std::unique_ptr<QgsMaterial3D> material = createTileMaterial( object.get(), map->isTerrainShadingEnabled(), map->terrainShadingMaterial(), !map->layers().empty() );

  object->addPart( QgsPrimitiveGeometry3D::plane(), std::move( material ), tileTransform() );
  return object;
}

Qt3DCore::QEntity *FlatTerrainChunkLoader::createEntity( Qt3DCore::QEntity *parent )
{
  QgsTerrainTileEntity *entity = new QgsTerrainTileEntity( mNode->tileId() );

  // make geometry renderer

  // simple quad geometry shared by all tiles
  // QPlaneGeometry by default is 1x1 with mesh resolution QSize(2,2), centered at 0
  // TODO: the geometry could be shared inside Terrain instance (within terrain-generator specific data?)
  mTileGeometry = new Qt3DExtras::QPlaneGeometry;

  Qt3DRender::QGeometryRenderer *mesh = new Qt3DRender::QGeometryRenderer;
  mesh->setGeometry( mTileGeometry ); // takes ownership if the component has no parent
  entity->addComponent( mesh );       // takes ownership if the component has no parent

  // create material

  const Qgs3DMapSettings *map = terrain()->mapSettings();

  // create transform

  QgsGeoTransform *transform = nullptr;
  transform = new QgsGeoTransform();
  entity->addComponent( transform );

  // set up transform according to the extent covered by the quad geometry
  const QgsBox3D box3D = mNode->box3D();
  const QgsBox3D mapFullBox3D( map->extent(), box3D.zMinimum(), box3D.zMaximum() );

  const QgsBox3D commonExtent(
    std::max( box3D.xMinimum(), mapFullBox3D.xMinimum() ),
    std::max( box3D.yMinimum(), mapFullBox3D.yMinimum() ),
    box3D.zMinimum(),
    std::min( box3D.xMaximum(), mapFullBox3D.xMaximum() ),
    std::min( box3D.yMaximum(), mapFullBox3D.yMaximum() ),
    box3D.zMaximum()
  );
  const double xSide = commonExtent.width();
  const double ySide = commonExtent.height();
  const double xMin = commonExtent.xMinimum();
  const double yMin = commonExtent.yMinimum();

  transform->setRotation( QQuaternion::fromAxisAndAngle( QVector3D( 1, 0, 0 ), 90 ) ); // QPlaneGeometry uses XZ as the base plane
  transform->setScale3D( QVector3D( static_cast<float>( xSide ), 1, static_cast<float>( ySide ) ) );
  transform->setGeoTranslation( QgsVector3D( xMin + xSide / 2, yMin + ySide / 2, 0 ) );

  createTextureComponent( entity, map->isTerrainShadingEnabled(), map->terrainShadingMaterial(), !map->layers().empty(), Qgs3DRenderContext::fromMapSettings( map ) );

  entity->setParent( parent );
  return entity;
}

/// @endcond

// ---------------

QgsTerrainGenerator *QgsFlatTerrainGenerator::create()
{
  return new QgsFlatTerrainGenerator();
}

QgsChunkLoader *QgsFlatTerrainGenerator::createChunkLoader( QgsChunkNode *node ) const
{
  return new FlatTerrainChunkLoader( mTerrain, node );
}

QgsTerrainGenerator *QgsFlatTerrainGenerator::clone() const
{
  QgsFlatTerrainGenerator *cloned = new QgsFlatTerrainGenerator;
  cloned->mCrs = mCrs;
  cloned->mExtent = mExtent;
  cloned->updateTilingScheme();
  return cloned;
}

QgsTerrainGenerator::Type QgsFlatTerrainGenerator::type() const
{
  return QgsTerrainGenerator::Flat;
}

QgsRectangle QgsFlatTerrainGenerator::rootChunkExtent() const
{
  return mTerrainTilingScheme.tileToExtent( 0, 0, 0 );
}

void QgsFlatTerrainGenerator::rootChunkHeightRange( float &hMin, float &hMax ) const
{
  hMin = 0;
  hMax = 0;
}

void QgsFlatTerrainGenerator::setCrs( const QgsCoordinateReferenceSystem &crs, const QgsCoordinateTransformContext & )
{
  mCrs = crs;
  updateTilingScheme();
}

void QgsFlatTerrainGenerator::setExtent( const QgsRectangle &extent )
{
  if ( mExtent == extent )
    return;

  mExtent = extent;
  updateTilingScheme();
}

float QgsFlatTerrainGenerator::heightAt( double x, double y, const Qgs3DRenderContext &context ) const
{
  Q_UNUSED( x )
  Q_UNUSED( y )
  Q_UNUSED( context )
  return 0;
}

void QgsFlatTerrainGenerator::updateTilingScheme()
{
  // the real extent will be a square where the given extent fully fits
  mTerrainTilingScheme = QgsTilingScheme( mExtent, mCrs );
}
