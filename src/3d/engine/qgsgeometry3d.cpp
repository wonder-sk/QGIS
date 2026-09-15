/***************************************************************************
  qgsgeometry3d.cpp
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
 ***************************************************************************/

#include "qgsgeometry3d.h"

#include "qgsgpubuffer.h"

#include <QHashFunctions>

int QgsVertexAttribute::elementSize() const
{
  switch ( format )
  {
    case QRhiVertexInputAttribute::Float4:
    case QRhiVertexInputAttribute::UInt4:
    case QRhiVertexInputAttribute::SInt4:
      return 16;
    case QRhiVertexInputAttribute::Float3:
    case QRhiVertexInputAttribute::UInt3:
    case QRhiVertexInputAttribute::SInt3:
      return 12;
    case QRhiVertexInputAttribute::Float2:
    case QRhiVertexInputAttribute::UInt2:
    case QRhiVertexInputAttribute::SInt2:
    case QRhiVertexInputAttribute::Half4:
    case QRhiVertexInputAttribute::UShort4:
    case QRhiVertexInputAttribute::SShort4:
      return 8;
    case QRhiVertexInputAttribute::Float:
    case QRhiVertexInputAttribute::UInt:
    case QRhiVertexInputAttribute::SInt:
    case QRhiVertexInputAttribute::UNormByte4:
    case QRhiVertexInputAttribute::Half2:
    case QRhiVertexInputAttribute::UShort2:
    case QRhiVertexInputAttribute::SShort2:
      return 4;
    case QRhiVertexInputAttribute::Half3:
    case QRhiVertexInputAttribute::UShort3:
    case QRhiVertexInputAttribute::SShort3:
      return 6;
    case QRhiVertexInputAttribute::UNormByte2:
    case QRhiVertexInputAttribute::Half:
    case QRhiVertexInputAttribute::UShort:
    case QRhiVertexInputAttribute::SShort:
      return 2;
    case QRhiVertexInputAttribute::UNormByte:
      return 1;
  }
  return 0;
}


void QgsGeometry3D::addAttribute( const QgsVertexAttribute &attribute )
{
  Q_ASSERT( !attribute.name.isEmpty() );
  Q_ASSERT( attribute.buffer );
  Q_ASSERT( !this->attribute( attribute.name ) );

  mAttributes.append( attribute );
  mLayoutKey = 0;
}

const QgsVertexAttribute *QgsGeometry3D::attribute( const QString &name ) const
{
  for ( const QgsVertexAttribute &attribute : mAttributes )
  {
    if ( attribute.name == name )
      return &attribute;
  }
  return nullptr;
}

void QgsGeometry3D::setIndexBuffer( QgsGpuBuffer *buffer, QRhiCommandBuffer::IndexFormat format )
{
  mIndexBuffer = buffer;
  mIndexFormat = format;
}

size_t QgsGeometry3D::layoutKey() const
{
  if ( mLayoutKey )
    return mLayoutKey;

  size_t key = qHash( mAttributes.size() );
  for ( const QgsVertexAttribute &attribute : mAttributes )
  {
    key = qHashMulti( key, attribute.name, static_cast<int>( attribute.format ), attribute.byteStride, attribute.instanceDivisor );
  }

  // a key of zero would be indistinguishable from "not computed yet"
  mLayoutKey = key ? key : 1;
  return mLayoutKey;
}
