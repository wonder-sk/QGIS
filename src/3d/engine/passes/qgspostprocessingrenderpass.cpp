/***************************************************************************
  qgspostprocessingrenderpass.cpp
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

#include "qgspostprocessingrenderpass.h"

#include "qgsgraphicspipelinecache.h"
#include "qgsoverlayquads.h"
#include "qgsrendercommand.h"
#include "qgsrhiengine.h"
#include "qgsscreenquad.h"

using namespace Qt::StringLiterals;

QgsPostprocessingRenderPass::QgsPostprocessingRenderPass()
{
  // the composed image first, then any overlays on top of it, in the order they were added
  mPass.setSortEnabled( false );
}

QgsPostprocessingRenderPass::~QgsPostprocessingRenderPass() = default;

void QgsPostprocessingRenderPass::releaseGpuResources()
{
  mPass.releaseGpuResources();
  mMaterial.releaseGpuResources();
}

bool QgsPostprocessingRenderPass::render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, const QgsScreenQuad *quad, QRhiRenderTarget *target, const QgsOverlayQuads *overlays )
{
  if ( !quad )
    return false;

  QgsRenderCommand command;
  command.geometry = quad->geometry();
  command.material = &mMaterial;

  mPass.clearCommands();
  mPass.addCommand( command );

  if ( overlays )
    overlays->appendCommands( &mPass, quad );

  return mPass.execute( engine, cache, target );
}
