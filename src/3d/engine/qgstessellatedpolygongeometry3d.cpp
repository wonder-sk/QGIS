/***************************************************************************
  qgstessellatedpolygongeometry3d.cpp
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

#include "qgstessellatedpolygongeometry3d.h"

#include <algorithm>

#include <QString>

using namespace Qt::StringLiterals;

QgsTessellatedPolygonGeometry3D::QgsTessellatedPolygonGeometry3D( bool withNormals, bool withTextureCoords, bool withTangents )
{
  // The attribute order here is the order QgsTessellator writes them in, which is what
  // fixes both the stride and each attribute's offset within it. The stride has to be
  // known before the first attribute is added, so it is counted up first.
  mStride = 3 * sizeof( float );
  if ( withNormals )
    mStride += 3 * sizeof( float );
  if ( withTangents )
    mStride += 4 * sizeof( float );
  if ( withTextureCoords )
    mStride += 2 * sizeof( float );

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

  if ( withNormals )
    addAttribute( u"vertexNormal"_s, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ) );

  if ( withTangents )
    addAttribute( u"vertexTangent"_s, QRhiVertexInputAttribute::Float4, 4 * sizeof( float ) );

  if ( withTextureCoords )
    addAttribute( u"vertexTexCoord"_s, QRhiVertexInputAttribute::Float2, 2 * sizeof( float ) );

  Q_ASSERT( offset == mStride );

  mGeometry.setIndexBuffer( &mIndexBuffer, QRhiCommandBuffer::IndexUInt32 );
}

void QgsTessellatedPolygonGeometry3D::setVertexBufferData( const QByteArray &vertexData, int vertexCount )
{
  mVertexBuffer.setData( vertexData );
  mGeometry.setVertexCount( vertexCount );
}

void QgsTessellatedPolygonGeometry3D::setIndexBufferData( const QByteArray &indexData, int indexCount )
{
  mIndexBuffer.setData( indexData );
  mGeometry.setIndexCount( indexCount );
}

void QgsTessellatedPolygonGeometry3D::setTriangleIndex( const QVector<QgsFeatureId> &featureIds, const QVector<uint> &startingIndices )
{
  mFeatureIds = featureIds;
  mStartingIndices = startingIndices;
}

QgsFeatureId QgsTessellatedPolygonGeometry3D::triangleIndexToFeatureId( uint triangleIndex ) const
{
  // binary search over the sorted starting indices for the bin triangleIndex falls in
  if ( mStartingIndices.isEmpty() || triangleIndex < mStartingIndices.first() )
    return FID_NULL;

  const auto it = std::upper_bound( mStartingIndices.cbegin(), mStartingIndices.cend(), triangleIndex );
  const int index = static_cast<int>( it - mStartingIndices.cbegin() ) - 1;
  return index < mFeatureIds.size() ? mFeatureIds.at( index ) : FID_NULL;
}

void QgsTessellatedPolygonGeometry3D::releaseGpuResources()
{
  mVertexBuffer.releaseGpuResources();
  mIndexBuffer.releaseGpuResources();
}
