/***************************************************************************
  qgsambientocclusionrenderpass.h
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

#ifndef QGSAMBIENTOCCLUSIONRENDERPASS_H
#define QGSAMBIENTOCCLUSIONRENDERPASS_H

#include "qgis_3d.h"
#include "qgsambientocclusionmaterial3d.h"
#include "qgsgpurendertarget.h"
#include "qgsrenderpass.h"

#include <QString>

#define SIP_NO_FILE

using namespace Qt::StringLiterals;


class QgsGraphicsPipelineCache;
class QgsRhiEngine;
class QgsScreenQuad;

/**
 * \ingroup qgis_3d
 * \brief Works out how shut in each pixel is, and smooths the result.
 *
 * The port of QgsAmbientOcclusionRenderView. Two full screen passes: the first estimates
 * occlusion from the forward pass's depth buffer, the second blurs away the noise pattern
 * the first samples with. The blurred result is what the post processing pass multiplies
 * the scene by.
 *
 * Both work at full resolution, as the Qt3D version did.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsAmbientOcclusionRenderPass
{
  public:
    QgsAmbientOcclusionRenderPass();
    ~QgsAmbientOcclusionRenderPass();

    QgsAmbientOcclusionRenderPass( const QgsAmbientOcclusionRenderPass & ) = delete;
    QgsAmbientOcclusionRenderPass &operator=( const QgsAmbientOcclusionRenderPass & ) = delete;

    //! Creates the occlusion and blur attachments at \a size. Returns TRUE on success.
    bool prepare( QRhi *rhi, QSize size );

    //! Drops every GPU resource. Must be called while the device is still alive.
    void releaseGpuResources();

    //! Sets the depth texture occlusion is estimated from, and updates what samples it
    void setDepthTexture( QRhiTexture *texture );

    //! Sets how strongly occlusion darkens a pixel
    void setIntensity( float intensity ) { mOcclusionMaterial.setIntensity( intensity ); }

    //! Sets the radius of the neighbourhood a pixel is compared against, in world units
    void setRadius( float radius ) { mOcclusionMaterial.setRadius( radius ); }

    //! Sets how much occlusion there has to be before anything is darkened
    void setThreshold( float threshold ) { mOcclusionMaterial.setThreshold( threshold ); }

    //! Sets the tangent of half the camera's vertical field of view, in radians
    void setTanHalfFov( float tanHalfFov ) { mOcclusionMaterial.setTanHalfFov( tanHalfFov ); }

    //! Returns the underlying occlusion pass, for the camera
    QgsRenderPass &occlusionPass() { return mOcclusionPass; }

    //! Returns the blurred occlusion factor the post processing pass reads
    QRhiTexture *blurredTexture() const { return mBlurTarget.colorTexture(); }

    /**
     * Records both passes. Must be called inside a frame.
     *
     * Returns FALSE if nothing was drawn, which includes the case of the forward pass
     * having no sampled depth texture to offer.
     */
    bool render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, const QgsScreenQuad *quad );

  private:
    QgsGpuRenderTarget mOcclusionTarget;
    QgsGpuRenderTarget mBlurTarget;
    QgsRenderPass mOcclusionPass { u"ambient occlusion"_s };
    QgsRenderPass mBlurPass { u"ambient occlusion blur"_s };
    QgsAmbientOcclusionMaterial3D mOcclusionMaterial;
    QgsAmbientOcclusionBlurMaterial3D mBlurMaterial;
    bool mHasDepthTexture = false;
};

#endif // QGSAMBIENTOCCLUSIONRENDERPASS_H
