/***************************************************************************
  qgswiredmeshgeometry3d.cpp
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

#include "qgswiredmeshgeometry3d.h"

#include <limits>

#include "qgsaabb.h"

#include <QString>

using namespace Qt::StringLiterals;

QgsWiredMeshGeometry3D::QgsWiredMeshGeometry3D()
{
  QgsVertexAttribute position;
  position.name = u"vertexPosition"_s;
  position.buffer = &mVertexBuffer;
  position.format = QRhiVertexInputAttribute::Float3;
  position.byteStride = 3 * sizeof( float );
  mGeometry.addAttribute( position );
}

void QgsWiredMeshGeometry3D::setVertices( const QList<QVector3D> &vertices )
{
  QByteArray data;
  data.resize( vertices.size() * 3 * sizeof( float ) );
  float *out = reinterpret_cast<float *>( data.data() );

  float xMin = std::numeric_limits<float>::max(), yMin = xMin, zMin = xMin;
  float xMax = std::numeric_limits<float>::lowest(), yMax = xMax, zMax = xMax;

  for ( const QVector3D &vertex : vertices )
  {
    *out++ = vertex.x();
    *out++ = vertex.y();
    *out++ = vertex.z();

    xMin = std::min( xMin, vertex.x() );
    yMin = std::min( yMin, vertex.y() );
    zMin = std::min( zMin, vertex.z() );
    xMax = std::max( xMax, vertex.x() );
    yMax = std::max( yMax, vertex.y() );
    zMax = std::max( zMax, vertex.z() );
  }

  mVertexBuffer.setData( data );
  mGeometry.setVertexCount( static_cast<int>( vertices.size() ) );
  mGeometry.setBoundingBox( vertices.isEmpty() ? QgsAABB() : QgsAABB( xMin, yMin, zMin, xMax, yMax, zMax ) );
}

void QgsWiredMeshGeometry3D::setVertices( const QList<QgsAABB> &boxes )
{
  QList<QVector3D> vertices;
  for ( const QgsAABB &box : boxes )
    vertices << box.verticesForLines();

  setVertices( vertices );
}

void QgsWiredMeshGeometry3D::releaseGpuResources()
{
  mVertexBuffer.releaseGpuResources();
}
