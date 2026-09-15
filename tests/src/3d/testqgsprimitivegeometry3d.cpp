/***************************************************************************
     testqgsprimitivegeometry3d.cpp
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

#include "qgsgeometry3d.h"
#include "qgsgraphicspipelinecache.h"
#include "qgsphongmaterial3d.h"
#include "qgsprimitivegeometry3d.h"
#include "qgsrendercommand.h"
#include "qgsrenderpass.h"
#include "qgsrenderuniforms.h"
#include "qgsrhiengine.h"
#include "qgsrhishader.h"
#include "qgstest.h"

#include <QFont>
#include <QImage>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

namespace
{
  //! One vertex read back out of a built shape
  struct Vertex
  {
      QVector3D position;
      QVector3D normal;
      QVector2D texCoord;
  };

  //! Reads the vertices back out of a shape, so that the generated mesh can be checked
  QVector<Vertex> readVertices( const QgsPrimitiveGeometry3D &shape, const QByteArray &vertexData )
  {
    const int floatsPerVertex = shape.stride() / int( sizeof( float ) );
    const float *data = reinterpret_cast<const float *>( vertexData.constData() );

    QVector<Vertex> vertices;
    for ( int i = 0; i < shape.vertexCount(); ++i )
    {
      const float *v = data + i * floatsPerVertex;
      // position, normal, then tangent when present, then texture coordinate
      const int texCoordOffset = shape.hasTangents() ? 10 : 6;
      vertices.append( { QVector3D( v[0], v[1], v[2] ), QVector3D( v[3], v[4], v[5] ), QVector2D( v[texCoordOffset], v[texCoordOffset + 1] ) } );
    }
    return vertices;
  }
} // namespace

/**
 * \ingroup UnitTests
 * Unit tests for the parametric shapes that replace the Qt3DExtras geometry generators.
 */
class TestQgsPrimitiveGeometry3D : public QgsTest
{
    Q_OBJECT
  public:
    TestQgsPrimitiveGeometry3D()
      : QgsTest( u"QgsPrimitiveGeometry3D tests"_s )
    {}

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void testSphere();
    void testSphereNormalsAreUnit();
    void testCuboid();
    void testCylinder();
    void testConeWithSharpPoint();
    void testConeWithoutEndcaps();
    void testTorus();
    void testPlane();
    void testTangents();
    void testExtrudedText();
    void testExtrudedTextEmpty();
    void testShapesRender();

  private:
    //! The vertex data a shape was built from, taken straight back out of its buffer
    static QByteArray vertexDataOf( const QgsPrimitiveGeometry3D &shape );

    //! Checks that every vertex sits within \a tolerance of the given axis-aligned box
    static bool withinBounds( const QVector<Vertex> &vertices, const QVector3D &halfExtents, float tolerance = 1e-4f );

    QRhi::Implementation mBackend = QRhi::Null;
};

QByteArray TestQgsPrimitiveGeometry3D::vertexDataOf( const QgsPrimitiveGeometry3D &shape )
{
  return shape.geometry()->attribute( u"vertexPosition"_s )->buffer->data();
}

bool TestQgsPrimitiveGeometry3D::withinBounds( const QVector<Vertex> &vertices, const QVector3D &halfExtents, float tolerance )
{
  for ( const Vertex &vertex : vertices )
  {
    if ( std::abs( vertex.position.x() ) > halfExtents.x() + tolerance || std::abs( vertex.position.y() ) > halfExtents.y() + tolerance || std::abs( vertex.position.z() ) > halfExtents.z() + tolerance )
      return false;
  }
  return true;
}

void TestQgsPrimitiveGeometry3D::initTestCase()
{
  mBackend = QgsRhiEngine::defaultBackend();
}

void TestQgsPrimitiveGeometry3D::cleanupTestCase()
{
  QgsRhiShader::clearCache();
  QgsApplication::exitQgis();
}

