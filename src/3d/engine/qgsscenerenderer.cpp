/***************************************************************************
  qgsscenerenderer.cpp
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

#include "qgsscenerenderer.h"

#include <cmath>

#include "qgsrhiengine.h"

using namespace Qt::StringLiterals;

QgsSceneRenderer::QgsSceneRenderer()
{
  // rubber bands are drawn over the scene in the order they were created, so there is
  // nothing for a sort to improve
  mRubberBandPass.setSortEnabled( false );
}

QgsSceneRenderer::~QgsSceneRenderer() = default;

void QgsSceneRenderer::setSampleCount( int samples )
{
  mSampleCount = std::max( 1, samples );
}

bool QgsSceneRenderer::prepare( QgsRhiEngine *engine, QSize size )
{
  if ( !engine || !engine->isValid() || size.isEmpty() )
    return false;

  QRhi *rhi = engine->rhi();
  mSize = size;

  if ( !mForwardPass.prepare( rhi, size, mSampleCount ) )
    return false;

  // The two optional passes that are sized like the view have to be rebuilt at the new
  // size, and both sample the depth attachment the forward pass has just replaced.
  // updateOptionalPasses() below does the rebuilding; this is what asks it to.
  mAmbientOcclusionPass.releaseGpuResources();
  mAmbientOcclusionPassPrepared = false;
  mDepthPass.releaseGpuResources();
  mDepthPassPrepared = false;

  if ( !mHighlightsPass.prepare( rhi, size ) )
    return false;

  if ( !mBloomPass.prepare( rhi, size ) )
    return false;

  // Every one of these is an attachment of a target that has just been rebuilt, so they
  // have to be pointed at the new textures rather than the ones that were freed.
  QRhiTexture *depthTexture = mForwardPass.depthTexture();
  mDepthPass.setDepthTexture( depthTexture );
  mAmbientOcclusionPass.setDepthTexture( depthTexture );
  mBloomPass.setSourceTexture( mForwardPass.colorTexture() );

  if ( !updateOptionalPasses( rhi ) )
    return false;

  QgsPostprocessingMaterial3D &postMaterial = mPostprocessingPass.material();
  postMaterial.setColorTexture( mForwardPass.colorTexture() );
  postMaterial.setDepthTexture( depthTexture );
  postMaterial.setAmbientOcclusionTexture( mAmbientOcclusionPass.blurredTexture() );
  postMaterial.setBloomTexture( mBloomPass.bloomTexture() );

  // Eye dome lighting and ambient occlusion both read the depth buffer, so multisampling
  // on a backend that cannot resolve depth takes them away. Say so rather than letting
  // them quietly sample the placeholder texture.
  if ( !depthTexture )
  {
    postMaterial.setEyeDomeLightingEnabled( false );
    postMaterial.setAmbientOcclusionEnabled( false );
  }

  return true;
}

void QgsSceneRenderer::releaseGpuResources()
{
  mShadowPass.releaseGpuResources();
  mForwardPass.releaseGpuResources();
  mHighlightsPass.releaseGpuResources();
  mRubberBandPass.releaseGpuResources();
  mDepthPass.releaseGpuResources();
  mAmbientOcclusionPass.releaseGpuResources();
  mBloomPass.releaseGpuResources();
  mPostprocessingPass.releaseGpuResources();
  mOverlays.releaseGpuResources();
  mScreenQuad.releaseGpuResources();
  mPipelineCache.clear();
  mSize = QSize();

  mPreparedShadowResolution = 0;
  mAmbientOcclusionPassPrepared = false;
  mDepthPassPrepared = false;
}

bool QgsSceneRenderer::updateOptionalPasses( QRhi *rhi )
{
  if ( !rhi || mSize.isEmpty() )
    return false;

  const int shadowResolution = mShadowsEnabled ? mShadowResolution : 0;
  if ( shadowResolution != mPreparedShadowResolution )
  {
    mShadowPass.releaseGpuResources();
    mPreparedShadowResolution = 0;
    if ( shadowResolution > 0 )
    {
      if ( !mShadowPass.prepare( rhi, shadowResolution ) )
        return false;
      mPreparedShadowResolution = shadowResolution;
    }
  }

  if ( mAmbientOcclusionEnabled != mAmbientOcclusionPassPrepared )
  {
    if ( mAmbientOcclusionEnabled )
    {
      if ( !mAmbientOcclusionPass.prepare( rhi, mSize ) )
        return false;
      mAmbientOcclusionPass.setDepthTexture( mForwardPass.depthTexture() );
    }
    else
    {
      mAmbientOcclusionPass.releaseGpuResources();
    }
    mAmbientOcclusionPassPrepared = mAmbientOcclusionEnabled;
  }

  if ( mDepthReadbackEnabled != mDepthPassPrepared )
  {
    if ( mDepthReadbackEnabled )
    {
      if ( !mDepthPass.prepare( rhi, mSize ) )
        return false;
      mDepthPass.setDepthTexture( mForwardPass.depthTexture() );
    }
    else
    {
      mDepthPass.releaseGpuResources();
    }
    mDepthPassPrepared = mDepthReadbackEnabled;
  }

  return true;
}

void QgsSceneRenderer::setCamera( const QMatrix4x4 &view, const QMatrix4x4 &projection, const QVector3D &eyePosition, float fieldOfView, float nearPlane, float farPlane )
{
  mView = view;
  mProjection = projection;
  mEyePosition = eyePosition;
  mFieldOfView = fieldOfView;
  mNearPlane = nearPlane;
  mFarPlane = farPlane;

  updatePassCameras();
}

void QgsSceneRenderer::updatePassCameras()
{
  // Every pass that draws something in world space, and every full screen pass that turns
  // a depth back into a position, sees the same camera. The full screen passes do not use
  // it to transform anything - their vertices are already in clip space - they use its
  // inverses to undo the projection.
  for ( QgsRenderPass *pass :
        { &mForwardPass.renderPass(), &mHighlightsPass.fillPass(), &mHighlightsPass.maskPass(), &mRubberBandPass, &mAmbientOcclusionPass.occlusionPass(), &mPostprocessingPass.renderPass() } )
  {
    pass->setCamera( mView, mProjection );
    pass->setEyePosition( mEyePosition );
    pass->setClipPlanes( mNearPlane, mFarPlane );
  }

  mAmbientOcclusionPass.setTanHalfFov( std::tan( mFieldOfView * 0.5f * static_cast<float>( M_PI ) / 180.0f ) );
}

void QgsSceneRenderer::setLights( const QgsLightUniforms &lights )
{
  mLights = lights;
  mForwardPass.renderPass().setLights( lights );
  mHighlightsPass.fillPass().setLights( lights );
  mRubberBandPass.setLights( lights );
}

void QgsSceneRenderer::setShadowsEnabled( bool enabled, const QVector3D &lightDirection, int lightIndex, int resolution, float maximumDistance, float bias )
{
  mShadowsEnabled = enabled;
  mShadowLightDirection = lightDirection;
  mShadowLightIndex = lightIndex;
  mShadowResolution = resolution;
  mShadowMaximumDistance = maximumDistance;
  mShadowBias = bias;
}

void QgsSceneRenderer::setShowCascadeSplits( bool enabled )
{
  mShadowPass.setShowCascadeSplits( enabled );
}

void QgsSceneRenderer::setAmbientOcclusionEnabled( bool enabled, float intensity, float radius, float threshold )
{
  mAmbientOcclusionEnabled = enabled;
  mAmbientOcclusionPass.setIntensity( intensity );
  mAmbientOcclusionPass.setRadius( radius );
  mAmbientOcclusionPass.setThreshold( threshold );
  mPostprocessingPass.material().setAmbientOcclusionEnabled( enabled );
}

void QgsSceneRenderer::setBloomEnabled( bool enabled, float intensity, float radius )
{
  mBloomPass.setEnabled( enabled );
  mBloomPass.setFilterRadius( radius );
  mPostprocessingPass.material().setBloomEnabled( enabled );
  mPostprocessingPass.material().setBloomFactor( intensity );
}

void QgsSceneRenderer::setEyeDomeLightingEnabled( bool enabled, float strength, float distance )
{
  QgsPostprocessingMaterial3D &material = mPostprocessingPass.material();
  material.setEyeDomeLightingEnabled( enabled );
  material.setEyeDomeLightingStrength( strength );
  material.setEyeDomeLightingDistance( distance );
}

void QgsSceneRenderer::setColorGrading( float exposureAdjustment, Qgis::ToneMappingMethod toneMapping )
{
  mPostprocessingPass.material().setExposureAdjustment( exposureAdjustment );
  mPostprocessingPass.material().setToneMapping( toneMapping );
}

void QgsSceneRenderer::clearCommands()
{
  mShadowPass.clearCommands();
  mForwardPass.clearCommands();
  mHighlightsPass.clearCommands();
  mRubberBandPass.clearCommands();
}

bool QgsSceneRenderer::renderFrame( QgsRhiEngine *engine, QRhiRenderTarget *output )
{
  if ( !engine || !engine->commandBuffer() )
    return false;

  // A setting may have been switched on since the last frame, in which case its pass owns
  // no target yet. Without this a pass that is asked to render does nothing at all: the
  // depth readback silently returns a null image, and camera navigation, which waits for
  // that image before it moves anything, stops responding.
  if ( !updateOptionalPasses( engine->rhi() ) )
    return false;

  QgsGraphicsPipelineCache *cache = &mPipelineCache;

  // The shadow map is filled first, because every lit material in the forward pass samples
  // it. Nothing after this point writes to it.
  QRhiTexture *shadowMap = nullptr;
  QgsShadowUniforms shadowUniforms;
  if ( mShadowsEnabled && !mShadowPass.commands().empty() )
  {
    const float aspectRatio = mSize.height() > 0 ? static_cast<float>( mSize.width() ) / static_cast<float>( mSize.height() ) : 1.0f;
    mShadowPass.updateCascades( engine->rhi(), mView, mFieldOfView, aspectRatio, mNearPlane, mFarPlane, mShadowLightDirection, mShadowLightIndex, mShadowBias, mShadowMaximumDistance );

    if ( mShadowPass.render( engine, cache ) )
    {
      shadowMap = mShadowPass.shadowMap();
      shadowUniforms = mShadowPass.uniforms();
    }
  }

  for ( QgsRenderPass *pass : { &mForwardPass.renderPass(), &mHighlightsPass.fillPass(), &mRubberBandPass, &mPostprocessingPass.renderPass() } )
    pass->setShadowMap( shadowMap, shadowUniforms );

  mForwardPass.render( engine, cache );

  // Highlights and rubber bands go on top of the scene, into the same attachments, so
  // they ask for the contents to be kept rather than cleared.
  mHighlightsPass.render( engine, cache, &mScreenQuad, mForwardPass.target()->rhiTarget( true ) );

  if ( !mRubberBandPass.commands().empty() )
    mRubberBandPass.execute( engine, cache, mForwardPass.target()->rhiTarget( true ) );

  if ( mDepthReadbackEnabled )
    mDepthPass.render( engine, cache, &mScreenQuad );

  if ( mAmbientOcclusionEnabled )
    mAmbientOcclusionPass.render( engine, cache, &mScreenQuad );

  if ( mBloomPass.isEnabled() )
    mBloomPass.render( engine, cache, &mScreenQuad );

  // and finally everything is composed into the frame's real output, with the overlays
  // drawn on top within the same render pass
  return mPostprocessingPass.render( engine, cache, &mScreenQuad, output, &mOverlays );
}

void QgsSceneRenderer::finishFrame( const QgsRhiEngine *engine )
{
  if ( mDepthReadbackEnabled )
    mDepthPass.collectReadback( engine );
}
