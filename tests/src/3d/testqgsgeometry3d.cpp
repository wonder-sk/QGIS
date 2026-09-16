/***************************************************************************
     testqgsgeometry3d.cpp
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

#include <cmath>
#include <limits>

#include "qgsbillboardgeometry3d.h"
#include "qgsbillboardmaterial3d.h"
#include "qgsdemterraintilegeometry3d.h"
#include "qgsgputexture.h"
#include "qgsgraphicspipelinecache.h"
#include "qgsmeshgeometry3d.h"
#include "qgsmeshmaterial3d.h"
#include "qgsphongmaterial3d.h"
#include "qgspointcloudgeometry3d.h"
#include "qgsray3d.h"
#include "qgsraycastcontext.h"
#include "qgsrendercommand.h"
#include "qgsrenderpass.h"
#include "qgsrenderuniforms.h"
#include "qgsrhiengine.h"
#include "qgsrhishader.h"
#include "qgstest.h"
#include "qgsunlitmaterial3d.h"
#include "qgswiredmeshgeometry3d.h"

#include <QImage>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

namespace
{
  //! Builds a height map of \a res × \a res floats from a function of the grid position
  template<typename Fn> QByteArray heightMap( int res, Fn height )
  {
    QByteArray data;
    data.resize( res * res * sizeof( float ) );
    float *out = reinterpret_cast<float *>( data.data() );
    for ( int j = 0; j < res; ++j )
      for ( int i = 0; i < res; ++i )
        *out++ = height( i, j );
    return data;
  }

  //! Reads the position of vertex \a index out of interleaved DEM tile vertex data
  QVector3D demPosition( const QByteArray &vertexData, int index )
  {
    const float *v = reinterpret_cast<const float *>( vertexData.constData() ) + index * QgsDemTerrainTileGeometry3D::stride() / sizeof( float );
    return QVector3D( v[0], v[1], v[2] );
  }

  //! Counts pixels in \a image whose red channel is above \a threshold
  int litPixels( const QImage &image, int threshold = 200 )
  {
    int count = 0;
    for ( int y = 0; y < image.height(); ++y )
      for ( int x = 0; x < image.width(); ++x )
        if ( image.pixelColor( x, y ).red() > threshold )
          ++count;
    return count;
  }
} // namespace

/**
 * \ingroup UnitTests
 * Unit tests for the geometry generators ported onto QgsGeometry3D: DEM terrain tiles,
 * billboards and wired meshes.
 */
class TestQgsGeometry3D : public QgsTest
{
    Q_OBJECT
  public:
    TestQgsGeometry3D()
      : QgsTest( u"QgsGeometry3D generator tests"_s )
    {}

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void testWiredMeshVertices();
    void testWiredMeshFromBoxes();
    void testWiredMeshRenders();

    void testDemTileLayout();
    void testDemTileHeights();
    void testDemTileSkirt();
    void testDemTileNoData();
    void testDemTileRayIntersection();
    void testDemTileRayMiss();
    void testDemTileRenders();

    void testBillboardPositions();
    void testBillboardPositionsAndSizes();
    void testBillboardAtlasData();
    void testBillboardAtlasDataWithPixelOffsets();
    void testBillboardAttributesFollowTheData();
    void testBillboardRenders();

    void testMeshTerrainLayout();
    void testMeshDatasetLayout();
    void testMeshWireframeIsNotIndexed();
    void testMeshRenders();

    void testPointCloudLayouts();
    void testPointCloudStrideFromSymbol();
    void testPointCloudTriangulation();
    void testPointCloudRenders();

  private:
    QRhi::Implementation mBackend = QRhi::Null;
};

void TestQgsGeometry3D::initTestCase()
{
  mBackend = QgsRhiEngine::defaultBackend();
}

void TestQgsGeometry3D::cleanupTestCase()
{
  QgsRhiShader::clearCache();
  QgsApplication::exitQgis();
}

void TestQgsGeometry3D::testWiredMeshVertices()
{
  QgsWiredMeshGeometry3D mesh;
  QCOMPARE( mesh.vertexCount(), 0 );

  // one attribute, tightly packed positions
  QCOMPARE( mesh.geometry()->attributes().size(), 1 );
  const QgsVertexAttribute *position = mesh.geometry()->attribute( u"vertexPosition"_s );
  QVERIFY( position );
  QCOMPARE( position->format, QRhiVertexInputAttribute::Float3 );
  QCOMPARE( position->byteStride, 3 * sizeof( float ) );
  QCOMPARE( position->instanceDivisor, 0u );

  // a wired mesh is drawn from an attribute buffer, never through indices
  QVERIFY( !mesh.geometry()->isIndexed() );

  mesh.setVertices( QList<QVector3D>() << QVector3D( 0, 0, 0 ) << QVector3D( 1, 2, 3 ) << QVector3D( -1, 0, 5 ) << QVector3D( 4, 4, 4 ) );
  QCOMPARE( mesh.vertexCount(), 4 );
  QCOMPARE( position->buffer->size(), 4 * 3 * int( sizeof( float ) ) );

  const float *data = reinterpret_cast<const float *>( position->buffer->data().constData() );
  QCOMPARE( data[3], 1.0f );
  QCOMPARE( data[4], 2.0f );
  QCOMPARE( data[5], 3.0f );

  const QgsAABB box = mesh.geometry()->boundingBox();
  QCOMPARE( box.xMin, -1.0f );
  QCOMPARE( box.zMax, 5.0f );

  // setting fewer vertices must shrink the geometry rather than leave stale ones behind
  mesh.setVertices( QList<QVector3D>() << QVector3D( 0, 0, 0 ) << QVector3D( 1, 1, 1 ) );
  QCOMPARE( mesh.vertexCount(), 2 );
  QCOMPARE( position->buffer->size(), 2 * 3 * int( sizeof( float ) ) );
}

