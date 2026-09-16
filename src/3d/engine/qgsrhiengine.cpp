/***************************************************************************
  qgsrhiengine.cpp
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

#include "qgsrhiengine.h"

#include <rhi/qrhi_platform.h>

#include "qgslogger.h"

#include <QFile>
#include <QOffscreenSurface>
#include <QString>
#include <QSurfaceFormat>
#include <QWindow>

using namespace Qt::StringLiterals;

#if QT_CONFIG( vulkan )
#include <QVulkanInstance>
#endif

namespace
{

  /**
   * Returns the shared Vulkan instance used by every QGIS Vulkan device and window.
   *
   * A swapchain can only be created for a window whose QVulkanInstance is the same one
   * the device was created with, so there is exactly one instance per process.
   */
#if QT_CONFIG( vulkan )
  QVulkanInstance *sharedVulkanInstance()
  {
    static QVulkanInstance *sInstance = [] {
      auto *instance = new QVulkanInstance;
      instance->setExtensions( QRhiVulkanInitParams::preferredInstanceExtensions() );
      if ( !instance->create() )
      {
        QgsDebugError( u"Failed to create Vulkan instance"_s );
        delete instance;
        return static_cast<QVulkanInstance *>( nullptr );
      }
      return instance;
    }();
    return sInstance;
  }
#endif

} // namespace


QgsRhiEngine::QgsRhiEngine() = default;

QgsRhiEngine::~QgsRhiEngine()
{
  // resources must go away before the device they belong to
  releaseOffscreenTarget();
  mSwapChainRenderPassDescriptor.reset();
  mSwapChainDepthStencil.reset();
  mSwapChain.reset();
  mRhi.reset();
  mFallbackSurface.reset();
}

QRhi::Implementation QgsRhiEngine::backendFromName( const QString &name, bool *ok )
{
  if ( ok )
    *ok = true;

  const QString lower = name.trimmed().toLower();
  if ( lower == "opengl"_L1 || lower == "gl"_L1 || lower == "gles2"_L1 )
    return QRhi::OpenGLES2;
  if ( lower == "vulkan"_L1 )
    return QRhi::Vulkan;
  if ( lower == "metal"_L1 )
    return QRhi::Metal;
  if ( lower == "d3d11"_L1 )
    return QRhi::D3D11;
  if ( lower == "d3d12"_L1 )
    return QRhi::D3D12;
  if ( lower == "null"_L1 )
    return QRhi::Null;

  if ( ok )
    *ok = false;
  return QRhi::Null;
}

QRhi::Implementation QgsRhiEngine::defaultBackend()
{
  // QGIS's own variable wins, so that the 3D view can be moved to another backend without
  // also moving Qt Quick. Qt's variable is honoured next, so that the usual Qt knob works.
  for ( const char *variable : { "QGIS_3D_RHI_BACKEND", "QSG_RHI_BACKEND" } )
  {
    if ( !qEnvironmentVariableIsSet( variable ) )
      continue;

    bool ok = false;
    const QRhi::Implementation backend = backendFromName( qEnvironmentVariable( variable ), &ok );
    if ( ok )
      return backend;

    QgsDebugError( u"Unrecognised %1 value: %2"_s.arg( QString::fromLatin1( variable ), qEnvironmentVariable( variable ) ) );
  }

#if defined( Q_OS_WIN )
  return QRhi::D3D11;
#elif defined( Q_OS_MACOS ) || defined( Q_OS_IOS )
  return QRhi::Metal;
#else
  return QRhi::OpenGLES2;
#endif
}

QSurface::SurfaceType QgsRhiEngine::surfaceTypeForBackend( QRhi::Implementation backend )
{
  switch ( backend )
  {
    case QRhi::Vulkan:
      return QSurface::VulkanSurface;
    case QRhi::Metal:
      return QSurface::MetalSurface;
    case QRhi::D3D11:
    case QRhi::D3D12:
      return QSurface::Direct3DSurface;
    case QRhi::OpenGLES2:
    case QRhi::Null:
      return QSurface::OpenGLSurface;
  }
  return QSurface::OpenGLSurface;
}