void TestQgsPrimitiveGeometry3D::testSphere()
{
  const std::unique_ptr<QgsPrimitiveGeometry3D> shape = QgsPrimitiveGeometry3D::sphere( 2.0f, 8, 12 );
  QVERIFY( shape );

  // one extra column repeats the seam so texture coordinates can reach 1
  QCOMPARE( shape->vertexCount(), ( 8 + 1 ) * ( 12 + 1 ) );
  QCOMPARE( shape->stride(), 8 * int( sizeof( float ) ) );
  QVERIFY( !shape->hasTangents() );

  // the poles need one triangle per slice, the rings in between need two
  QCOMPARE( shape->indexCount(), ( 12 * 2 + ( 8 - 2 ) * 12 * 2 ) * 3 );

  const QVector<Vertex> vertices = readVertices( *shape, vertexDataOf( *shape ) );
  QCOMPARE( vertices.size(), shape->vertexCount() );

  // every vertex is exactly one radius from the centre
  for ( const Vertex &vertex : vertices )
    QVERIFY2( std::abs( vertex.position.length() - 2.0f ) < 1e-4f, qPrintable( QString::number( vertex.position.length() ) ) );

  // the first vertex is the north pole and the last the south pole
  QVERIFY( std::abs( vertices.first().position.y() - 2.0f ) < 1e-4f );
  QVERIFY( std::abs( vertices.last().position.y() + 2.0f ) < 1e-4f );

  // texture coordinates span the full range
  QCOMPARE( vertices.first().texCoord.y(), 1.0f );
  QCOMPARE( vertices.last().texCoord.y(), 0.0f );
}

void TestQgsPrimitiveGeometry3D::testSphereNormalsAreUnit()
{
  const std::unique_ptr<QgsPrimitiveGeometry3D> shape = QgsPrimitiveGeometry3D::sphere( 5.0f, 16, 16 );
  const QVector<Vertex> vertices = readVertices( *shape, vertexDataOf( *shape ) );

  for ( const Vertex &vertex : vertices )
  {
    QVERIFY( std::abs( vertex.normal.length() - 1.0f ) < 1e-4f );
    // on a sphere the normal points straight out from the centre
    QVERIFY( QVector3D::dotProduct( vertex.normal, vertex.position.normalized() ) > 0.999f );
  }
}

void TestQgsPrimitiveGeometry3D::testCuboid()
{
  const std::unique_ptr<QgsPrimitiveGeometry3D> shape = QgsPrimitiveGeometry3D::cuboid( 2.0f, 4.0f, 6.0f );

  // six faces of four vertices, because a shared corner has a different normal per face
  QCOMPARE( shape->vertexCount(), 24 );
  QCOMPARE( shape->indexCount(), 36 );

  const QVector<Vertex> vertices = readVertices( *shape, vertexDataOf( *shape ) );
  QVERIFY( withinBounds( vertices, QVector3D( 1.0f, 2.0f, 3.0f ) ) );

  // every vertex sits on the surface, so each coordinate is at an extreme of its axis
  for ( const Vertex &vertex : vertices )
  {
    QVERIFY( std::abs( std::abs( vertex.position.x() ) - 1.0f ) < 1e-4f );
    QVERIFY( std::abs( std::abs( vertex.position.y() ) - 2.0f ) < 1e-4f );
    QVERIFY( std::abs( std::abs( vertex.position.z() ) - 3.0f ) < 1e-4f );
  }

  // the six face normals are the six axis directions, four vertices each
  QMap<QString, int> normalCounts;
  for ( const Vertex &vertex : vertices )
    normalCounts[u"%1,%2,%3"_s.arg( vertex.normal.x() ).arg( vertex.normal.y() ).arg( vertex.normal.z() )]++;

  QCOMPARE( normalCounts.size(), 6 );
  for ( int count : normalCounts )
    QCOMPARE( count, 4 );
}

void TestQgsPrimitiveGeometry3D::testCylinder()
{
  const std::unique_ptr<QgsPrimitiveGeometry3D> shape = QgsPrimitiveGeometry3D::cylinder( 3.0f, 10.0f, 2, 8 );

  const QVector<Vertex> vertices = readVertices( *shape, vertexDataOf( *shape ) );
  QVERIFY( withinBounds( vertices, QVector3D( 3.0f, 5.0f, 3.0f ) ) );

  int topCapVertices = 0;
  int bottomCapVertices = 0;
  for ( const Vertex &vertex : vertices )
  {
    // a cylinder's side normals are horizontal, its cap normals vertical
    if ( std::abs( vertex.normal.y() ) > 0.999f )
    {
      if ( vertex.normal.y() > 0 )
        ++topCapVertices;
      else
        ++bottomCapVertices;
    }
    else
    {
      QVERIFY2( std::abs( vertex.normal.y() ) < 1e-4f, "a cylinder's side normals should be horizontal" );
      const float radius = QVector2D( vertex.position.x(), vertex.position.z() ).length();
      QVERIFY( std::abs( radius - 3.0f ) < 1e-4f );
    }
  }

  // each cap is a fan: a centre plus one vertex per slice, plus the repeated seam
  QCOMPARE( topCapVertices, 8 + 2 );
  QCOMPARE( bottomCapVertices, 8 + 2 );
}