void TestQgsGeometry3D::testWiredMeshFromBoxes()
{
  QgsWiredMeshGeometry3D mesh;
  mesh.setVertices( QList<QgsAABB>() << QgsAABB( 0, 0, 0, 1, 1, 1 ) << QgsAABB( 10, 10, 10, 12, 12, 12 ) );

  // twelve edges per box, two vertices per edge
  QCOMPARE( mesh.vertexCount(), 2 * 12 * 2 );

  // the bounding box spans both boxes
  const QgsAABB box = mesh.geometry()->boundingBox();
  QCOMPARE( box.xMin, 0.0f );
  QCOMPARE( box.xMax, 12.0f );
  QCOMPARE( box.zMax, 12.0f );
}

void TestQgsGeometry3D::testWiredMeshRenders()
{
  if ( mBackend == QRhi::Null )
    QSKIP( "The Null backend draws no pixels" );

  QgsRhiEngine engine;
  if ( !engine.createOffscreen( QSize( 64, 64 ), mBackend ) )
    QSKIP( "No usable QRhi device for the default backend" );

  QgsGraphicsPipelineCache cache;

  QgsWiredMeshGeometry3D mesh;
  mesh.setVertices( QList<QgsAABB>() << QgsAABB( -3, -3, -3, 3, 3, 3 ) );

  QgsUnlitMaterial3D material;
  material.setColor( QColor( 255, 0, 0 ) );
  material.setTopology( QRhiGraphicsPipeline::Lines );
  material.setCullMode( QRhiGraphicsPipeline::None );

  QgsRenderPass pass( u"wired"_s );
  QMatrix4x4 view;
  view.lookAt( QVector3D( 0.0f, 0.0f, 20.0f ), QVector3D(), QVector3D( 0.0f, 1.0f, 0.0f ) );
  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );
  pass.setCamera( view, projection );
  pass.setClearColor( Qt::black );

  QgsRenderCommand command;
  command.geometry = mesh.geometry();
  command.material = &material;
  pass.addCommand( command );

  engine.setReadbackEnabled( true );
  QVERIFY( engine.beginFrame() );
  QVERIFY( pass.execute( &engine, &cache ) );
  engine.endFrame();

  // the box outline is drawn, but as lines it covers far fewer pixels than a solid shape
  const int lit = litPixels( engine.readbackImage() );
  QVERIFY2( lit > 20, qPrintable( u"only %1 pixels"_s.arg( lit ) ) );
  QVERIFY2( lit < 64 * 64 / 4, qPrintable( u"%1 pixels is too solid for an outline"_s.arg( lit ) ) );

  pass.releaseGpuResources();
  mesh.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsGeometry3D::testDemTileLayout()
{
  const int res = 8;
  const QgsDemTerrainTileGeometry3D tile( res, 100.0f, 1.0f, 0.0f, heightMap( res, []( int, int ) { return 0.0f; } ) );

  // the skirt adds a ring of vertices all the way around
  const int side = res + 2;
  QCOMPARE( tile.geometry()->vertexCount(), side * side );
  QCOMPARE( tile.geometry()->indexCount(), 2 * ( side - 1 ) * ( side - 1 ) * 3 );
  QCOMPARE( QgsDemTerrainTileGeometry3D::stride(), 8 * int( sizeof( float ) ) );

  QVERIFY( tile.geometry()->isIndexed() );
  QCOMPARE( tile.geometry()->indexFormat(), QRhiCommandBuffer::IndexUInt32 );

  // position, texture coordinate and normal, interleaved in that order
  QCOMPARE( tile.geometry()->attributes().size(), 3 );
  const QgsVertexAttribute *position = tile.geometry()->attribute( u"vertexPosition"_s );
  const QgsVertexAttribute *texCoord = tile.geometry()->attribute( u"vertexTexCoord"_s );
  const QgsVertexAttribute *normal = tile.geometry()->attribute( u"vertexNormal"_s );
  QVERIFY( position && texCoord && normal );
  QCOMPARE( position->byteOffset, 0u );
  QCOMPARE( texCoord->byteOffset, 3 * sizeof( float ) );
  QCOMPARE( normal->byteOffset, 5 * sizeof( float ) );
  for ( const QgsVertexAttribute &attribute : tile.geometry()->attributes() )
    QCOMPARE( attribute.byteStride, quint32( QgsDemTerrainTileGeometry3D::stride() ) );

  // all three share one interleaved buffer
  QCOMPARE( texCoord->buffer, position->buffer );
  QCOMPARE( normal->buffer, position->buffer );
}

void TestQgsGeometry3D::testDemTileHeights()
{
  const int res = 4;
  const float side = 30.0f;
  const float vertScale = 2.0f;

  // a ramp rising along the row index, so every vertex has a different height
  const QByteArray heights = heightMap( res, []( int i, int ) { return static_cast<float>( i ); } );
  const QgsDemTerrainTileGeometry3D tile( res, side, vertScale, 0.0f, heights );

  const QByteArray vertexData = tile.geometry()->attribute( u"vertexPosition"_s )->buffer->data();
  const int stride = res + 2;

  // the first interior vertex is the tile's own corner: x at the origin, y at the far
  // edge, because the grid runs from y = side downwards
  const QVector3D corner = demPosition( vertexData, stride + 1 );
  QCOMPARE( corner.x(), 0.0f );
  QCOMPARE( corner.y(), side );
  QCOMPARE( corner.z(), 0.0f );

  // heights are multiplied by the vertical scale
  const QVector3D nextAlongRow = demPosition( vertexData, stride + 2 );
  QCOMPARE( nextAlongRow.z(), 1.0f * vertScale );
  QCOMPARE( nextAlongRow.x(), side / ( res - 1 ) );

  // the bounding box covers the tile and the full height range
  const QgsAABB box = tile.geometry()->boundingBox();
  QCOMPARE( box.xMin, 0.0f );
  QCOMPARE( box.xMax, side );
  QCOMPARE( box.zMin, 0.0f );
  QCOMPARE( box.zMax, ( res - 1 ) * vertScale );
}

