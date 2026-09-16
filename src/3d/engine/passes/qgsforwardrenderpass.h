/***************************************************************************
  qgsforwardrenderpass.h
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

#ifndef QGSFORWARDRENDERPASS_H
#define QGSFORWARDRENDERPASS_H

#include "qgis_3d.h"
#include "qgsgpurendertarget.h"
#include "qgsrenderpass.h"

#include <QString>

#define SIP_NO_FILE

using namespace Qt::StringLiterals;


class QgsGraphicsPipelineCache;
class QgsRhiEngine;

/**
 * \ingroup qgis_3d
 * \brief Draws the scene itself, into an HDR colour buffer the post processing chain reads.
 *
 * The port of QgsForwardRenderView, and the place where the frame graph shrinks the most.
 * Qt3D needed four branches under a render target selector, each with its own layer filter
 * and render state set, because state was attached to the graph rather than to the object:
 * one branch for opaque geometry, one for the background, and two for transparent geometry,
 * which is drawn once for colour with depth writes off and once for depth with colour
 * writes off.
 *
 * Here every one of those states belongs to the material, so all four are one QRhi render
 * pass and what separated them becomes an ordering. Which group a command is in is said
 * when it is added, and Group is the high bits of its sort key, so within a group the
 * caller's own key still decides the order - grouping by pipeline for opaque geometry,
 * back to front for transparent geometry.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsForwardRenderPass
{
  public:
    //! Which of the forward pass's four ordered groups a command belongs to
    enum class Group
    {
      //! The gradient or skybox behind everything, drawn at the far plane
      Background = 0,

      //! Ordinary solid geometry, depth tested and depth writing
      Opaque = 1,

      //! Transparent geometry, blended without writing depth, back to front
      TransparentColor = 2,

      /**
       * Transparent geometry again, writing only depth.
       *
       * Drawing the same thing twice is what lets transparent surfaces blend with each
       * other while still occluding whatever is drawn after them.
       */
      TransparentDepth = 3,
    };

    QgsForwardRenderPass();
    ~QgsForwardRenderPass();

    QgsForwardRenderPass( const QgsForwardRenderPass & ) = delete;
    QgsForwardRenderPass &operator=( const QgsForwardRenderPass & ) = delete;

    /**
     * Creates the colour and depth attachments at \a size, with \a sampleCount samples.
     *
     * The colour attachment is RGBA16F, because the scene is lit and tone mapped in linear
     * light and bloom needs values above one to pick out. Returns TRUE on success.
     */
    bool prepare( QRhi *rhi, QSize size, int sampleCount = 1 );

    //! Drops every GPU resource. Must be called while the device is still alive.
    void releaseGpuResources();

    //! Returns the target the pass renders into
    QgsGpuRenderTarget *target() { return &mTarget; }

    //! Returns the rendered colour, which the post processing chain reads
    QRhiTexture *colorTexture() const { return mTarget.colorTexture(); }

    /**
     * Returns the rendered depth, or NULLPTR when the target could not give a sampled one.
     *
     * See QgsGpuRenderTarget::create(): multisampling and a sampled depth texture need the
     * device to be able to resolve depth.
     */
    QRhiTexture *depthTexture() const { return mTarget.depthTexture(); }

    //! Returns the underlying pass, for the camera, lights and shadow settings
    QgsRenderPass &renderPass() { return mPass; }

    //! Sets the colour the view is cleared to before anything is drawn
    void setClearColor( const QColor &color ) { mPass.setClearColor( color ); }

    //! Removes every command. Called at the start of each frame, before the scene is walked.
    void clearCommands() { mPass.clearCommands(); }

    /**
     * Appends \a command to \a group.
     *
     * The command's own sort key is kept, and only orders it within its group.
     */
    void addCommand( QgsRenderCommand command, Group group );

    //! Records the pass. Must be called inside a frame. Returns FALSE if nothing was drawn.
    bool render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache );

  private:
    QgsGpuRenderTarget mTarget;
    QgsRenderPass mPass { u"forward"_s };
};

#endif // QGSFORWARDRENDERPASS_H
