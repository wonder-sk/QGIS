#include "scene.h"

#include <Qt3DRender/QCamera>
#include <Qt3DRender/QPickingSettings>
#include <Qt3DRender/QRenderSettings>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QSkyboxEntity>
#include <Qt3DLogic/QFrameAction>

#include <QTimer>

#include "aabb.h"
#include "qgsabstract3drenderer.h"
#include "cameracontroller.h"
#include "map3d.h"
#include "terrain.h"
#include "terraingenerator.h"
//#include "testchunkloader.h"
#include "chunkedentity.h"
#include "utils.h"

#include <Qt3DRender/QMesh>
#include <Qt3DRender/QSceneLoader>
#include <Qt3DExtras/QPhongMaterial>

Scene::Scene( const Map3D &map, Qt3DExtras::QForwardRenderer *defaultFrameGraph, Qt3DRender::QCamera *camera, const QRect &viewportRect, Qt3DCore::QNode *parent )
  : Scene( map, defaultFrameGraph, nullptr, camera, viewportRect, parent )
{
}

Scene::Scene( const Map3D &map, Qt3DExtras::QForwardRenderer *defaultFrameGraph, Qt3DRender::QRenderSettings *renderSettings, Qt3DRender::QCamera *camera, const QRect &viewportRect, Qt3DCore::QNode *parent )
  : Qt3DCore::QEntity( parent )
  , mMap( map )
  , mTerrain( nullptr )
{
  defaultFrameGraph->setClearColor( map.backgroundColor );

  // TODO: strange - setting OnDemand render policy still keeps QGIS busy (Qt 5.9.0)
  // actually it is more busy than with the default "Always" policy although there are no changes in the scene.
  //renderSettings->setRenderPolicy( Qt3DRender::QRenderSettings::OnDemand );

#if QT_VERSION >= 0x050900
  // we want precise picking of terrain (also bounding volume picking does not seem to work - not sure why)
  if ( renderSettings )
    renderSettings->pickingSettings()->setPickMethod( Qt3DRender::QPickingSettings::TrianglePicking );
#endif

  // Camera
  float aspectRatio = ( float )viewportRect.width() / viewportRect.height();
  camera->lens()->setPerspectiveProjection( 45.0f, aspectRatio, 10.f, 10000.0f );

  mFrameAction = new Qt3DLogic::QFrameAction();
  connect( mFrameAction, &Qt3DLogic::QFrameAction::triggered,
           this, &Scene::onFrameTriggered );
  addComponent( mFrameAction ); // takes ownership

  // Camera controlling
  mCameraController = new CameraController( this ); // attaches to the scene
  mCameraController->setViewport( viewportRect );
  mCameraController->setCamera( camera );
  mCameraController->resetView();

  // create terrain entity

  createTerrain();
  connect( &map, &Map3D::terrainGeneratorChanged, this, &Scene::createTerrain );
  connect( &map, &Map3D::terrainVerticalScaleChanged, this, &Scene::createTerrain );
  connect( &map, &Map3D::mapTileResolutionChanged, this, &Scene::createTerrain );
  connect( &map, &Map3D::maxTerrainScreenErrorChanged, this, &Scene::createTerrain );
  connect( &map, &Map3D::maxTerrainGroundErrorChanged, this, &Scene::createTerrain );

  // create entities of renderers

  Q_FOREACH ( const QgsAbstract3DRenderer *renderer, map.renderers )
  {
    Qt3DCore::QEntity *p = renderer->createEntity( map );
    p->setParent( this );
  }

  // listen to changes of layers in order to add/remove 3D renderer entities
  connect( &map, &Map3D::layersChanged, this, &Scene::onLayersChanged );

  Qt3DCore::QEntity *lightEntity = new Qt3DCore::QEntity;
  Qt3DCore::QTransform *lightTransform = new Qt3DCore::QTransform;
  lightTransform->setTranslation( QVector3D( 0, 1000, 0 ) );
  // defaults: white, intensity 0.5
  // attenuation: constant 1.0, linear 0.0, quadratic 0.0
  Qt3DRender::QPointLight *light = new Qt3DRender::QPointLight;
  light->setConstantAttenuation( 0 );
  //light->setColor(Qt::white);
  //light->setIntensity(0.5);
  lightEntity->addComponent( light );
  lightEntity->addComponent( lightTransform );
  lightEntity->setParent( this );


#if 0
  ChunkedEntity *testChunkEntity = new ChunkedEntity( AABB( -500, 0, -500, 500, 100, 500 ), 2.f, 3.f, 7, new TestChunkLoaderFactory );
  testChunkEntity->setEnabled( false );
  testChunkEntity->setParent( this );
  chunkEntities << testChunkEntity;
#endif

  connect( mCameraController, &CameraController::cameraChanged, this, &Scene::onCameraChanged );
  connect( mCameraController, &CameraController::viewportChanged, this, &Scene::onCameraChanged );

#if 0
  // experiments with loading of existing 3D models.

  // scene loader only gets loaded only when added to a scene...
  // it loads everything: geometries, materials, transforms, lights, cameras (if any)
  Qt3DCore::QEntity *loaderEntity = new Qt3DCore::QEntity;
  Qt3DRender::QSceneLoader *loader = new Qt3DRender::QSceneLoader;
  loader->setSource( QUrl( "file:///home/martin/Downloads/LowPolyModels/tree.dae" ) );
  loaderEntity->addComponent( loader );
  loaderEntity->setParent( this );

  // mesh loads just geometry as one geometry...
  // so if there are different materials (e.g. colors) used in the model, this information is lost
  Qt3DCore::QEntity *meshEntity = new Qt3DCore::QEntity;
  Qt3DRender::QMesh *mesh = new Qt3DRender::QMesh;
  mesh->setSource( QUrl( "file:///home/martin/Downloads/LowPolyModels/tree.obj" ) );
  meshEntity->addComponent( mesh );
  Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial;
  material->setAmbient( Qt::red );
  meshEntity->addComponent( material );
  Qt3DCore::QTransform *meshTransform = new Qt3DCore::QTransform;
  meshTransform->setScale( 1 );
  meshEntity->addComponent( meshTransform );
  meshEntity->setParent( this );
#endif

  if ( map.skybox )
  {
    Qt3DExtras::QSkyboxEntity *skybox = new Qt3DExtras::QSkyboxEntity;
    skybox->setBaseName( map.skyboxFileBase );
    skybox->setExtension( map.skyboxFileExtension );
    skybox->setParent( this );

#if QT_VERSION >= 0x050900
    // docs say frustum culling must be disabled for skybox.
    // it _somehow_ works even when frustum culling is enabled with some camera positions,
    // but then when zoomed in more it would disappear - so let's keep frustum culling disabled
    defaultFrameGraph->setFrustumCullingEnabled( false );
#endif
  }

  // force initial update of chunked entities
  onCameraChanged();
}

