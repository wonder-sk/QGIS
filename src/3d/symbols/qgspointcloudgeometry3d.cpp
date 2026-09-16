/***************************************************************************
  qgspointcloudgeometry3d.cpp
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

#include "qgspointcloudgeometry3d.h"

#include <QString>

using namespace Qt::StringLiterals;

///@cond PRIVATE

int QgsPointCloudGeometry3D::floatsPerPoint( VertexLayout layout )
{
  switch ( layout )
  {
    case VertexLayout::PositionOnly:
      return 3;
    case VertexLayout::PositionAndParameter:
      return 3 + 1;
    case VertexLayout::PositionAndColor:
      return 3 + 3;
    case VertexLayout::PositionParameterAndSize:
      return 3 + 1 + 1;
  }
  return 3;
}

QgsPointCloudGeometry3D::QgsPointCloudGeometry3D( VertexLayout layout, int byteStride )
  : mLayout( layout )
{
  const int packed = floatsPerPoint( layout ) * static_cast<int>( sizeof( float ) );
  // the symbol's stride may leave room past the attributes this layout uses, but it can
  // never be smaller than them
  Q_ASSERT( byteStride == 0 || byteStride >= packed );
  mStride = byteStride > 0 ? byteStride : packed;

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

  switch ( layout )
  {
    case VertexLayout::PositionOnly:
      break;

    case VertexLayout::PositionAndParameter:
      addAttribute( u"vertexParameter"_s, QRhiVertexInputAttribute::Float, sizeof( float ) );
      break;

    case VertexLayout::PositionAndColor:
      addAttribute( u"vertexColor"_s, QRhiVertexInputAttribute::Float3, 3 * sizeof( float ) );
      break;

    case VertexLayout::PositionParameterAndSize:
      addAttribute( u"vertexParameter"_s, QRhiVertexInputAttribute::Float, sizeof( float ) );
      addAttribute( u"vertexSize"_s, QRhiVertexInputAttribute::Float, sizeof( float ) );
      break;
  }

  Q_ASSERT( offset == packed );
}

void QgsPointCloudGeometry3D::setPoints( const QVector<QVector3D> &positions, const QVector<float> &parameters, const QVector<QVector3D> &colors, const QVector<float> &pointSizes )
{
  const bool needsParameter = mLayout == VertexLayout::PositionAndParameter || mLayout == VertexLayout::PositionParameterAndSize;
  Q_ASSERT( !needsParameter || parameters.size() == positions.size() );
  Q_ASSERT( mLayout != VertexLayout::PositionAndColor || colors.size() == positions.size() );
  Q_ASSERT( mLayout != VertexLayout::PositionParameterAndSize || pointSizes.size() == positions.size() );

  QByteArray data;
  data.resize( positions.size() * mStride );
  char *point = data.data();

  for ( qsizetype i = 0; i < positions.size(); ++i, point += mStride )
  {
    float *out = reinterpret_cast<float *>( point );
    *out++ = positions.at( i ).x();
    *out++ = positions.at( i ).y();
    *out++ = positions.at( i ).z();

    switch ( mLayout )
    {
      case VertexLayout::PositionOnly:
        break;

      case VertexLayout::PositionAndParameter:
        *out++ = parameters.at( i );
        break;

      case VertexLayout::PositionAndColor:
        *out++ = colors.at( i ).x();
        *out++ = colors.at( i ).y();
        *out++ = colors.at( i ).z();
        break;

      case VertexLayout::PositionParameterAndSize:
        *out++ = parameters.at( i );
        *out++ = pointSizes.at( i );
        break;
    }
  }

  mVertexBuffer.setData( data );
  mGeometry.setVertexCount( static_cast<int>( positions.size() ) );
}

void QgsPointCloudGeometry3D::setTriangles( const QByteArray &indices, const QByteArray &normals )
{
  const bool triangulated = !indices.isEmpty();

  // the normal attribute has to come and go with the triangulation, since a shader that
  // reads one the geometry does not supply fails to build a pipeline at all
  if ( triangulated != mGeometry.isIndexed() )
  {
    if ( triangulated )
    {
      QgsVertexAttribute normal;
      normal.name = u"vertexNormal"_s;
      normal.buffer = &mNormalBuffer;
      normal.format = QRhiVertexInputAttribute::Float3;
      normal.byteStride = 3 * sizeof( float );
      mGeometry.addAttribute( normal );
      mGeometry.setIndexBuffer( &mIndexBuffer, QRhiCommandBuffer::IndexUInt32 );
    }
    else
    {
      const QList<QgsVertexAttribute> attributes = mGeometry.attributes();
      mGeometry.clearAttributes();
      for ( const QgsVertexAttribute &attribute : attributes )
      {
        if ( attribute.name != "vertexNormal"_L1 )
          mGeometry.addAttribute( attribute );
      }
      mGeometry.setIndexBuffer( nullptr );
    }
  }

  mIndexBuffer.setData( indices );
  mNormalBuffer.setData( normals );
  mGeometry.setIndexCount( static_cast<int>( indices.size() / sizeof( quint32 ) ) );
}

void QgsPointCloudGeometry3D::releaseGpuResources()
{
  mVertexBuffer.releaseGpuResources();
  mIndexBuffer.releaseGpuResources();
  mNormalBuffer.releaseGpuResources();
}

/// @endcond
