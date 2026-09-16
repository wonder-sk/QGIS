/***************************************************************************
  qgsglobetilegeometry3d.cpp
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

#include "qgsglobetilegeometry3d.h"

#include <limits>
#include <vector>

#include "qgscoordinatetransform.h"

#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

///@cond PRIVATE

QgsGlobeTileGeometry3D::QgsGlobeTileGeometry3D( const QgsCoordinateTransform &globeCrsToLatLon, double lonMin, double lonMax, double latMin, double latMax, int lonSliceCount, int latSliceCount )
{
  Q_ASSERT( lonSliceCount >= 2 && latSliceCount >= 2 );

  const double lonStep = ( lonMax - lonMin ) / static_cast<double>( lonSliceCount - 1 );
  const double latStep = ( latMax - latMin ) / static_cast<double>( latSliceCount - 1 );

  const int pointCount = latSliceCount * lonSliceCount;
  Q_ASSERT( pointCount <= 65536 ); // the index buffer is 16 bit

  std::vector<double> x, y, z;
  x.reserve( pointCount );
  y.reserve( pointCount );
  z.reserve( pointCount );

  for ( int latSliceIndex = 0; latSliceIndex < latSliceCount; ++latSliceIndex )
  {
    const double lat = latSliceIndex * latStep + latMin;
    for ( int lonSliceIndex = 0; lonSliceIndex < lonSliceCount; ++lonSliceIndex )
    {
      x.push_back( lonSliceIndex * lonStep + lonMin );
      y.push_back( lat );
      z.push_back( 0 );
    }
  }

  globeCrsToLatLon.transformCoords( pointCount, x.data(), y.data(), z.data(), Qgis::TransformDirection::Reverse );

  // estimate origin of coordinates for this tile, to make the relative coordinates
  // small to avoid numerical precision issues when rendering
  // (avoids mesh jumping around when zoomed in very close to it)
  const QgsVector3D meshOriginLatLon( ( lonMin + lonMax ) / 2, ( latMin + latMax ) / 2, 0 );
  mOrigin = globeCrsToLatLon.transform( meshOriginLatLon, Qgis::TransformDirection::Reverse );

  QByteArray vertexData;
  vertexData.resize( stride() * pointCount );
  float *out = reinterpret_cast<float *>( vertexData.data() );

  float xMin = std::numeric_limits<float>::max(), yMin = xMin, zMin = xMin;
  float xMax = std::numeric_limits<float>::lowest(), yMax = xMax, zMax = xMax;

  for ( int i = 0; i < pointCount; ++i )
  {
    const float px = static_cast<float>( x[i] - mOrigin.x() );
    const float py = static_cast<float>( y[i] - mOrigin.y() );
    const float pz = static_cast<float>( z[i] - mOrigin.z() );

    *out++ = px;
    *out++ = py;
    *out++ = pz;

    const int vi = i / lonSliceCount;
    const int ui = i % lonSliceCount;
    *out++ = static_cast<float>( ui ) / static_cast<float>( lonSliceCount - 1 );
    *out++ = 1 - static_cast<float>( vi ) / static_cast<float>( latSliceCount - 1 );

    // on a globe the surface normal is the direction from the earth's centre, which is
    // the untranslated position rather than the one written above
    const QVector3D normal = QVector3D( static_cast<float>( x[i] ), static_cast<float>( y[i] ), static_cast<float>( z[i] ) ).normalized();
    *out++ = normal.x();
    *out++ = normal.y();
    *out++ = normal.z();

    xMin = std::min( xMin, px );
    yMin = std::min( yMin, py );
    zMin = std::min( zMin, pz );
    xMax = std::max( xMax, px );
    yMax = std::max( yMax, py );
    zMax = std::max( zMax, pz );
  }

  const int faces = ( lonSliceCount - 1 ) * ( latSliceCount - 1 ) * 2;

  QByteArray indexData;
  indexData.resize( faces * 3 * static_cast<int>( sizeof( quint16 ) ) );
  quint16 *indexPtr = reinterpret_cast<quint16 *>( indexData.data() );
  for ( int latSliceIndex = 0; latSliceIndex < latSliceCount - 1; ++latSliceIndex )
  {
    const int latSliceStartIndex = latSliceIndex * lonSliceCount;
    const int nextLatSliceStartIndex = lonSliceCount + latSliceStartIndex;
    for ( int lonSliceIndex = 0; lonSliceIndex < lonSliceCount - 1; ++lonSliceIndex )
    {
      indexPtr[0] = latSliceStartIndex + lonSliceIndex;
      indexPtr[1] = lonSliceIndex + latSliceStartIndex + 1;
      indexPtr[2] = nextLatSliceStartIndex + lonSliceIndex;

      indexPtr[3] = nextLatSliceStartIndex + lonSliceIndex;
      indexPtr[4] = lonSliceIndex + latSliceStartIndex + 1;
      indexPtr[5] = lonSliceIndex + nextLatSliceStartIndex + 1;

      indexPtr += 6;
    }
  }

  mVertexBuffer.setData( vertexData );
  mIndexBuffer.setData( indexData );

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

  mGeometry.setVertexCount( pointCount );
  mGeometry.setIndexBuffer( &mIndexBuffer, QRhiCommandBuffer::IndexUInt16 );
  mGeometry.setIndexCount( faces * 3 );
  mGeometry.setBoundingBox( QgsAABB( xMin, yMin, zMin, xMax, yMax, zMax ) );
}

void QgsGlobeTileGeometry3D::releaseGpuResources()
{
  mVertexBuffer.releaseGpuResources();
  mIndexBuffer.releaseGpuResources();
}

/// @endcond