void TestQgsGeometry3D::testDemTileSkirt()
{
  const int res = 4;
  const float skirtHeight = 5.0f;
  const QByteArray heights = heightMap( res, []( int, int ) { return 10.0f; } );
  const QgsDemTerrainTileGeometry3D tile( res, 30.0f, 1.0f, skirtHeight, heights );

  const QByteArray vertexData = tile.geometry()->attribute( u"vertexPosition"_s )->buffer->data();
  const int stride = res + 2;

  // the outer ring hangs below the terrain by the skirt height, while the interior sits on it
  QCOMPARE( demPosition( vertexData, 0 ).z(), 10.0f - skirtHeight );
  QCOMPARE( demPosition( vertexData, stride + 1 ).z(), 10.0f );
  QCOMPARE( demPosition( vertexData, stride * stride - 1 ).z(), 10.0f - skirtHeight );

  // a skirt vertex sits directly above or beside its neighbour rather than outside the tile
  QCOMPARE( demPosition( vertexData, 1 ).x(), demPosition( vertexData, stride + 1 ).x() );

  // and it drags the bounding box down with it
  QCOMPARE( tile.geometry()->boundingBox().zMin, 10.0f - skirtHeight );
}

void TestQgsGeometry3D::testDemTileNoData()
{
  const int res = 4;

  // one no-data value in the middle of the tile
  const QByteArray heights = heightMap( res, []( int i, int j ) { return ( i == 1 && j == 1 ) ? std::numeric_limits<float>::quiet_NaN() : 1.0f; } );
  const QgsDemTerrainTileGeometry3D tile( res, 30.0f, 1.0f, 0.0f, heights );

  const QByteArray indexData = tile.geometry()->indexBuffer()->data();
  const quint32 *indices = reinterpret_cast<const quint32 *>( indexData.constData() );
  const int triangleCount = tile.geometry()->indexCount() / 3;

  // the quads touching the no-data vertex are emitted as degenerate triangles rather than
  // dropped, so that the index count stays a pure function of the resolution
  int degenerate = 0;
  for ( int i = 0; i < triangleCount; ++i )
  {
    if ( indices[i * 3] == indices[i * 3 + 1] && indices[i * 3] == indices[i * 3 + 2] )
      ++degenerate;
  }

  // the no-data vertex is a corner of four quads, two triangles each
  QCOMPARE( degenerate, 8 );

  // no vertex position is left as NaN, since those would poison the bounding box
  const QByteArray vertexData = tile.geometry()->attribute( u"vertexPosition"_s )->buffer->data();
  for ( int i = 0; i < tile.geometry()->vertexCount(); ++i )
    QVERIFY( !std::isnan( demPosition( vertexData, i ).z() ) );
}

void TestQgsGeometry3D::testDemTileRayIntersection()
{
  const int res = 4;
  const float side = 30.0f;
  const QByteArray heights = heightMap( res, []( int, int ) { return 7.0f; } );
  const QgsDemTerrainTileGeometry3D tile( res, side, 1.0f, 0.0f, heights );

  // straight down onto the middle of a flat tile
  const QgsRay3D ray( QVector3D( side / 2, side / 2, 100.0f ), QVector3D( 0.0f, 0.0f, -1.0f ) );
  QgsRayCastContext context;
  context.setMaximumDistance( 1000.0f );

  QVector3D intersection;
  QVERIFY( tile.rayIntersection( ray, context, QMatrix4x4(), intersection ) );
  QCOMPARE( intersection.x(), side / 2 );
  QCOMPARE( intersection.y(), side / 2 );
  QVERIFY2( std::abs( intersection.z() - 7.0f ) < 1e-3f, qPrintable( QString::number( intersection.z() ) ) );

  // the world transform is applied to the tile, not to the ray
  QMatrix4x4 lifted;
  lifted.translate( 0.0f, 0.0f, 20.0f );
  QVERIFY( tile.rayIntersection( ray, context, lifted, intersection ) );
  QVERIFY2( std::abs( intersection.z() - 27.0f ) < 1e-3f, qPrintable( QString::number( intersection.z() ) ) );
}

void TestQgsGeometry3D::testDemTileRayMiss()
{
  const int res = 4;
  const QByteArray heights = heightMap( res, []( int, int ) { return 0.0f; } );
  const QgsDemTerrainTileGeometry3D tile( res, 30.0f, 1.0f, 0.0f, heights );

  QgsRayCastContext context;
  context.setMaximumDistance( 1000.0f );
  QVector3D intersection;

  // well outside the tile, pointing down
  const QgsRay3D beside( QVector3D( 500.0f, 500.0f, 100.0f ), QVector3D( 0.0f, 0.0f, -1.0f ) );
  QVERIFY( !tile.rayIntersection( beside, context, QMatrix4x4(), intersection ) );

  // over the tile, but pointing away from it
  const QgsRay3D upwards( QVector3D( 15.0f, 15.0f, 100.0f ), QVector3D( 0.0f, 0.0f, 1.0f ) );
  QVERIFY( !tile.rayIntersection( upwards, context, QMatrix4x4(), intersection ) );
}

