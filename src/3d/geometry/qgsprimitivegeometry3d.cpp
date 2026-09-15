/***************************************************************************
  qgsprimitivegeometry3d.cpp
  --------------------------------------
  Date                 : September 2026
  Copyright            : (C) 2026 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsprimitivegeometry3d.h"

#include <cmath>
#include <private/qtriangulator_p.h>

#include "qgslogger.h"

#include <QFont>
#include <QPainterPath>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

using namespace Qt::StringLiterals;

namespace
{

  /**
   * Collects vertices and triangles for a shape being built.
   *
   * Vertices are interleaved as position, normal, optional tangent, texture coordinate,
   * the same order QgsTessellator writes them in.
   */
  class MeshBuilder
  {
    public:
      explicit MeshBuilder( bool withTangents )
        : mWithTangents( withTangents )
        , mFloatsPerVertex( withTangents ? 12 : 8 )
      {}

      //! Reserves room for \a vertices vertices and \a indices indices
      void reserve( int vertices, int indices )
      {
        mVertices.reserve( vertices * mFloatsPerVertex );
        mIndices.reserve( indices );
      }

      //! Appends a vertex and returns its index
      int addVertex( const QVector3D &position, const QVector3D &normal, const QVector2D &texCoord, const QVector4D &tangent = QVector4D( 1.0f, 0.0f, 0.0f, 1.0f ) )
      {
        const int index = vertexCount();
        mVertices << position.x() << position.y() << position.z();
        mVertices << normal.x() << normal.y() << normal.z();
        if ( mWithTangents )
          mVertices << tangent.x() << tangent.y() << tangent.z() << tangent.w();
        mVertices << texCoord.x() << texCoord.y();
        return index;
      }

      void addTriangle( int a, int b, int c ) { mIndices << static_cast<quint32>( a ) << static_cast<quint32>( b ) << static_cast<quint32>( c ); }

      //! Adds the two triangles of a quad wound a, b, c, d
      void addQuad( int a, int b, int c, int d )
      {
        addTriangle( a, b, c );
        addTriangle( a, c, d );
      }

      int vertexCount() const { return mVertices.size() / mFloatsPerVertex; }
      int indexCount() const { return mIndices.size(); }
      int stride() const { return mFloatsPerVertex * static_cast<int>( sizeof( float ) ); }

      QByteArray vertexData() const { return QByteArray( reinterpret_cast<const char *>( mVertices.constData() ), mVertices.size() * static_cast<qsizetype>( sizeof( float ) ) ); }

      QByteArray indexData() const { return QByteArray( reinterpret_cast<const char *>( mIndices.constData() ), mIndices.size() * static_cast<qsizetype>( sizeof( quint32 ) ) ); }

    private:
      bool mWithTangents = false;
      int mFloatsPerVertex = 8;
      QVector<float> mVertices;
      QVector<quint32> mIndices;
  };

  constexpr float TWO_PI = 2.0f * static_cast<float>( M_PI );

  /**
   * Adds a flat disc of \a radius at height \a y, facing along \a normalY.
   *
   * Used for the end caps of cylinders and cones. A radius of zero adds nothing, which is
   * what makes a cone with a sharp point work without a special case.
   */
  void addEndcap( MeshBuilder &builder, float radius, float y, float normalY, int slices )
  {
    if ( radius <= 0.0f )
      return;

    const QVector3D normal( 0.0f, normalY, 0.0f );
    const int centre = builder.addVertex( QVector3D( 0.0f, y, 0.0f ), normal, QVector2D( 0.5f, 0.5f ) );

    for ( int slice = 0; slice <= slices; ++slice )
    {
      const float theta = static_cast<float>( slice ) * TWO_PI / static_cast<float>( slices );
      const float ct = std::cos( theta );
      const float st = std::sin( theta );
      builder.addVertex( QVector3D( radius * ct, y, radius * st ), normal, QVector2D( 0.5f + 0.5f * ct, 0.5f + 0.5f * st ) );
    }

    for ( int slice = 0; slice < slices; ++slice )
    {
      // wind the two caps opposite ways so that both face outwards
      if ( normalY > 0.0f )
        builder.addTriangle( centre, centre + slice + 1, centre + slice + 2 );
      else
        builder.addTriangle( centre, centre + slice + 2, centre + slice + 1 );
    }
  }

  /**
   * Adds the tapering side wall shared by cylinders and cones.
   *
   * A cylinder is just the case where both radii are equal, so there is one implementation.
   */
  void addTaperedSides( MeshBuilder &builder, float bottomRadius, float topRadius, float length, int rings, int slices )
  {
    const int ringCount = std::max( 2, rings );
    const float dY = length / static_cast<float>( ringCount - 1 );

    // how much the surface leans, which is what tilts the normal away from horizontal
    const float slope = ( bottomRadius - topRadius ) / length;

    const int firstVertex = builder.vertexCount();

    for ( int ring = 0; ring < ringCount; ++ring )
    {
      const float y = -length / 2.0f + static_cast<float>( ring ) * dY;
      const float t = ( y + length / 2.0f ) / length;
      const float radius = bottomRadius * ( 1.0f - t ) + topRadius * t;

      for ( int slice = 0; slice <= slices; ++slice )
      {
        const float theta = static_cast<float>( slice ) * TWO_PI / static_cast<float>( slices );
        const float ct = std::cos( theta );
        const float st = std::sin( theta );

        const QVector3D normal = QVector3D( ct, slope, st ).normalized();
        builder.addVertex( QVector3D( radius * ct, y, radius * st ), normal, QVector2D( theta / TWO_PI, t ), QVector4D( -st, 0.0f, ct, 1.0f ) );
      }
    }

    for ( int ring = 0; ring < ringCount - 1; ++ring )
    {
      const int thisRing = firstVertex + ring * ( slices + 1 );
      const int nextRing = firstVertex + ( ring + 1 ) * ( slices + 1 );

      for ( int slice = 0; slice < slices; ++slice )
      {
        builder.addQuad( thisRing + slice, nextRing + slice, nextRing + slice + 1, thisRing + slice + 1 );
      }
    }
  }

} // namespace


