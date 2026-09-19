/***************************************************************************
  qgsdepthrenderpass.h
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

#ifndef QGSDEPTHRENDERPASS_H
#define QGSDEPTHRENDERPASS_H

#include <memory>

#include "qgis_3d.h"
#include "qgsdepthrendermaterial3d.h"
#include "qgsgpurendertarget.h"
#include "qgsrenderpass.h"

#include <QImage>
#include <QString>

#define SIP_NO_FILE

using namespace Qt::StringLiterals;


class QgsGraphicsPipelineCache;
class QgsRhiEngine;
class QgsScreenQuad;

/**
 * \ingroup qgis_3d
 * \brief Encodes the depth buffer into a colour image and reads it back to the CPU.
 *
 * The port of QgsDepthRenderView. QGIS turns a mouse position into a point in the scene by
 * looking up the depth of that pixel, which is what every camera navigation gesture is
 * built on, and it needs that value on the CPU.
 *
 * Reading a depth texture back is not portable, so the depth is spread over three colour
 * channels by QgsDepthRenderMaterial3D and an ordinary colour image comes back.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsDepthRenderPass
{
  public:
    QgsDepthRenderPass();
    ~QgsDepthRenderPass();

    QgsDepthRenderPass( const QgsDepthRenderPass & ) = delete;
    QgsDepthRenderPass &operator=( const QgsDepthRenderPass & ) = delete;

    //! Creates the colour attachment the encoded depth is written to. Returns TRUE on success.
    bool prepare( QRhi *rhi, QSize size );

    //! Drops every GPU resource. Must be called while the device is still alive.
    void releaseGpuResources();

    //! Sets the depth texture to encode, normally the forward pass's depth attachment
    void setDepthTexture( QRhiTexture *texture ) { mMaterial.setDepthTexture( texture ); }

    /**
     * Records the pass and queues the readback. Must be called inside a frame.
     *
     * \a quad is the geometry every full screen pass draws. Returns FALSE if nothing was
     * drawn, in which case no readback was queued either - which is also what happens
     * while an earlier readback has not yet been turned into an image by
     * collectReadback(), whether because it is still in flight or because it landed in
     * this very frame.
     */
    bool render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, const QgsScreenQuad *quad );

    /**
     * Returns TRUE while a queued readback has not been written back by the GPU.
     *
     * A frame rendered into a window is only submitted when it ends, so its readback lands
     * a frame or two later. Waiting for it costs a full pipeline stall, so the pass does
     * not: it reports that one is in flight, and image() keeps the previous one until it
     * arrives.
     */
    bool isReadbackPending() const { return mReadback && mReadback->pending; }

    //! Returns TRUE if a readback has arrived and has not been taken yet
    bool hasNewImage() const { return mHasNewImage; }

    //! Returns the collected image and marks it as taken, so hasNewImage() turns FALSE
    QImage takeImage();

    /**
     * Turns whatever the queued readback has produced into image().
     *
     * Must be called after QgsRhiEngine::endFrame(), because that is when an offscreen
     * frame's readback has landed. A frame rendered to a window may take a frame or two
     * longer, in which case this sees the previous frame's depth - which is what Qt3D's
     * QRenderCapture gave QGIS too.
     */
    void collectReadback( const QgsRhiEngine *engine );

    //! Returns the encoded depth image collected by collectReadback()
    QImage image() const { return mImage; }

  private:
    /**
     * What one queued readback needs, kept in a block of its own that outlives the pass.
     *
     * QRhi holds a raw pointer to \a result from the moment the readback is queued until
     * the backend writes it back, which is a frame or two later for a frame presented to
     * a window - and the view can be resized or closed in the meantime. The callback
     * stored inside \a result holds a reference to this block, so the memory the backend
     * writes into stays alive even once the pass has let go of it. That reference is a
     * cycle on purpose: the backend destroys the callback right after calling it, which
     * is what breaks it.
     */
    struct Readback
    {
        QRhiReadbackResult result;
        //! TRUE until the backend has written result back
        bool pending = true;
    };

    QgsGpuRenderTarget mTarget;
    QgsRenderPass mPass { u"depth"_s };
    QgsDepthRenderMaterial3D mMaterial;

    //! The readback queued but not yet turned into mImage, or null when there is none
    std::shared_ptr<Readback> mReadback;
    //! TRUE once a readback has been turned into mImage and nobody has taken it yet
    bool mHasNewImage = false;
    QImage mImage;
};

#endif // QGSDEPTHRENDERPASS_H