void QgsRhiEngine::prepareWindow( QWindow *window, QRhi::Implementation backend )
{
  if ( !window )
    return;

  window->setSurfaceType( surfaceTypeForBackend( backend ) );

#if QT_CONFIG( vulkan )
  if ( backend == QRhi::Vulkan )
    window->setVulkanInstance( sharedVulkanInstance() );
#endif
}

QString QgsRhiEngine::backendName() const
{
  return mRhi ? QString::fromLatin1( mRhi->backendName() ) : QString();
}

QSurfaceFormat QgsRhiEngine::openGlSurfaceFormat()
{
  QSurfaceFormat format = QSurfaceFormat::defaultFormat();

  // Qt's default format is a legacy compatibility context, which on macOS means OpenGL 2.1:
  // no instanced drawing, no multisampled framebuffers and no uniform buffer objects. Ask
  // for a core profile unless the application already set up something at least as capable.
  const bool goodEnough = format.renderableType() == QSurfaceFormat::OpenGL
                          && format.profile() == QSurfaceFormat::CoreProfile
                          && ( format.majorVersion() > 3 || ( format.majorVersion() == 3 && format.minorVersion() >= 3 ) );
  if ( !goodEnough )
  {
    format.setRenderableType( QSurfaceFormat::OpenGL );
    format.setProfile( QSurfaceFormat::CoreProfile );
#ifdef Q_OS_MACOS
    // 4.1 is the last version Apple ever shipped
    format.setVersion( 4, 1 );
#else
    format.setVersion( 4, 3 );
#endif
  }

  if ( format.depthBufferSize() < 24 )
    format.setDepthBufferSize( 24 );
  if ( format.stencilBufferSize() < 8 )
    format.setStencilBufferSize( 8 );

  return format;
}

bool QgsRhiEngine::createRhi( QRhi::Implementation backend )
{
  const QRhi::Flags flags = QRhi::EnablePipelineCacheDataSave;

  switch ( backend )
  {
    case QRhi::Null:
    {
      QRhiNullInitParams params;
      mRhi.reset( QRhi::create( QRhi::Null, &params, flags ) );
      break;
    }

    case QRhi::OpenGLES2:
    {
      const QSurfaceFormat format = openGlSurfaceFormat();

      // an OpenGL device always needs some surface to make a context current on, even
      // when everything it renders goes to a texture
      mFallbackSurface.reset( QRhiGles2InitParams::newFallbackSurface( format ) );
      QRhiGles2InitParams params;
      params.format = format;
      params.fallbackSurface = mFallbackSurface.get();
      if ( mMode == Mode::Window )
        params.window = mWindow;
      mRhi.reset( QRhi::create( QRhi::OpenGLES2, &params, flags ) );
      break;
    }

#if QT_CONFIG( vulkan )
    case QRhi::Vulkan:
    {
      QRhiVulkanInitParams params;
      params.inst = sharedVulkanInstance();
      if ( !params.inst )
      {
        QgsDebugError( u"No Vulkan instance available"_s );
        return false;
      }
      if ( mMode == Mode::Window )
        params.window = mWindow;
      mRhi.reset( QRhi::create( QRhi::Vulkan, &params, flags ) );
      break;
    }
#else
    case QRhi::Vulkan:
      QgsDebugError( u"Qt was built without Vulkan support"_s );
      return false;
#endif

#ifdef Q_OS_WIN
    case QRhi::D3D11:
    {
      QRhiD3D11InitParams params;
      mRhi.reset( QRhi::create( QRhi::D3D11, &params, flags ) );
      break;
    }
    case QRhi::D3D12:
    {
      QRhiD3D12InitParams params;
      mRhi.reset( QRhi::create( QRhi::D3D12, &params, flags ) );
      break;
    }
#else
    case QRhi::D3D11:
    case QRhi::D3D12:
      QgsDebugError( u"Direct3D is only available on Windows"_s );
      return false;
#endif

#if defined( Q_OS_MACOS ) || defined( Q_OS_IOS )
    case QRhi::Metal:
    {
      QRhiMetalInitParams params;
      mRhi.reset( QRhi::create( QRhi::Metal, &params, flags ) );
      break;
    }
#else
    case QRhi::Metal:
      QgsDebugError( u"Metal is only available on Apple platforms"_s );
      return false;
#endif
  }

  if ( !mRhi )
  {
    QgsDebugError( u"Failed to create RHI device for backend %1"_s.arg( QString::fromLatin1( QRhi::backendName( backend ) ) ) );
    mFallbackSurface.reset();
    return false;
  }

  // a sample count the device does not support silently degrades to something it does
  if ( mSampleCount > 1 && !mRhi->supportedSampleCounts().contains( mSampleCount ) )
  {
    const QList<int> supported = mRhi->supportedSampleCounts();
    int best = 1;
    for ( int count : supported )
    {
      if ( count <= mSampleCount && count > best )
        best = count;
    }
    QgsDebugMsgLevel( u"Sample count %1 not supported, using %2"_s.arg( mSampleCount ).arg( best ), 2 );
    mSampleCount = best;
  }

  return true;
}