QgsPrimitiveGeometry3D::QgsPrimitiveGeometry3D() = default;

QgsPrimitiveGeometry3D::~QgsPrimitiveGeometry3D() = default;

void QgsPrimitiveGeometry3D::setData( const QByteArray &vertexData, int vertexCount, const QByteArray &indexData, int indexCount, bool withTangents )
{
  mHasTangents = withTangents;
  mStride = ( withTangents ? 12 : 8 ) * static_cast<int>( sizeof( float ) );

  int offset = 0;
  const auto addAttribute = [this, &offset]( const QString &name, QRhiVertexInputAttribute::Format format, int size ) {
    QgsVertexAttribute attribute;
    attribute.name = name;
    attribute.buffer = &mVertexBuffer;
    attribute.format = format;
    attribute.byteOffset = static_cast<quint32>( offset );
    attribute.byteStride = static_cast<quint32>( mStride );
    mGeometry.addAttribute( attribute );
    offset += size;
  };

  addAttribute( u"vertexPosition"_s, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ) );
  addAttribute( u"vertexNormal"_s, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ) );
  if ( withTangents )
    addAttribute( u"vertexTangent"_s, QRhiVertexInputAttribute::Float4, 4 * sizeof( float ) );
  addAttribute( u"vertexTexCoord"_s, QRhiVertexInputAttribute::Float2, 2 * sizeof( float ) );

  Q_ASSERT( offset == mStride );

  mVertexBuffer.setData( vertexData );
  mIndexBuffer.setData( indexData );
  mGeometry.setIndexBuffer( &mIndexBuffer, QRhiCommandBuffer::IndexUInt32 );
  mGeometry.setVertexCount( vertexCount );
  mGeometry.setIndexCount( indexCount );
}

void QgsPrimitiveGeometry3D::releaseGpuResources()
{
  mVertexBuffer.releaseGpuResources();
  mIndexBuffer.releaseGpuResources();
}

