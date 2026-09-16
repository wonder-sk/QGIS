/***************************************************************************
  qgsdepthrenderpass.cpp
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

#include "qgsdepthrenderpass.h"

#include "qgsgraphicspipelinecache.h"
#include "qgsrendercommand.h"
#include "qgsrhiengine.h"
#include "qgsscreenquad.h"

using namespace Qt::StringLiterals;

QgsDepthRenderPass::QgsDepthRenderPass() = default;

QgsDepthRenderPass::~QgsDepthRenderPass() = default;

bool QgsDepthRenderPass::prepare( QRhi *rhi, QSize size )
{
  mTarget.setColorFormat( QRhiTexture::RGBA8 );
  mTarget.setDepthMode( QgsGpuRenderTarget::DepthMode::None );
  mTarget.setReadbackEnabled( true );

  return mTarget.resize( rhi, size );
}

void QgsDepthRenderPass::releaseGpuResources()
{
  mPass.releaseGpuResources();
  mMaterial.releaseGpuResources();
  mTarget.release();
  mReadback = QRhiReadbackResult();
}

bool QgsDepthRenderPass::render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, const QgsScreenQuad *quad )
{
  if ( !mTarget.isValid() || !quad )
    return false;

  QgsRenderCommand command;
  command.geometry = quad->geometry();
  command.material = &mMaterial;

  mPass.clearCommands();
  mPass.addCommand( command );

  if ( !mPass.execute( engine, cache, mTarget.rhiTarget() ) )
    return false;

  // Queue the copy to the CPU outside the pass. An offscreen frame finishes synchronously,
  // so collectReadback() will find it filled in as soon as the frame ends.
  QRhiResourceUpdateBatch *updates = engine->rhi()->nextResourceUpdateBatch();
  updates->readBackTexture( QRhiReadbackDescription( mTarget.colorTexture() ), &mReadback );
  engine->commandBuffer()->resourceUpdate( updates );

  return true;
}

void QgsDepthRenderPass::collectReadback( const QgsRhiEngine *engine )
{
  if ( mReadback.data.isEmpty() )
    return;

  mImage = engine->imageFromReadback( mReadback );
}
