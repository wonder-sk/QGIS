/***************************************************************************
  qgsbloomrenderpass.h
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

#ifndef QGSBLOOMRENDERPASS_H
#define QGSBLOOMRENDERPASS_H

#include <array>
#include <memory>

#include "qgis_3d.h"
#include "qgsbloommaterial3d.h"
#include "qgsgpurendertarget.h"
#include "qgsrenderpass.h"

#define SIP_NO_FILE

class QgsGraphicsPipelineCache;
class QgsRhiEngine;
class QgsScreenQuad;

/**
 * \ingroup qgis_3d
 * \brief Spreads bright parts of the image into a halo, the way a camera lens does.
 *
 * The port of QgsBloomRenderView, following the CoD: Advanced Warfare approach from ACM
 * Siggraph 2014. The scene is halved five times with a wide 13 tap filter, then rebuilt
 * upwards with a 9 tap tent filter, each level blended additively onto the next. The 13
 * tap filter on the way down is what stops a bright sub-pixel highlight from flickering as
 * the camera moves.
 *
 * The chain lives on whether it is enabled: when it is not, the mip textures are released
 * rather than kept at full size, because five levels of half float colour is real memory.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsBloomRenderPass
{
  public:
    /**
     * How many times the image is halved.
     *
     * "You may go as small as you wish, though 5 or 6 is usually good enough" (Jorge Jimenez)
     */
    static constexpr int MIP_LEVELS = 5;

    QgsBloomRenderPass();
    ~QgsBloomRenderPass();

    QgsBloomRenderPass( const QgsBloomRenderPass & ) = delete;
    QgsBloomRenderPass &operator=( const QgsBloomRenderPass & ) = delete;

    /**
     * Creates the mip chain for a view of \a size, each level half the one before it.
     *
     * Returns TRUE on success, and also when the effect is disabled, in which case nothing
     * is allocated at all.
     */
    bool prepare( QRhi *rhi, QSize size );

    //! Drops every GPU resource. Must be called while the device is still alive.
    void releaseGpuResources();

    //! Sets the HDR colour the halo is built from, normally the forward pass's output
    void setSourceTexture( QRhiTexture *texture );

    //! Sets whether the chain runs at all. A disabled chain holds no textures.
    void setEnabled( bool enabled ) { mEnabled = enabled; }

    //! Returns whether the chain runs
    bool isEnabled() const { return mEnabled; }

    //! Sets how far the upsampling tent filter reaches, in texture coordinates
    void setFilterRadius( float radius );

    //! Returns the finished halo, which is the top level of the chain
    QRhiTexture *bloomTexture() const;

    //! Records the whole chain. Must be called inside a frame. Returns FALSE if it did not run.
    bool render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, const QgsScreenQuad *quad );

  private:
    //! One level of the chain: where it is drawn, and the two steps that draw into it
    struct Level
    {
        QgsGpuRenderTarget target;
        QgsRenderPass downsamplePass;
        QgsRenderPass upsamplePass;
        std::unique_ptr<QgsBloomMaterial3D> downsampleMaterial;
        std::unique_ptr<QgsBloomMaterial3D> upsampleMaterial;
    };

    bool mEnabled = false;
    QSize mBaseSize;
    QRhiTexture *mSourceTexture = nullptr;
    float mFilterRadius = 0.005f;
    std::array<Level, MIP_LEVELS> mLevels;
};

#endif // QGSBLOOMRENDERPASS_H
