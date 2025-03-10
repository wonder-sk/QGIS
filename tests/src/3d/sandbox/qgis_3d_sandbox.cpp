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
#include "qgsgui.h"
#include "qgshelp.h"
#include "qgslayertree.h"
#include "qgsmapcanvas.h"
#include "qgsmapsettings.h"
#include "qgspointlightsettings.h"
#include "qgsproject.h"
#include "qgsprojectelevationproperties.h"
#include "qgsprojectviewsettings.h"
#include "qgsterrainprovider.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScreen>
#include <QToolBar>



double sDenom = 1;   // how much smaller the globe will be


#include <QByteArray>
#include <QDiffuseSpecularMaterial>

#include <Qt3DCore/QEntity>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QTextureImage>
#include <Qt3DExtras/QTextureMaterial>

#include "qgscoordinatereferencesystem.h"
#include "qgscoordinatetransform.h"

#include "qgsgeotransform.h"
#include "qgswindow3dengine.h"

using namespace Qt3DCore;
using namespace Qt3DRender;


QEntity *makeGlobeMesh( double lonMin, double lonMax,
                        double latMin, double latMax,
                        int lonSliceCount, int latSliceCount )
{
  double lonRange = lonMax - lonMin;
  double latRange = latMax - latMin;
  double lonStep = lonRange / ( double )( lonSliceCount - 1 );
  double latStep = latRange / ( double )( latSliceCount - 1 );

  QgsCoordinateTransform ct( QgsCoordinateReferenceSystem( "EPSG:4326" ), QgsCoordinateReferenceSystem( "EPSG:4978" ), QgsCoordinateTransformContext() );

  std::vector<double> x, y, z;
  uint pointCount = latSliceCount * lonSliceCount;
  x.reserve( pointCount );
  y.reserve( pointCount );
  z.reserve( pointCount );

  for ( int latSliceIndex = 0; latSliceIndex < latSliceCount; ++latSliceIndex )
  {
    double lat = latSliceIndex * latStep + latMin;
    for ( int lonSliceIndex = 0; lonSliceIndex < lonSliceCount; ++lonSliceIndex )
    {
      double lon = lonSliceIndex * lonStep + lonMin;
      x.push_back( lon );
      y.push_back( lat );
      z.push_back( 0 );
    }
  }

  ct.transformCoords( pointCount, x.data(), y.data(), z.data() );

  int stride = ( 3 + 2 + 3 ) * sizeof( float );

  QByteArray bufferBytes;
  bufferBytes.resize( stride * pointCount );
  float *fptr = ( float * ) bufferBytes.data();
  for ( int i = 0; i < ( int )pointCount; ++i )
  {
    *fptr++ = x[i] / sDenom;
    *fptr++ = y[i] / sDenom;
    *fptr++ = z[i] / sDenom;

    float v = ( float )( i / lonSliceCount ) / ( float )( latSliceCount - 1 );
    float u = ( float )( i % lonSliceCount ) / ( float )( lonSliceCount - 1 );
    *fptr++ = u;
    *fptr++ = v;
    qDebug() << u << v;

    QVector3D n = QVector3D( ( float )x[i], ( float )y[i], ( float )z[i] ).normalized();
    *fptr++ = n.x();
    *fptr++ = n.y();
    *fptr++ = n.z();
  }

  int faces = ( lonSliceCount - 1 ) * ( latSliceCount - 1 ) * 2;
  qsizetype indices = faces * 3;

  QByteArray indexBytes;
  indexBytes.resize( indices * sizeof( ushort ) );

  quint16 *indexPtr = ( unsigned short * ) indexBytes.data();
  for ( short latSliceIndex = 0; latSliceIndex < latSliceCount - 1; ++latSliceIndex )
  {
    short latSliceStartIndex = latSliceIndex * ( short )lonSliceCount;
    short nextLatSliceStartIndex = ( short )lonSliceCount + latSliceStartIndex;
    for ( short lonSliceIndex = 0; lonSliceIndex < lonSliceCount - 1; ++lonSliceIndex )
    {
      indexPtr[0] = latSliceStartIndex + lonSliceIndex;
      indexPtr[1] = lonSliceIndex + latSliceStartIndex + 1;
      indexPtr[2] = nextLatSliceStartIndex + lonSliceIndex;

      indexPtr[3] = nextLatSliceStartIndex + lonSliceIndex;
      indexPtr[4] = lonSliceIndex + latSliceStartIndex + 1;
      indexPtr[5] = lonSliceIndex + nextLatSliceStartIndex + 1;

      indexPtr = indexPtr + 6;
    }
  }

  QBuffer *m_vertexBuffer = new QBuffer();
  m_vertexBuffer->setData( bufferBytes );

  QBuffer *m_indexBuffer = new QBuffer();
  m_indexBuffer->setData( indexBytes );

  QAttribute *m_positionAttribute = new QAttribute;
  m_positionAttribute = new QAttribute;
  m_positionAttribute->setName( Qt3DRender::QAttribute::defaultPositionAttributeName() );
  m_positionAttribute->setVertexBaseType( QAttribute::Float );
  m_positionAttribute->setVertexSize( 3 );
  m_positionAttribute->setAttributeType( QAttribute::VertexAttribute );
  m_positionAttribute->setBuffer( m_vertexBuffer );
  m_positionAttribute->setByteStride( stride );
  m_positionAttribute->setCount( pointCount );

  QAttribute *m_texCoordAttribute = new QAttribute;
  m_texCoordAttribute = new QAttribute;
  m_texCoordAttribute->setName( QAttribute::defaultTextureCoordinateAttributeName() );
  m_texCoordAttribute->setVertexBaseType( QAttribute::Float );
  m_texCoordAttribute->setVertexSize( 2 );
  m_texCoordAttribute->setAttributeType( QAttribute::VertexAttribute );
  m_texCoordAttribute->setBuffer( m_vertexBuffer );
  m_texCoordAttribute->setByteStride( stride );
  m_texCoordAttribute->setByteOffset( 3 * sizeof( float ) );
  m_texCoordAttribute->setCount( pointCount );

  QAttribute *m_normalAttribute = new QAttribute;
  m_normalAttribute->setName( QAttribute::defaultNormalAttributeName() );
  m_normalAttribute->setVertexBaseType( QAttribute::Float );
  m_normalAttribute->setVertexSize( 3 );
  m_normalAttribute->setAttributeType( QAttribute::VertexAttribute );
  m_normalAttribute->setBuffer( m_vertexBuffer );
  m_normalAttribute->setByteStride( stride );
  m_normalAttribute->setByteOffset( 5 * sizeof( float ) );
  m_normalAttribute->setCount( pointCount );

  QAttribute *m_indexAttribute = new QAttribute;
  m_indexAttribute->setAttributeType( QAttribute::IndexAttribute );
  m_indexAttribute->setVertexBaseType( QAttribute::UnsignedShort );
  m_indexAttribute->setBuffer( m_indexBuffer );
  m_indexAttribute->setCount( faces * 3 );

  QGeometry *geom = new QGeometry;
  geom->addAttribute( m_positionAttribute );
  geom->addAttribute( m_texCoordAttribute );
  geom->addAttribute( m_normalAttribute );
  geom->addAttribute( m_indexAttribute );

  QGeometryRenderer *rend = new QGeometryRenderer;
  rend->setPrimitiveType( QGeometryRenderer::Triangles );
  rend->setVertexCount( faces * 3 );
  rend->setGeometry( geom );

  QTextureImage *textureImage = new QTextureImage;
  textureImage->setSource( QUrl( "file:///home/martin/Downloads/8081_earthmap4k.jpg" ) );

  QTexture2D *texture = new QTexture2D();
  texture->addTextureImage( textureImage );
  texture->setMinificationFilter( QTexture2D::Linear );
  texture->setMagnificationFilter( QTexture2D::Linear );

  Qt3DExtras::QTextureMaterial *mat = new Qt3DExtras::QTextureMaterial;
  mat->setTexture( texture );

  QgsGeoTransform *gt = new QgsGeoTransform;

  Qt3DCore::QEntity *e = new Qt3DCore::QEntity;
  e->addComponent( mat );
  e->addComponent( rend );
  e->addComponent( gt );
  return e;
}

