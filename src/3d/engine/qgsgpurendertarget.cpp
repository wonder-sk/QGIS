/***************************************************************************
  qgsgpurendertarget.cpp
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

#include "qgsgpurendertarget.h"

#include "qgslogger.h"

#include <QString>

using namespace Qt::StringLiterals;

QgsGpuRenderTarget::QgsGpuRenderTarget() = default;

QgsGpuRenderTarget::~QgsGpuRenderTarget() = default;

void QgsGpuRenderTarget::release()
{
  // render targets refer to the attachments, so they have to go first
  mTargets.clear();
  mDepthRenderBuffer.reset();
  mDepthResolveTexture.reset();
  mDepthTexture.reset();
  mColorRenderBuffer.reset();
  mColorTexture.reset();
  mSize = QSize();
}

QRhiTexture *QgsGpuRenderTarget::colorTexture() const
{
  return mColorTexture.get();
}

QRhiTexture *QgsGpuRenderTarget::depthTexture() const
{
  return mDepthResolveTexture ? mDepthResolveTexture.get() : mDepthTexture.get();
}

bool QgsGpuRenderTarget::create( QRhi *rhi, QSize size )
{
  release();

  if ( size.isEmpty() || mLayerCount < 1 )
    return false;

  const int samples = std::max( 1, mSampleCount );
  const bool multisampled = samples > 1;

  if ( !rhi->isTextureFormatSupported( mColorFormat ) )
  {
    QgsDebugError( u"Colour format %1 is not supported by this backend"_s.arg( static_cast<int>( mColorFormat ) ) );
    return false;
  }

  QRhiTexture::Flags colorFlags = QRhiTexture::RenderTarget;
  if ( mReadbackEnabled )
    colorFlags |= QRhiTexture::UsedAsTransferSource;

  if ( mLayerCount > 1 )
    mColorTexture.reset( rhi->newTextureArray( mColorFormat, mLayerCount, size, 1, colorFlags ) );
  else
    mColorTexture.reset( rhi->newTexture( mColorFormat, size, 1, colorFlags ) );

  if ( !mColorTexture->create() )
  {
    QgsDebugError( u"Failed to create a %1x%2 colour attachment"_s.arg( size.width() ).arg( size.height() ) );
    release();
    return false;
  }

  if ( multisampled )
  {
    // a multisample renderbuffer rather than a multisample texture, because QRhi's OpenGL
    // backend claims to support multisample textures on contexts that cannot create them.
    // Nothing samples the multisampled surface anyway, it only resolves into mColorTexture.
    mColorRenderBuffer.reset( rhi->newRenderBuffer( QRhiRenderBuffer::Color, size, samples, {}, mColorFormat ) );
    if ( !mColorRenderBuffer->create() )
    {
      QgsDebugError( u"Failed to create a %1 sample colour renderbuffer"_s.arg( samples ) );
      release();
      return false;
    }
  }

  // Sampling the depth of a multisampled target means resolving it, and on the OpenGL
  // backend a multisample texture cannot even be created. Where that is the case the depth
  // attachment falls back to a plain renderbuffer and depthTexture() stays NULLPTR, which
  // is what tells the passes that read depth to skip themselves.
  const bool canResolveDepth = rhi->isFeatureSupported( QRhi::ResolveDepthStencil ) && rhi->isFeatureSupported( QRhi::MultisampleTexture ) && rhi->backend() != QRhi::OpenGLES2;
  const DepthMode depthMode = ( multisampled && mDepthMode == DepthMode::Texture && !canResolveDepth ) ? DepthMode::RenderBuffer : mDepthMode;

  switch ( depthMode )
  {
    case DepthMode::None:
      break;

    case DepthMode::RenderBuffer:
      mDepthRenderBuffer.reset( rhi->newRenderBuffer( QRhiRenderBuffer::DepthStencil, size, samples ) );
      if ( !mDepthRenderBuffer->create() )
      {
        QgsDebugError( u"Failed to create a %1x%2 depth renderbuffer"_s.arg( size.width() ).arg( size.height() ) );
        release();
        return false;
      }
      break;

    case DepthMode::Texture:
      mDepthTexture.reset( rhi->newTexture( mDepthFormat, size, samples, QRhiTexture::RenderTarget ) );
      if ( !mDepthTexture->create() )
      {
        QgsDebugError( u"Failed to create a %1x%2 depth texture"_s.arg( size.width() ).arg( size.height() ) );
        release();
        return false;
      }

      if ( multisampled )
      {
        mDepthResolveTexture.reset( rhi->newTexture( mDepthFormat, size, 1, QRhiTexture::RenderTarget ) );
        if ( !mDepthResolveTexture->create() )
        {
          QgsDebugError( u"Failed to create a %1x%2 depth resolve texture"_s.arg( size.width() ).arg( size.height() ) );
          release();
          return false;
        }
      }
      break;
  }

  mSize = size;

  mTargets.resize( static_cast<size_t>( mLayerCount ) );
  for ( int layer = 0; layer < mLayerCount; ++layer )
  {
    if ( !buildLayerTargets( rhi, layer ) )
    {
      release();
      return false;
    }
  }

  return true;
}

bool QgsGpuRenderTarget::buildLayerTargets( QRhi *rhi, int layer )
{
  QRhiColorAttachment colorAttachment;
  if ( mColorRenderBuffer )
  {
    // render into the multisampled renderbuffer and resolve into the plain texture, which
    // is the one everything downstream samples
    colorAttachment.setRenderBuffer( mColorRenderBuffer.get() );
    colorAttachment.setResolveTexture( mColorTexture.get() );
    colorAttachment.setResolveLayer( layer );
  }
  else
  {
    colorAttachment.setTexture( mColorTexture.get() );
    colorAttachment.setLayer( layer );
  }

  QRhiTextureRenderTargetDescription description( colorAttachment );
  if ( mDepthTexture )
  {
    description.setDepthTexture( mDepthTexture.get() );
    if ( mDepthResolveTexture )
      description.setDepthResolveTexture( mDepthResolveTexture.get() );
  }
  else if ( mDepthRenderBuffer )
  {
    description.setDepthStencilBuffer( mDepthRenderBuffer.get() );
  }

  Target &target = mTargets[static_cast<size_t>( layer )];

  // Two targets over the same attachments. They differ only in whether the pass that draws
  // into them starts by clearing, which in QRhi is baked into the render pass rather than
  // given at beginPass() time.
  const struct
  {
      QRhiTextureRenderTarget::Flags flags;
      std::unique_ptr<QRhiTextureRenderTarget> *renderTarget;
      std::unique_ptr<QRhiRenderPassDescriptor> *descriptor;
  } variants[2] = {
    { {}, &target.clearing, &target.clearingDescriptor },
    { QRhiTextureRenderTarget::PreserveColorContents | QRhiTextureRenderTarget::PreserveDepthStencilContents, &target.preserving, &target.preservingDescriptor },
  };

  for ( const auto &variant : variants )
  {
    variant.renderTarget->reset( rhi->newTextureRenderTarget( description, variant.flags ) );
    variant.descriptor->reset( ( *variant.renderTarget )->newCompatibleRenderPassDescriptor() );
    ( *variant.renderTarget )->setRenderPassDescriptor( variant.descriptor->get() );
    if ( !( *variant.renderTarget )->create() )
    {
      QgsDebugError( u"Failed to create a render target for layer %1"_s.arg( layer ) );
      return false;
    }
  }

  return true;
}

bool QgsGpuRenderTarget::resize( QRhi *rhi, QSize size )
{
  if ( size == mSize && isValid() )
    return true;

  return create( rhi, size );
}

QRhiTextureRenderTarget *QgsGpuRenderTarget::rhiTarget( bool keepContents, int layer ) const
{
  if ( layer < 0 || layer >= static_cast<int>( mTargets.size() ) )
    return nullptr;

  const Target &target = mTargets[static_cast<size_t>( layer )];
  return keepContents ? target.preserving.get() : target.clearing.get();
}