std::unique_ptr<QgsPrimitiveGeometry3D> QgsPrimitiveGeometry3D::sphere( float radius, int rings, int slices, bool withTangents )
{
  rings = std::max( 2, rings );
  slices = std::max( 3, slices );

  MeshBuilder builder( withTangents );
  builder.reserve( ( rings + 1 ) * ( slices + 1 ), rings * slices * 6 );

  // an extra column of vertices repeats the seam so that texture coordinates can run all
  // the way to 1 instead of wrapping back to 0
  for ( int ring = 0; ring <= rings; ++ring )
  {
    const float phi = static_cast<float>( M_PI_2 ) - static_cast<float>( ring ) * static_cast<float>( M_PI ) / static_cast<float>( rings );
    const float cosPhi = std::cos( phi );
    const float sinPhi = std::sin( phi );
    const float v = 1.0f - static_cast<float>( ring ) / static_cast<float>( rings );

    for ( int slice = 0; slice <= slices; ++slice )
    {
      const float theta = static_cast<float>( slice ) * TWO_PI / static_cast<float>( slices );
      const float cosTheta = std::cos( theta );
      const float sinTheta = std::sin( theta );

      const QVector3D normal( cosTheta * cosPhi, sinPhi, sinTheta * cosPhi );
      builder.addVertex( normal * radius, normal, QVector2D( static_cast<float>( slice ) / static_cast<float>( slices ), v ), QVector4D( sinTheta, 0.0f, -cosTheta, 1.0f ) );
    }
  }

  for ( int ring = 0; ring < rings; ++ring )
  {
    const int thisRing = ring * ( slices + 1 );
    const int nextRing = ( ring + 1 ) * ( slices + 1 );

    for ( int slice = 0; slice < slices; ++slice )
    {
      // the poles collapse to a point, so there only one triangle is needed rather than two
      if ( ring != 0 )
        builder.addTriangle( thisRing + slice, thisRing + slice + 1, nextRing + slice );
      if ( ring != rings - 1 )
        builder.addTriangle( nextRing + slice, thisRing + slice + 1, nextRing + slice + 1 );
    }
  }

  auto geometry = std::make_unique<QgsPrimitiveGeometry3D>();
  geometry->setData( builder.vertexData(), builder.vertexCount(), builder.indexData(), builder.indexCount(), withTangents );
  return geometry;
}

std::unique_ptr<QgsPrimitiveGeometry3D> QgsPrimitiveGeometry3D::cuboid( float xExtent, float yExtent, float zExtent, bool withTangents )
{
  const float x = xExtent / 2.0f;
  const float y = yExtent / 2.0f;
  const float z = zExtent / 2.0f;

  MeshBuilder builder( withTangents );
  builder.reserve( 24, 36 );

  // each face gets its own four vertices, because the normals and texture coordinates at a
  // shared corner differ per face
  struct Face
  {
      QVector3D normal;
      QVector3D corners[4];
      QVector4D tangent;
  };

  const Face faces[6] = {
    { QVector3D( 0, 0, 1 ), { { -x, -y, z }, { x, -y, z }, { x, y, z }, { -x, y, z } }, QVector4D( 1, 0, 0, 1 ) },
    { QVector3D( 0, 0, -1 ), { { x, -y, -z }, { -x, -y, -z }, { -x, y, -z }, { x, y, -z } }, QVector4D( -1, 0, 0, 1 ) },
    { QVector3D( 1, 0, 0 ), { { x, -y, z }, { x, -y, -z }, { x, y, -z }, { x, y, z } }, QVector4D( 0, 0, -1, 1 ) },
    { QVector3D( -1, 0, 0 ), { { -x, -y, -z }, { -x, -y, z }, { -x, y, z }, { -x, y, -z } }, QVector4D( 0, 0, 1, 1 ) },
    { QVector3D( 0, 1, 0 ), { { -x, y, z }, { x, y, z }, { x, y, -z }, { -x, y, -z } }, QVector4D( 1, 0, 0, 1 ) },
    { QVector3D( 0, -1, 0 ), { { -x, -y, -z }, { x, -y, -z }, { x, -y, z }, { -x, -y, z } }, QVector4D( 1, 0, 0, 1 ) },
  };

  const QVector2D texCoords[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };

  for ( const Face &face : faces )
  {
    const int first = builder.vertexCount();
    for ( int corner = 0; corner < 4; ++corner )
      builder.addVertex( face.corners[corner], face.normal, texCoords[corner], face.tangent );
    builder.addQuad( first, first + 1, first + 2, first + 3 );
  }

  auto geometry = std::make_unique<QgsPrimitiveGeometry3D>();
  geometry->setData( builder.vertexData(), builder.vertexCount(), builder.indexData(), builder.indexCount(), withTangents );
  return geometry;
}