void TestQgsPrimitiveGeometry3D::testConeWithSharpPoint()
{
  // a top radius of zero is the ordinary cone, and must not produce a degenerate top cap
  const std::unique_ptr<QgsPrimitiveGeometry3D> shape = QgsPrimitiveGeometry3D::cone( 2.0f, 0.0f, 6.0f, true, true, 2, 8 );

  const QVector<Vertex> vertices = readVertices( *shape, vertexDataOf( *shape ) );
  QVERIFY( withinBounds( vertices, QVector3D( 2.0f, 3.0f, 2.0f ) ) );

  int upwardVertices = 0;
  for ( const Vertex &vertex : vertices )
  {
    QVERIFY( std::abs( vertex.normal.length() - 1.0f ) < 1e-3f );
    if ( vertex.normal.y() > 0.999f )
      ++upwardVertices;
  }

  // the top cap has no area, so nothing was emitted for it
  QCOMPARE( upwardVertices, 0 );

  // the tip is a ring of coincident vertices at the top
  int tipVertices = 0;
  for ( const Vertex &vertex : vertices )
  {
    const float radius = QVector2D( vertex.position.x(), vertex.position.z() ).length();
    if ( std::abs( vertex.position.y() - 3.0f ) < 1e-4f && radius < 1e-4f )
      ++tipVertices;
  }
  QVERIFY( tipVertices > 0 );
}

void TestQgsPrimitiveGeometry3D::testConeWithoutEndcaps()
{
  const std::unique_ptr<QgsPrimitiveGeometry3D> open = QgsPrimitiveGeometry3D::cone( 2.0f, 1.0f, 4.0f, false, false, 2, 8 );
  const std::unique_ptr<QgsPrimitiveGeometry3D> closed = QgsPrimitiveGeometry3D::cone( 2.0f, 1.0f, 4.0f, true, true, 2, 8 );

  QVERIFY( open->vertexCount() < closed->vertexCount() );
  QVERIFY( open->indexCount() < closed->indexCount() );

  // an open cone is nothing but its side wall
  const QVector<Vertex> vertices = readVertices( *open, vertexDataOf( *open ) );
  for ( const Vertex &vertex : vertices )
    QVERIFY2( std::abs( vertex.normal.y() ) < 0.999f, "an open cone should have no cap normals" );
}

void TestQgsPrimitiveGeometry3D::testTorus()
{
  const std::unique_ptr<QgsPrimitiveGeometry3D> shape = QgsPrimitiveGeometry3D::torus( 4.0f, 1.0f, 10, 6 );

  QCOMPARE( shape->vertexCount(), ( 10 + 1 ) * ( 6 + 1 ) );
  QCOMPARE( shape->indexCount(), 10 * 6 * 6 );

  const QVector<Vertex> vertices = readVertices( *shape, vertexDataOf( *shape ) );

  // every point on a torus is exactly the minor radius from the ring running through it
  for ( const Vertex &vertex : vertices )
  {
    const QVector2D inPlane( vertex.position.x(), vertex.position.y() );
    const float distanceFromRing = QVector2D( inPlane.length() - 4.0f, vertex.position.z() ).length();
    QVERIFY2( std::abs( distanceFromRing - 1.0f ) < 1e-4f, qPrintable( QString::number( distanceFromRing ) ) );
    QVERIFY( std::abs( vertex.normal.length() - 1.0f ) < 1e-4f );
  }
}

void TestQgsPrimitiveGeometry3D::testPlane()
{
  const std::unique_ptr<QgsPrimitiveGeometry3D> shape = QgsPrimitiveGeometry3D::plane( 4.0f, 6.0f, QSize( 3, 5 ) );

  QCOMPARE( shape->vertexCount(), 3 * 5 );
  QCOMPARE( shape->indexCount(), 2 * 4 * 6 );

  const QVector<Vertex> vertices = readVertices( *shape, vertexDataOf( *shape ) );
  for ( const Vertex &vertex : vertices )
  {
    // flat in the XZ plane, facing up
    QCOMPARE( vertex.position.y(), 0.0f );
    QCOMPARE( vertex.normal, QVector3D( 0.0f, 1.0f, 0.0f ) );
    QVERIFY( std::abs( vertex.position.x() ) <= 2.0f + 1e-4f );
    QVERIFY( std::abs( vertex.position.z() ) <= 3.0f + 1e-4f );
  }

  // a resolution below two vertices per side cannot make a quad, so it is clamped
  const std::unique_ptr<QgsPrimitiveGeometry3D> degenerate = QgsPrimitiveGeometry3D::plane( 1.0f, 1.0f, QSize( 1, 1 ) );
  QCOMPARE( degenerate->vertexCount(), 4 );
  QCOMPARE( degenerate->indexCount(), 6 );
}

