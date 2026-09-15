/***************************************************************************
  qgsuniformring.cpp
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

#include "qgsuniformring.h"

#include <cstring>

#include "qgslogger.h"

#include <QString>

using namespace Qt::StringLiterals;

QgsUniformRing::QgsUniformRing() = default;

QgsUniformRing::~QgsUniformRing() = default;

void QgsUniformRing::reset( QRhi *rhi, int elementSize )
{
  mDataSize = static_cast<quint32>( elementSize );
  mElementSize = static_cast<quint32>( rhi->ubufAligned( elementSize ) );
  mCount = 0;
  mStaging.resize( 0 );
}

quint32 QgsUniformRing::append( const void *data )
{
  Q_ASSERT( mElementSize > 0 );

  const quint32 offset = static_cast<quint32>( mCount ) * mElementSize;
  mStaging.resize( static_cast<qsizetype>( offset + mElementSize ) );

  std::memcpy( mStaging.data() + offset, data, mDataSize );

  // only mDataSize bytes exist at the caller's pointer, so the alignment padding that
  // follows has to be zeroed here rather than copied
  if ( mElementSize > mDataSize )
    std::memset( mStaging.data() + offset + mDataSize, 0, mElementSize - mDataSize );
  ++mCount;
  return offset;
}

QRhiBuffer *QgsUniformRing::upload( QRhi *rhi, QRhiResourceUpdateBatch *updates )
{
  if ( mStaging.isEmpty() )
    return nullptr;

  const quint32 required = static_cast<quint32>( mStaging.size() );

  if ( mBuffer && mBuffer->size() < required )
    mBuffer.reset();

  if ( !mBuffer )
  {
    // grow generously, so that a scene of roughly stable size stops reallocating quickly
    const quint32 capacity = required + required / 2;
    mBuffer.reset( rhi->newBuffer( QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, capacity ) );
    if ( !mBuffer->create() )
    {
      QgsDebugError( u"Failed to create uniform ring buffer of %1 bytes"_s.arg( capacity ) );
      mBuffer.reset();
      return nullptr;
    }
  }

  updates->updateDynamicBuffer( mBuffer.get(), 0, required, mStaging.constData() );
  return mBuffer.get();
}

void QgsUniformRing::releaseGpuResources()
{
  mBuffer.reset();
}