std::unique_ptr<QgsPrimitiveGeometry3D> QgsPrimitiveGeometry3D::cylinder( float radius, float length, int rings, int slices, bool withTangents )
{
  return cone( radius, radius, length, true, true, rings, slices, withTangents );
}

std::unique_ptr<QgsPrimitiveGeometry3D> QgsPrimitiveGeometry3D::cone( float bottomRadius, float topRadius, float length, bool hasBottomEndcap, bool hasTopEndcap, int rings, int slices, bool withTangents )
{
  slices = std::max( 3, slices );

  MeshBuilder builder( withTangents );

  addTaperedSides( builder, bottomRadius, topRadius, length, rings, slices );

  if ( hasBottomEndcap )
    addEndcap( builder, bottomRadius, -length / 2.0f, -1.0f, slices );
  if ( hasTopEndcap )
    addEndcap( builder, topRadius, length / 2.0f, 1.0f, slices );

  auto geometry = std::make_unique<QgsPrimitiveGeometry3D>();
  geometry->setData( builder.vertexData(), builder.vertexCount(), builder.indexData(), builder.indexCount(), withTangents );
  return geometry;
}

std::unique_ptr<QgsPrimitiveGeometry3D> QgsPrimitiveGeometry3D::torus( float radius, float minorRadius, int rings, int slices, bool withTangents )
{
  rings = std::max( 3, rings );
  slices = std::max( 3, slices );

  MeshBuilder builder( withTangents );
  builder.reserve( ( rings + 1 ) * ( slices + 1 ), rings * slices * 6 );

  for ( int ring = 0; ring <= rings; ++ring )
  {
    const float u = static_cast<float>( ring ) * TWO_PI / static_cast<float>( rings );
    const float cu = std::cos( u );
    const float su = std::sin( u );

    for ( int slice = 0; slice <= slices; ++slice )
    {
      const float v = static_cast<float>( slice ) * TWO_PI / static_cast<float>( slices );
      const float cv = std::cos( v + static_cast<float>( M_PI ) );
      const float sv = std::sin( v );
      const float r = radius + minorRadius * cv;

      const QVector3D normal = QVector3D( cv * cu, cv * su, sv ).normalized();
      builder.addVertex( QVector3D( r * cu, r * su, minorRadius * sv ), normal, QVector2D( u / TWO_PI, v / TWO_PI ), QVector4D( -su, cu, 0.0f, 1.0f ) );
    }
  }

  for ( int ring = 0; ring < rings; ++ring )
  {
    const int thisRing = ring * ( slices + 1 );
    const int nextRing = ( ring + 1 ) * ( slices + 1 );

    for ( int slice = 0; slice < slices; ++slice )
      builder.addQuad( thisRing + slice, nextRing + slice, nextRing + slice + 1, thisRing + slice + 1 );
  }

  auto geometry = std::make_unique<QgsPrimitiveGeometry3D>();
  geometry->setData( builder.vertexData(), builder.vertexCount(), builder.indexData(), builder.indexCount(), withTangents );
  return geometry;
}