void TestQgsGeometry3D::testDemTileRenders()
{
  if ( mBackend == QRhi::Null )
    QSKIP( "The Null backend draws no pixels" );

  QgsRhiEngine engine;
  if ( !engine.createOffscreen( QSize( 64, 64 ), mBackend ) )
    QSKIP( "No usable QRhi device for the default backend" );

  QgsGraphicsPipelineCache cache;

  const int res = 16;
  const float side = 20.0f;
  const QByteArray heights = heightMap( res, []( int i, int j ) { return std::sin( i * 0.5f ) + std::cos( j * 0.5f ); } );
  QgsDemTerrainTileGeometry3D tile( res, side, 1.0f, 1.0f, heights );

  QgsPhongMaterial3D material;
  material.setAmbient( QColor( 255, 0, 0 ) );
  material.setDiffuse( Qt::black );
  material.setCullMode( QRhiGraphicsPipeline::None );

  QgsRenderPass pass( u"dem"_s );
  QMatrix4x4 view;
  // look straight down at the tile, which lies in the XY plane from 0 to side
  view.lookAt( QVector3D( side / 2, side / 2, 40.0f ), QVector3D( side / 2, side / 2, 0.0f ), QVector3D( 0.0f, 1.0f, 0.0f ) );
  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 200.0f );
  pass.setCamera( view, projection );
  pass.setClearColor( Qt::black );

  QgsRenderCommand command;
  command.geometry = tile.geometry();
  command.material = &material;
  pass.addCommand( command );

  engine.setReadbackEnabled( true );
  QVERIFY( engine.beginFrame() );
  QVERIFY( pass.execute( &engine, &cache ) );
  engine.endFrame();

  // seen from above the tile fills most of the view
  const int lit = litPixels( engine.readbackImage() );
  QVERIFY2( lit > 64 * 64 / 4, qPrintable( u"only %1 pixels"_s.arg( lit ) ) );

  pass.releaseGpuResources();
  tile.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsGeometry3D::testBillboardPositions()
{
  QgsBillboardGeometry3D billboards;

  // the quad is always there, even before any billboard is added
  const QgsVertexAttribute *quad = billboards.geometry()->attribute( u"vertexPosition"_s );
  QVERIFY( quad );
  QCOMPARE( quad->format, QRhiVertexInputAttribute::Float2 );
  QCOMPARE( quad->instanceDivisor, 0u );
  QCOMPARE( billboards.geometry()->vertexCount(), 4 );
  QCOMPARE( billboards.count(), 0 );

  billboards.setPositions( QVector<QVector3D>() << QVector3D( 1, 2, 3 ) << QVector3D( 4, 5, 6 ) );

  QCOMPARE( billboards.count(), 2 );
  QCOMPARE( billboards.attributes(), QgsBillboardGeometry3D::Attributes( QgsBillboardGeometry3D::Attribute::Position ) );
  QCOMPARE( billboards.stride(), 3 * int( sizeof( float ) ) );

  // the quad still advances per vertex while the position advances per instance
  QCOMPARE( billboards.geometry()->vertexCount(), 4 );
  const QgsVertexAttribute *position = billboards.geometry()->attribute( u"instancePosition"_s );
  QVERIFY( position );
  QCOMPARE( position->instanceDivisor, 1u );
  QCOMPARE( position->byteOffset, 0u );
  QCOMPARE( position->buffer->size(), 2 * 3 * int( sizeof( float ) ) );

  const float *data = reinterpret_cast<const float *>( position->buffer->data().constData() );
  QCOMPARE( data[0], 1.0f );
  QCOMPARE( data[4], 5.0f );

  // nothing that was not asked for is declared
  QVERIFY( !billboards.geometry()->attribute( u"instanceSize"_s ) );
  QVERIFY( !billboards.geometry()->attribute( u"atlasOffset"_s ) );
  QVERIFY( !billboards.geometry()->attribute( u"pixelOffset"_s ) );
}

void TestQgsGeometry3D::testBillboardPositionsAndSizes()
{
  QgsBillboardGeometry3D billboards;
  billboards.setPositionsAndSizes( QVector<QVector3D>() << QVector3D( 1, 2, 3 ) << QVector3D( 4, 5, 6 ), QVector<QSizeF>() << QSizeF( 10, 20 ) << QSizeF( 30, 40 ) );

  QCOMPARE( billboards.count(), 2 );
  QCOMPARE( billboards.stride(), 5 * int( sizeof( float ) ) );

  const QgsVertexAttribute *size = billboards.geometry()->attribute( u"instanceSize"_s );
  QVERIFY( size );
  QCOMPARE( size->format, QRhiVertexInputAttribute::Float2 );
  QCOMPARE( size->byteOffset, 3 * sizeof( float ) );
  QCOMPARE( size->instanceDivisor, 1u );

  const float *data = reinterpret_cast<const float *>( size->buffer->data().constData() );
  QCOMPARE( data[3], 10.0f );
  QCOMPARE( data[4], 20.0f );
  QCOMPARE( data[8], 30.0f );

  // mismatched input lengths take the shorter of the two rather than reading past the end
  billboards.setPositionsAndSizes( QVector<QVector3D>() << QVector3D( 1, 2, 3 ) << QVector3D( 4, 5, 6 ), QVector<QSizeF>() << QSizeF( 10, 20 ) );
  QCOMPARE( billboards.count(), 1 );
  QCOMPARE( size->buffer->size(), billboards.stride() );
}

