/***************************************************************************
  qgsobjgeometry3d.cpp
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

#include "qgsobjgeometry3d.h"

#include <QString>

using namespace Qt::StringLiterals;

///@cond PRIVATE

QgsObjGeometry3D::QgsObjGeometry3D( bool withNormals, bool withTextureCoords )
{
  mStride = ( 3 + ( withNormals ? 3 : 0 ) + ( withTextureCoords ? 2 : 0 ) ) * sizeof( float );

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

  if ( withTextureCoords )
    addAttribute( u"vertexTexCoord"_s, QRhiVertexInputAttribute::Float2, 2 * sizeof( float ) );

  Q_ASSERT( offset == mStride );
}

void QgsObjGeometry3D::setVertexData( const QByteArray &vertexData )
{
  mVertexBuffer.setData( vertexData );
  mGeometry.setVertexCount( static_cast<int>( vertexData.size() ) / mStride );
}

void QgsObjGeometry3D::releaseGpuResources()
{
  mVertexBuffer.releaseGpuResources();
}

/// @endcond
