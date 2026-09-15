/***************************************************************************
  qgsgputexture.cpp
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

#include "qgsgputexture.h"

#include "qgslogger.h"

#include <QString>

using namespace Qt::StringLiterals;

QgsGpuTexture::~QgsGpuTexture() = default;

void QgsGpuTexture::setImage( const QImage &image )
{
  const QImage converted = image.format() == QImage::Format_RGBA8888 ? image : image.convertToFormat( QImage::Format_RGBA8888 );

  if ( mTexture && converted.size() != mImage.size() )
    mTexture.reset();

  mImage = converted;
  mDirty = true;
}

void QgsGpuTexture::setMipmapsEnabled( bool enabled )
{
  if ( enabled == mMipmaps )
    return;

  mMipmaps = enabled;
  // the mip count is fixed when the texture is created, so it has to be rebuilt
  mTexture.reset();
  mDirty = true;
}

QRhiTexture *QgsGpuTexture::rhiTexture( QRhi *rhi, QRhiResourceUpdateBatch *updates )
{
  if ( mImage.isNull() )
    return nullptr;

  if ( !mTexture )
  {
    QRhiTexture::Flags flags;
    if ( mMipmaps )
      flags |= QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips;

    mTexture.reset( rhi->newTexture( QRhiTexture::RGBA8, mImage.size(), 1, flags ) );
    if ( !mTexture->create() )
    {
      QgsDebugError( u"Failed to create %1x%2 texture"_s.arg( mImage.width() ).arg( mImage.height() ) );
      mTexture.reset();
      return nullptr;
    }
    mDirty = true;
  }

  if ( mDirty )
  {
    updates->uploadTexture( mTexture.get(), mImage );
    if ( mMipmaps )
      updates->generateMips( mTexture.get() );
    mDirty = false;
  }

  return mTexture.get();
}

void QgsGpuTexture::releaseGpuResources()
{
  mTexture.reset();
  mDirty = true;
}