std::unique_ptr<QgsPrimitiveGeometry3D> QgsPrimitiveGeometry3D::plane( float width, float height, QSize resolution, bool withTangents )
{
  const int columns = std::max( 2, resolution.width() );
  const int rows = std::max( 2, resolution.height() );

  MeshBuilder builder( withTangents );
  builder.reserve( columns * rows, ( columns - 1 ) * ( rows - 1 ) * 6 );

  const QVector3D normal( 0.0f, 1.0f, 0.0f );
  const QVector4D tangent( 1.0f, 0.0f, 0.0f, 1.0f );

  for ( int row = 0; row < rows; ++row )
  {
    const float v = static_cast<float>( row ) / static_cast<float>( rows - 1 );
    const float z = -height / 2.0f + v * height;

    for ( int column = 0; column < columns; ++column )
    {
      const float u = static_cast<float>( column ) / static_cast<float>( columns - 1 );
      const float x = -width / 2.0f + u * width;
      builder.addVertex( QVector3D( x, 0.0f, z ), normal, QVector2D( u, v ), tangent );
    }
  }

  for ( int row = 0; row < rows - 1; ++row )
  {
    for ( int column = 0; column < columns - 1; ++column )
    {
      const int corner = row * columns + column;
      builder.addQuad( corner, corner + columns, corner + columns + 1, corner + 1 );
    }
  }

  auto geometry = std::make_unique<QgsPrimitiveGeometry3D>();
  geometry->setData( builder.vertexData(), builder.vertexCount(), builder.indexData(), builder.indexCount(), withTangents );
  return geometry;
}

