/***************************************************************************
  qgsforwardrenderpass.cpp
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

#include "qgsforwardrenderpass.h"

#include "qgsgraphicspipelinecache.h"
#include "qgsrhiengine.h"

using namespace Qt::StringLiterals;

namespace
{
  //! How many low bits of a sort key belong to the caller; the rest carry the group
  constexpr int GROUP_SHIFT = 28;
} // namespace

QgsForwardRenderPass::QgsForwardRenderPass() = default;

QgsForwardRenderPass::~QgsForwardRenderPass() = default;

bool QgsForwardRenderPass::prepare( QRhi *rhi, QSize size, int sampleCount )
{
  // Sixteen bit float, not eight bit unsigned: the scene is lit in linear light, and
  // bloom, exposure and tone mapping all need values above one to still be there when the
  // post processing chain runs.
  mTarget.setColorFormat( QRhiTexture::RGBA16F );

  // depth as a texture, because ambient occlusion, eye dome lighting and the depth
  // readback that drives camera navigation all sample it
  mTarget.setDepthMode( QgsGpuRenderTarget::DepthMode::Texture );
  mTarget.setDepthFormat( QRhiTexture::D32F );
  mTarget.setSampleCount( sampleCount );

  return mTarget.resize( rhi, size );
}

void QgsForwardRenderPass::releaseGpuResources()
{
  mPass.releaseGpuResources();
  mTarget.release();
}

void QgsForwardRenderPass::addCommand( QgsRenderCommand command, Group group )
{
  // the group decides the order first, the caller's own key only orders within a group
  command.sortKey = ( static_cast<quint32>( group ) << GROUP_SHIFT ) | ( command.sortKey & ( ( 1u << GROUP_SHIFT ) - 1 ) );
  mPass.addCommand( command );
}

bool QgsForwardRenderPass::render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache )
{
  if ( !mTarget.isValid() )
    return false;

  return mPass.execute( engine, cache, mTarget.rhiTarget() );
}
