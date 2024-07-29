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

#include "qgsapplication.h"
#include "qgs3d.h"
#include "qgslayertree.h"
#include "qgsmapsettings.h"
#include "qgspointcloudlayer.h"
#include "qgspointcloudlayer3drenderer.h"
#include "qgsproject.h"
#include "qgsflatterraingenerator.h"
#include "qgs3dmapscene.h"
#include "qgs3dmapsettings.h"
#include "qgs3dmapcanvas.h"
#include "qgsprojectelevationproperties.h"
#include "qgsprojectviewsettings.h"
#include "qgspointlightsettings.h"
#include "qgsterrainprovider.h"
#include "qgstiledscenelayer.h"
#include "qgstiledscenelayer3drenderer.h"

#include <QScreen>



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
  double denom = 8000.0;
  for ( int i = 0; i < ( int )pointCount; ++i )
  {
    *fptr++ = x[i] / denom;
    *fptr++ = y[i] / denom;
    *fptr++ = z[i] / denom;

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

  Qt3DCore::QEntity *e = new Qt3DCore::QEntity;
  e->addComponent( mat );
  e->addComponent( rend );
  return e;
}


void initCanvas3D( Qgs3DMapCanvas *canvas )
{
  QgsLayerTree *root = QgsProject::instance()->layerTreeRoot();
  const QList< QgsMapLayer * > visibleLayers = root->checkedLayers();

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
  map->setOrigin( QgsVector3D( fullExtent.center().x(), fullExtent.center().y(), 0 ) );
  map->setLayers( visibleLayers );

  map->setExtent( fullExtent );

  Qgs3DAxisSettings axis;
  axis.setMode( Qgs3DAxisSettings::Mode::Off );
  map->set3DAxisSettings( axis );

  map->setTransformContext( QgsProject::instance()->transformContext() );
  map->setPathResolver( QgsProject::instance()->pathResolver() );
  map->setMapThemeCollection( QgsProject::instance()->mapThemeCollection() );
  QObject::connect( QgsProject::instance(), &QgsProject::transformContextChanged, map, [map]
  {
    map->setTransformContext( QgsProject::instance()->transformContext() );
  } );

  QgsFlatTerrainGenerator *flatTerrain = new QgsFlatTerrainGenerator;
  flatTerrain->setCrs( map->crs() );
  map->setTerrainGenerator( flatTerrain );
  map->setTerrainElevationOffset( QgsProject::instance()->elevationProperties()->terrainProvider()->offset() );

  QgsPointLightSettings defaultPointLight;
  defaultPointLight.setPosition( QgsVector3D( 0, 1000, 0 ) );
  defaultPointLight.setConstantAttenuation( 0 );
  map->setLightSources( {defaultPointLight.clone() } );
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
  const float dist = static_cast< float >( std::max( extent.width(), extent.height() ) );
  canvas->setViewFromTop( extent.center(), dist * 2, 0 );

  QObject::connect( canvas->scene(), &Qgs3DMapScene::totalPendingJobsCountChanged, canvas, [canvas]
  {
    qDebug() << "pending jobs:" << canvas->scene()->totalPendingJobsCount();
  } );

  qDebug() << "pending jobs:" << canvas->scene()->totalPendingJobsCount();

  QEntity *globe = makeGlobeMesh( -150, 150, -80, 80, 36, 18 );
  globe->setParent( canvas->scene() );
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

  // a hack to assign 3D renderer
  for ( QgsMapLayer *layer : QgsProject::instance()->layerTreeRoot()->checkedLayers() )
  {
    if ( QgsPointCloudLayer *pcLayer = qobject_cast<QgsPointCloudLayer *>( layer ) )
    {
      QgsPointCloudLayer3DRenderer *r = new QgsPointCloudLayer3DRenderer();
      r->setLayer( pcLayer );
      r->resolveReferences( *QgsProject::instance() );
      pcLayer->setRenderer3D( r );
    }

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
  canvas->resize( 800, 600 );
  canvas->show();

  return myApp.exec();
}
