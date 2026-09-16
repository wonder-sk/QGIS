/***************************************************************************
  qgsshadowrenderpass.cpp
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

#include "qgsshadowrenderpass.h"

#include <cmath>

#include "qgs3dutils.h"
#include "qgsgraphicspipelinecache.h"
#include "qgsrhiengine.h"

#include <QColor>
#include <QString>

using namespace Qt::StringLiterals;

namespace
{
  /**
   * How far behind a cascade's own slice its near plane is pulled, in world units.
   *
   * Without this, an object standing between the light and the slice is outside the
   * cascade's frustum and casts no shadow at all, so tall shadows lose their tops.
   */
  constexpr float NEAR_PLANE_RETREAT = 5000.0f;

  /**
   * How strongly the cascade splits are biased towards the camera.
   *
   * The "practical split scheme" blends a logarithmic and a uniform split; a high lambda
   * suits the near to far ratios QGIS 3D maps actually use, which run from well under a
   * metre to a couple of kilometres. This matches Cesium's choice.
   */
  constexpr float PRACTICAL_SPLIT_SCHEME_LAMBDA = 0.9f;
} // namespace

QgsShadowRenderPass::QgsShadowRenderPass() = default;

QgsShadowRenderPass::~QgsShadowRenderPass() = default;

bool QgsShadowRenderPass::prepare( QRhi *rhi, int resolution )
{
  if ( resolution <= 0 )
    return false;

  if ( mTarget.isValid() && resolution == mResolution )
    return true;

  // one layer per cascade, and a single channel float because what is stored is a depth
  // rather than a colour. There is no MSAA here: a shadow map is sampled sixteen times per
  // fragment anyway, so the filtering is already doing the smoothing.
  mTarget.setColorFormat( QRhiTexture::R32F );
  mTarget.setLayerCount( NUM_CASCADES );
  mTarget.setDepthMode( QgsGpuRenderTarget::DepthMode::RenderBuffer );
  mTarget.setSampleCount( 1 );

  if ( !mTarget.create( rhi, QSize( resolution, resolution ) ) )
    return false;

  mResolution = resolution;

  for ( int i = 0; i < NUM_CASCADES; ++i )
  {
    QgsRenderPass &pass = mCascadePasses[static_cast<size_t>( i )];
    // anything the cascade does not cover has to read as infinitely far away, otherwise
    // the world outside the map would shadow itself
    pass.setClearColor( QColor::fromRgbF( 1.0f, 1.0f, 1.0f, 1.0f ) );
    pass.setDepthClearValue( 1.0f );
    // casters are drawn with one material, so there is nothing for a sort to improve
    pass.setSortEnabled( false );
  }

  return true;
}

void QgsShadowRenderPass::releaseGpuResources()
{
  for ( QgsRenderPass &pass : mCascadePasses )
    pass.releaseGpuResources();
  mTarget.release();
  mResolution = 0;
}

