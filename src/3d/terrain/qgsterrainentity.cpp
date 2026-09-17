/***************************************************************************
  qgsterrainentity.cpp
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

#include "qgsterrainentity.h"

#include <memory>

#include "qgs3dmapsettings.h"
#include "qgs3dutils.h"
#include "qgsaabb.h"
#include "qgsabstractterrainsettings.h"
#include "qgschunknode.h"
#include "qgscoordinatetransform.h"
#include "qgseventtracing.h"
#include "qgsgputexture.h"
#include "qgslayerstylewatcher.h"
#include "qgsraycastingutils.h"
#include "qgssceneobject3d.h"
#include "qgsterraingenerator.h"
#include "qgsterraintexturegenerator_p.h"
#include "qgstilingscheme.h"

#include <QString>

#include "moc_qgsterrainentity.cpp"

using namespace Qt::StringLiterals;

///@cond PRIVATE

//! Factory for map update jobs
class TerrainMapUpdateJobFactory : public QgsChunkQueueJobFactory
{
  public:
    TerrainMapUpdateJobFactory( QgsTerrainTextureGenerator *textureGenerator, const QgsTilingScheme &tilingScheme )
      : mTextureGenerator( textureGenerator )
      , mTilingScheme( tilingScheme )
    {}

    QgsChunkQueueJob *createJob( QgsChunkNode *chunk ) override { return new TerrainMapUpdateJob( mTextureGenerator, mTilingScheme, chunk ); }

  private:
    QgsTerrainTextureGenerator *mTextureGenerator = nullptr;
    QgsTilingScheme mTilingScheme;
};


// -----------


QgsTerrainEntity::QgsTerrainEntity( Qgs3DMapSettings *map, QObject *parent )
  : QgsChunkedEntity( map, map->terrainSettings()->maximumScreenError(), map->terrainGenerator(), false, std::numeric_limits<int>::max(), parent )
{
  map->terrainGenerator()->setTerrain( this );
  mIsValid = map->terrainGenerator()->isValid();

  mLayerWatcher = make_qobject_unique<QgsLayerStyleWatcher>( map );
  connect( mLayerWatcher.get(), &QgsLayerStyleWatcher::styleChanged, this, &QgsTerrainEntity::invalidateMapImages );

  connect( map, &Qgs3DMapSettings::showTerrainTilesInfoChanged, this, &QgsTerrainEntity::invalidateMapImages );
  connect( map, &Qgs3DMapSettings::showLabelsChanged, this, &QgsTerrainEntity::invalidateMapImages );
  connect( map, &Qgs3DMapSettings::backgroundColorChanged, this, &QgsTerrainEntity::invalidateMapImages );
  connect( map, &Qgs3DMapSettings::terrainMapThemeChanged, this, &QgsTerrainEntity::invalidateMapImages );
  connect( map, &Qgs3DMapSettings::terrainSettingsChanged, this, &QgsTerrainEntity::onTerrainElevationOffsetChanged );

  mTextureGenerator = std::make_unique<QgsTerrainTextureGenerator>( *map );

  mUpdateJobFactory = std::make_unique<TerrainMapUpdateJobFactory>( mTextureGenerator.get(), map->terrainGenerator()->tilingScheme() );
}

QgsTerrainEntity::~QgsTerrainEntity()
{
  // cancel / wait for jobs
  cancelActiveJobs();
}

QList<QgsRayCastHit> QgsTerrainEntity::rayIntersection( const QgsRay3D &ray, const QgsRayCastContext &context ) const
{
  Q_UNUSED( context )
  QList<QgsRayCastHit> result;

  float minDist = -1;
  QgsVector3D intersectionPointMapCoords;
  switch ( mMapSettings->terrainGenerator()->type() )
  {
    case QgsTerrainGenerator::Flat:
    {
      if ( ray.direction().z() == 0 )
        break; // the ray is parallel to the flat terrain

      const float dist = static_cast<float>( mMapSettings->terrainSettings()->elevationOffset() - ray.origin().z() - mMapSettings->origin().z() ) / ray.direction().z();
      const QVector3D terrainPlanePoint = ray.point( dist );
      const QgsVector3D mapCoords = Qgs3DUtils::worldToMapCoordinates( terrainPlanePoint, mMapSettings->origin() );
      if ( mMapSettings->extent().contains( mapCoords.x(), mapCoords.y() ) )
      {
        minDist = dist;
        intersectionPointMapCoords = mapCoords;
      }
      break;
    }
    case QgsTerrainGenerator::Dem:
      // TODO: ray cast against a DEM tile. QgsDemTerrainTileGeometry3D::rayIntersection()
      // is there, but a chunk's scene object type erases the generator that produced it,
      // so there is no way back to it from here yet.
      break;
    case QgsTerrainGenerator::Mesh:
    case QgsTerrainGenerator::Online:
    case QgsTerrainGenerator::QuantizedMesh:
      // not supported
      break;
  }
  if ( minDist >= 0 )
  {
    QgsRayCastHit hit;
    hit.setDistance( minDist );
    hit.setMapCoordinates( intersectionPointMapCoords );
    result.append( hit );
  }
  return result;
}

void QgsTerrainEntity::invalidateMapImages()
{
  QgsEventTracing::addEvent( QgsEventTracing::Instant, u"3D"_s, u"Invalidate textures"_s );

  // handle active nodes

  updateNodes( mActiveNodes, mUpdateJobFactory.get() );

  // handle inactive nodes afterwards

  QList<QgsChunkNode *> inactiveNodes;
  const QList<QgsChunkNode *> descendants = mRootNode->descendants();
  for ( QgsChunkNode *node : descendants )
  {
    if ( !node->sceneObject() )
      continue;
    if ( mActiveNodes.contains( node ) )
      continue;
    inactiveNodes << node;
  }

  updateNodes( inactiveNodes, mUpdateJobFactory.get() );

  setNeedsUpdate( true );
}

void QgsTerrainEntity::onTerrainElevationOffsetChanged()
{
  // the offset is baked into each tile's geometry, so the tiles have to be rebuilt
  invalidateMapImages();
}

float QgsTerrainEntity::terrainElevationOffset() const
{
  return mMapSettings->terrainSettings()->elevationOffset();
}


// -----------


TerrainMapUpdateJob::TerrainMapUpdateJob( QgsTerrainTextureGenerator *textureGenerator, const QgsTilingScheme &tilingScheme, QgsChunkNode *node )
  : QgsChunkQueueJob( node )
  , mTextureGenerator( textureGenerator )
  , mTilingScheme( tilingScheme )
{}

void TerrainMapUpdateJob::start()
{
  QgsChunkNode *node = chunk();

  connect( mTextureGenerator, &QgsTerrainTextureGenerator::tileReady, this, &TerrainMapUpdateJob::onTileReady );
  mJobId = mTextureGenerator->render( mTilingScheme.tileToExtent( node->tileId() ), node->tileId() );
}

void TerrainMapUpdateJob::cancel()
{
  if ( mJobId != -1 )
    mTextureGenerator->cancelJob( mJobId );
}


void TerrainMapUpdateJob::onTileReady( int jobId, const QImage &image )
{
  if ( mJobId == jobId )
  {
    // a terrain tile owns exactly one texture, the rendered map image
    if ( QgsSceneObject3D *object = mNode->sceneObject() )
    {
      if ( !object->textures().empty() )
        object->textures().front()->setImage( image );
    }
    mJobId = -1;
    emit finished();
  }
}

/// @endcond
