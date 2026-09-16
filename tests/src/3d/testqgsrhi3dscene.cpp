/***************************************************************************
     testqgsrhi3dscene.cpp
     ----------------------
    Date                 : September 2026
    Copyright            : (C) 2026 by Martin Dobias
    Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "qgs3d.h"
#include "qgs3dmapscene.h"
#include "qgs3dmapsettings.h"
#include "qgsapplication.h"
#include "qgscamera3d.h"
#include "qgscameracontroller.h"
#include "qgschunkedentity.h"
#include "qgschunknode.h"
#include "qgscoordinatetransform.h"
#include "qgsdemterraingenerator.h"
#include "qgsdirectionallightsettings.h"
#include "qgsflatterraingenerator.h"
#include "qgsgeometry3d.h"
#include "qgspointcloud3dsymbol.h"
#include "qgspointcloudlayer.h"
#include "qgspointcloudlayer3drenderer.h"
#include "qgspointcloudmaterial3d.h"
#include "qgspointlightsettings.h"
#include "qgsproject.h"
#include "qgsrasterlayer.h"
#include "qgsrhi3dengine.h"
#include "qgssceneobject3d.h"
#include "qgsterrainentity.h"
#include "qgstest.h"
#include "qgstiledscenelayer.h"
#include "qgstiledscenelayer3drenderer.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QString>

using namespace Qt::StringLiterals;

/**
 * Renders a whole Qgs3DMapScene through the QRhi engine, offscreen.
 *
 * This is the end to end test of P5: the scene is assembled exactly as the 3D view
 * assembles it, the flat terrain's chunks produce QgsSceneObject3Ds instead of Qt3D
 * entities, and QgsRhi3DEngine turns them into a frame.
 */
class TestQgsRhi3DScene : public QgsTest
{
    Q_OBJECT

  public:
    TestQgsRhi3DScene()
      : QgsTest( u"QgsRhi3DScene"_s )
    {}

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void testOffscreenEngineInitializes();
    void testFlatTerrainProducesSceneObjects();
    void testRendersTerrain();
    void testCameraMovesWhatIsDrawn();
    void testRendersDemTerrain();

    void testPointCloudProducesSceneObjects();
    void testRendersTriangulatedPointCloud();

    void testRendersTiledScene();
    void testRendersInstancedTiledScene();

  private:
    /**
     * Builds the map settings a terrain scene needs, flat unless \a withDem is TRUE.
     */
    std::unique_ptr<Qgs3DMapSettings> createMapSettings( bool withDem = false ) const;

    /**
     * Builds the map settings a point cloud scene needs: the layer's own CRS and extent,
     * no terrain, and one overhead directional light.
     */
    std::unique_ptr<Qgs3DMapSettings> createPointCloudMapSettings() const;

    /**
     * Returns the active chunk nodes of the chunked entity \a scene built for \a layer.
     *
     * Every layer whose loader has been ported reaches the frame this way, so this is
     * what a test reads to see what a loader actually produced.
     */
    static QList<QgsChunkNode *> layerChunkNodes( Qgs3DMapScene *scene, QgsMapLayer *layer );

    //! Counts the pixels of \a image whose channels add up to more than \a threshold
    static int countLitPixels( const QImage &image, int threshold = 30 );

    /**
     * Runs frames until every chunk has loaded, and returns whether it got there.
     *
     * There is no aspect engine ticking the scene here, so the test drives the loop the
     * way the 3D view does: pump the event loop for the chunk jobs, advance the scene,
     * render. Returns FALSE if \a timeoutMs passed with jobs still outstanding, so that
     * a machine too loaded to finish loading fails loudly rather than as an empty scene.
     */
    Q_REQUIRED_RESULT bool renderUntilLoaded( QgsRhi3DEngine &engine, Qgs3DMapScene *scene, int timeoutMs = 60000 ) const;

    std::unique_ptr<QgsProject> mProject;
    QgsRasterLayer *mLayerDtm = nullptr;
    QgsPointCloudLayer *mLayerPointCloud = nullptr;
};

