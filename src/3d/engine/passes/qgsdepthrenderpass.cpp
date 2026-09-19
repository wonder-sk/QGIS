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
  // Letting go of a readback that is still in flight is safe: the block it lives in keeps
  // itself alive until the backend has written it back, and nobody will read it then.
  mReadback.reset();
  mHasNewImage = false;
}

bool QgsDepthRenderPass::render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, const QgsScreenQuad *quad )
{
  if ( !mTarget.isValid() || !quad )
    return false;

  // An earlier readback has not been turned into an image yet, so queueing another one
  // now would throw it away. Nothing is lost by waiting: the answer already on its way is
  // at most a frame or two old.
  //
  // This covers a readback that has *arrived* as well as one still in flight, and it has
  // to: QRhi reports a completed readback from inside beginFrame() (QRhiMetal does it at
  // the end of it), which is before this runs, while collectReadback() runs after
  // endFrame(). Without it, the frame the answer arrives in would queue a new readback
  // over the top of it, every time, and the pass would never produce an image at all.
  if ( mReadback )
    return false;

  QgsRenderCommand command;
  command.geometry = quad->geometry();
  command.material = &mMaterial;

  mPass.clearCommands();
  mPass.addCommand( command );

  if ( !mPass.execute( engine, cache, mTarget.rhiTarget() ) )
    return false;

  // Queue the copy to the CPU outside the pass. An offscreen frame finishes synchronously,
  // so collectReadback() will find it filled in as soon as the frame ends. A swapchain
  // frame only gets submitted, so the backend calls completed() a frame or two later,
  // when it retires the frame - and that is what tells us the data is really there.
  mReadback = std::make_shared<Readback>();
  mReadback->result.completed = [readback = mReadback] { readback->pending = false; };

  QRhiResourceUpdateBatch *updates = engine->rhi()->nextResourceUpdateBatch();
  updates->readBackTexture( QRhiReadbackDescription( mTarget.colorTexture() ), &mReadback->result );
  engine->commandBuffer()->resourceUpdate( updates );

  return true;
}

QImage QgsDepthRenderPass::takeImage()
{
  mHasNewImage = false;
  return mImage;
}

void QgsDepthRenderPass::collectReadback( const QgsRhiEngine *engine )
{
  // nothing queued, or still in flight - image() keeps the previous one until it lands
  if ( !mReadback || mReadback->pending )
    return;

  // QgsCameraController reads this both a pixel at a time, which any format survives, and
  // a scan line at a time cast to QRgb *, which only the 32 bit native orders survive. The
  // readback comes back as RGBA8888, so it is converted here rather than at each reader.
  const QImage image = engine->imageFromReadback( mReadback->result );

  // consumed either way, so that the next frame is free to queue another one - a readback
  // that came back empty must not block the pass for good
  mReadback.reset();

  if ( image.isNull() )
    return;

  mImage = image.convertToFormat( QImage::Format_RGB32 );
  mHasNewImage = true;
}
