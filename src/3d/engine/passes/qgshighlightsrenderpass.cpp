/***************************************************************************
  qgshighlightsrenderpass.cpp
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

#include "qgshighlightsrenderpass.h"

#include "qgsgraphicspipelinecache.h"
#include "qgsrendercommand.h"
#include "qgsrhiengine.h"
#include "qgsscreenquad.h"

#include <QColor>

using namespace Qt::StringLiterals;

QgsHighlightsRenderPass::QgsHighlightsRenderPass()
{
  // highlights are drawn in the order the handler produced them, and the band is one quad
  mFillPass.setSortEnabled( false );
  mMaskPass.setSortEnabled( false );
  mOutlinePass.setSortEnabled( false );

  // nothing covers anything in the mask: it is a footprint, not a picture
  mMaskPass.setClearColor( QColor::fromRgbF( 0.0f, 0.0f, 0.0f, 1.0f ) );
}

QgsHighlightsRenderPass::~QgsHighlightsRenderPass() = default;

bool QgsHighlightsRenderPass::prepare( QRhi *rhi, QSize size )
{
  // one byte per pixel: covered or not
  mMaskTarget.setColorFormat( QRhiTexture::R8 );
  mMaskTarget.setDepthMode( QgsGpuRenderTarget::DepthMode::None );

  if ( !mMaskTarget.resize( rhi, size ) )
    return false;

  mOutlineMaterial.setMaskTexture( mMaskTarget.colorTexture() );
  return true;
}

void QgsHighlightsRenderPass::releaseGpuResources()
{
  mFillPass.releaseGpuResources();
  mMaskPass.releaseGpuResources();
  mOutlinePass.releaseGpuResources();
  mOutlineMaterial.releaseGpuResources();
  mMaskTarget.release();
}

void QgsHighlightsRenderPass::clearCommands()
{
  mFillPass.clearCommands();
  mMaskPass.clearCommands();
}

bool QgsHighlightsRenderPass::render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, const QgsScreenQuad *quad, QRhiRenderTarget *target )
{
  if ( isEmpty() || !target )
    return false;

  bool ok = true;

  if ( !mFillPass.commands().empty() )
    ok = mFillPass.execute( engine, cache, target ) && ok;

  if ( !mMaskPass.commands().empty() && mMaskTarget.isValid() && quad )
  {
    ok = mMaskPass.execute( engine, cache, mMaskTarget.rhiTarget() ) && ok;

    QgsRenderCommand outline;
    outline.geometry = quad->geometry();
    outline.material = &mOutlineMaterial;

    mOutlinePass.clearCommands();
    mOutlinePass.addCommand( outline );
    ok = mOutlinePass.execute( engine, cache, target ) && ok;
  }

  return ok;
}