bool QgsRhiEngine::createForWindow( QWindow *window, QRhi::Implementation backend, int sampleCount )
{
  Q_ASSERT( !mRhi );
  Q_ASSERT( window );

  mMode = Mode::Window;
  mWindow = window;
  mSampleCount = sampleCount;

  if ( !createRhi( backend ) )
    return false;

  mSwapChain.reset( mRhi->newSwapChain() );
  mSwapChain->setWindow( window );
  mSwapChain->setSampleCount( mSampleCount );

  mSwapChainDepthStencil.reset( mRhi->newRenderBuffer( QRhiRenderBuffer::DepthStencil, QSize(), mSampleCount, QRhiRenderBuffer::UsedWithSwapChainOnly ) );
  mSwapChain->setDepthStencil( mSwapChainDepthStencil.get() );

  mSwapChainRenderPassDescriptor.reset( mSwapChain->newCompatibleRenderPassDescriptor() );
  mSwapChain->setRenderPassDescriptor( mSwapChainRenderPassDescriptor.get() );

  if ( !mSwapChain->createOrResize() )
  {
    QgsDebugError( u"Failed to create swapchain - does the window's surface type match the backend?"_s );
    return false;
  }

  mOutputSize = mSwapChain->currentPixelSize();
  return true;
}

bool QgsRhiEngine::createOffscreen( QSize size, QRhi::Implementation backend, int sampleCount )
{
  Q_ASSERT( !mRhi );

  mMode = Mode::Offscreen;
  mOutputSize = size;
  mSampleCount = sampleCount;

  if ( !createRhi( backend ) )
    return false;

  return createOffscreenTarget();
}