QgsVector3D point4978FromAngles( double lat, double lon, double elev = 0 )
{
  QgsCoordinateTransform ct( QgsCoordinateReferenceSystem( "EPSG:4326" ), QgsCoordinateReferenceSystem( "EPSG:4978" ), QgsCoordinateTransformContext() );
  return ct.transform( QgsVector3D( lon, lat, elev ) );
}

QgsVector3D movePoint4978ByAngle( QgsVector3D point, double latDiff, double lonDiff )  // angles in degrees
{
  QgsCoordinateTransform ct( QgsCoordinateReferenceSystem( "EPSG:4326" ), QgsCoordinateReferenceSystem( "EPSG:4978" ), QgsCoordinateTransformContext() );

  QgsVector3D pointLatLon = ct.transform( point, Qgis::TransformDirection::Reverse );
  pointLatLon.setX( pointLatLon.x() + lonDiff );
  pointLatLon.setY( std::clamp( pointLatLon.y() + latDiff, -90., 90. ) );

  return ct.transform( pointLatLon );
}

void point4978ToAngles( QgsVector3D point, double &lat, double &lon )
{
  QgsCoordinateTransform ct( QgsCoordinateReferenceSystem( "EPSG:4326" ), QgsCoordinateReferenceSystem( "EPSG:4978" ), QgsCoordinateTransformContext() );
  QgsVector3D centerLatLon = ct.transform( point, Qgis::TransformDirection::Reverse );
  lon = centerLatLon.x();
  lat = centerLatLon.y();
}