void TestQgsGeometry3D::testBillboardAtlasData()
{
  QVector<QgsBillboardGeometry3D::BillboardAtlasData> data;
  data.append( { QVector3D( 1, 2, 3 ), QVector2D( 0.25f, 0.5f ), QVector2D( 0.125f, 0.25f ), QPoint( 7, 8 ) } );

  QgsBillboardGeometry3D billboards;
  billboards.setBillboardData( data );

  QCOMPARE( billboards.count(), 1 );
  QCOMPARE( billboards.stride(), 7 * int( sizeof( float ) ) );
  QVERIFY( billboards.attributes().testFlag( QgsBillboardGeometry3D::Attribute::TextureData ) );
  QVERIFY( !billboards.attributes().testFlag( QgsBillboardGeometry3D::Attribute::PixelOffsets ) );

  const QgsVertexAttribute *offset = billboards.geometry()->attribute( u"atlasOffset"_s );
  const QgsVertexAttribute *size = billboards.geometry()->attribute( u"atlasSize"_s );
  QVERIFY( offset && size );
  QCOMPARE( offset->byteOffset, 3 * sizeof( float ) );
  QCOMPARE( size->byteOffset, 5 * sizeof( float ) );

  const float *raw = reinterpret_cast<const float *>( offset->buffer->data().constData() );
  QCOMPARE( raw[3], 0.25f );
  QCOMPARE( raw[4], 0.5f );
  QCOMPARE( raw[5], 0.125f );
  QCOMPARE( raw[6], 0.25f );

  // the pixel offset was supplied but not asked for, so it is not written at all
  QVERIFY( !billboards.geometry()->attribute( u"pixelOffset"_s ) );
}

void TestQgsGeometry3D::testBillboardAtlasDataWithPixelOffsets()
{
  QVector<QgsBillboardGeometry3D::BillboardAtlasData> data;
  data.append( { QVector3D( 1, 2, 3 ), QVector2D( 0.25f, 0.5f ), QVector2D( 0.125f, 0.25f ), QPoint( 7, -8 ) } );
  data.append( { QVector3D( 4, 5, 6 ), QVector2D( 0.0f, 0.0f ), QVector2D( 1.0f, 1.0f ), QPoint( -1, 2 ) } );

  QgsBillboardGeometry3D billboards;
  billboards.setBillboardData( data, true );

  QCOMPARE( billboards.count(), 2 );
  QCOMPARE( billboards.stride(), 7 * int( sizeof( float ) ) + 2 * int( sizeof( int ) ) );

  const QgsVertexAttribute *pixelOffset = billboards.geometry()->attribute( u"pixelOffset"_s );
  QVERIFY( pixelOffset );
  // an integer attribute, because the shader reads it as ivec2
  QCOMPARE( pixelOffset->format, QRhiVertexInputAttribute::SInt2 );
  QCOMPARE( pixelOffset->byteOffset, 7 * sizeof( float ) );

  const char *raw = pixelOffset->buffer->data().constData();
  const int *first = reinterpret_cast<const int *>( raw + pixelOffset->byteOffset );
  QCOMPARE( first[0], 7 );
  QCOMPARE( first[1], -8 );
  const int *second = reinterpret_cast<const int *>( raw + billboards.stride() + pixelOffset->byteOffset );
  QCOMPARE( second[0], -1 );
  QCOMPARE( second[1], 2 );
}

void TestQgsGeometry3D::testBillboardAttributesFollowTheData()
{
  // reusing one geometry for different kinds of billboard must leave behind no attribute
  // the new data does not fill, since a shader reading one would fail to build a pipeline
  QgsBillboardGeometry3D billboards;

  billboards.setPositionsAndSizes( QVector<QVector3D>() << QVector3D( 1, 2, 3 ), QVector<QSizeF>() << QSizeF( 4, 5 ) );
  QVERIFY( billboards.geometry()->attribute( u"instanceSize"_s ) );
  const size_t withSizes = billboards.geometry()->layoutKey();

  QVector<QgsBillboardGeometry3D::BillboardAtlasData> atlas;
  atlas.append( { QVector3D( 1, 2, 3 ), QVector2D(), QVector2D( 1, 1 ), QPoint() } );
  billboards.setBillboardData( atlas );
  QVERIFY( !billboards.geometry()->attribute( u"instanceSize"_s ) );
  QVERIFY( billboards.geometry()->attribute( u"atlasOffset"_s ) );
  QVERIFY( withSizes != billboards.geometry()->layoutKey() );

  billboards.setPositions( QVector<QVector3D>() << QVector3D( 1, 2, 3 ) );
  QCOMPARE( billboards.geometry()->attributes().size(), 2 );
  QVERIFY( !billboards.geometry()->attribute( u"atlasOffset"_s ) );

  // going back to a layout already used gives the same pipeline again
  billboards.setPositionsAndSizes( QVector<QVector3D>() << QVector3D( 1, 2, 3 ), QVector<QSizeF>() << QSizeF( 4, 5 ) );
  QCOMPARE( billboards.geometry()->layoutKey(), withSizes );

  // the geometry's attribute flags are what pick the material's shader variant
  QCOMPARE( static_cast<int>( QgsBillboardGeometry3D::Attribute::Size ) >> 1, static_cast<int>( QgsBillboardMaterial3D::InstanceAttribute::Size ) );
  QCOMPARE( static_cast<int>( QgsBillboardGeometry3D::Attribute::TextureData ) >> 1, static_cast<int>( QgsBillboardMaterial3D::InstanceAttribute::TextureData ) );
  QCOMPARE( static_cast<int>( QgsBillboardGeometry3D::Attribute::PixelOffsets ) >> 1, static_cast<int>( QgsBillboardMaterial3D::InstanceAttribute::PixelOffsets ) );
}