void TestQgsRhi3DScene::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
  Qgs3D::initialize();

  mProject = std::make_unique<QgsProject>();
  mProject->setCrs( QgsCoordinateReferenceSystem( u"EPSG:27700"_s ) );

  const QString dataDir( TEST_DATA_DIR );
  mLayerDtm = new QgsRasterLayer( dataDir + "/3d/dtm.tif", u"dtm"_s, u"gdal"_s );
  QVERIFY( mLayerDtm->isValid() );
  mProject->addMapLayer( mLayerDtm );

  mLayerPointCloud = new QgsPointCloudLayer( dataDir + "/point_clouds/ept/sunshine-coast-laz/ept.json", u"point cloud"_s, u"ept"_s );
  QVERIFY( mLayerPointCloud->isValid() );
  mProject->addMapLayer( mLayerPointCloud );
}

QList<QgsChunkNode *> TestQgsRhi3DScene::layerChunkNodes( Qgs3DMapScene *scene, QgsMapLayer *layer )
{
  Qt3DCore::QEntity *entity = scene->layerEntity( layer );

  // QgsChunkedEntity is not exported, so its metaobject cannot be reached from here and
  // qobject_cast will not link. inherits() asks the same question through the vtable, and
  // the inheritance is single, so a static_cast is all that is left to do.
  if ( !entity || !entity->inherits( "QgsChunkedEntity" ) )
    return QList<QgsChunkNode *>();

  return static_cast<QgsChunkedEntity *>( entity )->activeNodes();
}

void TestQgsRhi3DScene::cleanupTestCase()
{
  mProject.reset();
  QgsApplication::exitQgis();
}

std::unique_ptr<Qgs3DMapSettings> TestQgsRhi3DScene::createMapSettings( bool withDem ) const
{
  auto map = std::make_unique<Qgs3DMapSettings>();
  map->setCrs( mProject->crs() );
  map->setExtent( mLayerDtm->extent() );
  map->setLayers( QList<QgsMapLayer *>() << mLayerDtm );
  map->setTransformContext( mProject->transformContext() );
  map->setPathResolver( mProject->pathResolver() );
  map->setMapThemeCollection( mProject->mapThemeCollection() );
  map->setOutputDpi( 96 );
  map->setBackgroundColor( Qt::black );

  if ( withDem )
  {
    auto *demTerrain = new QgsDemTerrainGenerator;
    demTerrain->setCrs( map->crs(), map->transformContext() );
    demTerrain->setLayer( mLayerDtm );
    map->setTerrainGenerator( demTerrain );
  }
  else
  {
    auto *flatTerrain = new QgsFlatTerrainGenerator;
    flatTerrain->setCrs( map->crs(), map->transformContext() );
    map->setTerrainGenerator( flatTerrain );
  }

  QgsPointLightSettings light;
  light.setPosition( map->origin() + QgsVector3D( 0, 0, 1000 ) );
  light.setConstantAttenuation( 0 );
  map->setLightSources( { light.clone() } );

  // this is what makes the chunk loaders produce scene objects rather than entities
  map->setRhiEngineEnabled( true );
  return map;
}

bool TestQgsRhi3DScene::renderUntilLoaded( QgsRhi3DEngine &engine, Qgs3DMapScene *scene, int timeoutMs ) const
{
  QElapsedTimer timer;
  timer.start();

  // the chunk loaders run on worker threads, so a few quiet frames in a row is what says
  // the scene has settled rather than that a job simply has not been posted back yet
  int idleFrames = 0;
  while ( idleFrames < 10 )
  {
    if ( timer.elapsed() > timeoutMs )
      return false;

    QCoreApplication::processEvents( QEventLoop::AllEvents, 10 );
    scene->onFrameTriggered( 0.016f );
    engine.renderFrame();

    if ( scene->totalPendingJobsCount() == 0 && scene->sceneState() == Qgs3DMapScene::Ready )
      ++idleFrames;
    else
      idleFrames = 0;
  }
  return true;
}

void TestQgsRhi3DScene::testOffscreenEngineInitializes()
{
  QgsRhi3DEngine engine( QSize( 400, 300 ) );
  QVERIFY( engine.initialize() );
  QVERIFY( engine.isValid() );
  QCOMPARE( engine.size(), QSize( 400, 300 ) );
  QVERIFY( engine.sceneRenderer() );

  // the Qt3D scaffolding is present but inert
  QVERIFY( engine.camera() );
  QVERIFY( engine.renderSettings() );
  QVERIFY( engine.frameGraph() );
  QVERIFY( !engine.window() );
}