QVector3D globeMapToScene( QgsVector3D point, QgsVector3D origin )
{
  return QVector3D( (point.x() - origin.x()) / sDenom, (point.y() - origin.y()) / sDenom, (point.z() - origin.z()) / sDenom );
}

QgsVector3D globeSceneToMap( QVector3D point, QgsVector3D origin )
{
  return QgsVector3D( point.x() * sDenom + origin.x(), point.y() * sDenom + origin.y(), point.z() * sDenom + origin.z() );
}


struct CameraPose3   // like QgsCameraPose
{
    QVector3D viewCenter;
    float distance;

    float pitchAngle = 0;
    float headingAngle = 0;

    void toCamera( QgsVector3D origin, Qt3DRender::QCamera *camera )
    {
      QgsVector3D vc = globeSceneToMap( viewCenter, origin );
      qDebug() << "vc" << vc.toString(1);

      double lat, lon;
      point4978ToAngles( vc, lat, lon );

      qDebug() << "lat" << lat << "lon" << lon << "pitch" << pitchAngle << "heading" << headingAngle;
      QQuaternion q = QQuaternion::fromAxisAndAngle( QVector3D( 0, 0, 1), lon ) * QQuaternion::fromAxisAndAngle( QVector3D( 0, -1, 0 ), lat );

      // pitch/heading
      QQuaternion qPitchHeading = QQuaternion::fromAxisAndAngle( QVector3D( 1, 0, 0 ), headingAngle ) * QQuaternion::fromAxisAndAngle( QVector3D( 0, 1, 0 ), pitchAngle );
      q = q * qPitchHeading;

      QVector3D cameraToCenter = ( q * QVector3D( -1, 0, 0 ) ) * distance;
      camera->setUpVector( q * QVector3D( 0, 0, 1 ) );
      camera->setPosition( viewCenter - cameraToCenter );
      camera->setViewCenter( viewCenter );
    }

};