void TestQgsPrimitiveGeometry3D::testTangents()
{
  const std::unique_ptr<QgsPrimitiveGeometry3D> without = QgsPrimitiveGeometry3D::sphere( 1.0f, 8, 8, false );
  const std::unique_ptr<QgsPrimitiveGeometry3D> with = QgsPrimitiveGeometry3D::sphere( 1.0f, 8, 8, true );

  QVERIFY( !without->hasTangents() );
  QVERIFY( with->hasTangents() );
  QCOMPARE( without->stride(), 8 * int( sizeof( float ) ) );
  QCOMPARE( with->stride(), 12 * int( sizeof( float ) ) );

  // the same mesh either way, just with four more floats per vertex
  QCOMPARE( with->vertexCount(), without->vertexCount() );
  QCOMPARE( with->indexCount(), without->indexCount() );

  QVERIFY( with->geometry()->attribute( u"vertexTangent"_s ) );
  QVERIFY( !without->geometry()->attribute( u"vertexTangent"_s ) );

  // the texture coordinate moves along to make room for the tangent
  QCOMPARE( without->geometry()->attribute( u"vertexTexCoord"_s )->byteOffset, 6 * sizeof( float ) );
  QCOMPARE( with->geometry()->attribute( u"vertexTexCoord"_s )->byteOffset, 10 * sizeof( float ) );
}

void TestQgsPrimitiveGeometry3D::testExtrudedText()
{
  QFont font( u"Arial"_s );
  font.setPointSize( 10 );

  const std::unique_ptr<QgsPrimitiveGeometry3D> shape = QgsPrimitiveGeometry3D::extrudedText( u"QGIS"_s, font, 0.5f );
  QVERIFY( shape );
  QVERIFY2( shape->vertexCount() > 0, "no geometry was produced for the text" );
  QVERIFY( shape->indexCount() > 0 );
  QCOMPARE( shape->indexCount() % 3, 0 );
  QVERIFY( !shape->hasTangents() );

  const QVector<Vertex> vertices = readVertices( *shape, vertexDataOf( *shape ) );

  int frontFaceVertices = 0;
  int backFaceVertices = 0;
  for ( const Vertex &vertex : vertices )
  {
    // the text is extruded from z = 0 to z = depth, and nothing lies outside that
    QVERIFY2( vertex.position.z() >= -1e-4f && vertex.position.z() <= 0.5f + 1e-4f, qPrintable( QString::number( vertex.position.z() ) ) );
    QVERIFY( std::abs( vertex.normal.length() - 1.0f ) < 1e-3f );

    if ( vertex.normal.z() < -0.999f )
      ++frontFaceVertices;
    else if ( vertex.normal.z() > 0.999f )
      ++backFaceVertices;
  }

  QVERIFY2( frontFaceVertices > 0, "no front face" );
  QCOMPARE( frontFaceVertices, backFaceVertices );

  // the side walls are what make it solid rather than two flat sheets
  QVERIFY2( vertices.size() > frontFaceVertices + backFaceVertices, "no side walls" );

  // scaling by the font size means a larger font gives the same size text, just smoother
  QFont bigger = font;
  bigger.setPointSize( 40 );
  const std::unique_ptr<QgsPrimitiveGeometry3D> biggerShape = QgsPrimitiveGeometry3D::extrudedText( u"QGIS"_s, bigger, 0.5f );
  const QVector<Vertex> biggerVertices = readVertices( *biggerShape, vertexDataOf( *biggerShape ) );

  const auto widthOf = []( const QVector<Vertex> &v ) {
    float minimum = std::numeric_limits<float>::max();
    float maximum = std::numeric_limits<float>::lowest();
    for ( const Vertex &vertex : v )
    {
      minimum = std::min( minimum, vertex.position.x() );
      maximum = std::max( maximum, vertex.position.x() );
    }
    return maximum - minimum;
  };

  QVERIFY2( std::abs( widthOf( vertices ) - widthOf( biggerVertices ) ) < 0.2f, qPrintable( u"%1 vs %2"_s.arg( widthOf( vertices ) ).arg( widthOf( biggerVertices ) ) ) );
}

