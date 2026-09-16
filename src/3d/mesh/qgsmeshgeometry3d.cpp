/***************************************************************************
  qgsmeshgeometry3d.cpp
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

#include "qgsmeshgeometry3d.h"

#include <QString>

using namespace Qt::StringLiterals;

///@cond PRIVATE

QgsMeshGeometry3D::QgsMeshGeometry3D( bool withScalarMagnitude, bool indexed )
  : mHasScalarMagnitude( withScalarMagnitude )
{
  mStride = ( withScalarMagnitude ? 3 + 3 + 1 : 3 + 3 ) * sizeof( float );

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

  if ( withScalarMagnitude )
    addAttribute( u"scalarMagnitude"_s, QRhiVertexInputAttribute::Float, sizeof( float ) );

  Q_ASSERT( offset == mStride );

  if ( indexed )
    mGeometry.setIndexBuffer( &mIndexBuffer, QRhiCommandBuffer::IndexUInt32 );
}

void QgsMeshGeometry3D::setVertexData( const QByteArray &vertexData )
{
  mVertexBuffer.setData( vertexData );
  mGeometry.setVertexCount( static_cast<int>( vertexData.size() ) / mStride );
}

void QgsMeshGeometry3D::setIndexData( const QByteArray &indexData )
{
  Q_ASSERT( mGeometry.isIndexed() );

  mIndexBuffer.setData( indexData );
  mGeometry.setIndexCount( static_cast<int>( indexData.size() / sizeof( quint32 ) ) );
}

void QgsMeshGeometry3D::releaseGpuResources()
{
  mVertexBuffer.releaseGpuResources();
  mIndexBuffer.releaseGpuResources();
}

/// @endcond