bool QgsRhiEngine::createOffscreenTarget()
{
  releaseOffscreenTarget();

  if ( mOutputSize.isEmpty() )
    return false;

  // The multisampled colour attachment is a renderbuffer rather than a texture. Nothing
  // ever samples it - the pass resolves into mResolveTexture and later passes read that -
  // and multisample renderbuffers work everywhere, while multisample textures need OpenGL
  // 4.3. Worse, QRhi's OpenGL backend reports MultisampleTexture as supported on an
  // OpenGL 4.1 core context, which is all Apple ever shipped, and then calls an entry
  // point that is not there.
  bool multisampled = mSampleCount > 1;
  if ( multisampled && !mRhi->isFeatureSupported( QRhi::MultisampleRenderBuffer ) )
  {
    QgsDebugMsgLevel( u"Device cannot multisample offscreen targets, rendering without MSAA"_s, 2 );
    multisampled = false;
    mSampleCount = 1;
  }

  QRhiColorAttachment colorAttachment;

  if ( multisampled )
  {
    mColorRenderBuffer.reset( mRhi->newRenderBuffer( QRhiRenderBuffer::Color, mOutputSize, mSampleCount ) );
    if ( !mColorRenderBuffer->create() )
    {
      QgsDebugError( u"Failed to create offscreen multisample colour buffer"_s );
      releaseOffscreenTarget();
      return false;
    }
    colorAttachment.setRenderBuffer( mColorRenderBuffer.get() );

    // resolving straight out of the render pass is what makes multisampled offscreen
    // rendering cheap - no separate blit node or manual resolve pass is needed
    mResolveTexture.reset( mRhi->newTexture( QRhiTexture::RGBA8, mOutputSize, 1, QRhiTexture::RenderTarget ) );
    if ( !mResolveTexture->create() )
    {
      QgsDebugError( u"Failed to create offscreen resolve texture"_s );
      releaseOffscreenTarget();
      return false;
    }
    colorAttachment.setResolveTexture( mResolveTexture.get() );
  }
  else
  {
    mColorTexture.reset( mRhi->newTexture( QRhiTexture::RGBA8, mOutputSize, 1, QRhiTexture::RenderTarget ) );
    if ( !mColorTexture->create() )
    {
      QgsDebugError( u"Failed to create offscreen colour texture"_s );
      releaseOffscreenTarget();
      return false;
    }
    colorAttachment.setTexture( mColorTexture.get() );
  }

  mDepthStencil.reset( mRhi->newRenderBuffer( QRhiRenderBuffer::DepthStencil, mOutputSize, mSampleCount ) );
  if ( !mDepthStencil->create() )
  {
    QgsDebugError( u"Failed to create offscreen depth buffer"_s );
    releaseOffscreenTarget();
    return false;
  }

  QRhiTextureRenderTargetDescription description( colorAttachment );
  description.setDepthStencilBuffer( mDepthStencil.get() );

  mRenderTarget.reset( mRhi->newTextureRenderTarget( description ) );
  mRenderPassDescriptor.reset( mRenderTarget->newCompatibleRenderPassDescriptor() );
  mRenderTarget->setRenderPassDescriptor( mRenderPassDescriptor.get() );

  if ( !mRenderTarget->create() )
  {
    QgsDebugError( u"Failed to create offscreen render target"_s );
    releaseOffscreenTarget();
    return false;
  }

  return true;
}

void QgsRhiEngine::releaseOffscreenTarget()
{
  mRenderTarget.reset();
  mRenderPassDescriptor.reset();
  mDepthStencil.reset();
  mResolveTexture.reset();
  mColorRenderBuffer.reset();
  mColorTexture.reset();
}

QRhiTexture *QgsRhiEngine::resolvedColorTexture() const
{
  return mResolveTexture ? mResolveTexture.get() : mColorTexture.get();
}

bool QgsRhiEngine::setOutputSize( QSize size )
{
  Q_ASSERT( !mInFrame );

  if ( size == mOutputSize )
    return true;

  mOutputSize = size;

  if ( mMode == Mode::Offscreen )
    return createOffscreenTarget();

  if ( !mSwapChain->createOrResize() )
    return false;

  mOutputSize = mSwapChain->currentPixelSize();
  return true;
}

QRhiRenderTarget *QgsRhiEngine::renderTarget() const
{
  if ( mMode == Mode::Offscreen )
    return mRenderTarget.get();

  return mInFrame ? mSwapChain->currentFrameRenderTarget() : nullptr;
}

QRhiRenderPassDescriptor *QgsRhiEngine::renderPassDescriptor() const
{
  return mMode == Mode::Offscreen ? mRenderPassDescriptor.get() : mSwapChainRenderPassDescriptor.get();
}