void TestQgsGeometry3D::testBillboardRenders()
{
  if ( mBackend == QRhi::Null )
    QSKIP( "The Null backend draws no pixels" );

  QgsRhiEngine engine;
  if ( !engine.createOffscreen( QSize( 64, 64 ), mBackend ) )
    QSKIP( "No usable QRhi device for the default backend" );

  QgsGraphicsPipelineCache cache;

  QImage sprite( 8, 8, QImage::Format_RGBA8888 );
  sprite.fill( QColor( 255, 0, 0 ) );
  QgsGpuTexture texture;
  texture.setImage( sprite );

  QgsBillboardGeometry3D billboards;
  billboards.setPositionsAndSizes( QVector<QVector3D>() << QVector3D( -4, 0, 0 ) << QVector3D( 4, 0, 0 ), QVector<QSizeF>() << QSizeF( 16, 16 ) << QSizeF( 16, 16 ) );

  QgsBillboardMaterial3D material( QgsBillboardMaterial3D::InstanceAttribute::Size );
  material.setSpriteTexture( &texture );
  material.setTopology( QRhiGraphicsPipeline::TriangleStrip );
  material.setCullMode( QRhiGraphicsPipeline::None );

  QgsRenderPass pass( u"billboards"_s );
  QMatrix4x4 view;
  view.lookAt( QVector3D( 0.0f, 0.0f, 20.0f ), QVector3D(), QVector3D( 0.0f, 1.0f, 0.0f ) );
  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );
  pass.setCamera( view, projection );
  pass.setClearColor( Qt::black );

  QgsRenderCommand command;
  command.geometry = billboards.geometry();
  command.material = &material;
  command.instanceCount = billboards.count();
  pass.addCommand( command );

  engine.setReadbackEnabled( true );
  QVERIFY( engine.beginFrame() );
  QVERIFY( pass.execute( &engine, &cache ) );
  engine.endFrame();

  // two 16 pixel sprites, so a few hundred pixels rather than the whole frame
  const int lit = litPixels( engine.readbackImage() );
  QVERIFY2( lit > 100, qPrintable( u"only %1 pixels"_s.arg( lit ) ) );
  QVERIFY2( lit < 64 * 64 / 2, qPrintable( u"%1 pixels is more than two sprites"_s.arg( lit ) ) );

  pass.releaseGpuResources();
  billboards.releaseGpuResources();
  material.releaseGpuResources();
  texture.releaseGpuResources();
  cache.clear();
}

void TestQgsGeometry3D::testMeshTerrainLayout()
{
  QgsMeshGeometry3D mesh( false );

  QCOMPARE( mesh.stride(), 6 * int( sizeof( float ) ) );
  QVERIFY( !mesh.hasScalarMagnitude() );
  QCOMPARE( mesh.geometry()->attributes().size(), 2 );

  const QgsVertexAttribute *position = mesh.geometry()->attribute( u"vertexPosition"_s );
  const QgsVertexAttribute *normal = mesh.geometry()->attribute( u"vertexNormal"_s );
  QVERIFY( position && normal );
  QCOMPARE( position->byteOffset, 0u );
  QCOMPARE( normal->byteOffset, 3 * sizeof( float ) );
  QVERIFY( !mesh.geometry()->attribute( u"scalarMagnitude"_s ) );

  // the vertex count follows from the data, since the builder writes one vertex per mesh
  // vertex rather than reporting a count of its own
  QByteArray vertexData;
  vertexData.resize( 4 * mesh.stride() );
  mesh.setVertexData( vertexData );
  QCOMPARE( mesh.geometry()->vertexCount(), 4 );

  QByteArray indexData;
  indexData.resize( 6 * sizeof( quint32 ) );
  mesh.setIndexData( indexData );
  QCOMPARE( mesh.geometry()->indexCount(), 6 );
  QCOMPARE( mesh.geometry()->drawCount(), 6 );
}

void TestQgsGeometry3D::testMeshDatasetLayout()
{
  QgsMeshGeometry3D mesh( true );

  QCOMPARE( mesh.stride(), 7 * int( sizeof( float ) ) );
  QVERIFY( mesh.hasScalarMagnitude() );

  const QgsVertexAttribute *magnitude = mesh.geometry()->attribute( u"scalarMagnitude"_s );
  QVERIFY( magnitude );
  QCOMPARE( magnitude->format, QRhiVertexInputAttribute::Float );
  QCOMPARE( magnitude->byteOffset, 6 * sizeof( float ) );
  QCOMPARE( magnitude->byteStride, quint32( mesh.stride() ) );

  // the magnitude attribute is the one thing that differs, so the two layouts must not
  // collide in the pipeline cache
  const QgsMeshGeometry3D terrain( false );
  QVERIFY( mesh.geometry()->layoutKey() != terrain.geometry()->layoutKey() );
}

void TestQgsGeometry3D::testMeshWireframeIsNotIndexed()
{
  // wireframe rendering needs each triangle to own its three vertices, because the
  // material derives the barycentric coordinates it draws edges from out of them
  QgsMeshGeometry3D wireframe( false, false );
  QVERIFY( !wireframe.geometry()->isIndexed() );

  QByteArray vertexData;
  vertexData.resize( 9 * wireframe.stride() );
  wireframe.setVertexData( vertexData );
  QCOMPARE( wireframe.geometry()->vertexCount(), 9 );
  QCOMPARE( wireframe.geometry()->drawCount(), 9 );

  const QgsMeshGeometry3D indexed( false, true );
  QVERIFY( indexed.geometry()->isIndexed() );
}

