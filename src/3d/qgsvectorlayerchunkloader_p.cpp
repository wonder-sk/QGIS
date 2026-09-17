/***************************************************************************
  qgsvectorlayerchunkloader_p.cpp
  --------------------------------------
  Date                 : July 2019
  Copyright            : (C) 2019 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsvectorlayerchunkloader_p.h"

#include "qgs3dmapsettings.h"
#include "qgs3dsymbolregistry.h"
#include "qgs3dutils.h"
#include "qgsabstract3dsymbol.h"
#include "qgsabstractterrainsettings.h"
#include "qgsabstractvectorlayer3drenderer.h"
#include "qgsapplication.h"
#include "qgschunknode.h"
#include "qgseventtracing.h"
#include "qgsexpressioncontextutils.h"
#include "qgsfeature3dhandler_p.h"
#include "qgsline3dsymbol.h"
#include "qgslogger.h"
#include "qgspoint3dsymbol.h"
#include "qgspolygon3dsymbol.h"
#include "qgssceneobject3d.h"
#include "qgsvectorlayer.h"
#include "qgsvectorlayerfeatureiterator.h"

#include <QString>
#include <QtConcurrentRun>

#include "moc_qgsvectorlayerchunkloader_p.cpp"

using namespace Qt::StringLiterals;

///@cond PRIVATE


QgsVectorLayerChunkLoader::QgsVectorLayerChunkLoader( const QgsVectorLayerChunkLoaderFactory *factory, QgsChunkNode *node )
  : QgsChunkLoader( node )
  , mFactory( factory )
  , mRenderContext( factory->mRenderContext )
  , mSource( new QgsVectorLayerFeatureSource( factory->mLayer ) )
{}

void QgsVectorLayerChunkLoader::start()
{
  QgsChunkNode *node = chunk();

  QgsVectorLayer *layer = mFactory->mLayer;
  mLayerName = mFactory->mLayer->name();

  QgsFeature3DHandler *handler = QgsApplication::symbol3DRegistry()->createHandlerForSymbol( layer, mFactory->mSymbol.get() );
  if ( !handler )
  {
    // either the symbol type is unknown, or the QRhi engine has no way of drawing it yet -
    // the symbol implementation says which in the log
    QgsDebugError( u"No 3D symbol handler for vector layer symbol type: "_s + mFactory->mSymbol->type() );
    return;
  }
  mHandler.reset( handler );

  QgsExpressionContext exprContext;
  exprContext.appendScopes( QgsExpressionContextUtils::globalProjectLayerScopes( layer ) );
  exprContext.setFields( layer->fields() );
  mRenderContext.setExpressionContext( exprContext );

  QSet<QString> attributeNames;
  if ( !mHandler->prepare( mRenderContext, attributeNames, node->box3D() ) )
  {
    QgsDebugError( u"Failed to prepare 3D feature handler!"_s );
    mHandler.reset();
    return;
  }

  // build the feature request
  // only a subset of data to be queried
  const QgsRectangle rect = node->box3D().toRectangle();
  QgsFeatureRequest req;
  req.setCoordinateTransform( QgsCoordinateTransform( layer->crs3D(), mRenderContext.crs(), mRenderContext.transformContext() ) );
  req.setSubsetOfAttributes( attributeNames, layer->fields() );
  req.setFilterRect( rect );

  //
  // this will be run in a background thread
  //
  mFutureWatcher = new QFutureWatcher<void>( this );

  connect( mFutureWatcher, &QFutureWatcher<void>::finished, this, [this] {
    if ( !mCanceled )
      mFactory->mNodesAreLeafs[mNode->tileId().text()] = mNodeIsLeaf;
  } );

  connect( mFutureWatcher, &QFutureWatcher<void>::finished, this, &QgsChunkQueueJob::finished );

  const QFuture<void> future = QtConcurrent::run( [req = std::move( req ), this] {
    const QgsScopedEvent e( u"3D"_s, u"VL chunk load"_s );

    QgsFeature f;
    QgsFeatureIterator fi = mSource->getFeatures( req );
    int featureCount = 0;
    bool featureLimitReached = false;
    while ( fi.nextFeature( f ) )
    {
      if ( mCanceled )
        return;

      if ( ++featureCount > mFactory->mMaxFeatures )
      {
        featureLimitReached = true;
        break;
      }

      mRenderContext.expressionContext().setFeature( f );
      mHandler->processFeature( f, mRenderContext );
    }

    if ( !featureLimitReached )
    {
      QgsDebugMsgLevel( u"All features fetched for node: %1"_s.arg( mNode->tileId().text() ), 3 );

      if ( featureCount == 0 || std::max<double>( mNode->box3D().width(), mNode->box3D().height() ) < QgsVectorLayer3DTilingSettings::maximumLeafExtent() )
        mNodeIsLeaf = true;
    }
  } );

  // emit finished() as soon as the handler is populated with features
  mFutureWatcher->setFuture( future );
}

QgsVectorLayerChunkLoader::~QgsVectorLayerChunkLoader()
{
  if ( mFutureWatcher && !mFutureWatcher->isFinished() )
  {
    disconnect( mFutureWatcher, &QFutureWatcher<void>::finished, this, &QgsChunkQueueJob::finished );
    mFutureWatcher->waitForFinished();
  }
}

void QgsVectorLayerChunkLoader::cancel()
{
  mCanceled = true;
}

std::unique_ptr<QgsSceneObject3D> QgsVectorLayerChunkLoader::createSceneObject()
{
  // the handler is only built in start(), and only for a symbol the QRhi engine can draw
  if ( !mHandler || mHandler->featureCount() == 0 )
  {
    // an empty node, so we return no scene object. This tags the node as having no data and effectively removes it.
    // we just make sure first that its initial estimated vertical range does not affect its parents' bboxes calculation
    mNode->setExactBox3D( QgsBox3D() );
    mNode->updateParentBoundingBoxesRecursively();
    return nullptr;
  }

  std::unique_ptr<QgsSceneObject3D> object = mHandler->createSceneObject( mRenderContext );
  if ( !object )
    return nullptr;

  // the terrain can be raised or lowered as a whole, and the features that follow it move
  // with it; the chunk's own vertices know nothing about that
  QMatrix4x4 transform;
  transform.translate( 0.0f, 0.0f, mFactory->terrainElevationOffset() );
  object->setTransform( transform );

  // fix the vertical range of the node from the estimated vertical range to the true range
  if ( mHandler->zMinimum() != std::numeric_limits<float>::max() && mHandler->zMaximum() != std::numeric_limits<float>::lowest() )
  {
    QgsBox3D box = mNode->box3D();
    box.setZMinimum( mHandler->zMinimum() );
    box.setZMaximum( mHandler->zMaximum() );
    mNode->setExactBox3D( box );
    mNode->updateParentBoundingBoxesRecursively();
  }

  return object;
}


///////////////


QgsVectorLayerChunkLoaderFactory::QgsVectorLayerChunkLoaderFactory( const Qgs3DRenderContext &context, QgsVectorLayer *vl, QgsAbstract3DSymbol *symbol, double zMin, double zMax, int maxFeatures )
  : mRenderContext( context )
  , mLayer( vl )
  , mSymbol( symbol->clone() )
  , mMaxFeatures( maxFeatures )
{
  if ( context.crs().type() == Qgis::CrsType::Geocentric )
  {
    // TODO: add support for handling of vector layers
    // (we're using dummy quadtree here to make sure the empty extent does not break the scene completely)
    QgsDebugError( u"Vector layers in globe scenes are not supported yet!"_s );
    setupQuadtree( QgsBox3D( -7e6, -7e6, -7e6, 7e6, 7e6, 7e6 ), -1, 3 );
    return;
  }

  // choose the smaller root extent between context and mLayer ones:
  QgsRectangle extent = context.extent();
  const QgsRectangle layerExtentInMapCrs = Qgs3DUtils::tryReprojectExtent2D( mLayer->extent(), mLayer->crs(), context.crs(), context.transformContext() );
  if ( layerExtentInMapCrs.isValid() )
  {
    extent = context.extent().intersect( layerExtentInMapCrs );
  }
  if ( extent.isValid() )
  {
    QgsBox3D rootBox3D( extent, zMin, zMax );
    // add small padding to avoid clipping of point features located at the edge of the bounding box
    rootBox3D.grow( 1.0 );

    const float rootError = static_cast<float>( std::max<double>( rootBox3D.width(), rootBox3D.height() ) * QgsVectorLayer3DTilingSettings::tileGeometryErrorRatio() );
    setupQuadtree( rootBox3D, rootError );
  }
}

QgsChunkLoader *QgsVectorLayerChunkLoaderFactory::createChunkLoader( QgsChunkNode *node ) const
{
  return new QgsVectorLayerChunkLoader( this, node );
}

bool QgsVectorLayerChunkLoaderFactory::canCreateChildren( QgsChunkNode *node )
{
  return mNodesAreLeafs.contains( node->tileId().text() );
}

QVector<QgsChunkNode *> QgsVectorLayerChunkLoaderFactory::createChildren( QgsChunkNode *node ) const
{
  if ( mNodesAreLeafs.value( node->tileId().text(), false ) )
    return {};

  return QgsQuadtreeChunkLoaderFactory::createChildren( node );
}

// if the AltitudeClamping is `Absolute`, the features do not follow the terrain
bool QgsVectorLayerChunkLoaderFactory::symbolFollowsTerrain() const
{
  const QString symbolType = mSymbol->type();
  if ( symbolType == "line"_L1 )
    return static_cast<QgsLine3DSymbol *>( mSymbol.get() )->altitudeClamping() != Qgis::AltitudeClamping::Absolute;
  if ( symbolType == "point"_L1 )
    return static_cast<QgsPoint3DSymbol *>( mSymbol.get() )->altitudeClamping() != Qgis::AltitudeClamping::Absolute;
  if ( symbolType == "polygon"_L1 )
    return static_cast<QgsPolygon3DSymbol *>( mSymbol.get() )->altitudeClamping() != Qgis::AltitudeClamping::Absolute;

  QgsDebugMsgLevel( u"QgsVectorLayerChunkLoaderFactory::symbolFollowsTerrain, unhandled symbol type %1"_s.arg( symbolType ), 2 );
  return true;
}


///////////////


QgsVectorLayerChunkedEntity::QgsVectorLayerChunkedEntity(
  Qgs3DMapSettings *map, QgsVectorLayer *vl, double zMin, double zMax, const QgsVectorLayer3DTilingSettings &tilingSettings, QgsAbstract3DSymbol *symbol
)
  : QgsChunkedEntity( map, 3, new QgsVectorLayerChunkLoaderFactory( Qgs3DRenderContext::fromMapSettings( map ), vl, symbol, zMin, zMax, tilingSettings.maximumChunkFeatures() ), true )
{
  connect( map, &Qgs3DMapSettings::terrainSettingsChanged, this, &QgsVectorLayerChunkedEntity::onTerrainElevationOffsetChanged );
  onTerrainElevationOffsetChanged();
}

QgsVectorLayerChunkedEntity::~QgsVectorLayerChunkedEntity()
{
  // cancel / wait for jobs
  cancelActiveJobs();
}

QgsVectorLayerChunkLoaderFactory *QgsVectorLayerChunkedEntity::loaderFactory() const
{
  return static_cast<QgsVectorLayerChunkLoaderFactory *>( mChunkLoaderFactory );
}

void QgsVectorLayerChunkedEntity::onTerrainElevationOffsetChanged()
{
  QgsVectorLayerChunkLoaderFactory *factory = loaderFactory();
  const float offset = factory->symbolFollowsTerrain() ? static_cast<float>( mMapSettings->terrainSettings()->elevationOffset() ) : 0.0f;
  factory->setTerrainElevationOffset( offset );

  // chunks that are already loaded only have to be moved, not rebuilt: the offset is a
  // transform on top of their vertices rather than something baked into them
  QMatrix4x4 transform;
  transform.translate( 0.0f, 0.0f, offset );

  if ( mRootNode )
  {
    const QList<QgsChunkNode *> nodes = mRootNode->descendants();
    for ( QgsChunkNode *node : nodes )
    {
      if ( QgsSceneObject3D *object = node->sceneObject() )
        object->setTransform( transform );
    }
  }

  setNeedsUpdate( true );
}

/// @endcond
