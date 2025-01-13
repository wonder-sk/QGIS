/***************************************************************************
                         qgis_3d_sandbox.cpp
                         --------------------
    begin                : October 2020
    copyright            : (C) 2020 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QApplication>

#include "qgs3d.h"
#include "qgs3ddebugwidget.h"
#include "qgs3dmapcanvas.h"
#include "qgs3dmapconfigwidget.h"
#include "qgs3dmapscene.h"
#include "qgs3dmapsettings.h"
#include "qgs3dutils.h"
#include "qgsapplication.h"
#include "qgsflatterrainsettings.h"
#include "qgsframegraph.h"
#include "qgsgui.h"
#include "qgshelp.h"
#include "qgslayertree.h"
#include "qgsmapcanvas.h"
#include "qgsmapsettings.h"
#include "qgspointcloudlayer.h"
#include "qgspointcloudlayer3drenderer.h"
#include "qgspointlightsettings.h"
#include "qgsproject.h"
#include "qgsprojectelevationproperties.h"
#include "qgsprojectviewsettings.h"
#include "qgsterrainprovider.h"
#include "qgstiledscenelayer.h"
#include "qgstiledscenelayer3drenderer.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScreen>
#include <QToolBar>

#include "qgs3dmaptool.h"
#include "qgschunknode.h"
#include <Qt3DCore/QTransform>
#include "qgsmultipoint.h"
#include "qgsrubberband3d.h"
#include "qgswindow3dengine.h"
#include "qgspolygon.h"

#include "qgscopcupdate.h"

QVector<QgsVector3D> box3DCorners( QgsBox3D box )
{
  QgsVector3D lc = box.lowerCorner(), uc = box.upperCorner();
  return {
    QgsVector3D( lc.x(), lc.y(), lc.z() ),
    QgsVector3D( lc.x(), lc.y(), uc.z() ),
    QgsVector3D( lc.x(), uc.y(), lc.z() ),
    QgsVector3D( lc.x(), uc.y(), uc.z() ),
    QgsVector3D( uc.x(), lc.y(), lc.z() ),
    QgsVector3D( uc.x(), lc.y(), uc.z() ),
    QgsVector3D( uc.x(), uc.y(), lc.z() ),
    QgsVector3D( uc.x(), uc.y(), uc.z() ),
  };
}

struct MapToPixel3D
{
    QMatrix4x4 VP;  // combined view-projection matrix
    QgsVector3D origin;  // shift of world coordinates
    QSize canvasSize;

    QPointF transform( double x, double y, double z ) const
    {
      QVector4D cClip = VP * QVector4D( x - origin.x(), y - origin.y(), z - origin.z(), 1 );
      float xNdc = cClip.x()/cClip.w();
      float yNdc = cClip.y()/cClip.w();
      float xScreen = ( xNdc + 1 ) * 0.5 * canvasSize.width();
      float yScreen = ( -yNdc + 1 ) * 0.5 * canvasSize.height();
      return QPointF( xScreen, yScreen );
    }
};


QgsGeometry box3DToPolygonInScreenSpace( QgsBox3D box, const MapToPixel3D &mapToPixel3D )
{
  QVector<QgsPoint*> pts;
  for ( QgsVector3D c : box3DCorners( box ) )
  {
    QPointF pt = mapToPixel3D.transform( c.x(), c.y(), c.z() );
    pts.append( new QgsPoint( pt.x(), pt.y() ) );
  }

  // TODO: maybe we should only do rectangle check rather than (more precise) convex hull?

  // combine into QgsMultiPoint + apply convex hull
  QgsGeometry g( new QgsMultiPoint( pts ) );
  return g.convexHull();
}

struct SelectedPointInNode
{
    int pointIndex;
    double x, y, z;   // in map coordinates
};

typedef QHash<QgsPointCloudNodeId, QVector<SelectedPointInNode> > SelectedPoints;



QVector<SelectedPointInNode> selectedPointsInNode( const QgsGeometry &searchPolygon, const QgsChunkNode *ch, const MapToPixel3D &mapToPixel3D, QgsPointCloudIndex &pcIndex )
{
  QVector<SelectedPointInNode> selected;

  QgsPointCloudNodeId n( ch->tileId().d, ch->tileId().x, ch->tileId().y, ch->tileId().z );
  QgsPointCloudRequest request;
  // TODO: apply filtering (if any)
  request.setAttributes( pcIndex.attributes() );

  // TODO: reuse cached block(s) if possible

  std::unique_ptr<QgsPointCloudBlock> block( pcIndex.nodeData( n, request ) );
  if ( !block )
    return selected;

  const QgsVector3D blockScale = block->scale();
  const QgsVector3D blockOffset = block->offset();

  const char *ptr = block->data();
  const QgsPointCloudAttributeCollection blockAttributes = block->attributes();
  const std::size_t recordSize = blockAttributes.pointRecordSize();
  int xOffset = 0, yOffset = 0, zOffset = 0;
  const QgsPointCloudAttribute::DataType xType = blockAttributes.find( QStringLiteral( "X" ), xOffset )->type();
  const QgsPointCloudAttribute::DataType yType = blockAttributes.find( QStringLiteral( "Y" ), yOffset )->type();
  const QgsPointCloudAttribute::DataType zType = blockAttributes.find( QStringLiteral( "Z" ), zOffset )->type();
  for ( int i = 0; i < block->pointCount(); ++i )
  {
    // get map coordinates
    double x, y, z;
    QgsPointCloudAttribute::getPointXYZ( ptr, i, recordSize, xOffset, xType, yOffset, yType, zOffset, zType, blockScale, blockOffset, x, y, z );

    // project to screen (map coords -> world coords -> clip coords -> NDC -> screen coords)
    QPointF ptScreen = mapToPixel3D.transform( x, y, z );

    if ( searchPolygon.intersects( QgsGeometry( new QgsPoint( ptScreen.x(), ptScreen.y() ) ) ) )
    {
      SelectedPointInNode p;
      p.x = x; p.y = y; p.z = z;
      p.pointIndex = i;
      selected.append( p );
    }
  }
  return selected;
}


class MyTool : public Qgs3DMapTool
{
  public:
    MyTool( Qgs3DMapCanvas *canvas ) : Qgs3DMapTool( canvas )
    {
      mPolygonRubberBand = new QgsRubberBand3D( *mCanvas->mapSettings(), mCanvas->engine(), mCanvas->engine()->frameGraph()->rubberBandsRootEntity(), Qgis::GeometryType::Polygon );
      mPolygonRubberBand->setHideLastMarker( true );

      mSearchResultsRubberBand = new QgsRubberBand3D( *mCanvas->mapSettings(), mCanvas->engine(), mCanvas->engine()->frameGraph()->rubberBandsRootEntity(), Qgis::GeometryType::Point );
      mSearchResultsRubberBand->setColor( Qt::yellow );
      mSearchResultsRubberBand->setMarkerType( QgsRubberBand3D::MarkerType::Square );
    }

    QgsPoint screenPointToMap( QPoint pos )
    {
      const QgsRay3D ray = Qgs3DUtils::rayFromScreenPoint( pos, mCanvas->size(), mCanvas->cameraController()->camera() );

      // pick an arbitrary point mid-way between near and far plane
      float pointDistance = ( mCanvas->cameraController()->camera()->farPlane() + mCanvas->cameraController()->camera()->nearPlane() ) / 2;
      QVector3D aa = ray.origin() + pointDistance * ray.direction().normalized();

      QgsVector3D origin = mCanvas->mapSettings()->origin();
      QgsPoint newPoint( aa.x() + origin.x(), aa.y() + origin.y(), aa.z() + origin.z() );
      return newPoint;
    }

    void mousePressEvent( QMouseEvent *event ) override
    {
      mClickPoint = event->pos();
    }

    void mouseReleaseEvent( QMouseEvent *event ) override
    {
      if ( ( event->pos() - mClickPoint ).manhattanLength() > QApplication::startDragDistance() )
        return;  // no dragging in this tool

      qDebug() << event->pos();

      QgsPoint newPoint = screenPointToMap( event->pos() );
      //qDebug() << newPoint.asWkt(1);

      if ( event->button() == Qt::LeftButton )
      {
        mStarted = true;
        mCanvas->cameraController()->setInputHandlersEnabled( false );

        if ( mFirstPoint )
        {
          mPolygonRubberBand->addPoint( newPoint );
          mFirstPoint = false;
        }
        else
        {
          mPolygonRubberBand->moveLastPoint( newPoint );
        }
        mPolygonRubberBand->addPoint( newPoint );
        mScreenPoints.addVertex( QgsPoint( event->x(), event->y() ) );
      }
      else
      {
        QgsGeometry searchPolygon = QgsGeometry( new QgsPolygon( mScreenPoints.clone() ) );
        qDebug() << searchPolygon.asWkt(1);
        QElapsedTimer t; t.start();
        SelectedPoints sel = searchPoints( searchPolygon );
        qDebug() << "search took " << t.elapsed() / 1000. << "secs";

#if 0
        t.start();
        mSearchResultsRubberBand->reset();

        int totalPoints = 0;
        for ( QgsPointCloudNodeId n : sel.keys() )
          totalPoints += sel[n].count();

        QVector<double> xArray, yArray, zArray;
        xArray.reserve( totalPoints );
        yArray.reserve( totalPoints );
        zArray.reserve( totalPoints );
        for ( QgsPointCloudNodeId n : sel.keys() )
        {
          QVector< SelectedPointInNode > pts = sel.value( n );
          if ( pts.count() )
          {
            for ( SelectedPointInNode p : pts )
            {
              xArray.append( p.x );
              yArray.append( p.y );
              zArray.append( p.z );
            }
          }
        }
        mSearchResultsRubberBand->setPoints( QgsLineString( xArray, yArray, zArray ) );

        qDebug() << "found points: " << totalPoints;
#endif
        mPolygonRubberBand->reset();
        mStarted = false;
        mCanvas->cameraController()->setInputHandlersEnabled( true );
        mFirstPoint = true;
        mScreenPoints.clear();

        QgsMapLayer *mapLayer = QgsProject::instance()->mapLayers().first();
        QgsPointCloudLayer *pcLayer = qobject_cast<QgsPointCloudLayer*>( mapLayer );
        Q_ASSERT( pcLayer );
        qDebug() << "src: " << mapLayer->source();

        int attrOffset;
        const QgsPointCloudAttribute *classificationAttribute = pcLayer->attributes().find( "Classification", attrOffset );

        if ( !pcLayer->isEditable() )
          pcLayer->startEditing();

        QHash<QgsPointCloudNodeId, QgsCopcUpdate::UpdatedChunk> updatedChunks;

        for ( QgsPointCloudNodeId nodeId : sel.keys() )
        {
          qDebug() << "node" << nodeId.toString() << sel[nodeId].count();
          QVector<int> indices;
          for ( SelectedPointInNode p : sel[nodeId] )
            indices << p.pointIndex;

          pcLayer->changeAttributeValue( nodeId, indices, *classificationAttribute, 12 );
        }

      }

    }

    void mouseMoveEvent( QMouseEvent *event ) override
    {
      if ( !mStarted )
        return;
      QgsPoint movedPoint = screenPointToMap( event->pos() );
      mPolygonRubberBand->moveLastPoint( movedPoint );

    }

    SelectedPoints searchPoints( QgsGeometry searchPolygon )
    {
      SelectedPoints result;

      MapToPixel3D mapToPixel3D;
      mapToPixel3D.VP = mCanvas->camera()->projectionMatrix() * mCanvas->camera()->viewMatrix();
      mapToPixel3D.origin = mCanvas->mapSettings()->origin();
      mapToPixel3D.canvasSize = mCanvas->size();

      QgsMapLayer *mapLayer = QgsProject::instance()->mapLayers().first();
      Q_ASSERT( mapLayer->type() == Qgis::LayerType::PointCloud );
      QgsPointCloudLayer *pcLayer = qobject_cast<QgsPointCloudLayer *>( mapLayer );
      QgsPointCloudIndex pcIndex = pcLayer->dataProvider()->index();
      const QVector<const QgsChunkNode *> chunks = mCanvas->scene()->getLayerActiveChunkNodes( mapLayer );
      for ( const QgsChunkNode *ch : chunks )
      {
        qDebug() << " - " << ch->tileId().text() << ch->box3D().toString(1);

        // check whether the hull intersects the search polygon
        QgsGeometry hull = box3DToPolygonInScreenSpace( ch->box3D(), mapToPixel3D );
        if ( !hull.intersects( searchPolygon ) )
          continue;

        QVector<SelectedPointInNode> pts = selectedPointsInNode( searchPolygon, ch, mapToPixel3D, pcIndex );
        if ( !pts.isEmpty() )
        {
          QgsPointCloudNodeId n( ch->tileId().d, ch->tileId().x, ch->tileId().y, ch->tileId().z );
          result.insert( n, pts );
        }
      }
      return result;
    }

    void save()
    {
      QgsMapLayer *mapLayer = QgsProject::instance()->mapLayers().first();
      QgsPointCloudLayer *pcLayer = qobject_cast<QgsPointCloudLayer*>( mapLayer );
      Q_ASSERT( pcLayer );

      if ( !pcLayer->isModified() )
        return;

      qDebug() << "saving!";
      pcLayer->commitChanges();
      qDebug() << "done.";
    }

  protected:
    QgsRubberBand3D* mPolygonRubberBand;
    QgsRubberBand3D* mSearchResultsRubberBand;
    bool mFirstPoint = true;
    bool mStarted = false;
    QgsLineString mScreenPoints;
    QPoint mClickPoint;

};

void initCanvas3D( Qgs3DMapCanvas *canvas )
{
  QgsLayerTree *root = QgsProject::instance()->layerTreeRoot();
  const QList<QgsMapLayer *> visibleLayers = root->checkedLayers();

  QgsCoordinateReferenceSystem crs = QgsProject::instance()->crs();
  if ( crs.isGeographic() )
  {
    // we can't deal with non-projected CRS, so let's just pick something
    QgsProject::instance()->setCrs( QgsCoordinateReferenceSystem( QStringLiteral( "EPSG:3857" ) ) );
  }

  QgsMapSettings ms;
  ms.setDestinationCrs( QgsProject::instance()->crs() );
  ms.setLayers( visibleLayers );
  QgsRectangle fullExtent = QgsProject::instance()->viewSettings()->fullExtent();
  qDebug() << "full extent" << fullExtent.toString(1);

  Qgs3DMapSettings *map = new Qgs3DMapSettings;
  map->setCrs( QgsProject::instance()->crs() );
  map->setOrigin( QgsVector3D( fullExtent.center().x(), fullExtent.center().y(), 0 ) );
  map->setLayers( visibleLayers );

  map->setExtent( fullExtent );

  Qgs3DAxisSettings axis;
  axis.setMode( Qgs3DAxisSettings::Mode::Off );
  map->set3DAxisSettings( axis );

  map->setTransformContext( QgsProject::instance()->transformContext() );
  map->setPathResolver( QgsProject::instance()->pathResolver() );
  map->setMapThemeCollection( QgsProject::instance()->mapThemeCollection() );
  QObject::connect( QgsProject::instance(), &QgsProject::transformContextChanged, map, [map] {
    map->setTransformContext( QgsProject::instance()->transformContext() );
  } );

  QgsFlatTerrainSettings *flatTerrain = new QgsFlatTerrainSettings();
  flatTerrain->setElevationOffset( QgsProject::instance()->elevationProperties()->terrainProvider()->offset() );
  map->setTerrainSettings( flatTerrain );

  QgsPointLightSettings defaultPointLight;
  defaultPointLight.setPosition( QgsVector3D( 0, 0, 1000 ) );
  defaultPointLight.setConstantAttenuation( 0 );
  map->setLightSources( { defaultPointLight.clone() } );
  if ( QScreen *screen = QGuiApplication::primaryScreen() )
  {
    map->setOutputDpi( screen->physicalDotsPerInch() );
  }
  else
  {
    map->setOutputDpi( 96 );
  }

  canvas->setMapSettings( map );

  QgsRectangle extent = fullExtent;
  extent.scale( 1.3 );
  const float dist = static_cast<float>( std::max( extent.width(), extent.height() ) );
  canvas->setViewFromTop( extent.center(), dist * 2, 0 );
  qDebug() << "view from top:" << extent.center().toString(1) << "dist" << dist*2;

  QObject::connect( canvas->scene(), &Qgs3DMapScene::totalPendingJobsCountChanged, canvas, [canvas] {
    qDebug() << "pending jobs:" << canvas->scene()->totalPendingJobsCount();
  } );

  qDebug() << "pending jobs:" << canvas->scene()->totalPendingJobsCount();
}

QDialog *createConfigDialog( Qgs3DMapCanvas *canvas )
{
  const QPointer configDialog = new QDialog;
  configDialog->setWindowTitle( QStringLiteral( "3D Configuration" ) );
  configDialog->setObjectName( QStringLiteral( "3DConfigurationDialog" ) );
  configDialog->setMinimumSize( 600, 460 );
  QgsGui::enableAutoGeometryRestore( configDialog );

  Qgs3DMapSettings *map = canvas->mapSettings();
  Qgs3DMapConfigWidget *w = new Qgs3DMapConfigWidget( map, nullptr, canvas, configDialog );
  QDialogButtonBox *buttons = new QDialogButtonBox( QDialogButtonBox::Apply | QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, configDialog );

  auto applyConfig = [=] {
    const QgsVector3D oldOrigin = map->origin();
    const QgsCoordinateReferenceSystem oldCrs = map->crs();
    const QgsCameraPose oldCameraPose = canvas->cameraController()->cameraPose();
    const QgsVector3D oldLookingAt = oldCameraPose.centerPoint();

    // update map
    w->apply();

    const QgsVector3D p = Qgs3DUtils::transformWorldCoordinates(
      oldLookingAt,
      oldOrigin, oldCrs,
      map->origin(), map->crs(), QgsProject::instance()->transformContext()
    );

    if ( p != oldLookingAt )
    {
      // apply() call has moved origin of the world so let's move camera so we look still at the same place
      QgsCameraPose newCameraPose = oldCameraPose;
      newCameraPose.setCenterPoint( p );
      canvas->cameraController()->setCameraPose( newCameraPose );
    }
  };

  QObject::connect( buttons, &QDialogButtonBox::rejected, configDialog, &QDialog::reject );
  QObject::connect( buttons, &QDialogButtonBox::clicked, configDialog, [=]( const QAbstractButton *button ) {
    if ( button == buttons->button( QDialogButtonBox::Apply ) || button == buttons->button( QDialogButtonBox::Ok ) )
      applyConfig();
    if ( button == buttons->button( QDialogButtonBox::Ok ) )
      configDialog->accept();
  } );
  QObject::connect( buttons, &QDialogButtonBox::helpRequested, w, []() { QgsHelp::openHelp( QStringLiteral( "map_views/3d_map_view.html#scene-configuration" ) ); } );

  QObject::connect( w, &Qgs3DMapConfigWidget::isValidChanged, configDialog, [=]( const bool valid ) {
    buttons->button( QDialogButtonBox::Apply )->setEnabled( valid );
    buttons->button( QDialogButtonBox::Ok )->setEnabled( valid );
  } );

  QVBoxLayout *layout = new QVBoxLayout( configDialog );
  layout->addWidget( w, 1 );
  layout->addWidget( buttons );
  return configDialog;
}


#if 0
void copc_rewrite()
{
  //QString inputFilename = "/home/martin/qgis/git-master/tests/testdata/point_clouds/copc/extrabytes-dataset.copc.laz";
  QString inputFilename = "/home/martin/data/las/sk-tatry/tatry_54_1.copc.laz";

  QString outputFilename = "/tmp/output.copc.laz";

  qDebug() << "hello COPC";

  //
  // do some updates to the point data
  //

  QHash<VoxelKey, CopcUpdater::UpdatedChunk> updatedChunks;

  CopcUpdater copc;
  copc.read( inputFilename );

  // just modify the root node
  Entry e = copc.findVoxel(VoxelKey{0,0,0,0});
  QSet<int> indices;
  for (int i = 0; i < e.pointCount/2; ++i)
    indices.insert(i);

  updatedChunks[e.key].pointCount = e.pointCount;
  updatedChunks[e.key].chunkData = copc.updateChunkValues(12, VoxelKey{0,0,0,0}, indices);

  copc.write( outputFilename, updatedChunks );
}
#endif



int main( int argc, char *argv[] )
{
  QgsApplication myApp( argc, argv, true, QString(), QStringLiteral( "desktop" ) );

  // init QGIS's paths - true means that all path will be inited from prefix
  QgsApplication::init();
  QgsApplication::initQgis();
  Qgs3D::initialize();

  if ( argc < 2 )
  {
    qDebug() << "need QGIS project file";
    return 1;
  }

  const QString projectFile = argv[1];
  const bool res = QgsProject::instance()->read( projectFile );
  if ( !res )
  {
    qDebug() << "can't open project file" << projectFile;
    return 1;
  }

  // a hack to assign 3D renderer
  for ( QgsMapLayer *layer : QgsProject::instance()->layerTreeRoot()->checkedLayers() )
  {
    /*
    if ( QgsPointCloudLayer *pcLayer = qobject_cast<QgsPointCloudLayer *>( layer ) )
    {
      QgsPointCloudLayer3DRenderer *r = new QgsPointCloudLayer3DRenderer();
      r->setLayer( pcLayer );
      r->resolveReferences( *QgsProject::instance() );
      pcLayer->setRenderer3D( r );
    }*/

    if ( QgsTiledSceneLayer *tsLayer = qobject_cast<QgsTiledSceneLayer *>( layer ) )
    {
      QgsTiledSceneLayer3DRenderer *r = new QgsTiledSceneLayer3DRenderer();
      r->setLayer( tsLayer );
      r->resolveReferences( *QgsProject::instance() );
      tsLayer->setRenderer3D( r );
    }
  }

  Qgs3DMapCanvas *canvas = new Qgs3DMapCanvas;
  initCanvas3D( canvas );

  // %%%
  canvas->setMapTool( new MyTool( canvas ) );

  // set up the UI
  QWidget *windowWidget = new QWidget;

  QToolBar *toolBar = new QToolBar( windowWidget );
  toolBar->setIconSize( QgsGuiUtils::iconSize() );
  toolBar->addAction( QIcon( QgsApplication::iconPath( "mActionZoomFullExtent.svg" ) ), QStringLiteral( "Reset camera to default position" ), windowWidget, [canvas] {
    canvas->resetView();
  } );
  QAction *toggleDebugPanel = toolBar->addAction(
    QgsApplication::getThemeIcon( QStringLiteral( "/propertyicons/general.svg" ) ),
    QStringLiteral( "Toggle on-screen Debug panel" )
  );
  toggleDebugPanel->setCheckable( true );
  QAction *configureAction = new QAction( QgsApplication::getThemeIcon( QStringLiteral( "mActionOptions.svg" ) ), QStringLiteral( "Configure…" ), windowWidget );
  QDialog *configDialog = createConfigDialog( canvas );
  QObject::connect( configureAction, &QAction::triggered, windowWidget, [configDialog] {
    configDialog->setVisible( true );
  } );
  toolBar->addAction( configureAction );

  QAction *saveAction = new QAction( QgsApplication::getThemeIcon( QStringLiteral( "mActionSave.svg" ) ), QStringLiteral( "Save" ), windowWidget );
  QObject::connect( saveAction, &QAction::triggered, windowWidget, [canvas] {
    ((MyTool*)canvas->mapTool())->save();
  } );
  toolBar->addAction( saveAction );

  QWidget *container = QWidget::createWindowContainer( canvas );
  container->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
  Qgs3DDebugWidget *debugWidget = new Qgs3DDebugWidget( canvas );
  debugWidget->setMapSettings( canvas->mapSettings() );
  debugWidget->setVisible( false );
  QObject::connect( canvas->mapSettings(), &Qgs3DMapSettings::showDebugPanelChanged, windowWidget, [=]( const bool enabled ) {
    debugWidget->setVisible( enabled );
  } );

  // Connect the camera to the debug widget.
  QObject::connect( canvas->cameraController(), &QgsCameraController::cameraChanged, debugWidget, &Qgs3DDebugWidget::updateFromCamera );
  QObject::connect( canvas->cameraController()->camera(), &Qt3DRender::QCamera::nearPlaneChanged, debugWidget, &Qgs3DDebugWidget::updateFromCamera );
  QObject::connect( canvas->cameraController()->camera(), &Qt3DRender::QCamera::farPlaneChanged, debugWidget, &Qgs3DDebugWidget::updateFromCamera );
  QObject::connect( toggleDebugPanel, &QAction::toggled, windowWidget, [=]( const bool enabled ) {
    debugWidget->setVisible( enabled );
  } );

  // construct the layout of sandbox
  QVBoxLayout *vLayout = new QVBoxLayout;
  vLayout->setContentsMargins( 0, 0, 0, 0 );
  vLayout->setSpacing( 0 );
  vLayout->addWidget( toolBar );
  QHBoxLayout *hLayout = new QHBoxLayout;
  vLayout->addLayout( hLayout );
  hLayout->setContentsMargins( 0, 0, 0, 0 );
  hLayout->addWidget( container );
  hLayout->addWidget( debugWidget );


  windowWidget->resize( 800, debugWidget->height() );
  windowWidget->setLayout( vLayout );
  windowWidget->show();

  return myApp.exec();
}