void QgsShadowRenderPass::updateCascades(
  QRhi *rhi, const QMatrix4x4 &cameraView, float fieldOfView, float aspectRatio, float nearPlane, float farPlane, const QVector3D &lightDirection, int lightIndex, float shadowBias, float maximumDistance
)
{
  // We are using "Cascading Shadow Maps". Reading which was useful during development:
  // https://learnopengl.com/Guest-Articles/2021/CSM
  // https://developer.download.nvidia.com/SDK/10.5/opengl/src/cascaded_shadow_maps/doc/cascaded_shadow_maps.pdf
  // https://web.archive.org/web/20170710150304/https://cesiumjs.org/presentations/ShadowsAndCesiumImplementation.pdf

  const QVector3D direction = lightDirection.normalized();
  const QVector3D up = Qgs3DUtils::calculateDirectionalLightUpVector( direction );

  // cap the far plane to the shadow rendering distance, so that resolution is not spent on
  // scene that will never be shadowed
  const float shadowFarPlane = std::min( farPlane, maximumDistance );

  const std::vector<float> cascadeSplits = Qgs3DUtils::calculateCascadeSplits( NUM_CASCADES, nearPlane, shadowFarPlane, PRACTICAL_SPLIT_SCHEME_LAMBDA );

  const QMatrix4x4 invertedCameraView = cameraView.inverted();
  const float resolution = static_cast<float>( mResolution );

  // the matrices the shaders look the map up with carry the backend's clip space
  // correction, exactly as the ones the map is rendered with do, so that the z they
  // produce is the same number the shadow caster wrote
  const QMatrix4x4 clipCorrection = rhi->clipSpaceCorrMatrix();

  for ( int i = 0; i < NUM_CASCADES; ++i )
  {
    const float zNear = cascadeSplits[static_cast<size_t>( i )];
    const float zFar = cascadeSplits[static_cast<size_t>( i ) + 1];

    QVector3D worldFrustumCorners[8];
    QVector3D worldFrustumCenter;
    Qgs3DUtils::calculateFrustumSliceCorners( zNear, zFar, fieldOfView, aspectRatio, invertedCameraView, worldFrustumCorners, worldFrustumCenter );

    // fit a sphere rather than a box: a sphere does not change size as the camera turns,
    // which is half of what keeps the shadows from shimmering
    float rawRadius = 0.0f;
    for ( const QVector3D &corner : worldFrustumCorners )
      rawRadius = std::max( rawRadius, ( corner - worldFrustumCenter ).length() );

    // round the radius up in steps that scale with it, so that floating point wobble in
    // the corner positions does not change the radius at all
    const float stepSize = std::max( std::pow( 2.0f, std::floor( std::log2( rawRadius ) ) - 4.0f ), 0.01f );
    const float radius = std::ceil( rawRadius / stepSize ) * stepSize;

    QMatrix4x4 lightRotation;
    lightRotation.lookAt( QVector3D( 0, 0, 0 ), direction, up );

    // snap the centre to a whole texel of the shadow map, which is the other half: without
    // it the map's texel grid slides under the scene as the camera moves
    QVector3D centerLightSpace = lightRotation * worldFrustumCenter;
    const float worldUnitsPerTexel = ( 2.0f * radius ) / resolution;
    centerLightSpace.setX( std::floor( centerLightSpace.x() / worldUnitsPerTexel ) * worldUnitsPerTexel );
    centerLightSpace.setY( std::floor( centerLightSpace.y() / worldUnitsPerTexel ) * worldUnitsPerTexel );
    const QVector3D snappedWorldCenter = lightRotation.inverted() * centerLightSpace;

    QMatrix4x4 lightView;
    const QVector3D lightPosition = snappedWorldCenter - ( direction * radius );
    lightView.lookAt( lightPosition, snappedWorldCenter, up );

    const float left = -radius;
    const float right = radius;
    const float bottom = -radius;
    const float top = radius;
    // the z range has to cover the whole bounding sphere
    const float boundsNear = -radius;
    const float boundsFar = radius * 2.0f;

    // The exact bounds of the cascade, which is what decides whether a fragment belongs to
    // it. Deliberately left without the clip space correction: the test in qgsshadows.glsl
    // assumes the same [-1, 1] cube on every backend.
    QMatrix4x4 boundsProjection;
    boundsProjection.ortho( left, right, bottom, top, boundsNear, boundsFar );
    mUniforms.setCascadeBoundsMatrix( i, boundsProjection * lightView );

    // The camera the map is actually rendered with differs in one way: its near plane is
    // pulled back, so that casters standing between the light and this slice are included.
    QMatrix4x4 renderProjection;
    renderProjection.ortho( left, right, bottom, top, boundsNear - NEAR_PLANE_RETREAT, boundsFar );

    mCascadePasses[static_cast<size_t>( i )].setCamera( lightView, renderProjection );
    mUniforms.setCascadeMatrix( i, clipCorrection * renderProjection * lightView );
  }

  mUniforms.renderShadows = 1;
  mUniforms.shadowLightIndex = lightIndex;
  mUniforms.shadowBias = shadowBias;
  mUniforms.maxShadowDistance = shadowFarPlane;
}

bool QgsShadowRenderPass::render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache )
{
  if ( !mTarget.isValid() )
    return false;

  bool ok = true;
  for ( int i = 0; i < NUM_CASCADES; ++i )
  {
    QgsRenderPass &pass = mCascadePasses[static_cast<size_t>( i )];

    // the same casters go into every cascade; only the camera differs
    pass.clearCommands();
    for ( const QgsRenderCommand &command : mCasters )
      pass.addCommand( command );

    ok = pass.execute( engine, cache, mTarget.rhiTarget( false, i ) ) && ok;
  }
  return ok;
}
