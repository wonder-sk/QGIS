/***************************************************************************
  qgsbillboardgeometry3d.cpp
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

#include "qgsbillboardgeometry3d.h"

#include <algorithm>

#include <QSizeF>
#include <QString>

using namespace Qt::StringLiterals;

namespace
{
  //! The four corners of a unit quad, in triangle strip order
  constexpr float QUAD_VERTICES[] = { -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
  constexpr int QUAD_VERTEX_COUNT = 4;
} // namespace

QgsBillboardGeometry3D::QgsBillboardGeometry3D()
{
  mQuadBuffer.setData( QByteArray( reinterpret_cast<const char *>( QUAD_VERTICES ), sizeof( QUAD_VERTICES ) ) );
  mGeometry.setVertexCount( QUAD_VERTEX_COUNT );

  setAttributes( Attribute::Position );
}

void QgsBillboardGeometry3D::setAttributes( Attributes attributes )
{
  // position must always be present
  Q_ASSERT( attributes.testFlag( Attribute::Position ) );

  if ( mAttributes == attributes && !mGeometry.attributes().isEmpty() )
    return;

  mAttributes = attributes;

  mStride = 3 * sizeof( float );
  if ( attributes.testFlag( Attribute::Size ) )
    mStride += 2 * sizeof( float );
  if ( attributes.testFlag( Attribute::TextureData ) )
    mStride += ( 2 + 2 ) * sizeof( float );
  if ( attributes.testFlag( Attribute::PixelOffsets ) )
    mStride += 2 * sizeof( int );

  mGeometry.clearAttributes();

  // the quad itself advances per vertex; everything else advances per instance
  QgsVertexAttribute quad;
  quad.name = u"vertexPosition"_s;
  quad.buffer = &mQuadBuffer;
  quad.format = QRhiVertexInputAttribute::Float2;
  quad.byteStride = 2 * sizeof( float );
  mGeometry.addAttribute( quad );

  int offset = 0;
  const auto addInstanceAttribute = [this, &offset]( const QString &name, QRhiVertexInputAttribute::Format format, int size ) {
    QgsVertexAttribute attribute;
    attribute.name = name;
    attribute.buffer = &mInstanceBuffer;
    attribute.format = format;
    attribute.byteOffset = static_cast<quint32>( offset );
    attribute.byteStride = static_cast<quint32>( mStride );
    attribute.instanceDivisor = 1;
    mGeometry.addAttribute( attribute );
    offset += size;
  };

  addInstanceAttribute( u"instancePosition"_s, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ) );

  if ( attributes.testFlag( Attribute::Size ) )
    addInstanceAttribute( u"instanceSize"_s, QRhiVertexInputAttribute::Float2, 2 * sizeof( float ) );

  if ( attributes.testFlag( Attribute::TextureData ) )
  {
    addInstanceAttribute( u"atlasOffset"_s, QRhiVertexInputAttribute::Float2, 2 * sizeof( float ) );
    addInstanceAttribute( u"atlasSize"_s, QRhiVertexInputAttribute::Float2, 2 * sizeof( float ) );
  }

  if ( attributes.testFlag( Attribute::PixelOffsets ) )
    addInstanceAttribute( u"pixelOffset"_s, QRhiVertexInputAttribute::SInt2, 2 * sizeof( int ) );

  Q_ASSERT( offset == mStride );
}

void QgsBillboardGeometry3D::setPositions( const QVector<QVector3D> &positions )
{
  setAttributes( Attribute::Position );

  QByteArray data;
  data.resize( positions.size() * mStride );
  float *out = reinterpret_cast<float *>( data.data() );
  for ( const QVector3D &position : positions )
  {
    *out++ = position.x();
    *out++ = position.y();
    *out++ = position.z();
  }

  mCount = static_cast<int>( positions.size() );
  mInstanceBuffer.setData( data );
}

void QgsBillboardGeometry3D::setPositionsAndSizes( const QVector<QVector3D> &positions, const QVector<QSizeF> &sizes )
{
  setAttributes( Attribute::Position | Attribute::Size );

  const qsizetype count = std::min( positions.size(), sizes.size() );
  QByteArray data;
  data.resize( count * mStride );
  float *out = reinterpret_cast<float *>( data.data() );
  for ( qsizetype i = 0; i < count; ++i )
  {
    *out++ = positions[i].x();
    *out++ = positions[i].y();
    *out++ = positions[i].z();

    *out++ = static_cast<float>( sizes[i].width() );
    *out++ = static_cast<float>( sizes[i].height() );
  }

  mCount = static_cast<int>( count );
  mInstanceBuffer.setData( data );
}

void QgsBillboardGeometry3D::setBillboardData( const QVector<BillboardAtlasData> &billboards, bool includePixelOffsets )
{
  Attributes attributes = Attribute::Position | Attribute::TextureData;
  attributes.setFlag( Attribute::PixelOffsets, includePixelOffsets );
  setAttributes( attributes );

  QByteArray data;
  data.resize( billboards.size() * mStride );
  char *vertex = data.data();
  for ( const BillboardAtlasData &billboard : billboards )
  {
    float *out = reinterpret_cast<float *>( vertex );

    *out++ = billboard.position.x();
    *out++ = billboard.position.y();
    *out++ = billboard.position.z();

    *out++ = billboard.textureAtlasOffset.x();
    *out++ = billboard.textureAtlasOffset.y();

    *out++ = billboard.textureAtlasSize.x();
    *out++ = billboard.textureAtlasSize.y();

    if ( includePixelOffsets )
    {
      int *pixelOffset = reinterpret_cast<int *>( out );
      *pixelOffset++ = billboard.pixelOffset.x();
      *pixelOffset++ = billboard.pixelOffset.y();
    }

    vertex += mStride;
  }

  mCount = static_cast<int>( billboards.size() );
  mInstanceBuffer.setData( data );
}

void QgsBillboardGeometry3D::releaseGpuResources()
{
  mQuadBuffer.releaseGpuResources();
  mInstanceBuffer.releaseGpuResources();
}