// copy of QgsCameraController::screenPointToWorldPos()
QgsVector3D mapPositionAtPoint( QPoint point, QImage &depthImage, Qt3DRender::QCamera *cam, QgsVector3D origin )
{
  double depth = 1;

  int px = point.x();
  int py = point.y();

  // Sample the neighbouring pixels for the closest point to the camera
  for ( int x = px - 3; x <= px + 3; ++x )
  {
    for ( int y = py - 3; y <= py + 3; ++y )
    {
      if ( depthImage.valid( x, y ) )
      {
        depth = std::min( depth, Qgs3DUtils::decodeDepth( depthImage.pixel( x, y ) ) );
      }
    }
  }

  if ( depth >= 1 )
  {
    qDebug() << "bad depth" << depth;
    return QgsVector3D(0,0,0);
  }

  if ( !std::isfinite( depth ) )
  {
    qDebug() << QStringLiteral( "screenPointToWorldPos: depth is NaN or Inf. This should not happen." );
    return QgsVector3D(0,0,0);
  }

  QVector3D worldPosition = Qgs3DUtils::screenPointToWorldPos( point, depth, depthImage.size(), cam );
  if ( !std::isfinite( worldPosition.x() ) || !std::isfinite( worldPosition.y() ) || !std::isfinite( worldPosition.z() ) )
  {
    qDebug() << QStringLiteral( "screenPointToWorldPos: position is NaN or Inf. This should not happen." );
    return QgsVector3D(0,0,0);
  }

  QgsVector3D mapPos = QgsVector3D(worldPosition) + origin;
  return mapPos;
}



CameraPose3 sCP;

float sZoomFactor = 0.9f;
float sMoveFactor = 0.000001f;  // multiplied by distance to get angle

#include <Qt3DInput/QMouseHandler>
#include <Qt3DInput/QMouseDevice>

class CameraWidget : public QWidget
{
  public:
    CameraWidget( Qgs3DMapCanvas *canvas )
      : mCanvas( canvas )
    {
      QPushButton *btnL = new QPushButton("<-");
      QPushButton *btnR = new QPushButton("->");
      QPushButton *btnU = new QPushButton("/\\");
      QPushButton *btnD = new QPushButton("\\/");
      QPushButton *btnIn = new QPushButton("in");
      QPushButton *btnOut = new QPushButton("out");
      QPushButton *btnP0 = new QPushButton("p-");
      QPushButton *btnP1 = new QPushButton("p+");
      QPushButton *btnH0 = new QPushButton("h-");
      QPushButton *btnH1 = new QPushButton("h+");
      btnL->setShortcut(QKeySequence(Qt::Key_Left));
      btnR->setShortcut(QKeySequence(Qt::Key_Right));
      btnU->setShortcut(QKeySequence(Qt::Key_Up));
      btnD->setShortcut(QKeySequence(Qt::Key_Down));
      btnIn->setShortcut(QKeySequence(Qt::Key_Comma));
      btnOut->setShortcut(QKeySequence(Qt::Key_Period));
      QHBoxLayout *l = new QHBoxLayout;
      l->addWidget(btnL);
      l->addWidget(btnR);
      l->addWidget(btnU);
      l->addWidget(btnD);
      l->addWidget(btnIn);
      l->addWidget(btnOut);
      l->addWidget(btnP0);
      l->addWidget(btnP1);
      l->addWidget(btnH0);
      l->addWidget(btnH1);
      setLayout( l );

      Qt3DInput::QMouseHandler *mMouseHandler = new Qt3DInput::QMouseHandler;
      mMouseHandler->setSourceDevice( new Qt3DInput::QMouseDevice() );
      connect( mMouseHandler, &Qt3DInput::QMouseHandler::positionChanged, this, [=](Qt3DInput::QMouseEvent *mouse){ onMousePositionChanged(mouse); } );
      connect( mMouseHandler, &Qt3DInput::QMouseHandler::pressed, this, [=](Qt3DInput::QMouseEvent *mouse){ onMousePressed(mouse); } );
      connect( mMouseHandler, &Qt3DInput::QMouseHandler::released, this, [=](Qt3DInput::QMouseEvent *mouse){ onMouseReleased(mouse); } );
      connect( mMouseHandler, &Qt3DInput::QMouseHandler::wheel, this, [=](Qt3DInput::QWheelEvent *mouse){ onMouseWheel(mouse); } );
      canvas->scene()->addComponent( mMouseHandler );

      connect( canvas->engine(), &QgsAbstract3DEngine::depthBufferCaptured, this, [=](const QImage &depthImage) { onDepthBufferCaptured(depthImage); } );

      mOrigin = canvas->mapSettings()->origin();

      sCP.viewCenter = globeMapToScene( point4978FromAngles( 0, 0 ), canvas->mapSettings()->origin() );
      sCP.distance = 5'000'000;
      qDebug() << sCP.viewCenter;

      updateCamera();

      connect( btnL, &QPushButton::clicked, this, [=] { moveGlobe( 0, -sMoveFactor * sCP.distance ); });
      connect( btnR, &QPushButton::clicked, this, [=] { moveGlobe( 0, sMoveFactor * sCP.distance ); });
      connect( btnU, &QPushButton::clicked, this, [=] { moveGlobe( sMoveFactor * sCP.distance, 0 ); });
      connect( btnD, &QPushButton::clicked, this, [=] { moveGlobe( -sMoveFactor * sCP.distance, 0 ); });

      connect( btnIn, &QPushButton::clicked, this, [=] { zoomGlobe( sZoomFactor ); });
      connect( btnOut, &QPushButton::clicked, this, [=] { zoomGlobe( 1.f / sZoomFactor ); });

      connect( btnP0, &QPushButton::clicked, this, [=] { pitchGlobe( -5 ); });
      connect( btnP1, &QPushButton::clicked, this, [=] { pitchGlobe( 5 ); });
      connect( btnH0, &QPushButton::clicked, this, [=] { headingGlobe( -5 ); });
      connect( btnH1, &QPushButton::clicked, this, [=] { headingGlobe( 5 ); });

      connect( mCanvas->mapSettings(), &Qgs3DMapSettings::originChanged, this, [=] {
        QgsVector3D newOrigin = mCanvas->mapSettings()->origin();
        qDebug() << "origin change!" << newOrigin.toString(1);
        QgsVector3D diff = newOrigin - mOrigin;
        mOrigin = newOrigin;

        sCP.viewCenter = sCP.viewCenter - diff.toVector3D();
        updateCamera();
      });
    }