void TestQgsRhi3DScene::testFlatTerrainProducesSceneObjects()
{
  std::unique_ptr<Qgs3DMapSettings> map = createMapSettings();

  QgsRhi3DEngine engine( QSize( 400, 300 ) );
  QVERIFY( engine.initialize() );

  auto scene = std::make_unique<Qgs3DMapScene>( *map, &engine );
  engine.setRootEntity( scene.get() );
  engine.setScene( scene.get() );
  QVERIFY( renderUntilLoaded( engine, scene.get() ) );

  QgsTerrainEntity *terrain = scene->terrainEntity();
  QVERIFY( terrain );

  const QList<QgsChunkNode *> nodes = terrain->activeNodes();
  QVERIFY( !nodes.isEmpty() );

  // every loaded terrain chunk is a scene object rather than a Qt3D entity, and it draws
  // one textured quad
  for ( QgsChunkNode *node : nodes )
  {
    QVERIFY( node->hasPayload() );
    QVERIFY( !node->entity() );
    QgsSceneObject3D *object = node->sceneObject();
    QVERIFY( object );
    QCOMPARE( object->parts().size(), size_t( 1 ) );
    QVERIFY( object->parts()[0]->geometry->vertexCount() > 0 );
    QVERIFY( object->gpuMemorySize() > 0 );
  }
}

void TestQgsRhi3DScene::testRendersTerrain()
{
  std::unique_ptr<Qgs3DMapSettings> map = createMapSettings();

  QgsRhi3DEngine engine( QSize( 400, 300 ) );
  QVERIFY( engine.initialize() );

  auto scene = std::make_unique<Qgs3DMapScene>( *map, &engine );
  engine.setRootEntity( scene.get() );
  engine.setScene( scene.get() );

  scene->viewZoomFull();
  QVERIFY( renderUntilLoaded( engine, scene.get() ) );

  const QImage image = engine.renderedImage();
  QVERIFY( !image.isNull() );
  QCOMPARE( image.size(), QSize( 400, 300 ) );

  // the background is black, so anything that is not black is terrain
  int litPixels = 0;
  for ( int y = 0; y < image.height(); ++y )
  {
    for ( int x = 0; x < image.width(); ++x )
    {
      const QColor color = image.pixelColor( x, y );
      if ( color.red() + color.green() + color.blue() > 30 )
        ++litPixels;
    }
  }
  QVERIFY2( litPixels > image.width() * image.height() / 10, qPrintable( u"only %1 pixels were drawn"_s.arg( litPixels ) ) );
}

void TestQgsRhi3DScene::testCameraMovesWhatIsDrawn()
{
  std::unique_ptr<Qgs3DMapSettings> map = createMapSettings();

  QgsRhi3DEngine engine( QSize( 400, 300 ) );
  QVERIFY( engine.initialize() );

  auto scene = std::make_unique<Qgs3DMapScene>( *map, &engine );
  engine.setRootEntity( scene.get() );
  engine.setScene( scene.get() );

  scene->viewZoomFull();
  QVERIFY( renderUntilLoaded( engine, scene.get() ) );
  const QImage fromTop = engine.renderedImage();

  // tilt right over to the side; the terrain should no longer fill the frame the same way
  QgsCameraController *controller = scene->cameraController();
  controller->setLookingAtPoint( controller->lookingAtPoint(), controller->distance(), 85.0f, 30.0f );
  QVERIFY( renderUntilLoaded( engine, scene.get() ) );
  const QImage fromSide = engine.renderedImage();

  QVERIFY( !fromTop.isNull() );
  QVERIFY( !fromSide.isNull() );
  QVERIFY( fromTop != fromSide );

  // and the camera the passes rendered with is the one the controller owns
  QCOMPARE( controller->pitch(), 85.0f );
  QCOMPARE( controller->yaw(), 30.0f );
}

