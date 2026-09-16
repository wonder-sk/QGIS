/***************************************************************************
  qgsdemterraintilegeometry3d.cpp
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

#include "qgsdemterraintilegeometry3d.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "qgsray3d.h"
#include "qgsraycastcontext.h"
#include "qgsraycastingutils.h"

#include <QMatrix4x4>
#include <QSize>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

///@cond PRIVATE

QByteArray QgsDemTerrainTileGeometry3D::createVertexData( int res, float side, float vertScale, float skirtHeight, const QByteArray &heights )
{
  Q_ASSERT( res >= 2 );
  Q_ASSERT( heights.count() == res * res * static_cast<int>( sizeof( float ) ) );

  const float *zData = reinterpret_cast<const float *>( heights.constData() );
  const float *zBits = zData;

  const int nVerts = ( res + 2 ) * ( res + 2 );

  // Populate a buffer with the interleaved per-vertex data with
  // vec3 pos, vec2 texCoord, vec3 normal
  QByteArray bufferBytes;
  bufferBytes.resize( stride() * nVerts );
  float *fptr = reinterpret_cast<float *>( bufferBytes.data() );

  const QSize resolution( res, res );
  const float x0 = 0;
  const float y0 = side;
  const float dx = side / static_cast<float>( resolution.width() - 1 );
  const float dy = side / static_cast<float>( resolution.height() - 1 );
  const float du = 1.0f / static_cast<float>( resolution.width() - 1 );
  const float dv = 1.0f / static_cast<float>( resolution.height() - 1 );

  // the height of vertices with no-data value... the value should not really matter
  // as we do not create valid triangles that would use such vertices
  const float noDataHeight = 0;

  const int iMax = resolution.width() - 1;
  const int jMax = resolution.height() - 1;

  // Iterate over y
  for ( int j = -1; j <= resolution.height(); ++j )
  {
    const int jBound = std::clamp( j, 0, jMax );
    const float y = y0 - static_cast<float>( jBound ) * dy;
    const float v = static_cast<float>( jBound ) * dv;

    // Iterate over x
    for ( int i = -1; i <= resolution.width(); ++i )
    {
      const int iBound = std::clamp( i, 0, iMax );
      const float x = x0 + static_cast<float>( iBound ) * dx;
      const float u = static_cast<float>( iBound ) * du;

      float height;
      if ( i == iBound && j == jBound )
        height = *zBits++;
      else
        height = zData[jBound * resolution.width() + iBound] - skirtHeight;

      if ( std::isnan( height ) )
        height = noDataHeight;

      // position
      *fptr++ = x;
      *fptr++ = y;
      *fptr++ = height * vertScale;

      // texture coordinates
      *fptr++ = u;
      *fptr++ = v;

      // calculate normal coordinates
#define zAt( ii, jj ) zData[jj * resolution.width() + ii] * vertScale
      const float zi0 = zAt( std::clamp( i - 1, 0, iMax ), jBound );
      const float zi1 = zAt( std::clamp( i + 1, 0, iMax ), jBound );
      const float zj0 = zAt( iBound, std::clamp( j - 1, 0, jMax ) );
      const float zj1 = zAt( iBound, std::clamp( j + 1, 0, jMax ) );
#undef zAt

      QVector3D n;
      if ( std::isnan( zi0 ) || std::isnan( zi1 ) || std::isnan( zj0 ) || std::isnan( zj1 ) )
        n = QVector3D( 0, 0, 1 );
      else
      {
        float di, dj;
        const float zij = height * vertScale;

        if ( i == 0 )
          di = 2 * ( zij - zi1 );
        else if ( i == iMax )
          di = 2 * ( zi0 - zij );
        else
          di = zi0 - zi1;

        if ( j == 0 )
          dj = 2 * ( zij - zj1 );
        else if ( j == jMax )
          dj = 2 * ( zj0 - zij );
        else
          dj = zj0 - zj1;

        n = QVector3D( di, -dj, 2 * side / static_cast<float>( res ) );
        n.normalize();
      }

      *fptr++ = n.x();
      *fptr++ = n.y();
      *fptr++ = n.z();
    }
  }

  return bufferBytes;
}

static inline int ijToHeightMapIndex( int i, int j, int resX, int resZ )
{
  i = std::clamp( i, 1, resX ) - 1;
  j = std::clamp( j, 1, resZ ) - 1;
  return j * resX + i;
}

static bool hasNoData( int i, int j, const float *heightMap, int resX, int resZ )
{
  return std::isnan( heightMap[ijToHeightMapIndex( i, j, resX, resZ )] )
         || std::isnan( heightMap[ijToHeightMapIndex( i + 1, j, resX, resZ )] )
         || std::isnan( heightMap[ijToHeightMapIndex( i, j + 1, resX, resZ )] )
         || std::isnan( heightMap[ijToHeightMapIndex( i + 1, j + 1, resX, resZ )] );
}

QByteArray QgsDemTerrainTileGeometry3D::createIndexData( int res, const QByteArray &heightMap )
{
  const QSize resolution( res, res );
  const int numVerticesX = resolution.width() + 2;
  const int numVerticesZ = resolution.height() + 2;

  // Create the index data. 2 triangles per rectangular face
  const int faces = 2 * ( numVerticesX - 1 ) * ( numVerticesZ - 1 );
  const quint32 indices = 3 * faces;
  Q_ASSERT( indices < std::numeric_limits<quint32>::max() );
  QByteArray indexBytes;
  indexBytes.resize( indices * sizeof( quint32 ) );
  quint32 *indexPtr = reinterpret_cast<quint32 *>( indexBytes.data() );

  const float *heightMapFloat = reinterpret_cast<const float *>( heightMap.constData() );

  // Iterate over z
  for ( int j = 0; j < numVerticesZ - 1; ++j )
  {
    const int rowStartIndex = j * numVerticesX;
    const int nextRowStartIndex = ( j + 1 ) * numVerticesX;

    // Iterate over x
    for ( int i = 0; i < numVerticesX - 1; ++i )
    {
      if ( hasNoData( i, j, heightMapFloat, res, res ) )
      {
        // at least one corner of the quad has no-data value
        // so let's make two invalid triangles
        *indexPtr++ = rowStartIndex + i;
        *indexPtr++ = rowStartIndex + i;
        *indexPtr++ = rowStartIndex + i;

        *indexPtr++ = rowStartIndex + i;
        *indexPtr++ = rowStartIndex + i;
        *indexPtr++ = rowStartIndex + i;
        continue;
      }

      // Split quad into two triangles
      *indexPtr++ = rowStartIndex + i;
      *indexPtr++ = nextRowStartIndex + i;
      *indexPtr++ = rowStartIndex + i + 1;

      *indexPtr++ = nextRowStartIndex + i;
      *indexPtr++ = nextRowStartIndex + i + 1;
      *indexPtr++ = rowStartIndex + i + 1;
    }
  }

  return indexBytes;
}

bool QgsDemTerrainTileGeometry3D::rayIntersection(
  const QByteArray &vertexData, const QByteArray &indexData, const QgsRay3D &ray, const QgsRayCastContext &context, const QMatrix4x4 &worldTransform, QVector3D &intersectionPoint
)
{
  // WARNING! this code is specific to how vertex buffers are built for DEM tiles,
  // it is not usable for any mesh...

  constexpr int floatsPerVertex = stride() / sizeof( float );

  const float *vertices = reinterpret_cast<const float *>( vertexData.constData() );
  const uint *indices = reinterpret_cast<const uint *>( indexData.constData() );
#ifdef QGISDEBUG
  const int vertexCnt = vertexData.count() / sizeof( float );
  Q_ASSERT( vertexCnt % floatsPerVertex == 0 );
#endif
  const int indexCnt = indexData.count() / sizeof( uint );
  Q_ASSERT( indexCnt % 3 == 0 );
  const int triangleCount = indexCnt / 3;

  QVector3D minIntersectionPt;
  float minDistance = -1;

  for ( int i = 0; i < triangleCount; ++i )
  {
    const int v0 = indices[i * 3], v1 = indices[i * 3 + 1], v2 = indices[i * 3 + 2];
    const QVector3D a( vertices[v0 * floatsPerVertex], vertices[v0 * floatsPerVertex + 1], vertices[v0 * floatsPerVertex + 2] );
    const QVector3D b( vertices[v1 * floatsPerVertex], vertices[v1 * floatsPerVertex + 1], vertices[v1 * floatsPerVertex + 2] );
    const QVector3D c( vertices[v2 * floatsPerVertex], vertices[v2 * floatsPerVertex + 1], vertices[v2 * floatsPerVertex + 2] );

    const QVector3D tA = worldTransform * a;
    const QVector3D tB = worldTransform * b;
    const QVector3D tC = worldTransform * c;

    QVector3D uvw;
    float t = 0;
    if ( QgsRayCastingUtils::rayTriangleIntersection( ray, context.maximumDistance(), tA, tB, tC, uvw, t ) )
    {
      const QVector3D intersectionPt = ray.point( t * context.maximumDistance() );
      const float distance = ray.projectedDistance( intersectionPt );

      // we only want the first intersection of the ray with the mesh (closest to the ray origin)
      if ( minDistance == -1 || distance < minDistance )
      {
        minDistance = distance;
        minIntersectionPt = intersectionPt;
      }
    }
  }

  if ( minDistance == -1 )
    return false;

  intersectionPoint = minIntersectionPt;
  return true;
}

QgsDemTerrainTileGeometry3D::QgsDemTerrainTileGeometry3D( int resolution, float side, float vertScale, float skirtHeight, const QByteArray &heightMap )
{
  const int nVertsPerSide = resolution + 2;
  const int nVerts = nVertsPerSide * nVertsPerSide;
  const int faces = 2 * ( nVertsPerSide - 1 ) * ( nVertsPerSide - 1 );

  const QByteArray vertexData = createVertexData( resolution, side, vertScale, skirtHeight, heightMap );
  mVertexBuffer.setData( vertexData );
  mIndexBuffer.setData( createIndexData( resolution, heightMap ) );

  // the interleave order is the one createVertexData() writes: position, texture
  // coordinate, normal. Attributes bind by name, so the order does not have to match
  // what a shader declares.
  const auto addAttribute = [this]( const QString &name, QRhiVertexInputAttribute::Format format, int offset ) {
    QgsVertexAttribute attribute;
    attribute.name = name;
    attribute.buffer = &mVertexBuffer;
    attribute.format = format;
    attribute.byteOffset = static_cast<quint32>( offset );
    attribute.byteStride = static_cast<quint32>( stride() );
    mGeometry.addAttribute( attribute );
  };

  addAttribute( u"vertexPosition"_s, QRhiVertexInputAttribute::Float3, 0 );
  addAttribute( u"vertexTexCoord"_s, QRhiVertexInputAttribute::Float2, 3 * sizeof( float ) );
  addAttribute( u"vertexNormal"_s, QRhiVertexInputAttribute::Float3, 5 * sizeof( float ) );

  mGeometry.setVertexCount( nVerts );
  mGeometry.setIndexBuffer( &mIndexBuffer, QRhiCommandBuffer::IndexUInt32 );
  mGeometry.setIndexCount( faces * 3 );

  // Qt3D derived the bounding volume from the position attribute. The tile is a regular
  // grid in X and Y, so only the height range has to be scanned for.
  float zMin = std::numeric_limits<float>::max();
  float zMax = std::numeric_limits<float>::lowest();
  const float *vertex = reinterpret_cast<const float *>( vertexData.constData() );
  for ( int i = 0; i < nVerts; ++i, vertex += stride() / sizeof( float ) )
  {
    zMin = std::min( zMin, vertex[2] );
    zMax = std::max( zMax, vertex[2] );
  }
  mGeometry.setBoundingBox( QgsAABB( 0, 0, zMin, side, side, zMax ) );
}

bool QgsDemTerrainTileGeometry3D::rayIntersection( const QgsRay3D &ray, const QgsRayCastContext &context, const QMatrix4x4 &worldTransform, QVector3D &intersectionPoint ) const
{
  return rayIntersection( mVertexBuffer.data(), mIndexBuffer.data(), ray, context, worldTransform, intersectionPoint );
}

void QgsDemTerrainTileGeometry3D::releaseGpuResources()
{
  mVertexBuffer.releaseGpuResources();
  mIndexBuffer.releaseGpuResources();
}

/// @endcond