    void moveGlobe( double lat, double lon )
    {
      QgsVector3D vc = globeSceneToMap( sCP.viewCenter, mCanvas->mapSettings()->origin() );
      QgsVector3D vc2 = movePoint4978ByAngle( vc, lat, lon );
      qDebug() << vc2.toString(1);
      sCP.viewCenter = globeMapToScene( vc2, mCanvas->mapSettings()->origin() );
      updateCamera();
    }

    void zoomGlobe( float factor )
    {
      sCP.distance = sCP.distance * factor;
      updateCamera();
    }

    void pitchGlobe( float amount )
    {
      sCP.pitchAngle = std::clamp( sCP.pitchAngle + amount, 0.f, 90.f );
      updateCamera();
    }

    void headingGlobe( float amount )
    {
      sCP.headingAngle += amount;
      updateCamera();
    }

    void updateCamera()
    {
      sCP.toCamera( mCanvas->mapSettings()->origin(), mCanvas->camera() );
      emit mCanvas->cameraController()->cameraChanged();
    }

    void onMousePositionChanged(Qt3DInput::QMouseEvent *mouse)
    {
      if ( mouse->buttons() & Qt::LeftButton )
      {
        qDebug() << "pos" << mouse->x() << mouse->y();

        // TODO: this is very crude

        QgsVector3D newMapPos = mapPositionAtPoint( QPoint(mouse->x(), mouse->y()), mDepthImage, &mCameraPress, mPressOrigin );
        if ( newMapPos == QgsVector3D(0,0,0) )
        {
          qDebug() << "out of earth - ignoring";
          return;
        }
        qDebug() << "new map pos" << newMapPos.toString(1);

        double oldLat, oldLon;
        point4978ToAngles( mMapPressPos, oldLat, oldLon );
        double newLat, newLon;
        point4978ToAngles( newMapPos, newLat, newLon );
        qDebug() << oldLat << oldLon << " -> " << newLat << newLon;

        QgsVector3D newVC = movePoint4978ByAngle( mPressVC, oldLat - newLat, oldLon - newLon );
        QVector3D newVCWorld = globeMapToScene( newVC, mCanvas->mapSettings()->origin() );
        sCP.viewCenter = newVCWorld;
        updateCamera();
      }
    }