void TestQgsPrimitiveGeometry3D::testExtrudedTextEmpty()
{
  const QFont font( u"Arial"_s );

  for ( const QString &text : { QString(), u" "_s, u"\t "_s } )
  {
    const std::unique_ptr<QgsPrimitiveGeometry3D> shape = QgsPrimitiveGeometry3D::extrudedText( text, font, 1.0f );
    QVERIFY( shape );
    QCOMPARE( shape->vertexCount(), 0 );
    QCOMPARE( shape->indexCount(), 0 );
  }
}

void TestQgsPrimitiveGeometry3D::testShapesRender()
{
  if ( mBackend == QRhi::Null )
    QSKIP( "The Null backend draws no pixels" );

  QgsRhiEngine engine;
  if ( !engine.createOffscreen( QSize( 64, 64 ), mBackend ) )
    QSKIP( "No usable QRhi device for the default backend" );

  QgsGraphicsPipelineCache cache;

  // every shape must actually draw through the engine, not just produce plausible numbers
  struct Shape
  {
      QString name;
      std::unique_ptr<QgsPrimitiveGeometry3D> geometry;
  };

  std::vector<Shape> shapes;
  shapes.push_back( { u"sphere"_s, QgsPrimitiveGeometry3D::sphere( 3.0f ) } );
  shapes.push_back( { u"cuboid"_s, QgsPrimitiveGeometry3D::cuboid( 4.0f, 4.0f, 4.0f ) } );
  shapes.push_back( { u"cylinder"_s, QgsPrimitiveGeometry3D::cylinder( 2.0f, 5.0f ) } );
  shapes.push_back( { u"cone"_s, QgsPrimitiveGeometry3D::cone( 3.0f, 0.0f, 5.0f ) } );
  shapes.push_back( { u"torus"_s, QgsPrimitiveGeometry3D::torus( 3.0f, 1.0f ) } );
  shapes.push_back( { u"plane"_s, QgsPrimitiveGeometry3D::plane( 6.0f, 6.0f ) } );

  QgsPhongMaterial3D material;
  material.setCullMode( QRhiGraphicsPipeline::None );
  material.setAmbient( QColor( 255, 0, 0 ) );
  material.setDiffuse( Qt::black );

  for ( const Shape &shape : shapes )
  {
    QgsRenderPass pass( shape.name );

    // look down the Z axis at whatever the shape is, from far enough out to see all of it
    QMatrix4x4 view;
    view.lookAt( QVector3D( 0.0f, 0.0f, 20.0f ), QVector3D(), QVector3D( 0.0f, 1.0f, 0.0f ) );
    QMatrix4x4 projection;
    projection.perspective( 45.0f, 1.0f, 1.0f, 100.0f );
    pass.setCamera( view, projection );
    pass.setClearColor( Qt::black );

    QgsRenderCommand command;
    command.geometry = shape.geometry->geometry();
    command.material = &material;

    // a plane lying flat is edge on to this camera, so tip it towards us
    if ( shape.name == "plane"_L1 )
      command.worldTransform.rotate( 90.0f, 1.0f, 0.0f, 0.0f );

    pass.addCommand( command );

    engine.setReadbackEnabled( true );
    QVERIFY2( engine.beginFrame(), qPrintable( shape.name ) );
    QVERIFY2( pass.execute( &engine, &cache ), qPrintable( shape.name ) );
    engine.endFrame();

    const QImage image = engine.readbackImage();
    int litPixels = 0;
    for ( int y = 0; y < image.height(); ++y )
    {
      for ( int x = 0; x < image.width(); ++x )
      {
        if ( image.pixelColor( x, y ).red() > 200 )
          ++litPixels;
      }
    }
    QVERIFY2( litPixels > 50, qPrintable( u"%1 drew only %2 pixels"_s.arg( shape.name ).arg( litPixels ) ) );

    pass.releaseGpuResources();
    shape.geometry->releaseGpuResources();
  }

  material.releaseGpuResources();
  cache.clear();
}

QGSTEST_MAIN( TestQgsPrimitiveGeometry3D )
#include "testqgsprimitivegeometry3d.moc"
