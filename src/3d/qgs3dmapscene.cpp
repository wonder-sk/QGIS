/***************************************************************************
  qgs3dmapscene.cpp
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

#include "qgs3dmapscene.h"

#include <limits>

#include "qgs3dmapsettings.h"
#include "qgs3dutils.h"
#include "qgsaabb.h"
#include "qgsabstract3drenderer.h"
#include "qgsabstractterrainsettings.h"
#include "qgsannotationlayer.h"
#include "qgsannotationlayer3drenderer.h"
#include "qgsapplication.h"
#include "qgscamera3d.h"
#include "qgscameracontroller.h"
#include "qgscategorized3drenderer.h"
#include "qgschunkedentity.h"
#include "qgschunknode.h"
#include "qgseventtracing.h"
#include "qgslightsource.h"
#include "qgslogger.h"
#include "qgsmaplayerelevationproperties.h"
#include "qgsmaplayertemporalproperties.h"
#include "qgsmeshlayer.h"
#include "qgsmeshlayer3drenderer.h"
#include "qgsmessageoutput.h"
#include "qgspoint3dsymbol.h"
#include "qgspointcloudlayer.h"
#include "qgspointcloudlayer3drenderer.h"
#include "qgsrhi3dengine.h"
#include "qgsrulebased3drenderer.h"
#include "qgssceneobject3d.h"
#include "qgsscenerenderer.h"
#include "qgssourcecache.h"
#include "qgsterrainentity.h"
#include "qgsterraingenerator.h"
#include "qgstiledscenelayer.h"
#include "qgstiledscenelayer3drenderer.h"
#include "qgsvectorlayer.h"
#include "qgsvectorlayer3drenderer.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QString>
#include <QSurface>
#include <QTimer>
#include <QUrl>
#include <QtMath>

#ifdef HAVE_TRACY
#include "tracy/Tracy.hpp"
#endif

#include "moc_qgs3dmapscene.cpp"

using namespace Qt::StringLiterals;

std::function<QMap<QString, Qgs3DMapScene *>()> Qgs3DMapScene::sOpenScenesFunction = [] { return QMap<QString, Qgs3DMapScene *>(); };

Qgs3DMapScene::Qgs3DMapScene( Qgs3DMapSettings &map, QgsRhi3DEngine *engine )
  : mMap( map )
  , mEngine( engine )
{
  mEngine->setClearColor( mMap.backgroundColor() );
  connect( &map, &Qgs3DMapSettings::backgroundColorChanged, this, [this] { mEngine->setClearColor( mMap.backgroundColor() ); } );

  // Get the maximum of clip planes available
  mMaxClipPlanes = Qgs3DUtils::openGlMaxClipPlanes( mEngine->surface() );

  // Camera controlling
  mCameraController = new QgsCameraController( this );

  if ( mMap.sceneMode() == Qgis::SceneMode::Globe )
    mCameraController->resetGlobe( 10'000'000 );
  else
    mCameraController->resetView( 1000 );

  // create terrain entity

  createTerrainDeferred();
  connect( &map, &Qgs3DMapSettings::extentChanged, this, &Qgs3DMapScene::createTerrain );
  connect( &map, &Qgs3DMapSettings::terrainGeneratorChanged, this, &Qgs3DMapScene::createTerrain );

  connect( &map, &Qgs3DMapSettings::terrainSettingsChanged, this, &Qgs3DMapScene::createTerrain );

  connect( &map, &Qgs3DMapSettings::terrainShadingChanged, this, &Qgs3DMapScene::createTerrain );
  connect( &map, &Qgs3DMapSettings::fieldOfViewChanged, this, &Qgs3DMapScene::updateCameraLens );
  connect( &map, &Qgs3DMapSettings::projectionTypeChanged, this, &Qgs3DMapScene::updateCameraLens );
  connect( &map, &Qgs3DMapSettings::msaaEnabledChanged, this, &Qgs3DMapScene::onMsaaEnabledChanged );
  connect( &map, &Qgs3DMapSettings::fpsCounterEnabledChanged, this, &Qgs3DMapScene::fpsCounterEnabledChanged );
  connect( &map, &Qgs3DMapSettings::cameraMovementSpeedChanged, this, &Qgs3DMapScene::onCameraMovementSpeedChanged );
  connect( &map, &Qgs3DMapSettings::cameraNavigationModeChanged, this, &Qgs3DMapScene::onCameraNavigationModeChanged );
  connect( &map, &Qgs3DMapSettings::stopUpdatesChanged, this, &Qgs3DMapScene::onStopUpdatesChanged );

  connect( &map, &Qgs3DMapSettings::originChanged, this, &Qgs3DMapScene::onOriginChanged );

  connect( QgsApplication::sourceCache(), &QgsSourceCache::remoteSourceFetched, this, [this]( const QString &url ) {
    const QList<QgsMapLayer *> modelVectorLayers = mModelVectorLayers;
    for ( QgsMapLayer *layer : modelVectorLayers )
    {
      QgsAbstract3DRenderer *renderer = layer->renderer3D();
      if ( renderer )
      {
        if ( renderer->type() == "vector"_L1 )
        {
          const QgsPoint3DSymbol *pointSymbol = static_cast<const QgsPoint3DSymbol *>( static_cast<QgsVectorLayer3DRenderer *>( renderer )->symbol() );
          if ( pointSymbol && pointSymbol->shapeProperty( u"model"_s ).toString() == url )
          {
            removeLayerEntity( layer );
            addLayerEntity( layer );
          }
        }
        else if ( renderer->type() == "rulebased"_L1 )
        {
          const QgsRuleBased3DRenderer::RuleList rules = static_cast<QgsRuleBased3DRenderer *>( renderer )->rootRule()->descendants();
          for ( const QgsRuleBased3DRenderer::Rule *rule : rules )
          {
            const QgsPoint3DSymbol *pointSymbol = dynamic_cast<const QgsPoint3DSymbol *>( rule->symbol() );
            if ( pointSymbol && pointSymbol->shapeProperty( u"model"_s ).toString() == url )
            {
              removeLayerEntity( layer );
              addLayerEntity( layer );
              break;
            }
          }
        }
      }
    }
  } );

  // listen to changes of layers in order to add/remove 3D renderer entities
  connect( &map, &Qgs3DMapSettings::layersChanged, this, &Qgs3DMapScene::onLayersChanged );

  connect( mCameraController, &QgsCameraController::cameraChanged, this, &Qgs3DMapScene::onCameraChanged );
  connect( mEngine, &QgsRhi3DEngine::sizeChanged, this, &Qgs3DMapScene::onCameraChanged );
  connect( mCameraController, &QgsCameraController::depthBufferReady, this, &Qgs3DMapScene::onViewed2DExtentFrom3DChanged );

  // every other effect setting is read out of the map settings once per frame by the
  // engine, so the only one that still needs a signal is the sample count, which is
  // baked into the render targets

  // force initial update of chunked entities
  onCameraChanged();
  // force initial update of MSAA setting
  onMsaaEnabledChanged();

  onCameraMovementSpeedChanged();

  mDepthBufferRefreshTimer = new QTimer( this );
  mDepthBufferRefreshTimer->setSingleShot( true );
  mDepthBufferRefreshTimer->setInterval( 100 );
  connect( mDepthBufferRefreshTimer, &QTimer::timeout, mCameraController, &QgsCameraController::requestDepthBufferCapture );
}

void Qgs3DMapScene::viewZoomFull()
{
  if ( mMap.sceneMode() == Qgis::SceneMode::Globe )
  {
    mCameraController->resetGlobe( 10'000'000 );
    return;
  }

  const QgsDoubleRange zRange = elevationRange();
  const QgsRectangle extent = sceneExtent();
  const double side = std::max( extent.width(), extent.height() );
  double d = side / 2 / std::tan( cameraController()->camera()->fieldOfView() / 2 * M_PI / 180 );
  d += zRange.isInfinite() ? 0. : zRange.upper();
  mCameraController->resetView( static_cast<float>( d ) );
}

void Qgs3DMapScene::setViewFrom2DExtent( const QgsRectangle &extent )
{
  QgsPointXY center = extent.center();
  const QgsVector3D origin = mMap.origin();

  const QgsVector3D p1 = mMap.mapToWorldCoordinates( QgsVector3D( extent.xMinimum(), extent.yMinimum(), 0 ) );
  const QgsVector3D p2 = mMap.mapToWorldCoordinates( QgsVector3D( extent.xMaximum(), extent.yMaximum(), 0 ) );

  const double xSide = std::abs( p1.x() - p2.x() );
  const double ySide = std::abs( p1.y() - p2.y() );
  const double side = std::max( xSide, ySide );

  const double fov = qDegreesToRadians( cameraController()->camera()->fieldOfView() );
  double distance = side / 2.0f / std::tan( fov / 2.0f );

  // adjust by elevation
  const QgsDoubleRange zRange = elevationRange();
  if ( !zRange.isInfinite() )
    distance += zRange.upper();

  // subtract map origin so coordinates are relative to it
  // clang-format off
  mCameraController->setViewFromTop(
    static_cast<float>( center.x() - origin.x() ),
    static_cast<float>( center.y() - origin.y() ),
    static_cast<float>( distance )
  );
  // clang-format on
}

QVector<QgsPointXY> Qgs3DMapScene::viewFrustum2DExtent() const
{
  QgsCamera3D *camera = mCameraController->camera();
  QVector<QgsPointXY> extent;

  const QSize size = mEngine->size();

  const QPoint center( size.width() / 2, size.height() / 2 );

  const QVector3D centerWorldPos = Qgs3DUtils::screenPointToWorldPos( center, static_cast<float>( mLastCenterDepth ), size, camera );
  const float centerZ = centerWorldPos.z();
  const QVector3D cameraPosition = camera->position();

  QVector<int> pointsOrder = { 0, 1, 3, 2 };
  for ( int i : pointsOrder )
  {
    const QPoint p( ( ( i >> 0 ) & 1 ) ? 0 : size.width(), ( ( i >> 1 ) & 1 ) ? 0 : size.height() );

    const QVector3D edgePoint = Qgs3DUtils::screenPointToWorldPos( p, static_cast<float>( mLastCenterDepth ), size, camera );
    const QVector3D rayDir = ( edgePoint - cameraPosition ).normalized();

    QVector3D worldPos;
    if ( std::abs( rayDir.z() ) > 0 )
    {
      float t = ( centerZ - cameraPosition.z() ) / rayDir.z();
      if ( t < 0 )
        t = camera->farPlane();
      else
        t = std::min<float>( t, camera->farPlane() );
      worldPos = cameraPosition + t * rayDir;
    }
    else
    {
      worldPos = cameraPosition + camera->farPlane() * rayDir;
      worldPos.setZ( centerZ );
    }

    QgsVector3D mapPos = mMap.worldToMapCoordinates( worldPos );
    extent.push_back( QgsPointXY( mapPos.x(), mapPos.y() ) );
  }
  return extent;
}

int Qgs3DMapScene::totalPendingJobsCount() const
{
  int count = 0;
  for ( Qgs3DMapSceneEntity *entity : std::as_const( mSceneEntities ) )
    count += entity->pendingJobsCount();
  return count;
}

double Qgs3DMapScene::worldSpaceError( double epsilon, double distance ) const
{
  QgsCamera3D *camera = mCameraController->camera();
  const double fov = camera->fieldOfView();
  const QSize size = mEngine->size();
  const int screenSizePx = std::max( size.width(), size.height() ); // TODO: is this correct?

  // see Qgs3DUtils::screenSpaceError() for the inverse calculation (world space error to screen space error)
  // with explanation of the math.
  const double frustumWidthAtDistance = 2 * distance * tan( fov / 2 );
  const double err = frustumWidthAtDistance * epsilon / screenSizePx;
  return err;
}

void Qgs3DMapScene::onCameraChanged()
{
  const QSize size = mEngine->size();
  mCameraController->camera()->setAspectRatio( static_cast<float>( size.width() ) / static_cast<float>( std::max( 1, size.height() ) ) );

  if ( mDepthBufferRefreshTimer )
    mDepthBufferRefreshTimer->start();

  updateScene( true );
  updateCameraNearFarPlanes();

  const QVector<QgsPointXY> extent2D = viewFrustum2DExtent();
  emit viewed2DExtentFrom3DChanged( extent2D );

  // The magic to make things work better in large scenes (e.g. more than 50km across)
  // is here: we will simply move the origin of the scene, and update transforms
  // of the camera and all other entities. That should ensure we will not need to deal
  // with large coordinates in 32-bit floats (and if we do have large coordinates,
  // because the scene is far from the camera, we don't care, because those errors
  // end up being tiny when viewed from far away).
  constexpr float ORIGIN_SHIFT_THRESHOLD = 10'000;
  if ( mSceneOriginShiftEnabled && mCameraController->camera()->position().length() > ORIGIN_SHIFT_THRESHOLD )
  {
    const QgsVector3D newOrigin = mMap.origin() + QgsVector3D( mCameraController->camera()->position() );
    QgsDebugMsgLevel( u"Rebasing scene origin from %1 to %2"_s.arg( mMap.origin().toString( 1 ), newOrigin.toString( 1 ) ), 2 );
    mMap.setOrigin( newOrigin );
  }
}

void Qgs3DMapScene::onViewed2DExtentFrom3DChanged()
{
  const QSize size = mEngine->size();
  const QPoint center( size.width() / 2, size.height() / 2 );

  const double sampledDepth = mCameraController->sampleDepthBuffer( center.x(), center.y() );

  if ( sampledDepth != 1.0 )
    mLastCenterDepth = sampledDepth;

  const QVector<QgsPointXY> extent2D = viewFrustum2DExtent();
  emit viewed2DExtentFrom3DChanged( extent2D );
}

bool Qgs3DMapScene::updateScene( bool forceUpdate )
{
  if ( !mSceneUpdatesEnabled )
  {
    QgsDebugMsgLevel( "Scene update skipped", 2 );
    return false;
  }

  QgsScopedEvent traceEvent( u"3D"_s, forceUpdate ? u"Force update scene"_s : u"Update scene"_s );

  Qgs3DMapSceneEntity::SceneContext sceneContext;
  QgsCamera3D *camera = mCameraController->camera();
  sceneContext.cameraFov = camera->fieldOfView();
  sceneContext.cameraPos = camera->position();
  const QSize size = mEngine->size();
  sceneContext.screenSizePx = std::max( size.width(), size.height() ); // TODO: is this correct?

  // Make our own projection matrix so that frustum culling done by the
  // entities isn't dependent on the current near/far planes, which would then
  // require multiple steps to stabilize.
  // The matrix is constructed just like in QMatrix4x4::perspective() /
  // ortho(), but for all elements involving the near and far plane, the limit
  // of the expression with the far plane going to infinity is taken.
  QMatrix4x4 projMatrix;
  switch ( mMap.projectionType() )
  {
    case Qgis::Map3DProjectionType::Perspective:
    {
      float fovRadians = ( camera->fieldOfView() / 2.0f ) * static_cast<float>( M_PI ) / 180.0f;
      float fovCotan = std::cos( fovRadians ) / std::sin( fovRadians );
      // clang-format off
      projMatrix = {
        fovCotan / camera->aspectRatio(), 0, 0, 0,
        0, fovCotan, 0, 0,
        0, 0, -1, -2,
        0, 0, -1, 0
      };
      // clang-format on
      break;
    }
    case Qgis::Map3DProjectionType::Orthographic:
    {
      // clang-format off
      projMatrix = {
        2.0f / ( camera->right() - camera->left() ), 0, 0, 0,
        0, 2.0f / ( camera->top() - camera->bottom() ), 0, 0,
        0, 0, 1, 0,
        -( camera->left() + camera->right() ) / ( camera->right() - camera->left() ), -( camera->top() + camera->bottom() ) / ( camera->top() - camera->bottom() ), -1.0f, 1.0f
      };
      // clang-format on
      break;
    }
    default:
      QgsDebugError( "Unhandled 3D projection type" );
      projMatrix = camera->projectionMatrix(); // Give up and use the current matrix
  }
  sceneContext.viewProjectionMatrix = projMatrix * camera->viewMatrix();


  bool anyUpdated = false;
  for ( Qgs3DMapSceneEntity *entity : std::as_const( mSceneEntities ) )
  {
    if ( forceUpdate || ( entity->isEnabled() && entity->needsUpdate() ) )
    {
      anyUpdated = true;
      entity->handleSceneUpdate( sceneContext );
      if ( entity->hasReachedGpuMemoryLimit() )
        emit gpuMemoryLimitReached();
    }
  }

  updateSceneState();

  return anyUpdated;
}

bool Qgs3DMapScene::updateCameraNearFarPlanes()
{
  // Update near and far plane from the terrain.
  // this needs to be done with great care as we have kind of circular dependency here:
  // active nodes are culled based on the current frustum (which involves near + far plane)
  // and then based on active nodes we set near and far plane.
  //
  // All of this is just heuristics assuming that all other stuff is being rendered somewhere
  // around the area where the terrain is.
  //
  // Near/far plane is setup in order to make best use of the depth buffer to avoid:
  // 1. precision errors - if the range is too great
  // 2. unwanted clipping of scene - if the range is too small

  QgsCamera3D *camera = cameraController()->camera();
  QMatrix4x4 viewMatrix = camera->viewMatrix();
  float fnear = 1e9;
  float ffar = 0;

  // Iterate all scene entities to make sure that they will not get
  // clipped by the near or far plane
  for ( Qgs3DMapSceneEntity *se : std::as_const( mSceneEntities ) )
  {
    const QgsRange<float> depthRange = se->getNearFarPlaneRange( viewMatrix );

    fnear = std::min( fnear, depthRange.lower() );
    ffar = std::max( ffar, depthRange.upper() );
  }

  if ( fnear < 1 )
    fnear = 1; // does not really make sense to use negative far plane (behind camera)

  // the update didn't work out... this can happen if the scene does not contain
  // any Qgs3DMapSceneEntity. Use the scene extent to compute near and far planes
  // as a fallback.
  if ( fnear == 1e9 && ffar == 0 )
  {
    QgsDoubleRange sceneZRange = elevationRange();
    sceneZRange = sceneZRange.isInfinite() ? QgsDoubleRange( 0.0, 0.0 ) : sceneZRange;
    const QgsAABB sceneBbox = Qgs3DUtils::mapToWorldExtent( mMap.extent(), sceneZRange.lower(), sceneZRange.upper(), mMap.origin() );
    Qgs3DUtils::computeBoundingBoxNearFarPlanes( sceneBbox, viewMatrix, fnear, ffar );
  }

  // when zooming in a lot, fnear can become smaller than ffar. This should not happen
  if ( fnear > ffar )
    std::swap( fnear, ffar );

  // set near/far plane - with some tolerance in front/behind expected near/far planes
  float newFar = ffar * 2;
  float newNear = fnear / 2;
  if ( !qgsFloatNear( newFar, camera->farPlane() ) || !qgsFloatNear( newNear, camera->nearPlane() ) )
  {
    cameraController()->setNearFarPlanes( newNear, newFar );
    return true;
  }

  return false;
}

void Qgs3DMapScene::onFrameTriggered( float dt )
{
#ifdef HAVE_TRACY
  FrameMark;
#endif
  QgsEventTracing::addEventToQgisTrace( QgsEventTracing::EventType::Instant, u"3D"_s, u"Frame begins"_s );

  mCameraController->frameTriggered( dt );

  if ( updateScene() )
    // If the scene was changed, node bboxes might have changed, so we need to
    // update near/far planes.
    updateCameraNearFarPlanes();

  // lock changing the FPS counter to 5 fps
  static int frameCount = 0;
  static float accumulatedTime = 0.0f;

  if ( !mMap.debugFlags().testFlag( Qgis::Map3DDebugFlag::ShowFPS ) )
  {
    frameCount = 0;
    accumulatedTime = 0;
    return;
  }

  frameCount++;
  accumulatedTime += dt;
  if ( accumulatedTime >= 0.2f )
  {
    float fps = ( float ) frameCount / accumulatedTime;
    frameCount = 0;
    accumulatedTime = 0.0f;
    emit fpsCountChanged( fps );
  }
}

void Qgs3DMapScene::createTerrain()
{
  if ( mTerrain )
  {
    mSceneEntities.removeOne( mTerrain );

    delete mTerrain;
    mTerrain = nullptr;
  }

  if ( !mTerrainUpdateScheduled )
  {
    // defer re-creation of terrain: there may be multiple invocations of this slot, so create the new entity just once
    QTimer::singleShot( 0, this, &Qgs3DMapScene::createTerrainDeferred );
    mTerrainUpdateScheduled = true;
    setSceneState( Updating );
  }
  else
  {
    emit terrainEntityChanged();
  }
}

void Qgs3DMapScene::createTerrainDeferred()
{
  if ( mMap.sceneMode() == Qgis::SceneMode::Local && mMap.terrainRenderingEnabled() && mMap.terrainGenerator() )
  {
    double tile0width = mMap.terrainGenerator()->rootChunkExtent().width();
    int maxZoomLevel = Qgs3DUtils::maxZoomLevel( tile0width, mMap.terrainSettings()->mapTileResolution(), mMap.terrainSettings()->maximumGroundError() );
    const QgsBox3D rootBox3D = mMap.terrainGenerator()->rootChunkBox3D( mMap );
    float rootError = mMap.terrainGenerator()->rootChunkError( mMap );
    const QgsBox3D clippingBox3D( mMap.extent(), rootBox3D.zMinimum(), rootBox3D.zMaximum() );
    mMap.terrainGenerator()->setupQuadtree( rootBox3D, rootError, maxZoomLevel, clippingBox3D );

    mTerrain = new QgsTerrainEntity( &mMap );
    mTerrain->setObjectName( u"Terrain"_s );
    mTerrain->setParent( this );

    mSceneEntities << mTerrain;

    connect( mTerrain, &QgsChunkedEntity::pendingJobsCountChanged, this, &Qgs3DMapScene::totalPendingJobsCountChanged );
  }

  // make sure that renderers for layers are re-created as well
  const QList<QgsMapLayer *> layers = mMap.layers();
  for ( QgsMapLayer *layer : layers )
  {
    // remove old entity - if any
    removeLayerEntity( layer );

    // add new entity - if any 3D renderer
    addLayerEntity( layer );
  }

  emit terrainEntityChanged();
  onCameraChanged(); // force update of the new terrain
  mTerrainUpdateScheduled = false;
}

void Qgs3DMapScene::updateCameraLens()
{
  QgsCamera3D *camera = mCameraController->camera();
  switch ( mMap.projectionType() )
  {
    case Qgis::Map3DProjectionType::Perspective:
      camera->setPerspectiveProjection( static_cast<float>( mMap.fieldOfView() ), camera->aspectRatio(), camera->nearPlane(), camera->farPlane() );
      break;
    case Qgis::Map3DProjectionType::Orthographic:
      // the box is derived from the distance to the view centre, which the controller owns
      camera->setFieldOfView( static_cast<float>( mMap.fieldOfView() ) );
      break;
  }
  onCameraChanged();
}

void Qgs3DMapScene::onLayerRenderer3DChanged()
{
  QgsMapLayer *layer = qobject_cast<QgsMapLayer *>( sender() );
  Q_ASSERT( layer );

  // remove old entity - if any
  removeLayerEntity( layer );

  // add new entity - if any 3D renderer
  addLayerEntity( layer );
}

void Qgs3DMapScene::onLayersChanged()
{
  QSet<QgsMapLayer *> layersBefore = qgis::listToSet( mLayerEntities.keys() );
  QList<QgsMapLayer *> layersAdded;
  const QList<QgsMapLayer *> layers = mMap.layers();
  for ( QgsMapLayer *layer : layers )
  {
    if ( !layersBefore.contains( layer ) )
    {
      layersAdded << layer;
    }
    else
    {
      layersBefore.remove( layer );
    }
  }

  // what is left in layersBefore are layers that have been removed
  for ( QgsMapLayer *layer : std::as_const( layersBefore ) )
  {
    removeLayerEntity( layer );
  }

  for ( QgsMapLayer *layer : std::as_const( layersAdded ) )
  {
    addLayerEntity( layer );
  }
}

void Qgs3DMapScene::updateTemporal()
{
  const QList<QgsMapLayer *> layers = mLayerEntities.keys();
  for ( QgsMapLayer *layer : layers )
  {
    if ( QgsMapLayerTemporalProperties *temporalProperties = layer->temporalProperties() )
    {
      if ( temporalProperties->isActive() )
      {
        removeLayerEntity( layer );
        addLayerEntity( layer );
      }
    }
  }
}

void Qgs3DMapScene::addSceneEntity( Qgs3DMapSceneEntity *sceneNewEntity )
{
  Q_ASSERT( sceneNewEntity );

  mSceneEntities.append( sceneNewEntity );

  sceneNewEntity->setParent( this );

  connect( sceneNewEntity, &Qgs3DMapSceneEntity::pendingJobsCountChanged, this, &Qgs3DMapScene::totalPendingJobsCountChanged );

  onCameraChanged(); // needed for chunked entities
}

void Qgs3DMapScene::removeSceneEntity( Qgs3DMapSceneEntity *sceneEntity )
{
  Q_ASSERT( sceneEntity );

  mSceneEntities.removeOne( sceneEntity );

  sceneEntity->deleteLater();
}


void Qgs3DMapScene::addLayerEntity( QgsMapLayer *layer )
{
  QgsAbstract3DRenderer *renderer = layer->renderer3D();
  if ( renderer )
  {
    // Fix vector layer's renderer to make sure the renderer is pointing to its layer.
    // It has happened before that renderer pointed to a different layer (probably after copying a style).
    // This is a bit of a hack and it should be handled in QgsMapLayer::setRenderer3D() but in qgis_core
    // the vector layer 3D renderer classes are not available.
    if ( layer->type() == Qgis::LayerType::Vector && ( renderer->type() == "vector"_L1 || renderer->type() == "rulebased"_L1 || renderer->type() == "categorized"_L1 ) )
    {
      static_cast<QgsAbstractVectorLayer3DRenderer *>( renderer )->setLayer( static_cast<QgsVectorLayer *>( layer ) );
      if ( renderer->type() == "vector"_L1 )
      {
        QgsVectorLayer *vlayer = qobject_cast<QgsVectorLayer *>( layer );
        if ( vlayer->geometryType() == Qgis::GeometryType::Point )
        {
          const QgsPoint3DSymbol *pointSymbol = static_cast<const QgsPoint3DSymbol *>( static_cast<QgsVectorLayer3DRenderer *>( renderer )->symbol() );
          if ( pointSymbol->shape() == Qgis::Point3DShape::Model )
          {
            mModelVectorLayers.append( layer );
          }
        }
      }
      else if ( renderer->type() == "rulebased"_L1 )
      {
        const QgsRuleBased3DRenderer::RuleList rules = static_cast<QgsRuleBased3DRenderer *>( renderer )->rootRule()->descendants();
        for ( auto rule : rules )
        {
          const QgsPoint3DSymbol *pointSymbol = dynamic_cast<const QgsPoint3DSymbol *>( rule->symbol() );
          if ( pointSymbol && pointSymbol->shape() == Qgis::Point3DShape::Model )
          {
            mModelVectorLayers.append( layer );
            break;
          }
        }
      }
      else if ( renderer->type() == "categorized"_L1 )
      {
        const Qgs3DCategoryList categories = static_cast<QgsCategorized3DRenderer *>( renderer )->categories();
        for ( const Qgs3DRendererCategory &category : categories )
        {
          const QgsPoint3DSymbol *pointSymbol = dynamic_cast<const QgsPoint3DSymbol *>( category.symbol() );
          if ( pointSymbol && pointSymbol->shape() == Qgis::Point3DShape::Model )
          {
            mModelVectorLayers.append( layer );
            break;
          }
        }
      }
    }
    else if ( layer->type() == Qgis::LayerType::Mesh && renderer->type() == "mesh"_L1 )
    {
      QgsMeshLayer3DRenderer *meshRenderer = static_cast<QgsMeshLayer3DRenderer *>( renderer );
      meshRenderer->setLayer( static_cast<QgsMeshLayer *>( layer ) );

      // Before entity creation, set the maximum texture size
      // Not very clean, but for now, only place found in the workflow to do that simple
      QgsMesh3DSymbol *sym = meshRenderer->symbol()->clone();
      sym->setMaximumTextureSize( maximumTextureSize() );
      meshRenderer->setSymbol( sym );
    }
    else if ( layer->type() == Qgis::LayerType::PointCloud && renderer->type() == "pointcloud"_L1 )
    {
      QgsPointCloudLayer3DRenderer *pointCloudRenderer = static_cast<QgsPointCloudLayer3DRenderer *>( renderer );
      pointCloudRenderer->setLayer( static_cast<QgsPointCloudLayer *>( layer ) );
    }
    else if ( layer->type() == Qgis::LayerType::TiledScene && renderer->type() == "tiledscene"_L1 )
    {
      QgsTiledSceneLayer3DRenderer *tiledSceneRenderer = static_cast<QgsTiledSceneLayer3DRenderer *>( renderer );
      tiledSceneRenderer->setLayer( static_cast<QgsTiledSceneLayer *>( layer ) );
    }
    else if ( layer->type() == Qgis::LayerType::Annotation && renderer->type() == "annotation"_L1 )
    {
      auto annotationLayerRenderer = qgis::down_cast<QgsAnnotationLayer3DRenderer *>( renderer );
      annotationLayerRenderer->setLayer( qobject_cast<QgsAnnotationLayer *>( layer ) );
    }

    if ( Qgs3DMapSceneEntity *newEntity = renderer->createEntity( &mMap ) )
    {
      // Add name to QObject for debugging
      newEntity->setObjectName( u"%1 3D entity"_s.arg( layer->name() ) );

      mLayerEntities.insert( layer, newEntity );

      // also sets this scene as the entity's parent
      addSceneEntity( newEntity );
    }
  }

  connect( layer, &QgsMapLayer::request3DUpdate, this, &Qgs3DMapScene::onLayerRenderer3DChanged );

  if ( layer->type() == Qgis::LayerType::Vector )
  {
    QgsVectorLayer *vlayer = qobject_cast<QgsVectorLayer *>( layer );
    connect( vlayer, &QgsVectorLayer::selectionChanged, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
    connect( vlayer, &QgsVectorLayer::layerModified, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
    connect( vlayer, &QgsVectorLayer::subsetStringChanged, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
  }

  if ( layer->type() == Qgis::LayerType::Mesh )
  {
    connect( layer, &QgsMapLayer::rendererChanged, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
  }

  if ( layer->type() == Qgis::LayerType::PointCloud )
  {
    QgsPointCloudLayer *pclayer = qobject_cast<QgsPointCloudLayer *>( layer );
    connect( pclayer, &QgsPointCloudLayer::renderer3DChanged, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
    connect( pclayer, &QgsPointCloudLayer::subsetStringChanged, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
  }
}

void Qgs3DMapScene::removeLayerEntity( QgsMapLayer *layer )
{
  if ( Qgs3DMapSceneEntity *entity = mLayerEntities.take( layer ) )
  {
    // also schedules the entity for deletion
    removeSceneEntity( entity );
  }

  disconnect( layer, &QgsMapLayer::request3DUpdate, this, &Qgs3DMapScene::onLayerRenderer3DChanged );

  if ( layer->type() == Qgis::LayerType::Vector )
  {
    QgsVectorLayer *vlayer = qobject_cast<QgsVectorLayer *>( layer );
    disconnect( vlayer, &QgsVectorLayer::selectionChanged, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
    disconnect( vlayer, &QgsVectorLayer::layerModified, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
    disconnect( vlayer, &QgsVectorLayer::subsetStringChanged, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
    mModelVectorLayers.removeAll( layer );
  }

  if ( layer->type() == Qgis::LayerType::Mesh )
  {
    disconnect( layer, &QgsMapLayer::rendererChanged, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
  }

  if ( layer->type() == Qgis::LayerType::PointCloud )
  {
    QgsPointCloudLayer *pclayer = qobject_cast<QgsPointCloudLayer *>( layer );
    disconnect( pclayer, &QgsPointCloudLayer::renderer3DChanged, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
    disconnect( pclayer, &QgsPointCloudLayer::subsetStringChanged, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
    disconnect( pclayer, &QgsPointCloudLayer::layerModified, this, &Qgs3DMapScene::onLayerRenderer3DChanged );
  }
}

int Qgs3DMapScene::maximumTextureSize() const
{
  QSurface *surface = mEngine->surface();
  QOpenGLContext context;
  context.create();
  bool success = context.makeCurrent( surface );

  if ( success )
  {
    QOpenGLFunctions openglFunctions = QOpenGLFunctions( &context );

    GLint size;
    openglFunctions.initializeOpenGLFunctions();
    openglFunctions.glGetIntegerv( GL_MAX_TEXTURE_SIZE, &size );
    return int( size );
  }
  else
  {
    return 4096; //we can't have a context to defined the max texture size, we use this reasonable value
  }
}

void Qgs3DMapScene::setSceneState( Qgs3DMapScene::SceneState state )
{
  if ( mSceneState == state )
    return;
  mSceneState = state;
  emit sceneStateChanged();
}

void Qgs3DMapScene::updateSceneState()
{
  if ( mTerrainUpdateScheduled )
  {
    setSceneState( Updating );
    return;
  }

  for ( Qgs3DMapSceneEntity *entity : std::as_const( mSceneEntities ) )
  {
    if ( entity->isEnabled() && entity->pendingJobsCount() > 0 )
    {
      setSceneState( Updating );
      return;
    }
  }

  setSceneState( Ready );
}

void Qgs3DMapScene::onMsaaEnabledChanged()
{
  if ( QgsRhi3DEngine *engine = qobject_cast<QgsRhi3DEngine *>( mEngine ) )
    engine->sceneRenderer()->setSampleCount( mMap.isMsaaEnabled() ? 4 : 1 );
}

void Qgs3DMapScene::onCameraMovementSpeedChanged()
{
  mCameraController->setCameraMovementSpeed( mMap.cameraMovementSpeed() );
}

void Qgs3DMapScene::onCameraNavigationModeChanged()
{
  mCameraController->setCameraNavigationMode( mMap.cameraNavigationMode() );
}

QVector<const QgsChunkNode *> Qgs3DMapScene::getLayerActiveChunkNodes( QgsMapLayer *layer )
{
  QVector<const QgsChunkNode *> chunks;
  if ( !mLayerEntities.contains( layer ) )
    return chunks;
  if ( QgsChunkedEntity *c = qobject_cast<QgsChunkedEntity *>( mLayerEntities[layer] ) )
  {
    const QList<QgsChunkNode *> activeNodes = c->activeNodes();
    for ( QgsChunkNode *n : activeNodes )
      chunks.push_back( n );
  }
  return chunks;
}

QgsRectangle Qgs3DMapScene::sceneExtent() const
{
  return mMap.extent();
}

QgsDoubleRange Qgs3DMapScene::elevationRange( const bool ignoreTerrain ) const
{
  double zMin = std::numeric_limits<double>::max();
  double zMax = std::numeric_limits<double>::lowest();
  if ( mMap.terrainRenderingEnabled() && mTerrain && !ignoreTerrain )
  {
    const QgsBox3D box3D = mTerrain->rootNode()->box3D();
    zMin = std::min( zMin, box3D.zMinimum() );
    zMax = std::max( zMax, box3D.zMaximum() );
  }

  for ( auto it = mLayerEntities.constBegin(); it != mLayerEntities.constEnd(); it++ )
  {
    QgsMapLayer *layer = it.key();
    switch ( layer->type() )
    {
      case Qgis::LayerType::PointCloud:
      {
        QgsPointCloudLayer *pcl = qobject_cast<QgsPointCloudLayer *>( layer );
        QgsDoubleRange zRange = pcl->elevationProperties()->calculateZRange( pcl );
        zMin = std::min( zMin, zRange.lower() );
        zMax = std::max( zMax, zRange.upper() );
        break;
      }
      case Qgis::LayerType::Mesh:
      {
        QgsMeshLayer *meshLayer = qobject_cast<QgsMeshLayer *>( layer );
        QgsAbstract3DRenderer *renderer3D = meshLayer->renderer3D();
        if ( renderer3D )
        {
          QgsMeshLayer3DRenderer *meshLayerRenderer = static_cast<QgsMeshLayer3DRenderer *>( renderer3D );
          const int verticalGroupDatasetIndex = meshLayerRenderer->symbol()->verticalDatasetGroupIndex();
          const QgsMeshDatasetGroupMetadata verticalGroupMetadata = meshLayer->datasetGroupMetadata( verticalGroupDatasetIndex );
          const double verticalScale = meshLayerRenderer->symbol()->verticalScale();
          zMin = std::min( zMin, verticalGroupMetadata.minimum() * verticalScale );
          zMax = std::max( zMax, verticalGroupMetadata.maximum() * verticalScale );
        }
        break;
      }
      case Qgis::LayerType::TiledScene:
      {
        QgsTiledSceneLayer *sceneLayer = qobject_cast<QgsTiledSceneLayer *>( layer );
        const QgsDoubleRange zRange = sceneLayer->elevationProperties()->calculateZRange( sceneLayer );
        if ( !zRange.isInfinite() && !zRange.isEmpty() )
        {
          zMin = std::min( zMin, zRange.lower() );
          zMax = std::max( zMax, zRange.upper() );
        }
        break;
      }
      case Qgis::LayerType::Annotation:
      case Qgis::LayerType::Group:
      case Qgis::LayerType::Plugin:
      case Qgis::LayerType::Raster:
      case Qgis::LayerType::Vector:
      case Qgis::LayerType::VectorTile:
        break;
    }
  }
  const QgsDoubleRange zRange( std::min( zMin, std::numeric_limits<double>::max() ), std::max( zMax, std::numeric_limits<double>::lowest() ) );
  return zRange.isEmpty() ? QgsDoubleRange() : zRange;
}

QMap<QString, Qgs3DMapScene *> Qgs3DMapScene::openScenes()
{
  return sOpenScenesFunction();
}

void Qgs3DMapScene::onOriginChanged()
{
  // scene objects keep their position in map coordinates, so where they are drawn
  // already follows the origin; only the camera and the clip planes need moving

  const QgsVector3D oldOrigin = mCameraController->origin();
  mCameraController->setOrigin( mMap.origin() );

  if ( !mClipPlanesEquations.isEmpty() )
  {
    // how the math works - for a plane defined as (a,b,c,d), only "d" changes when
    // moving the origin - the plane normal vector (a,b,c) stays the same.
    // - line equation for old shift: a * (x - x0) + b * (y - y0) + c * (z - z0) + d0 = 0
    // - line equation for new shift: a * (x - x1) + b * (y - y1) + c * (z - z1) + d1 = 0
    // - we solve for d1:
    //     d1 = a * (x1 - x0) + b * (y1 - y0) + c * (z1 - z0) + d0

    QList<QVector4D> newPlanes;
    QgsVector3D originShift = mMap.origin() - oldOrigin;
    for ( QVector4D plane : std::as_const( mClipPlanesEquations ) )
    {
      plane.setW( originShift.x() * plane.x() + originShift.y() * plane.y() + originShift.z() * plane.z() + plane.w() );
      newPlanes.append( plane );
    }
    enableClipping( newPlanes );
  }
}

void Qgs3DMapScene::collectRenderCommands( QgsSceneRenderer &renderer )
{
  const bool shadowsEnabled = renderer.shadowsEnabled();

  // scene objects hold their position in map coordinates, so where they are drawn follows
  // whatever the origin is now rather than whatever it was when their chunk was loaded
  const QgsVector3D origin = mMap.origin();

  for ( Qgs3DMapSceneEntity *entity : std::as_const( mSceneEntities ) )
  {
    QgsChunkedEntity *chunkedEntity = qobject_cast<QgsChunkedEntity *>( entity );
    if ( !chunkedEntity || !chunkedEntity->isEnabled() )
      continue;

    const QList<QgsChunkNode *> nodes = chunkedEntity->activeNodes();
    for ( QgsChunkNode *node : nodes )
    {
      QgsSceneObject3D *object = node->sceneObject();
      if ( !object )
        continue;

      object->collectCommands( renderer.forwardPass().renderPass(), origin );
      if ( shadowsEnabled )
        object->collectShadowCommands( renderer.shadowPass(), origin );
    }
  }
}

void Qgs3DMapScene::enableClipping( const QList<QVector4D> &clipPlaneEquations )
{
  if ( clipPlaneEquations.size() > mMaxClipPlanes )
  {
    QgsDebugMsgLevel( u"Qgs3DMapScene::enableClipping: it is not possible to use more than %1 clipping planes."_s.arg( mMaxClipPlanes ), 2 );
  }
  mClipPlanesEquations = clipPlaneEquations.mid( 0, mMaxClipPlanes );

  // TODO: clipping is not implemented in the QRhi engine yet - it was a fixed function
  // state under Qt3D and has to become a shader side test
}

void Qgs3DMapScene::disableClipping()
{
  mClipPlanesEquations.clear();
}

void Qgs3DMapScene::onStopUpdatesChanged()
{
  mSceneUpdatesEnabled = !mMap.stopUpdates();
}
