/***************************************************************************
  qgsscenerenderer.h
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

#ifndef QGSSCENERENDERER_H
#define QGSSCENERENDERER_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsambientocclusionrenderpass.h"
#include "qgsbloomrenderpass.h"
#include "qgsdepthrenderpass.h"
#include "qgsforwardrenderpass.h"
#include "qgsgraphicspipelinecache.h"
#include "qgshighlightsrenderpass.h"
#include "qgsoverlayquads.h"
#include "qgspostprocessingrenderpass.h"
#include "qgsrenderpass.h"
#include "qgsscreenquad.h"
#include "qgsshadowrenderpass.h"

#include <QSize>
#include <QString>

#define SIP_NO_FILE

using namespace Qt::StringLiterals;


class QgsRhiEngine;

/**
 * \ingroup qgis_3d
 * \brief One frame of a 3D scene, written as a sequence of passes rather than a graph.
 *
 * This is what replaces QgsFrameGraph. Under Qt3D the order things happened in was a
 * property of a tree of frame graph nodes, and which objects took part in a pass was
 * decided by matching layers against filters. Here renderFrame() is an ordinary function
 * that can be read top to bottom and stepped through, and an object is in a pass because
 * a command for it was appended to that pass.
 *
 * The passes and what they hand each other:
 *
 * \code
 *   shadow      -> shadow map, sampled by every lit material in the forward pass
 *   forward     -> HDR colour + depth, with highlights and rubber bands drawn over them
 *   depth       -> the depth buffer as a colour image, read back for camera navigation
 *   occlusion   -> how shut in each pixel is, from the depth buffer
 *   bloom       -> a halo around the bright parts of the colour buffer
 *   post        -> all of the above composed, tone mapped, with the overlays on top
 * \endcode
 *
 * Producing the commands is not this class's job: whatever walks the scene appends them to
 * forwardPass() and shadowPass() each frame, and clearCommands() empties them again.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsSceneRenderer
{
  public:
    QgsSceneRenderer();
    ~QgsSceneRenderer();

    QgsSceneRenderer( const QgsSceneRenderer & ) = delete;
    QgsSceneRenderer &operator=( const QgsSceneRenderer & ) = delete;

    /**
     * Creates or resizes everything a frame of \a size pixels needs.
     *
     * Must be called before the first renderFrame(), and again whenever the view is
     * resized or an effect is switched on or off, since that changes what has to exist.
     * Returns TRUE on success.
     */
    bool prepare( QgsRhiEngine *engine, QSize size );

    //! Returns the size the passes were last prepared at
    QSize size() const { return mSize; }

    /**
     * Sets how many samples the forward pass renders each pixel with.
     *
     * Takes effect at the next prepare(). Multisampling costs the depth dependent effects
     * on backends that cannot resolve depth, see QgsGpuRenderTarget::create().
     */
    void setSampleCount( int samples );

    //! Returns how many samples the forward pass renders each pixel with
    int sampleCount() const { return mSampleCount; }

    //! Drops every GPU resource. Must be called while the device is still alive.
    void releaseGpuResources();

    /**
     * Sets the camera every pass renders or reconstructs positions from.
     *
     * \a projection is the plain projection matrix, without any clip space correction.
     * The field of view and the planes are passed separately because the post processing
     * and occlusion passes need them as numbers rather than as a matrix.
     */
    void setCamera( const QMatrix4x4 &view, const QMatrix4x4 &projection, const QVector3D &eyePosition, float fieldOfView, float nearPlane, float farPlane );

    //! Sets the light sources every lit material shades with
    void setLights( const QgsLightUniforms &lights );

    //! Sets the colour the view is cleared to before anything is drawn
    void setClearColor( const QColor &color ) { mForwardPass.setClearColor( color ); }

    /**
     * Turns shadow casting on and points it at one of the scene's lights.
     *
     * \a lightDirection is the direction that light shines in and \a lightIndex says which
     * light of the block set by setLights() it is. \a resolution is the side of one
     * cascade's shadow map, \a maximumDistance how far from the camera shadows are drawn
     * at all, and \a bias how far a caster is pushed away from the light to stop a surface
     * shadowing itself.
     */
    void setShadowsEnabled( bool enabled, const QVector3D &lightDirection = QVector3D(), int lightIndex = 0, int resolution = 2048, float maximumDistance = 500.0f, float bias = 0.00001f );

    //! Returns whether shadows are being rendered
    bool shadowsEnabled() const { return mShadowsEnabled; }

    //! Sets whether each shadow cascade is shaded in its own colour. For testing only.
    void setShowCascadeSplits( bool enabled );

    //! Turns screen space ambient occlusion on, with the given strength, radius and threshold
    void setAmbientOcclusionEnabled( bool enabled, float intensity = 0.5f, float radius = 25.0f, float threshold = 0.5f );

    //! Turns the bloom effect on, with the given strength and filter radius
    void setBloomEnabled( bool enabled, float intensity = 0.05f, float radius = 0.005f );

    //! Turns eye dome lighting on, which shades an unlit point cloud by local depth changes
    void setEyeDomeLightingEnabled( bool enabled, float strength = 1000.0f, float distance = 2.0f );

    //! Sets the exposure adjustment in stops and how the result is brought into display range
    void setColorGrading( float exposureAdjustment, Qgis::ToneMappingMethod toneMapping );

    /**
     * Sets whether the depth buffer is encoded and read back each frame.
     *
     * Camera navigation needs it; a frame being rendered for an image does not, and the
     * readback is a stall, so it is off by default.
     */
    void setDepthReadbackEnabled( bool enabled ) { mDepthReadbackEnabled = enabled; }

    //! Returns the encoded depth image of the most recent frame, or a null image
    QImage depthImage() const { return mDepthPass.image(); }

    //! Returns the pass the scene itself is drawn in
    QgsForwardRenderPass &forwardPass() { return mForwardPass; }

    //! Returns the pass shadow casters are drawn in
    QgsShadowRenderPass &shadowPass() { return mShadowPass; }

    /**
     * Returns the pass that draws identified features over the scene.
     *
     * It draws into the forward pass's attachments without clearing them, so the fill and
     * the band around it land on top of the rendered scene.
     */
    QgsHighlightsRenderPass &highlightsPass() { return mHighlightsPass; }

    //! Returns the pass that draws rubber bands over everything else in the scene
    QgsRenderPass &rubberBandPass() { return mRubberBandPass; }

    //! Returns the textures drawn on top of the finished image
    QgsOverlayQuads &overlays() { return mOverlays; }

    //! Returns the pipeline cache the passes share
    QgsGraphicsPipelineCache *pipelineCache() { return &mPipelineCache; }

    //! Empties every pass's command list. Called at the start of each frame.
    void clearCommands();

    /**
     * Renders one frame, ending in \a output or in the engine's own output if it is NULLPTR.
     *
     * Must be called between QgsRhiEngine::beginFrame() and QgsRhiEngine::endFrame().
     * Returns FALSE if the final pass could not be recorded.
     */
    bool renderFrame( QgsRhiEngine *engine, QRhiRenderTarget *output = nullptr );

    /**
     * Collects anything the frame asked to be read back.
     *
     * Must be called after QgsRhiEngine::endFrame(), which is when an offscreen frame's
     * readbacks have landed.
     */
    void finishFrame( const QgsRhiEngine *engine );

  private:
    //! Sets the camera on the passes that need it, and the derived values they need with it
    void updatePassCameras();

    /**
     * Brings the optional passes into line with the settings that switch them on.
     *
     * Shadows, ambient occlusion and the depth readback each own a render target that only
     * needs to exist while that feature is on, and the feature can be switched between two
     * frames - the map settings are read into the renderer every frame, while prepare()
     * only runs when the view is created or resized. Returns FALSE if a pass that is now
     * needed could not be built.
     */
    bool updateOptionalPasses( QRhi *rhi );

    QSize mSize;
    int mSampleCount = 1;

    QMatrix4x4 mView;
    QMatrix4x4 mProjection;
    QVector3D mEyePosition;
    float mFieldOfView = 45.0f;
    float mNearPlane = 0.1f;
    float mFarPlane = 1000.0f;
    QgsLightUniforms mLights;

    bool mShadowsEnabled = false;
    QVector3D mShadowLightDirection { 0.0f, -1.0f, 0.0f };
    int mShadowLightIndex = 0;
    int mShadowResolution = 2048;
    float mShadowMaximumDistance = 500.0f;
    float mShadowBias = 0.00001f;

    bool mAmbientOcclusionEnabled = false;
    bool mDepthReadbackEnabled = false;

    // What updateOptionalPasses() has actually built, against which the three settings
    // above are compared. The shadow map is tracked by its resolution because that is a
    // setting too; zero means the pass is not built.
    int mPreparedShadowResolution = 0;
    bool mAmbientOcclusionPassPrepared = false;
    bool mDepthPassPrepared = false;

    QgsScreenQuad mScreenQuad;
    QgsGraphicsPipelineCache mPipelineCache;

    QgsShadowRenderPass mShadowPass;
    QgsForwardRenderPass mForwardPass;
    QgsHighlightsRenderPass mHighlightsPass;
    QgsRenderPass mRubberBandPass { u"rubber bands"_s };
    QgsDepthRenderPass mDepthPass;
    QgsAmbientOcclusionRenderPass mAmbientOcclusionPass;
    QgsBloomRenderPass mBloomPass;
    QgsPostprocessingRenderPass mPostprocessingPass;
    QgsOverlayQuads mOverlays;
};

#endif // QGSSCENERENDERER_H