bool QgsRhiEngine::beginFrame()
{
  Q_ASSERT( !mInFrame );

  if ( !mRhi )
    return false;

  mReadbackImage = QImage();

  if ( mMode == Mode::Offscreen )
  {
    if ( !mRenderTarget )
      return false;

    if ( mRhi->beginOffscreenFrame( &mCommandBuffer ) != QRhi::FrameOpSuccess )
      return false;

    mInFrame = true;
    return true;
  }

  // the swapchain follows the window, which may have been resized or hidden behind our back
  const QSize surfaceSize = mSwapChain->surfacePixelSize();
  if ( surfaceSize.isEmpty() )
    return false;

  if ( surfaceSize != mSwapChain->currentPixelSize() )
  {
    if ( !mSwapChain->createOrResize() )
      return false;
    mOutputSize = mSwapChain->currentPixelSize();
  }

  const QRhi::FrameOpResult result = mRhi->beginFrame( mSwapChain.get() );
  if ( result == QRhi::FrameOpSwapChainOutOfDate )
  {
    if ( !mSwapChain->createOrResize() )
      return false;
    mOutputSize = mSwapChain->currentPixelSize();
    if ( mRhi->beginFrame( mSwapChain.get() ) != QRhi::FrameOpSuccess )
      return false;
  }
  else if ( result != QRhi::FrameOpSuccess )
  {
    return false;
  }

  mCommandBuffer = mSwapChain->currentFrameCommandBuffer();
  mInFrame = true;
  return true;
}

void QgsRhiEngine::endFrame()
{
  Q_ASSERT( mInFrame );

  if ( mMode == Mode::Offscreen )
  {
    if ( mReadbackEnabled )
    {
      QRhiReadbackResult readback;
      QRhiResourceUpdateBatch *updates = mRhi->nextResourceUpdateBatch();
      updates->readBackTexture( QRhiReadbackDescription( resolvedColorTexture() ), &readback );
      mCommandBuffer->resourceUpdate( updates );

      // an offscreen frame finishes synchronously, so the readback data is ready afterwards
      mRhi->endOffscreenFrame();
      mReadbackImage = imageFromReadback( readback );
    }
    else
    {
      mRhi->endOffscreenFrame();
    }
  }
  else
  {
    mRhi->endFrame( mSwapChain.get() );
  }

  mCommandBuffer = nullptr;
  mInFrame = false;
}

QImage QgsRhiEngine::imageFromReadback( const QRhiReadbackResult &result ) const
{
  if ( result.data.isEmpty() )
    return QImage();

  QImage::Format format = QImage::Format_RGBA8888;
  switch ( result.format )
  {
    case QRhiTexture::RGBA8:
      format = QImage::Format_RGBA8888;
      break;
    case QRhiTexture::BGRA8:
      format = QImage::Format_ARGB32;
      break;
    default:
      QgsDebugError( u"Unexpected readback texture format: %1"_s.arg( static_cast<int>( result.format ) ) );
      return QImage();
  }

  const QImage image( reinterpret_cast<const uchar *>( result.data.constData() ), result.pixelSize.width(), result.pixelSize.height(), format );

  // the image borrows the readback buffer, so it must be copied before returning. Backends
  // whose framebuffers are bottom-up hand back rows in the opposite order to QImage's.
  if ( !mRhi->isYUpInFramebuffer() )
    return image.copy();

#if QT_VERSION >= QT_VERSION_CHECK( 6, 9, 0 )
  return image.flipped( Qt::Vertical );
#else
  return image.mirrored();
#endif
}

void QgsRhiEngine::loadPipelineCache( const QString &path )
{
  if ( !mRhi || !mRhi->isFeatureSupported( QRhi::PipelineCacheDataLoadSave ) )
    return;

  QFile file( path );
  if ( !file.open( QIODevice::ReadOnly ) )
    return;

  mRhi->setPipelineCacheData( file.readAll() );
}

void QgsRhiEngine::savePipelineCache( const QString &path ) const
{
  if ( !mRhi || !mRhi->isFeatureSupported( QRhi::PipelineCacheDataLoadSave ) )
    return;

  const QByteArray data = mRhi->pipelineCacheData();
  if ( data.isEmpty() )
    return;

  QFile file( path );
  if ( !file.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
  {
    QgsDebugError( u"Could not write pipeline cache to %1"_s.arg( path ) );
    return;
  }
  file.write( data );
}