void TestQgsRhi3DScene::testRendersDemTerrain()
{
  std::unique_ptr<Qgs3DMapSettings> map = createMapSettings( true );
  map->setTerrainShadingEnabled( true );

  QgsRhi3DEngine engine( QSize( 400, 300 ) );
  QVERIFY( engine.initialize() );

  auto scene = std::make_unique<Qgs3DMapScene>( *map, &engine );
  engine.setRootEntity( scene.get() );
  engine.setScene( scene.get() );

  // the DEM's elevation range is only known once tiles have loaded, and viewZoomFull()
  // uses it to choose the camera distance, so frame the view after the first load
  QVERIFY( renderUntilLoaded( engine, scene.get() ) );
  scene->viewZoomFull();

  // look at the relief from the side, where a DEM differs from a flat plane
  QgsCameraController *controller = scene->cameraController();
  controller->setLookingAtPoint( controller->lookingAtPoint(), controller->distance(), 60.0f, 0.0f );
  QVERIFY( renderUntilLoaded( engine, scene.get() ) );

  QgsTerrainEntity *terrain = scene->terrainEntity();
  QVERIFY( terrain );

  const QList<QgsChunkNode *> nodes = terrain->activeNodes();
  QVERIFY( !nodes.isEmpty() );

  // a DEM tile is a grid rather than the flat terrain's two triangles
  bool sawGrid = false;
  for ( QgsChunkNode *node : nodes )
  {
    QgsSceneObject3D *object = node->sceneObject();
    QVERIFY( object );
    QCOMPARE( object->parts().size(), size_t( 1 ) );
    if ( object->parts()[0]->geometry->vertexCount() > 100 )
      sawGrid = true;
  }
  QVERIFY( sawGrid );

  const QImage image = engine.renderedImage();
  QVERIFY( !image.isNull() );

  int litPixels = 0;
  for ( int y = 0; y < image.height(); ++y )
  {
    for ( int x = 0; x < image.width(); ++x )
    {
      const QColor color = image.pixelColor( x, y );
      if ( color.red() + color.green() + color.blue() > 30 )
        ++litPixels;
    }
  }
  QVERIFY2( litPixels > 1000, qPrintable( u"only %1 pixels were drawn"_s.arg( litPixels ) ) );
}

std::unique_ptr<Qgs3DMapSettings> TestQgsRhi3DScene::createPointCloudMapSettings() const
{
  auto map = std::make_unique<Qgs3DMapSettings>();
  map->setCrs( mLayerPointCloud->crs() );
  map->setExtent( mLayerPointCloud->extent() );
  map->setLayers( QList<QgsMapLayer *>() << mLayerPointCloud );
  map->setTransformContext( mProject->transformContext() );
  map->setPathResolver( mProject->pathResolver() );
  map->setMapThemeCollection( mProject->mapThemeCollection() );
  map->setOutputDpi( 96 );
  map->setBackgroundColor( Qt::black );

  // the point cloud is the only thing in the frame, so that counting lit pixels counts it
  map->setTerrainRenderingEnabled( false );

  QgsDirectionalLightSettings light;
  light.setDirection( QgsVector3D( 0, 0, -1 ) );
  map->setLightSources( { light.clone() } );

  map->setRhiEngineEnabled( true );
  return map;
}

int TestQgsRhi3DScene::countLitPixels( const QImage &image, int threshold )
{
  int count = 0;
  for ( int y = 0; y < image.height(); ++y )
  {
    for ( int x = 0; x < image.width(); ++x )
    {
      const QColor color = image.pixelColor( x, y );
      if ( color.red() + color.green() + color.blue() > threshold )
        ++count;
    }
  }
  return count;
}

void TestQgsRhi3DScene::testPointCloudProducesSceneObjects()
{
  std::unique_ptr<Qgs3DMapSettings> map = createPointCloudMapSettings();

  auto *symbol = new QgsSingleColorPointCloud3DSymbol();
  symbol->setSingleColor( QColor( 255, 0, 0 ) );
  symbol->setPointSize( 4 );
  auto *renderer = new QgsPointCloudLayer3DRenderer();
  renderer->setSymbol( symbol );
  mLayerPointCloud->setRenderer3D( renderer );

  QgsRhi3DEngine engine( QSize( 400, 300 ) );
  QVERIFY( engine.initialize() );

  auto scene = std::make_unique<Qgs3DMapScene>( *map, &engine );
  engine.setRootEntity( scene.get() );
  engine.setScene( scene.get() );

  scene->viewZoomFull();
  QVERIFY( renderUntilLoaded( engine, scene.get() ) );

  int chunksWithPoints = 0;
  const QList<QgsChunkNode *> nodes = layerChunkNodes( scene.get(), mLayerPointCloud );
  QVERIFY( !nodes.isEmpty() );

  for ( QgsChunkNode *node : nodes )
  {
    QVERIFY( node->hasPayload() );
    // the loader produces a scene object, so there is no Qt3D entity left to produce
    QVERIFY( !node->entity() );

    QgsSceneObject3D *object = node->sceneObject();
    QVERIFY( object );
    QCOMPARE( object->parts().size(), size_t( 1 ) );

    const QgsSceneObject3D::Part *part = object->parts()[0].get();
    QVERIFY( part->geometry->vertexCount() > 0 );
    QVERIFY( !part->geometry->isIndexed() ); // drawn as points, so no triangles

    const auto *material = dynamic_cast<const QgsPointCloudMaterial3D *>( part->material.get() );
    QVERIFY( material );
    QCOMPARE( material->style(), QgsPointCloudMaterial3D::Style::SingleColor );
    QVERIFY( !material->isTriangulated() );
    QCOMPARE( material->topology(), QRhiGraphicsPipeline::Points );

    ++chunksWithPoints;
  }
  QVERIFY( chunksWithPoints > 0 );
}