std::unique_ptr<QgsPrimitiveGeometry3D> QgsPrimitiveGeometry3D::extrudedText( const QString &text, const QFont &font, float depth )
{
  auto geometry = std::make_unique<QgsPrimitiveGeometry3D>();

  // no tangents: nothing textures extruded text, and the glyph outlines give no sensible
  // tangent frame anyway
  MeshBuilder builder( false );

  if ( text.trimmed().isEmpty() )
  {
    geometry->setData( QByteArray(), 0, QByteArray(), 0, false );
    return geometry;
  }

  QPainterPath textPath;
  textPath.setFillRule( Qt::WindingFill );
  textPath.addText( 0, 0, font, text );

  // Qt lays text out with Y growing downwards, so the outlines are flipped to sit the right
  // way up in a Y-up world. The flip happens before triangulation so that the resulting
  // triangles wind consistently.
  const QList<QPolygonF> outlines = textPath.toSubpathPolygons( QTransform().scale( 1.0, -1.0 ) );
  if ( outlines.isEmpty() )
  {
    geometry->setData( QByteArray(), 0, QByteArray(), 0, false );
    return geometry;
  }

  QPainterPath flippedPath;
  flippedPath.setFillRule( Qt::WindingFill );
  for ( const QPolygonF &outline : outlines )
    flippedPath.addPolygon( outline );

  // the mesh is scaled down by the font size so that changing the size changes how finely
  // the curves are approximated rather than how large the text comes out
  const qreal fontSize = font.pointSizeF() > 0 ? font.pointSizeF() : ( font.pixelSize() > 0 ? font.pixelSize() : 1.0 );
  const float scale = static_cast<float>( 1.0 / fontSize );

  const QRectF bounds = flippedPath.boundingRect();
  const auto texCoordFor = [&bounds]( qreal x, qreal y ) {
    return QVector2D( bounds.width() > 0 ? static_cast<float>( ( x - bounds.left() ) / bounds.width() ) : 0.0f, bounds.height() > 0 ? static_cast<float>( ( y - bounds.top() ) / bounds.height() ) : 0.0f );
  };

  const QTriangleSet triangles = qTriangulate( flippedPath );
  const int capVertexCount = static_cast<int>( triangles.vertices.size() / 2 );

  const QVector3D frontNormal( 0.0f, 0.0f, -1.0f );
  const QVector3D backNormal( 0.0f, 0.0f, 1.0f );

  for ( int i = 0; i < capVertexCount; ++i )
  {
    const qreal x = triangles.vertices.at( i * 2 );
    const qreal y = triangles.vertices.at( i * 2 + 1 );
    builder.addVertex( QVector3D( static_cast<float>( x ) * scale, static_cast<float>( y ) * scale, 0.0f ), frontNormal, texCoordFor( x, y ) );
  }
  for ( int i = 0; i < capVertexCount; ++i )
  {
    const qreal x = triangles.vertices.at( i * 2 );
    const qreal y = triangles.vertices.at( i * 2 + 1 );
    builder.addVertex( QVector3D( static_cast<float>( x ) * scale, static_cast<float>( y ) * scale, depth ), backNormal, texCoordFor( x, y ) );
  }

  // the triangulator returns either 16 or 32 bit indices depending on how many vertices
  // it ended up with, so both have to be read
  const int capIndexCount = static_cast<int>( triangles.indices.size() );
  const bool wideIndices = triangles.indices.type() == QVertexIndexVector::UnsignedInt;
  const void *rawIndices = triangles.indices.data();
  const auto capIndexAt = [rawIndices, wideIndices]( int i ) {
    return wideIndices ? static_cast<int>( static_cast<const quint32 *>( rawIndices )[i] ) : static_cast<int>( static_cast<const quint16 *>( rawIndices )[i] );
  };

  for ( int i = 0; i + 2 < capIndexCount; i += 3 )
  {
    const int a = capIndexAt( i );
    const int b = capIndexAt( i + 1 );
    const int c = capIndexAt( i + 2 );

    builder.addTriangle( a, b, c );
    // the back face is the same triangle wound the other way, so that it faces outwards
    builder.addTriangle( c + capVertexCount, b + capVertexCount, a + capVertexCount );
  }

  // Side walls, built straight from the outlines rather than from the triangulated vertex
  // set. Each edge gets its own four vertices, so a corner can be sharp on one side and
  // smooth on the other without any vertex sharing.
  constexpr float SMOOTH_THRESHOLD = 0.9f; // roughly 25 degrees between adjacent edges

  for ( const QPolygonF &outline : outlines )
  {
    // a closed outline repeats its first point, which would make a zero length edge
    QPolygonF points = outline;
    if ( points.size() > 1 && points.first() == points.last() )
      points.removeLast();

    const int count = static_cast<int>( points.size() );
    if ( count < 3 )
      continue;

    // the outward normal of each edge, in the plane of the text
    QVector<QVector3D> edgeNormals( count );
    for ( int i = 0; i < count; ++i )
    {
      const QPointF &current = points.at( i );
      const QPointF &next = points.at( ( i + 1 ) % count );
      const QVector3D along( static_cast<float>( next.x() - current.x() ), static_cast<float>( next.y() - current.y() ), 0.0f );
      edgeNormals[i] = QVector3D( -along.y(), along.x(), 0.0f ).normalized();
    }

    for ( int i = 0; i < count; ++i )
    {
      const QPointF &current = points.at( i );
      const QPointF &next = points.at( ( i + 1 ) % count );

      const QVector3D &normal = edgeNormals.at( i );
      const QVector3D &previousNormal = edgeNormals.at( ( i + count - 1 ) % count );
      const QVector3D &nextNormal = edgeNormals.at( ( i + 1 ) % count );

      // a gentle turn means the two edges are part of a curve and should share a normal,
      // a sharp one means a corner that should stay crisp
      const QVector3D startNormal = QVector3D::dotProduct( previousNormal, normal ) > SMOOTH_THRESHOLD ? ( previousNormal + normal ).normalized() : normal;
      const QVector3D endNormal = QVector3D::dotProduct( nextNormal, normal ) > SMOOTH_THRESHOLD ? ( nextNormal + normal ).normalized() : normal;

      const QVector3D currentFront( static_cast<float>( current.x() ) * scale, static_cast<float>( current.y() ) * scale, 0.0f );
      const QVector3D nextFront( static_cast<float>( next.x() ) * scale, static_cast<float>( next.y() ) * scale, 0.0f );

      const int v0 = builder.addVertex( currentFront, startNormal, texCoordFor( current.x(), current.y() ) );
      const int v1 = builder.addVertex( QVector3D( currentFront.x(), currentFront.y(), depth ), startNormal, texCoordFor( current.x(), current.y() ) );
      const int v2 = builder.addVertex( nextFront, endNormal, texCoordFor( next.x(), next.y() ) );
      const int v3 = builder.addVertex( QVector3D( nextFront.x(), nextFront.y(), depth ), endNormal, texCoordFor( next.x(), next.y() ) );

      builder.addTriangle( v0, v1, v2 );
      builder.addTriangle( v2, v1, v3 );
    }
  }

  geometry->setData( builder.vertexData(), builder.vertexCount(), builder.indexData(), builder.indexCount(), false );
  return geometry;
}
