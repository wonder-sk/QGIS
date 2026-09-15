/***************************************************************************
  qgsgpubuffer.cpp
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

#include "qgsgpubuffer.h"

#include "qgslogger.h"

#include <QString>

using namespace Qt::StringLiterals;

QgsGpuBuffer::QgsGpuBuffer( Role role, Usage usage )
  : mRole( role )
  , mUsage( usage )
{}

QgsGpuBuffer::~QgsGpuBuffer() = default;

void QgsGpuBuffer::setData( const QByteArray &data )
{
  mData = data;
  mDirty = true;

  // a size change cannot be handled by an update, the GPU buffer has to be rebuilt
  if ( mBuffer && mBuffer->size() != static_cast<quint32>( mData.size() ) )
    mBuffer.reset();
}

QRhiBuffer *QgsGpuBuffer::rhiBuffer( QRhi *rhi, QRhiResourceUpdateBatch *updates )
{
  if ( mData.isEmpty() )
    return nullptr;

  if ( !mBuffer )
  {
    QRhiBuffer::UsageFlags usageFlags;
    switch ( mRole )
    {
      case Role::Vertex:
        usageFlags = QRhiBuffer::VertexBuffer;
        break;
      case Role::Index:
        usageFlags = QRhiBuffer::IndexBuffer;
        break;
      case Role::Uniform:
        usageFlags = QRhiBuffer::UniformBuffer;
        break;
    }

    // uniform buffers have to be Dynamic: that is the only type every backend lets us
    // update from the CPU each frame without a staging copy
    QRhiBuffer::Type type;
    if ( mRole == Role::Uniform )
      type = QRhiBuffer::Dynamic;
    else if ( mUsage == Usage::Static )
      type = QRhiBuffer::Immutable;
    else
      type = QRhiBuffer::Static;

    mBuffer.reset( rhi->newBuffer( type, usageFlags, static_cast<quint32>( mData.size() ) ) );
    if ( !mBuffer->create() )
    {
      QgsDebugError( u"Failed to create GPU buffer of %1 bytes"_s.arg( mData.size() ) );
      mBuffer.reset();
      return nullptr;
    }
    mDirty = true;
  }

  if ( mDirty )
  {
    if ( mRole == Role::Uniform )
      updates->updateDynamicBuffer( mBuffer.get(), 0, static_cast<quint32>( mData.size() ), mData.constData() );
    else
      updates->uploadStaticBuffer( mBuffer.get(), mData );
    mDirty = false;
  }

  return mBuffer.get();
}

void QgsGpuBuffer::releaseGpuResources()
{
  mBuffer.reset();
  mDirty = true;
}
