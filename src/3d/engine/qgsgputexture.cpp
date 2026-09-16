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

  if ( mTexture && ( converted.size() != mSize || mFormat != Format::RGBA8 ) )
    mTexture.reset();

  mImage = converted;
  mData.clear();
  mSize = converted.size();
  mFormat = Format::RGBA8;
  mDirty = true;
}

void QgsGpuTexture::setData( const QByteArray &data, const QSize &size, Format format )
{
  if ( mTexture && ( size != mSize || format != mFormat ) )
    mTexture.reset();

  mImage = QImage();
  mData = data;
  mSize = size;
  mFormat = format;
  mDirty = true;
}

void QgsGpuTexture::setExternalTexture( QRhiTexture *texture )
{
  // an external texture and an uploaded one are alternatives, never both at once, so drop
  // whatever we were keeping for the uploaded form
  mTexture.reset();
  mImage = QImage();
  mData.clear();
  mDirty = false;

  mExternalTexture = texture;
  mSize = texture ? texture->pixelSize() : QSize();
  mFormat = Format::RGBA8;
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
  if ( mExternalTexture )
    return mExternalTexture;

  if ( isEmpty() )
    return nullptr;

  const QRhiTexture::Format rhiFormat = mFormat == Format::RGBA32F ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8;

  if ( !mTexture )
  {
    if ( !rhi->isTextureFormatSupported( rhiFormat ) )
    {
      QgsDebugError( u"Texture format %1 is not supported by this backend"_s.arg( static_cast<int>( rhiFormat ) ) );
      return nullptr;
    }

    QRhiTexture::Flags flags;
    if ( mMipmaps )
      flags |= QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips;

    mTexture.reset( rhi->newTexture( rhiFormat, mSize, 1, flags ) );
    if ( !mTexture->create() )
    {
      QgsDebugError( u"Failed to create %1x%2 texture"_s.arg( mSize.width() ).arg( mSize.height() ) );
      mTexture.reset();
      return nullptr;
    }
    mDirty = true;
  }

  if ( mDirty )
  {
    if ( mImage.isNull() )
    {
      QRhiTextureSubresourceUploadDescription subresource( mData.constData(), mData.size() );
      subresource.setSourceSize( mSize );
      updates->uploadTexture( mTexture.get(), QRhiTextureUploadDescription( { 0, 0, subresource } ) );
    }
    else
    {
      updates->uploadTexture( mTexture.get(), mImage );
    }

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