SceneState _sceneState( CameraController *cameraController )
{
  Qt3DRender::QCamera *camera = cameraController->camera();
  SceneState state;
  state.cameraFov = camera->fieldOfView();
  state.cameraPos = camera->position();
  QRect rect = cameraController->viewport();
  state.screenSizePx = qMax( rect.width(), rect.height() ); // TODO: is this correct?
  state.viewProjectionMatrix = camera->projectionMatrix() * camera->viewMatrix();
  return state;
}

void Scene::onCameraChanged()
{
  Q_FOREACH ( ChunkedEntity *entity, chunkEntities )
  {
    if ( entity->isEnabled() )
      entity->update( _sceneState( mCameraController ) );
  }
}

void Scene::onFrameTriggered( float dt )
{
  mCameraController->frameTriggered( dt );

  Q_FOREACH ( ChunkedEntity *entity, chunkEntities )
  {
    if ( entity->isEnabled() && entity->needsUpdate )
    {
      qDebug() << "need for update";
      entity->update( _sceneState( mCameraController ) );
    }
  }
}

void Scene::createTerrain()
{
  if ( mTerrain )
  {
    chunkEntities.removeOne( mTerrain );

    mTerrain->deleteLater();
    mTerrain = nullptr;
  }

  if ( !mTerrainUpdateScheduled )
  {
    // defer re-creation of terrain: there may be multiple invokations of this slot, so create the new entity just once
    QTimer::singleShot( 0, this, &Scene::createTerrainDeferred );
    mTerrainUpdateScheduled = true;
  }
}

void Scene::createTerrainDeferred()
{
  double tile0width = mMap.terrainGenerator()->extent().width();
  int maxZoomLevel = Utils::maxZoomLevel( tile0width, mMap.mapTileResolution(), mMap.maxTerrainGroundError() );

  mTerrain = new Terrain( maxZoomLevel, mMap );
  //mTerrain->setEnabled(false);
  mTerrain->setParent( this );

  if ( mMap.showTerrainBoundingBoxes() )
    mTerrain->setShowBoundingBoxes( true );

  mCameraController->addTerrainPicker( mTerrain->terrainPicker() );

  chunkEntities << mTerrain;

  onCameraChanged();  // force update of the new terrain

  // make sure that renderers for layers are re-created as well
  Q_FOREACH ( QgsMapLayer *layer, mMap.layers() )
  {
    // remove old entity - if any
    removeLayerEntity( layer );

    // add new entity - if any 3D renderer
    addLayerEntity( layer );
  }

  mTerrainUpdateScheduled = false;
}

void Scene::onLayerRenderer3DChanged()
{
  QgsMapLayer *layer = qobject_cast<QgsMapLayer *>( sender() );
  Q_ASSERT( layer );

  // remove old entity - if any
  removeLayerEntity( layer );

  // add new entity - if any 3D renderer
  addLayerEntity( layer );
}

void Scene::onLayersChanged()
{
  QSet<QgsMapLayer *> layersBefore = QSet<QgsMapLayer *>::fromList( mLayerEntities.keys() );
  QList<QgsMapLayer *> layersAdded;
  Q_FOREACH ( QgsMapLayer *layer, mMap.layers() )
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
  Q_FOREACH ( QgsMapLayer *layer, layersBefore )
  {
    removeLayerEntity( layer );
  }

  Q_FOREACH ( QgsMapLayer *layer, layersAdded )
  {
    addLayerEntity( layer );
  }
}

void Scene::addLayerEntity( QgsMapLayer *layer )
{
  QgsAbstract3DRenderer *renderer = layer->renderer3D();
  if ( renderer )
  {
    Qt3DCore::QEntity *newEntity = renderer->createEntity( mMap );
    newEntity->setParent( this );
    mLayerEntities.insert( layer, newEntity );
  }

  connect( layer, &QgsMapLayer::renderer3DChanged, this, &Scene::onLayerRenderer3DChanged );
}

void Scene::removeLayerEntity( QgsMapLayer *layer )
{
  Qt3DCore::QEntity *entity = mLayerEntities.take( layer );
  if ( entity )
    entity->deleteLater();

  disconnect( layer, &QgsMapLayer::renderer3DChanged, this, &Scene::onLayerRenderer3DChanged );
}
