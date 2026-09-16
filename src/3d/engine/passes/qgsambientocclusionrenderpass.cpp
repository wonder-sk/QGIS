/***************************************************************************
  qgsambientocclusionrenderpass.cpp
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

#include "qgsambientocclusionrenderpass.h"

#include "qgsgraphicspipelinecache.h"
#include "qgsrendercommand.h"
#include "qgsrhiengine.h"
#include "qgsscreenquad.h"

using namespace Qt::StringLiterals;

QgsAmbientOcclusionRenderPass::QgsAmbientOcclusionRenderPass() = default;

QgsAmbientOcclusionRenderPass::~QgsAmbientOcclusionRenderPass() = default;

bool QgsAmbientOcclusionRenderPass::prepare( QRhi *rhi, QSize size )
{
  // a single channel float: what is stored is how much light reaches a pixel, not a colour
  for ( QgsGpuRenderTarget *target : { &mOcclusionTarget, &mBlurTarget } )
  {
    target->setColorFormat( QRhiTexture::R32F );
    target->setDepthMode( QgsGpuRenderTarget::DepthMode::None );
    if ( !target->resize( rhi, size ) )
      return false;
  }

  mBlurMaterial.setOcclusionTexture( mOcclusionTarget.colorTexture() );
  return true;
}

void QgsAmbientOcclusionRenderPass::releaseGpuResources()
{
  mOcclusionPass.releaseGpuResources();
  mBlurPass.releaseGpuResources();
  mOcclusionMaterial.releaseGpuResources();
  mBlurMaterial.releaseGpuResources();
  mOcclusionTarget.release();
  mBlurTarget.release();
  mHasDepthTexture = false;
}

void QgsAmbientOcclusionRenderPass::setDepthTexture( QRhiTexture *texture )
{
  mOcclusionMaterial.setDepthTexture( texture );
  mHasDepthTexture = texture != nullptr;
}

bool QgsAmbientOcclusionRenderPass::render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, const QgsScreenQuad *quad )
{
  if ( !mOcclusionTarget.isValid() || !mBlurTarget.isValid() || !quad )
    return false;

  // With multisampling on a backend that cannot resolve depth there is no depth texture to
  // estimate occlusion from. Drawing anyway would sample the placeholder and darken the
  // whole scene, so the pass simply does not run and the blurred factor stays as it was.
  if ( !mHasDepthTexture )
    return false;

  QgsRenderCommand occlusion;
  occlusion.geometry = quad->geometry();
  occlusion.material = &mOcclusionMaterial;

  mOcclusionPass.clearCommands();
  mOcclusionPass.addCommand( occlusion );
  if ( !mOcclusionPass.execute( engine, cache, mOcclusionTarget.rhiTarget() ) )
    return false;

  QgsRenderCommand blur;
  blur.geometry = quad->geometry();
  blur.material = &mBlurMaterial;

  mBlurPass.clearCommands();
  mBlurPass.addCommand( blur );
  return mBlurPass.execute( engine, cache, mBlurTarget.rhiTarget() );
}
