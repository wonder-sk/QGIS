/***************************************************************************
  qgsrhiengine.h
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

#ifndef QGSRHIENGINE_H
#define QGSRHIENGINE_H

#include <memory>
#include <rhi/qrhi.h>

#include "qgis_3d.h"

#include <QImage>
#include <QSize>
#include <QString>
#include <QSurface>
#include <QSurfaceFormat>

#define SIP_NO_FILE

class QOffscreenSurface;
class QWindow;

/**
 * \ingroup qgis_3d
 * \brief Owns a QRhi graphics device and drives the frame loop for a 3D view.
 *
 * The engine works in one of two modes, selected by the create function used:
 *
 * - window mode (createForWindow()) renders through a swapchain attached to a QWindow
 * - offscreen mode (createOffscreen()) renders into a texture render target and can
 *   read the result back as a QImage
 *
 * Unlike the Qt3D based engines, offscreen rendering is the plain path here: it needs
 * no window, no surface and no particular platform plugin, so it works under
 * QT_QPA_PLATFORM=offscreen on every backend.
 *
 * Typical frame:
 *
 * \code
 *   if ( engine.beginFrame() )
 *   {
 *     QRhiCommandBuffer *cb = engine.commandBuffer();
 *     cb->beginPass( engine.renderTarget(), Qt::black, { 1.0f, 0 } );
 *     ...
 *     cb->endPass();
 *     engine.endFrame();
 *   }
 * \endcode
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsRhiEngine
{
  public:
    QgsRhiEngine();
    ~QgsRhiEngine();

    QgsRhiEngine( const QgsRhiEngine & ) = delete;
    QgsRhiEngine &operator=( const QgsRhiEngine & ) = delete;

    /**
     * Returns the graphics backend QGIS should use.
     *
     * The QGIS_3D_RHI_BACKEND environment variable takes precedence, then Qt's own
     * QSG_RHI_BACKEND, and finally a per-platform default is used: Direct3D 11 on
     * Windows, Metal on macOS and iOS, OpenGL everywhere else. Accepted values are
     * "opengl", "vulkan", "metal", "d3d11", "d3d12" and "null".
     */
    static QRhi::Implementation defaultBackend();

    //! Returns the backend matching \a name, or the default backend if the name is not recognised
    static QRhi::Implementation backendFromName( const QString &name, bool *ok = nullptr );

    /**
     * Returns the window surface type a swapchain for \a backend requires.
     *
     * A QWindow must carry the matching surface type before a swapchain can be created
     * for it, otherwise swapchain creation fails.
     */
    static QSurface::SurfaceType surfaceTypeForBackend( QRhi::Implementation backend );

    /**
     * Configures \a window so that a swapchain for \a backend can be created for it.
     *
     * This sets the window's surface type and, on the Vulkan backend, associates the
     * shared QVulkanInstance with the window. Must be called before the window is shown.
     */
    static void prepareWindow( QWindow *window, QRhi::Implementation backend );

    /**
     * Creates the graphics device and a swapchain rendering into \a window.
     *
     * The window must already have been passed to prepareWindow() with the same \a backend.
     * Returns TRUE on success.
     */
    bool createForWindow( QWindow *window, QRhi::Implementation backend, int sampleCount = 1 );

    /**
     * Creates the graphics device and an offscreen render target of the given \a size.
     *
     * When \a sampleCount is greater than one, rendering goes to a multisample texture
     * which is resolved into a single sample texture at the end of the render pass.
     * Returns TRUE on success.
     */
    bool createOffscreen( QSize size, QRhi::Implementation backend, int sampleCount = 1 );

    //! Returns TRUE if the graphics device was created successfully
    bool isValid() const { return mRhi != nullptr; }

    //! Returns the graphics device, or NULLPTR if the engine was not created successfully
    QRhi *rhi() const { return mRhi.get(); }

    //! Returns the name of the graphics backend in use, for diagnostics
    QString backendName() const;

    //! Returns the size of the rendering output in pixels
    QSize outputSize() const { return mOutputSize; }

    //! Returns the number of samples used for multisample anti-aliasing (1 means no MSAA)
    int sampleCount() const { return mSampleCount; }

    /**
     * Resizes the rendering output to \a size.
     *
     * For a window this resizes the swapchain, for an offscreen engine it rebuilds the
     * render target. Must not be called between beginFrame() and endFrame().
     * Returns TRUE on success.
     */
    bool setOutputSize( QSize size );

    /**
     * Returns the render target to draw into.
     *
     * In window mode this is only valid between beginFrame() and endFrame(), because the
     * swapchain hands out a different target per frame.
     */
    QRhiRenderTarget *renderTarget() const;

    /**
     * Returns the render pass descriptor graphics pipelines must be built against.
     *
     * This stays valid across frames, and is what the pipeline cache keys on.
     */
    QRhiRenderPassDescriptor *renderPassDescriptor() const;

    //! Returns the command buffer for the frame being recorded, or NULLPTR outside a frame
    QRhiCommandBuffer *commandBuffer() const { return mCommandBuffer; }

    /**
     * Starts recording a frame.
     *
     * Returns FALSE if no frame can be recorded right now, for instance because the
     * window is not exposed yet, in which case endFrame() must not be called.
     */
    bool beginFrame();

    //! Finishes recording the frame started by beginFrame() and submits it
    void endFrame();

    /**
     * Sets whether the colour output should be read back to an image at the end of each frame.
     *
     * Only supported in offscreen mode. Reading back stalls the pipeline, so this is meant
     * for image export and tests rather than for interactive rendering.
     */
    void setReadbackEnabled( bool enabled ) { mReadbackEnabled = enabled; }

    //! Returns whether colour readback is enabled
    bool isReadbackEnabled() const { return mReadbackEnabled; }

    /**
     * Returns the image read back at the end of the last frame.
     *
     * Only non-null when setReadbackEnabled() was TRUE for that frame. The image is
     * already flipped to the usual top-down orientation and uses QImage::Format_RGBA8888.
     */
    QImage readbackImage() const { return mReadbackImage; }

    /**
     * Returns the colour texture the offscreen engine renders into.
     *
     * With multisampling this is the multisample texture, and later passes should sample
     * resolvedColorTexture() instead. Returns NULLPTR in window mode, and also when the
     * device can only multisample into a renderbuffer, in which case the resolved texture
     * is the only one there is.
     */
    QRhiTexture *colorTexture() const { return mColorTexture.get(); }

    /**
     * Returns the single sample colour texture holding the offscreen result.
     *
     * Without multisampling this is the same as colorTexture(). Returns NULLPTR in window mode.
     */
    QRhiTexture *resolvedColorTexture() const;

    //! Returns the depth/stencil buffer of the offscreen render target, or NULLPTR in window mode
    QRhiRenderBuffer *depthStencilBuffer() const { return mDepthStencil.get(); }

    /**
     * Loads the graphics pipeline cache previously written by savePipelineCache().
     *
     * Restoring the cache avoids re-compiling pipelines that were already built in an
     * earlier session. Missing or stale cache data is ignored, it is only an optimisation.
     */
    void loadPipelineCache( const QString &path );

    //! Writes the graphics pipeline cache to \a path so that the next session can reuse it
    void savePipelineCache( const QString &path ) const;

    /**
     * Converts a finished texture readback into a QImage, right way up and in a sane format.
     *
     * Passes that read their own output back, such as the depth pass feeding camera
     * navigation, queue their readback themselves and then come here to make sense of it,
     * rather than each repeating the format mapping and the framebuffer flip.
     */
    QImage imageFromReadback( const QRhiReadbackResult &result ) const;

  private:
    enum class Mode
    {
      Window,
      Offscreen
    };

    /**
     * Returns the surface format the OpenGL backend should use.
     *
     * Qt's default format is a legacy compatibility context, which is not enough for the
     * features the engine relies on, so a core profile is requested unless the application
     * already installed a suitable default.
     */
    static QSurfaceFormat openGlSurfaceFormat();

    bool createRhi( QRhi::Implementation backend );
    bool createOffscreenTarget();
    void releaseOffscreenTarget();

    Mode mMode = Mode::Offscreen;
    std::unique_ptr<QRhi> mRhi;
    QSize mOutputSize;
    int mSampleCount = 1;

    // window mode
    QWindow *mWindow = nullptr;
    std::unique_ptr<QRhiSwapChain> mSwapChain;
    std::unique_ptr<QRhiRenderBuffer> mSwapChainDepthStencil;
    std::unique_ptr<QRhiRenderPassDescriptor> mSwapChainRenderPassDescriptor;

    // offscreen mode
    std::unique_ptr<QRhiTexture> mColorTexture;
    std::unique_ptr<QRhiRenderBuffer> mColorRenderBuffer;
    std::unique_ptr<QRhiTexture> mResolveTexture;
    std::unique_ptr<QRhiRenderBuffer> mDepthStencil;
    std::unique_ptr<QRhiTextureRenderTarget> mRenderTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> mRenderPassDescriptor;

    // fallback surface needed to create an OpenGL device without a window
    std::unique_ptr<QOffscreenSurface> mFallbackSurface;

    QRhiCommandBuffer *mCommandBuffer = nullptr;
    bool mInFrame = false;
    bool mReadbackEnabled = false;
    QImage mReadbackImage;
};

#endif // QGSRHIENGINE_H
