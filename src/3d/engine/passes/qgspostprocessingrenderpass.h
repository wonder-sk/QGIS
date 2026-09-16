/***************************************************************************
  qgspostprocessingrenderpass.h
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

#ifndef QGSPOSTPROCESSINGRENDERPASS_H
#define QGSPOSTPROCESSINGRENDERPASS_H

#include "qgis_3d.h"
#include "qgspostprocessingmaterial3d.h"
#include "qgsrenderpass.h"

#include <QString>

#define SIP_NO_FILE

using namespace Qt::StringLiterals;


class QgsGraphicsPipelineCache;
class QgsOverlayQuads;
class QgsRhiEngine;
class QgsScreenQuad;

/**
 * \ingroup qgis_3d
 * \brief Composes everything the earlier passes produced into the finished image.
 *
 * The port of QgsPostprocessingRenderView. Unlike the other passes this one owns no render
 * target: it is the last thing in the frame, so it draws into whatever the frame is for,
 * which is the window's swapchain or the texture an image export is being rendered into.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsPostprocessingRenderPass
{
  public:
    QgsPostprocessingRenderPass();
    ~QgsPostprocessingRenderPass();

    QgsPostprocessingRenderPass( const QgsPostprocessingRenderPass & ) = delete;
    QgsPostprocessingRenderPass &operator=( const QgsPostprocessingRenderPass & ) = delete;

    //! Drops every GPU resource. Must be called while the device is still alive.
    void releaseGpuResources();

    //! Returns the material, which is where every effect is switched on and tuned
    QgsPostprocessingMaterial3D &material() { return mMaterial; }

    //! Returns the underlying pass, for the camera and the shadow settings the tint needs
    QgsRenderPass &renderPass() { return mPass; }

    /**
     * Records the pass into \a target, or into the engine's own output if it is NULLPTR.
     *
     * \a overlays, if given, are drawn on top of the composed image within this same
     * render pass. They ride along here rather than getting a pass of their own because
     * \a target is the frame's final output, which cannot be drawn into twice without
     * either clearing it or paying for a copy.
     *
     * Must be called inside a frame. Returns FALSE if nothing was drawn.
     */
    bool render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, const QgsScreenQuad *quad, QRhiRenderTarget *target = nullptr, const QgsOverlayQuads *overlays = nullptr );

  private:
    QgsRenderPass mPass { u"post processing"_s };
    QgsPostprocessingMaterial3D mMaterial;
};

#endif // QGSPOSTPROCESSINGRENDERPASS_H