void TestQgsGeometry3D::testMeshRenders()
{
  if ( mBackend == QRhi::Null )
    QSKIP( "The Null backend draws no pixels" );

  QgsRhiEngine engine;
  if ( !engine.createOffscreen( QSize( 64, 64 ), mBackend ) )
    QSKIP( "No usable QRhi device for the default backend" );

  QgsGraphicsPipelineCache cache;

  // two triangles covering a square in the XY plane, with normals pointing up
  QgsMeshGeometry3D mesh( false );
  QVector<float> vertices;
  const float corners[4][3] = { { -5, -5, 0 }, { 5, -5, 0 }, { 5, 5, 0 }, { -5, 5, 0 } };
  for ( const auto &corner : corners )
    vertices << corner[0] << corner[1] << corner[2] << 0.0f << 0.0f << 1.0f;
  mesh.setVertexData( QByteArray( reinterpret_cast<const char *>( vertices.constData() ), vertices.size() * int( sizeof( float ) ) ) );

  const QVector<quint32> indices { 0, 1, 2, 0, 2, 3 };
  mesh.setIndexData( QByteArray( reinterpret_cast<const char *>( indices.constData() ), indices.size() * int( sizeof( quint32 ) ) ) );

  QgsMeshMaterial3D material( false );
  material.setStyle( QgsMeshMaterial3D::Style::SingleColor );
  material.setMeshColor( QColor( 255, 0, 0 ) );
  material.setCullMode( QRhiGraphicsPipeline::None );

  QgsRenderPass pass( u"mesh"_s );
  const QVector3D eye( 0.0f, 0.0f, 20.0f );
  QMatrix4x4 view;
  view.lookAt( eye, QVector3D(), QVector3D( 0.0f, 1.0f, 0.0f ) );
  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );
  pass.setCamera( view, projection );
  pass.setEyePosition( eye );
  pass.setClearColor( Qt::black );

  // the mesh material is lit, so without a light the surface comes out black
  QgsLightUniforms lights;
  lights.addDirectionalLight( QVector3D( 0.0f, 0.0f, -1.0f ), QColor( 255, 255, 255 ), 1.0f );
  pass.setLights( lights );

  QgsRenderCommand command;
  command.geometry = mesh.geometry();
  command.material = &material;
  pass.addCommand( command );

  engine.setReadbackEnabled( true );
  QVERIFY( engine.beginFrame() );
  QVERIFY( pass.execute( &engine, &cache ) );
  engine.endFrame();

  const int lit = litPixels( engine.readbackImage() );
  QVERIFY2( lit > 64 * 64 / 8, qPrintable( u"only %1 pixels"_s.arg( lit ) ) );

  pass.releaseGpuResources();
  mesh.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

void TestQgsGeometry3D::testPointCloudLayouts()
{
  const QVector<QVector3D> positions { QVector3D( 1, 2, 3 ), QVector3D( 4, 5, 6 ) };
  const QVector<float> parameters { 0.25f, 0.75f };
  const QVector<QVector3D> colors { QVector3D( 1, 0, 0 ), QVector3D( 0, 1, 0 ) };
  const QVector<float> sizes { 2.0f, 3.0f };

  // single colour: nothing but the position
  {
    QgsPointCloudGeometry3D cloud( QgsPointCloudGeometry3D::VertexLayout::PositionOnly );
    cloud.setPoints( positions );
    QCOMPARE( cloud.stride(), 3 * int( sizeof( float ) ) );
    QCOMPARE( cloud.pointCount(), 2 );
    QCOMPARE( cloud.geometry()->attributes().size(), 1 );
    QVERIFY( !cloud.isTriangulated() );
  }

  // colour ramp: one float the ramp is sampled with
  {
    QgsPointCloudGeometry3D cloud( QgsPointCloudGeometry3D::VertexLayout::PositionAndParameter );
    cloud.setPoints( positions, parameters );
    QCOMPARE( cloud.stride(), 4 * int( sizeof( float ) ) );
    const QgsVertexAttribute *parameter = cloud.geometry()->attribute( u"vertexParameter"_s );
    QVERIFY( parameter );
    QCOMPARE( parameter->format, QRhiVertexInputAttribute::Float );
    QCOMPARE( parameter->byteOffset, 3 * sizeof( float ) );

    const float *data = reinterpret_cast<const float *>( parameter->buffer->data().constData() );
    QCOMPARE( data[3], 0.25f );
    QCOMPARE( data[7], 0.75f );
  }

  // RGB: a colour straight out of the cloud
  {
    QgsPointCloudGeometry3D cloud( QgsPointCloudGeometry3D::VertexLayout::PositionAndColor );
    cloud.setPoints( positions, QVector<float>(), colors );
    QCOMPARE( cloud.stride(), 6 * int( sizeof( float ) ) );
    const QgsVertexAttribute *color = cloud.geometry()->attribute( u"vertexColor"_s );
    QVERIFY( color );
    QCOMPARE( color->format, QRhiVertexInputAttribute::Float3 );
    QCOMPARE( color->byteOffset, 3 * sizeof( float ) );
    QVERIFY( !cloud.geometry()->attribute( u"vertexParameter"_s ) );
  }

  // classification: a parameter and a per-point size
  {
    QgsPointCloudGeometry3D cloud( QgsPointCloudGeometry3D::VertexLayout::PositionParameterAndSize );
    cloud.setPoints( positions, parameters, QVector<QVector3D>(), sizes );
    QCOMPARE( cloud.stride(), 5 * int( sizeof( float ) ) );
    const QgsVertexAttribute *size = cloud.geometry()->attribute( u"vertexSize"_s );
    QVERIFY( size );
    QCOMPARE( size->byteOffset, 4 * sizeof( float ) );

    const float *data = reinterpret_cast<const float *>( size->buffer->data().constData() );
    QCOMPARE( data[4], 2.0f );
    QCOMPARE( data[9], 3.0f );
  }
}

