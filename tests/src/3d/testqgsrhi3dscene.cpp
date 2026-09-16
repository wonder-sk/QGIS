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
#include "qgsflatterraingenerator.h"
#include "qgspointlightsettings.h"
#include "qgsproject.h"
#include "qgsrasterlayer.h"
#include "qgsrhi3dengine.h"
#include "qgssceneobject3d.h"
#include "qgsterrainentity.h"
#include "qgstest.h"

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

  private:
    //! Builds the map settings a flat terrain scene needs
    std::unique_ptr<Qgs3DMapSettings> createMapSettings() const;

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
}

void TestQgsRhi3DScene::cleanupTestCase()
{
  mProject.reset();
  QgsApplication::exitQgis();
}

std::unique_ptr<Qgs3DMapSettings> TestQgsRhi3DScene::createMapSettings() const
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

  auto *flatTerrain = new QgsFlatTerrainGenerator;
  flatTerrain->setCrs( map->crs(), map->transformContext() );
  map->setTerrainGenerator( flatTerrain );

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

QGSTEST_MAIN( TestQgsRhi3DScene )
#include "testqgsrhi3dscene.moc"