void TestQgsRhi3DScene::testRendersTriangulatedPointCloud()
{
  std::unique_ptr<Qgs3DMapSettings> map = createPointCloudMapSettings();

  // the triangulated path is the one that does not wait for P6's instanced quads, so it
  // is the one a pixel count can lean on
  auto *symbol = new QgsSingleColorPointCloud3DSymbol();
  symbol->setSingleColor( QColor( 255, 0, 0 ) );
  symbol->setRenderAsTriangles( true );
  auto *renderer = new QgsPointCloudLayer3DRenderer();
  renderer->setSymbol( symbol );
  mLayerPointCloud->setRenderer3D( renderer );

  QgsRhi3DEngine engine( QSize( 400, 300 ) );
  QVERIFY( engine.initialize() );

  auto scene = std::make_unique<Qgs3DMapScene>( *map, &engine );
  engine.setRootEntity( scene.get() );
  engine.setScene( scene.get() );

  scene->viewZoomFull();
  QVERIFY( renderUntilLoaded( engine, scene.get() ) );

  const QList<QgsChunkNode *> nodes = layerChunkNodes( scene.get(), mLayerPointCloud );
  QVERIFY( !nodes.isEmpty() );

  bool sawTriangles = false;
  for ( QgsChunkNode *node : nodes )
  {
    QgsSceneObject3D *object = node->sceneObject();
    if ( !object || object->parts().empty() )
      continue;

    const QgsSceneObject3D::Part *part = object->parts()[0].get();
    const auto *material = dynamic_cast<const QgsPointCloudMaterial3D *>( part->material.get() );
    QVERIFY( material );
    if ( material->isTriangulated() )
    {
      // a triangulated chunk is indexed and carries the normal that lighting needs
      QVERIFY( part->geometry->isIndexed() );
      QVERIFY( part->geometry->attribute( u"vertexNormal"_s ) );
      QCOMPARE( material->topology(), QRhiGraphicsPipeline::Triangles );
      sawTriangles = true;
    }
  }
  QVERIFY( sawTriangles );

  const QImage image = engine.renderedImage();
  QVERIFY( !image.isNull() );

  const int lit = countLitPixels( image );
  QVERIFY2( lit > 1000, qPrintable( u"only %1 pixels were drawn"_s.arg( lit ) ) );
}