void TestQgsGeometry3D::testPointCloudStrideFromSymbol()
{
  // the symbol's stride can leave room past the attributes the layout fills, and the
  // attributes have to be spaced by it rather than by their own packed size
  QgsPointCloudGeometry3D cloud( QgsPointCloudGeometry3D::VertexLayout::PositionAndParameter, 32 );
  QCOMPARE( cloud.stride(), 32 );

  const QVector<QVector3D> positions { QVector3D( 1, 2, 3 ), QVector3D( 4, 5, 6 ) };
  cloud.setPoints( positions, QVector<float> { 0.5f, 0.25f } );

  const QgsVertexAttribute *parameter = cloud.geometry()->attribute( u"vertexParameter"_s );
  QCOMPARE( parameter->byteStride, 32u );
  QCOMPARE( parameter->buffer->size(), 2 * 32 );

  const char *raw = parameter->buffer->data().constData();
  QCOMPARE( *reinterpret_cast<const float *>( raw + parameter->byteOffset ), 0.5f );
  QCOMPARE( *reinterpret_cast<const float *>( raw + 32 + parameter->byteOffset ), 0.25f );
  QCOMPARE( *reinterpret_cast<const float *>( raw + 32 ), 4.0f );
}

void TestQgsGeometry3D::testPointCloudTriangulation()
{
  QgsPointCloudGeometry3D cloud( QgsPointCloudGeometry3D::VertexLayout::PositionOnly );
  cloud.setPoints( QVector<QVector3D> { QVector3D( 0, 0, 0 ), QVector3D( 1, 0, 0 ), QVector3D( 0, 1, 0 ) } );
  QVERIFY( !cloud.isTriangulated() );
  QVERIFY( !cloud.geometry()->attribute( u"vertexNormal"_s ) );

  const QVector<quint32> indices { 0, 1, 2 };
  QVector<float> normals;
  for ( int i = 0; i < 3; ++i )
    normals << 0.0f << 0.0f << 1.0f;

  cloud.setTriangles(
    QByteArray( reinterpret_cast<const char *>( indices.constData() ), indices.size() * int( sizeof( quint32 ) ) ),
    QByteArray( reinterpret_cast<const char *>( normals.constData() ), normals.size() * int( sizeof( float ) ) )
  );

  QVERIFY( cloud.isTriangulated() );
  QCOMPARE( cloud.geometry()->indexCount(), 3 );

  // the normal comes from its own buffer rather than the interleaved one
  const QgsVertexAttribute *normal = cloud.geometry()->attribute( u"vertexNormal"_s );
  QVERIFY( normal );
  QCOMPARE( normal->byteStride, 3 * sizeof( float ) );
  QVERIFY( normal->buffer != cloud.geometry()->attribute( u"vertexPosition"_s )->buffer );

  // and going back to points has to take the attribute away again, or a shader reading it
  // would leave the chunk undrawn
  cloud.setTriangles( QByteArray(), QByteArray() );
  QVERIFY( !cloud.isTriangulated() );
  QVERIFY( !cloud.geometry()->attribute( u"vertexNormal"_s ) );
  QCOMPARE( cloud.geometry()->attributes().size(), 1 );
}

void TestQgsGeometry3D::testPointCloudRenders()
{
  if ( mBackend == QRhi::Null )
    QSKIP( "The Null backend draws no pixels" );

  QgsRhiEngine engine;
  if ( !engine.createOffscreen( QSize( 64, 64 ), mBackend ) )
    QSKIP( "No usable QRhi device for the default backend" );

  QgsGraphicsPipelineCache cache;

  // a triangulated patch, which is the point cloud path that does not wait for P6's
  // instanced quads
  QgsPointCloudGeometry3D cloud( QgsPointCloudGeometry3D::VertexLayout::PositionOnly );
  cloud.setPoints( QVector<QVector3D> { QVector3D( -5, -5, 0 ), QVector3D( 5, -5, 0 ), QVector3D( 5, 5, 0 ), QVector3D( -5, 5, 0 ) } );

  const QVector<quint32> indices { 0, 1, 2, 0, 2, 3 };
  QVector<float> normals;
  for ( int i = 0; i < 4; ++i )
    normals << 0.0f << 0.0f << 1.0f;
  cloud.setTriangles(
    QByteArray( reinterpret_cast<const char *>( indices.constData() ), indices.size() * int( sizeof( quint32 ) ) ),
    QByteArray( reinterpret_cast<const char *>( normals.constData() ), normals.size() * int( sizeof( float ) ) )
  );

  QgsPhongMaterial3D material;
  material.setAmbient( QColor( 255, 0, 0 ) );
  material.setDiffuse( Qt::black );
  material.setCullMode( QRhiGraphicsPipeline::None );

  QgsRenderPass pass( u"pointcloud"_s );
  QMatrix4x4 view;
  view.lookAt( QVector3D( 0.0f, 0.0f, 20.0f ), QVector3D(), QVector3D( 0.0f, 1.0f, 0.0f ) );
  QMatrix4x4 projection;
  projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );
  pass.setCamera( view, projection );
  pass.setClearColor( Qt::black );

  QgsRenderCommand command;
  command.geometry = cloud.geometry();
  command.material = &material;
  pass.addCommand( command );

  engine.setReadbackEnabled( true );
  QVERIFY( engine.beginFrame() );
  QVERIFY( pass.execute( &engine, &cache ) );
  engine.endFrame();

  const int lit = litPixels( engine.readbackImage() );
  QVERIFY2( lit > 64 * 64 / 8, qPrintable( u"only %1 pixels"_s.arg( lit ) ) );

  pass.releaseGpuResources();
  cloud.releaseGpuResources();
  material.releaseGpuResources();
  cache.clear();
}

QGSTEST_MAIN( TestQgsGeometry3D )
#include "testqgsgeometry3d.moc"
