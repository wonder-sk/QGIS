/***************************************************************************
  qgsbloomrenderpass.cpp
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

#include "qgsbloomrenderpass.h"

#include "qgsgraphicspipelinecache.h"
#include "qgsrendercommand.h"
#include "qgsrhiengine.h"
#include "qgsscreenquad.h"

#include <QString>

using namespace Qt::StringLiterals;

QgsBloomRenderPass::QgsBloomRenderPass()
{
  for ( int i = 0; i < MIP_LEVELS; ++i )
  {
    Level &level = mLevels[static_cast<size_t>( i )];
    level.downsampleMaterial = std::make_unique<QgsBloomMaterial3D>( QgsBloomMaterial3D::Step::Downsample );
    level.upsampleMaterial = std::make_unique<QgsBloomMaterial3D>( QgsBloomMaterial3D::Step::Upsample );
  }
}

QgsBloomRenderPass::~QgsBloomRenderPass() = default;

bool QgsBloomRenderPass::prepare( QRhi *rhi, QSize size )
{
  mBaseSize = size;

  if ( !mEnabled )
  {
    // nothing is going to sample these until the effect is switched back on, and five
    // levels of half float colour is not worth holding on to meanwhile
    for ( Level &level : mLevels )
      level.target.release();
    return true;
  }

  const float aspectRatio = size.height() > 0 ? static_cast<float>( size.width() ) / static_cast<float>( size.height() ) : 1.0f;

  QSize levelSize = size;
  for ( int i = 0; i < MIP_LEVELS; ++i )
  {
    // stop at one texel rather than at zero
    levelSize = QSize( std::max( 1, levelSize.width() / 2 ), std::max( 1, levelSize.height() / 2 ) );

    Level &level = mLevels[static_cast<size_t>( i )];
    // half float, because the whole point is to carry the values above one that the
    // forward pass produced and the post processing pass will tone map
    level.target.setColorFormat( QRhiTexture::RGBA16F );
    level.target.setDepthMode( QgsGpuRenderTarget::DepthMode::None );
    if ( !level.target.resize( rhi, levelSize ) )
      return false;

    level.upsampleMaterial->setFilterRadius( mFilterRadius );
    level.upsampleMaterial->setAspectRatio( aspectRatio );
  }

  // each level reads the one above it on the way down, and the one below it on the way up
  for ( int i = 0; i < MIP_LEVELS; ++i )
  {
    Level &level = mLevels[static_cast<size_t>( i )];
    level.downsampleMaterial->setSourceTexture( i == 0 ? mSourceTexture : mLevels[static_cast<size_t>( i ) - 1].target.colorTexture() );
    if ( i + 1 < MIP_LEVELS )
      level.upsampleMaterial->setSourceTexture( mLevels[static_cast<size_t>( i ) + 1].target.colorTexture() );
  }

  return true;
}

void QgsBloomRenderPass::releaseGpuResources()
{
  for ( Level &level : mLevels )
  {
    level.downsamplePass.releaseGpuResources();
    level.upsamplePass.releaseGpuResources();
    level.downsampleMaterial->releaseGpuResources();
    level.upsampleMaterial->releaseGpuResources();
    level.target.release();
  }
}

void QgsBloomRenderPass::setSourceTexture( QRhiTexture *texture )
{
  mSourceTexture = texture;
  mLevels[0].downsampleMaterial->setSourceTexture( texture );
}

void QgsBloomRenderPass::setFilterRadius( float radius )
{
  mFilterRadius = radius;
  for ( Level &level : mLevels )
    level.upsampleMaterial->setFilterRadius( radius );
}

QRhiTexture *QgsBloomRenderPass::bloomTexture() const
{
  return mLevels[0].target.colorTexture();
}

bool QgsBloomRenderPass::render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, const QgsScreenQuad *quad )
{
  if ( !mEnabled || !quad || !mSourceTexture || !mLevels[0].target.isValid() )
    return false;

  // down the chain: each level halves the one above it, writing over whatever was there
  for ( int i = 0; i < MIP_LEVELS; ++i )
  {
    Level &level = mLevels[static_cast<size_t>( i )];

    QgsRenderCommand command;
    command.geometry = quad->geometry();
    command.material = level.downsampleMaterial.get();

    level.downsamplePass.clearCommands();
    level.downsamplePass.addCommand( command );
    if ( !level.downsamplePass.execute( engine, cache, level.target.rhiTarget() ) )
      return false;
  }

  // and back up: each level is blurred and added onto the one above it. The bottom level
  // has nothing below it to add, so the chain starts one up from there.
  for ( int i = MIP_LEVELS - 2; i >= 0; --i )
  {
    Level &level = mLevels[static_cast<size_t>( i )];

    QgsRenderCommand command;
    command.geometry = quad->geometry();
    command.material = level.upsampleMaterial.get();

    level.upsamplePass.clearCommands();
    level.upsamplePass.addCommand( command );
    // keepContents, because the material blends additively onto what the downsampling
    // stage left in this level
    if ( !level.upsamplePass.execute( engine, cache, level.target.rhiTarget( true ) ) )
      return false;
  }

  return true;
}