void TestQgsRhi3DScene::testRendersTiledScene()
{
  auto layer = std::make_unique<QgsTiledSceneLayer>( QString( TEST_DATA_DIR ) + "/tiled_scene/tileset.json", u"tiled scene"_s, u"cesiumtiles"_s );
  QVERIFY( layer->dataProvider()->isValid() );
  layer->setRenderer3D( new QgsTiledSceneLayer3DRenderer() );

  auto map = std::make_unique<Qgs3DMapSettings>();
  map->setCrs( QgsCoordinateReferenceSystem( u"EPSG:3857"_s ) );
  map->setTransformContext( mProject->transformContext() );
  map->setOutputDpi( 96 );
  map->setBackgroundColor( Qt::black );
  map->setTerrainRenderingEnabled( false );
  map->setLayers( QList<QgsMapLayer *>() << layer.get() );

  const QgsCoordinateTransform toMapCrs( layer->crs(), map->crs(), mProject->transformContext() );
  map->setExtent( toMapCrs.transformBoundingBox( layer->extent() ) );

  QgsDirectionalLightSettings light;
  map->setLightSources( { light.clone() } );
  map->setRhiEngineEnabled( true );

  QgsRhi3DEngine engine( QSize( 400, 300 ) );
  QVERIFY( engine.initialize() );

  auto scene = std::make_unique<Qgs3DMapScene>( *map, &engine );
  engine.setRootEntity( scene.get() );
  engine.setScene( scene.get() );

  scene->viewZoomFull();
  QVERIFY( renderUntilLoaded( engine, scene.get() ) );

  // the b3dm tiles are glTF meshes, so every loaded chunk is a scene object of one part
  // per mesh primitive
  int parts = 0;
  const QList<QgsChunkNode *> nodes = layerChunkNodes( scene.get(), layer.get() );
  QVERIFY( !nodes.isEmpty() );
  for ( QgsChunkNode *node : nodes )
  {
    QgsSceneObject3D *object = node->sceneObject();
    if ( !object )
      continue;
    QVERIFY( !node->entity() );
    for ( const std::unique_ptr<QgsSceneObject3D::Part> &part : object->parts() )
    {
      QVERIFY( part->geometry->vertexCount() > 0 );
      QCOMPARE( part->instanceCount, 1 );
      ++parts;
    }
  }
  QVERIFY2( parts > 0, "no tiled scene chunk produced any parts" );

  const QImage image = engine.renderedImage();
  QVERIFY( !image.isNull() );
  const int lit = countLitPixels( image );
  QVERIFY2( lit > 500, qPrintable( u"only %1 pixels were drawn"_s.arg( lit ) ) );
}

void TestQgsRhi3DScene::testRendersInstancedTiledScene()
{
  auto layer = std::make_unique<QgsTiledSceneLayer>( QString( TEST_DATA_DIR ) + "/3dtiles/instanced_rtc/tileset.json", u"instanced"_s, u"cesiumtiles"_s );
  QVERIFY( layer->dataProvider()->isValid() );
  layer->setRenderer3D( new QgsTiledSceneLayer3DRenderer() );

  auto map = std::make_unique<Qgs3DMapSettings>();
  map->setCrs( QgsCoordinateReferenceSystem( u"EPSG:3857"_s ) );
  map->setExtent( QgsRectangle( -8417257.0, 4871967.0, -8416943.0, 4872283.0 ) );
  map->setTransformContext( mProject->transformContext() );
  map->setOutputDpi( 96 );
  map->setBackgroundColor( Qt::black );
  map->setTerrainRenderingEnabled( false );
  map->setLayers( QList<QgsMapLayer *>() << layer.get() );

  QgsDirectionalLightSettings light;
  map->setLightSources( { light.clone() } );
  map->setRhiEngineEnabled( true );

  QgsRhi3DEngine engine( QSize( 400, 300 ) );
  QVERIFY( engine.initialize() );

  auto scene = std::make_unique<Qgs3DMapScene>( *map, &engine );
  engine.setRootEntity( scene.get() );
  engine.setScene( scene.get() );

  scene->cameraController()->setLookingAtPoint( QgsVector3D( 0, 0, 0 ), 500, 30, 0 );
  QVERIFY( renderUntilLoaded( engine, scene.get() ) );

  // an i3dm tile draws one mesh once per instance, which is a per-instance vertex
  // attribute rather than one part per instance
  bool sawInstancedPart = false;
  const QList<QgsChunkNode *> nodes = layerChunkNodes( scene.get(), layer.get() );
  QVERIFY( !nodes.isEmpty() );
  for ( QgsChunkNode *node : nodes )
  {
    QgsSceneObject3D *object = node->sceneObject();
    if ( !object )
      continue;
    for ( const std::unique_ptr<QgsSceneObject3D::Part> &part : object->parts() )
    {
      if ( part->instanceCount > 1 )
      {
        QVERIFY( part->geometry->attribute( u"instanceTranslation"_s ) );
        QVERIFY( part->geometry->attribute( u"instanceRotation"_s ) );
        QVERIFY( part->geometry->attribute( u"instanceScale"_s ) );
        sawInstancedPart = true;
      }
    }
  }
  QVERIFY2( sawInstancedPart, "no instanced part was produced" );

  const QImage image = engine.renderedImage();
  QVERIFY( !image.isNull() );
  const int lit = countLitPixels( image );
  QVERIFY2( lit > 100, qPrintable( u"only %1 pixels were drawn"_s.arg( lit ) ) );
}

QGSTEST_MAIN( TestQgsRhi3DScene )
#include "testqgsrhi3dscene.moc"