    QPoint mPressPos;
    QgsVector3D mMapPressPos;
    QgsVector3D mPressVC;
    QgsVector3D mPressOrigin;

    Qt3DRender::QCamera mCameraPress;
    QImage mDepthImage;

    void onMousePressed(Qt3DInput::QMouseEvent *mouse)
    {
      qDebug() << "press";
      mCanvas->captureDepthBuffer();
      mPressPos = QPoint( mouse->x(), mouse->y() );
      mPressOrigin = mOrigin;
      mPressVC = globeSceneToMap( sCP.viewCenter, mCanvas->mapSettings()->origin() );

      // needed for deph checking
      mCameraPress.setPosition( mCanvas->camera()->position() );
      mCameraPress.setViewCenter( mCanvas->camera()->viewCenter() );
      mCameraPress.setUpVector( mCanvas->camera()->upVector() );
      mCameraPress.setProjectionMatrix( mCanvas->camera()->projectionMatrix() );
      mCameraPress.setNearPlane( mCanvas->camera()->nearPlane() );
      mCameraPress.setFarPlane( mCanvas->camera()->farPlane() );
      mCameraPress.setAspectRatio( mCanvas->camera()->aspectRatio() );
      mCameraPress.setFieldOfView( mCanvas->camera()->fieldOfView() );
    }

    void onMouseReleased(Qt3DInput::QMouseEvent *mouse)
    {
    }

    void onMouseWheel(Qt3DInput::QWheelEvent *mouse)
    {
      qDebug() << "wheel" << mouse->angleDelta();

      float factor = abs(mouse->angleDelta().y()) / 1000.f;
      sCP.distance *= mouse->angleDelta().y() > 0 ? (1-factor) : (1+factor);
      updateCamera();
    }

    void onDepthBufferCaptured( const QImage &depthImage )
    {
      qDebug() << "got depth";
      mDepthImage = depthImage;
      mMapPressPos = mapPositionAtPoint( mPressPos, mDepthImage, &mCameraPress, mPressOrigin );
      qDebug() << "press map pos" << mMapPressPos.toString(1);
    }

    Qgs3DMapCanvas *mCanvas;
    QgsVector3D mOrigin;
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

  Qgs3DMapSettings *map = new Qgs3DMapSettings;
  map->setCrs( QgsProject::instance()->crs() );
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

  QObject::connect( canvas->scene(), &Qgs3DMapScene::totalPendingJobsCountChanged, canvas, [canvas] {
    qDebug() << "pending jobs:" << canvas->scene()->totalPendingJobsCount();
  } );

  qDebug() << "pending jobs:" << canvas->scene()->totalPendingJobsCount();

  QEntity *globe = makeGlobeMesh( -180, 180, -90, 90, 36, 18 );
  globe->setParent( canvas->scene() );

  //canvas->scene()->setSceneOriginShiftEnabled( false );
  canvas->mapSettings()->setShowCameraViewCenter( true );
  canvas->scene()->cameraController()->setInputHandlersEnabled( false );
  canvas->mapSettings()->setTerrainRenderingEnabled( false );  // %%%
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

  Qgs3DMapCanvas *canvas = new Qgs3DMapCanvas;
  initCanvas3D( canvas );

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

  CameraWidget *cameraWidget = new CameraWidget( canvas );

  // construct the layout of sandbox
  QVBoxLayout *vLayout = new QVBoxLayout;
  vLayout->setContentsMargins( 0, 0, 0, 0 );
  vLayout->setSpacing( 0 );
  vLayout->addWidget( toolBar );
  QHBoxLayout *hLayout = new QHBoxLayout;
  vLayout->addLayout( hLayout );
  vLayout->addWidget( cameraWidget );
  hLayout->setContentsMargins( 0, 0, 0, 0 );
  hLayout->addWidget( container );
  hLayout->addWidget( debugWidget );


  windowWidget->resize( 800, debugWidget->height() );
  windowWidget->setLayout( vLayout );
  windowWidget->show();

  return myApp.exec();
}
