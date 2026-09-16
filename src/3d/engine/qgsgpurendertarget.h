/***************************************************************************
  qgsgpurendertarget.h
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

#ifndef QGSGPURENDERTARGET_H
#define QGSGPURENDERTARGET_H

#include <memory>
#include <rhi/qrhi.h>
#include <vector>

#include "qgis_3d.h"

#include <QSize>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief Textures a pass draws into, and the QRhi render targets that address them.
 *
 * This is what the framegraph's QRenderTarget / QRenderTargetOutput / QTexture2D trio
 * became. A pass renders into one of these and the next pass samples colorTexture() or
 * depthTexture(), which is the whole of what QGIS used Qt3D's render target plumbing for.
 *
 * Two QRhi render targets are built over the same attachments, because whether a pass
 * clears is a property of the render target in QRhi rather than of the draw:
 *
 * - the one rhiTarget() returns clears the attachments at the start of the pass
 * - the one rhiTarget( TRUE ) returns loads whatever is already there
 *
 * So the forward pass draws into the clearing target and the highlight and rubber band
 * passes that follow it draw into the preserving one. This matters on tiled GPUs, where a
 * pass that does not say it wants the previous contents genuinely does not get them.
 *
 * Multisampling resolves into a separate single sample texture, so colorTexture() is
 * always something a later pass can sample.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsGpuRenderTarget
{
  public:
    //! What the target has for depth, and whether other passes can sample it
    enum class DepthMode
    {
      //! No depth attachment at all, for full screen passes that only write colour
      None,

      //! A depth/stencil renderbuffer, enough to depth test against but not to sample
      RenderBuffer,

      /**
       * A depth/stencil texture, which later passes can sample.
       *
       * Ambient occlusion, eye dome lighting and the depth readback used for camera
       * navigation all need this. Together with multisampling it needs the device to be
       * able to resolve depth, see create().
       */
      Texture,
    };

    QgsGpuRenderTarget();
    ~QgsGpuRenderTarget();

    QgsGpuRenderTarget( const QgsGpuRenderTarget & ) = delete;
    QgsGpuRenderTarget &operator=( const QgsGpuRenderTarget & ) = delete;

    //! Sets the format of the colour attachment. The default is eight bits per channel.
    void setColorFormat( QRhiTexture::Format format ) { mColorFormat = format; }

    //! Returns the format of the colour attachment
    QRhiTexture::Format colorFormat() const { return mColorFormat; }

    //! Sets what the target has for depth, and whether other passes can sample it
    void setDepthMode( DepthMode mode ) { mDepthMode = mode; }

    //! Returns what the target has for depth
    DepthMode depthMode() const { return mDepthMode; }

    /**
     * Sets the format used when the depth mode is DepthMode::Texture.
     *
     * The default is a plain 32 bit float depth. A format carrying stencil bits is
     * accepted but does not give a usable stencil buffer everywhere: QRhi's Metal backend
     * attaches only the depth aspect of a depth texture, so anything that needs to stencil
     * has to do without a sampled depth texture, or do without the stencil. The highlight
     * pass takes the second way out and dilates a coverage mask instead.
     */
    void setDepthFormat( QRhiTexture::Format format ) { mDepthFormat = format; }

    /**
     * Sets how many samples each pixel is rendered with.
     *
     * Anything above one resolves into a separate single sample colour texture at the end
     * of the pass, so colorTexture() is unaffected by the choice.
     */
    void setSampleCount( int samples ) { mSampleCount = samples; }

    //! Returns how many samples each pixel is rendered with
    int sampleCount() const { return mSampleCount; }

    /**
     * Sets how many layers the colour attachment has, making it a texture array.
     *
     * Each layer gets its own pair of render targets, which is how the shadow pass renders
     * one cascade per layer into a single array the lighting shaders then sample.
     */
    void setLayerCount( int layers ) { mLayerCount = layers; }

    //! Returns how many layers the colour attachment has
    int layerCount() const { return mLayerCount; }

    /**
     * Sets whether the colour attachment can be read back to the CPU.
     *
     * Only the passes whose result leaves the GPU need this: the depth pass, which feeds
     * camera navigation, and the final pass when an image is being exported.
     */
    void setReadbackEnabled( bool enabled ) { mReadbackEnabled = enabled; }

    /**
     * Creates every attachment and render target at \a size.
     *
     * DepthMode::Texture together with multisampling needs the device to support both
     * multisample textures and resolving depth, which desktop OpenGL does not. Where it
     * cannot, the depth attachment quietly becomes a renderbuffer and depthTexture()
     * returns NULLPTR: the target still renders, but the passes that read depth, meaning
     * ambient occlusion, eye dome lighting and the depth readback, have nothing to read.
     *
     * Returns TRUE on success. Calling this again rebuilds everything.
     */
    bool create( QRhi *rhi, QSize size );

    //! Rebuilds the attachments at \a size, doing nothing if the size has not changed
    bool resize( QRhi *rhi, QSize size );

    //! Destroys every GPU resource. Must be called while the device is still alive.
    void release();

    //! Returns TRUE if create() succeeded and the target can be drawn into
    bool isValid() const { return !mTargets.empty(); }

    //! Returns the size of the attachments in pixels
    QSize size() const { return mSize; }

    /**
     * Returns the colour texture later passes sample.
     *
     * When multisampling this is the resolved single sample texture, never the
     * multisampled surface being rendered into.
     */
    QRhiTexture *colorTexture() const;

    //! Returns the depth texture later passes sample, or NULLPTR unless DepthMode::Texture applies
    QRhiTexture *depthTexture() const;

    /**
     * Returns the render target to draw into.
     *
     * When \a keepContents is TRUE the attachments keep what an earlier pass left in them,
     * otherwise they are cleared to the values the pass was given. \a layer picks a layer
     * of a layered target and is ignored otherwise.
     */
    QRhiTextureRenderTarget *rhiTarget( bool keepContents = false, int layer = 0 ) const;

  private:
    //! One layer's worth of render targets and the descriptors they were built against
    struct Target
    {
        std::unique_ptr<QRhiTextureRenderTarget> clearing;
        std::unique_ptr<QRhiRenderPassDescriptor> clearingDescriptor;
        std::unique_ptr<QRhiTextureRenderTarget> preserving;
        std::unique_ptr<QRhiRenderPassDescriptor> preservingDescriptor;
    };

    bool buildLayerTargets( QRhi *rhi, int layer );

    QRhiTexture::Format mColorFormat = QRhiTexture::RGBA8;
    QRhiTexture::Format mDepthFormat = QRhiTexture::D32F;
    DepthMode mDepthMode = DepthMode::None;
    int mSampleCount = 1;
    int mLayerCount = 1;
    bool mReadbackEnabled = false;
    QSize mSize;

    std::unique_ptr<QRhiTexture> mColorTexture;
    std::unique_ptr<QRhiRenderBuffer> mColorRenderBuffer;
    std::unique_ptr<QRhiTexture> mDepthTexture;
    std::unique_ptr<QRhiTexture> mDepthResolveTexture;
    std::unique_ptr<QRhiRenderBuffer> mDepthRenderBuffer;
    std::vector<Target> mTargets;
};

#endif // QGSGPURENDERTARGET_H
