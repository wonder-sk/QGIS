/***************************************************************************
  qgsshadowrenderpass.h
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

#ifndef QGSSHADOWRENDERPASS_H
#define QGSSHADOWRENDERPASS_H

#include <array>
#include <vector>

#include "qgis_3d.h"
#include "qgsgpurendertarget.h"
#include "qgsrenderpass.h"

#include <QVector3D>

#define SIP_NO_FILE

class QgsGraphicsPipelineCache;
class QgsRhiEngine;

/**
 * \ingroup qgis_3d
 * \brief Renders the scene from the light, once per cascade, into the shadow map.
 *
 * The port of QgsShadowRenderView together with the cascade maths that used to live in
 * QgsPostprocessingEntity::updateShadowSettings(). Putting the two in one place is the
 * point: the matrices the map is rendered with and the matrices the lighting shaders look
 * it up with have to agree exactly, and in the Qt3D version they were computed in one
 * class and rendered by another.
 *
 * The cascades are fitted to bounding spheres of the view frustum slices and snapped to
 * texels, which is what stops shadow edges from shimmering as the camera moves.
 *
 * The map itself is a single channel float texture array holding gl_FragCoord.z, one layer
 * per cascade, rather than a depth texture array. That is forced by QRhi: a render target
 * can select a layer of a colour attachment but not of a depth attachment.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsShadowRenderPass
{
  public:
    //! How many cascades the view frustum is split into, matching Qgs3D::NUM_SHADOW_CASCADES
    static constexpr int NUM_CASCADES = QgsShadowUniforms::NUM_CASCADES;

    QgsShadowRenderPass();
    ~QgsShadowRenderPass();

    QgsShadowRenderPass( const QgsShadowRenderPass & ) = delete;
    QgsShadowRenderPass &operator=( const QgsShadowRenderPass & ) = delete;

    /**
     * Creates a \a resolution by \a resolution shadow map for each cascade.
     *
     * Returns TRUE on success. Calling this with the resolution it already has does nothing.
     */
    bool prepare( QRhi *rhi, int resolution );

    //! Returns the side of one cascade's shadow map in texels
    int resolution() const { return mResolution; }

    //! Drops every GPU resource. Must be called while the device is still alive.
    void releaseGpuResources();

    /**
     * Fits the cascades to the view frustum and works out every matrix the frame needs.
     *
     * \a cameraView, \a fieldOfView, \a aspectRatio, \a nearPlane and \a farPlane describe
     * the camera the scene is being rendered with, \a lightDirection is the direction the
     * shadow casting light shines in, and \a lightIndex says which light of the pass's
     * light block that is, so that the shaders only shadow the one.
     *
     * \a maximumDistance caps how far from the camera shadows are rendered at all, which
     * is what keeps the cascades from being spread over a horizon's worth of scene.
     */
    void updateCascades(
      QRhi *rhi, const QMatrix4x4 &cameraView, float fieldOfView, float aspectRatio, float nearPlane, float farPlane, const QVector3D &lightDirection, int lightIndex, float shadowBias, float maximumDistance
    );

    //! Sets whether the lighting shaders shade each cascade in its own colour, for testing
    void setShowCascadeSplits( bool enabled ) { mUniforms.showCascadeSplits = enabled ? 1 : 0; }

    //! Removes every caster. Called at the start of each frame, before the scene is walked.
    void clearCommands() { mCasters.clear(); }

    /**
     * Appends something that casts a shadow.
     *
     * The command's material should be a QgsShadowCasterMaterial3D rather than the one the
     * object is drawn with in the forward pass: nothing about a surface but its depth
     * reaches the shadow map.
     */
    void addCommand( const QgsRenderCommand &command ) { mCasters.push_back( command ); }

    //! Returns the casters appended so far
    const std::vector<QgsRenderCommand> &commands() const { return mCasters; }

    //! Records every cascade. Must be called inside a frame. Returns FALSE if nothing was drawn.
    bool render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache );

    /**
     * Returns the shadow map, a float texture array with one layer per cascade.
     *
     * This and uniforms() are what a pass drawing lit geometry passes to
     * QgsRenderPass::setShadowMap().
     */
    QRhiTexture *shadowMap() const { return mTarget.colorTexture(); }

    //! Returns the matrices and settings the lighting shaders need to read the shadow map
    const QgsShadowUniforms &uniforms() const { return mUniforms; }

  private:
    int mResolution = 0;
    QgsGpuRenderTarget mTarget;
    std::array<QgsRenderPass, NUM_CASCADES> mCascadePasses;
    std::vector<QgsRenderCommand> mCasters;
    QgsShadowUniforms mUniforms;
};

#endif // QGSSHADOWRENDERPASS_H
