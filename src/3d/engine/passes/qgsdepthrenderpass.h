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
     * drawn, in which case no readback was queued either.
     */
    bool render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, const QgsScreenQuad *quad );

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
    QgsGpuRenderTarget mTarget;
    QgsRenderPass mPass { u"depth"_s };
    QgsDepthRenderMaterial3D mMaterial;

    // The backend writes into this when the GPU reaches the copy, which can be after the
    // frame that queued it has been submitted. It is a member rather than a local for
    // that reason, and releaseGpuResources() must not run while one is still in flight.
    QRhiReadbackResult mReadback;
    QImage mImage;
};

#endif // QGSDEPTHRENDERPASS_H
