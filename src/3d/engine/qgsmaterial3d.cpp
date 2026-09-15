/***************************************************************************
  qgsmaterial3d.cpp
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

#include "qgsmaterial3d.h"

#include <algorithm>

#include "qgsgputexture.h"
#include "qgsgraphicspipelinecache.h"
#include "qgslogger.h"
#include "qgsrenderuniforms.h"

#include <QHashFunctions>
#include <QString>

using namespace Qt::StringLiterals;

QgsMaterial3D::QgsMaterial3D( std::shared_ptr<QgsRhiShader> shader )
  : mShader( std::move( shader ) )
{
  Q_ASSERT( mShader );
}

QgsMaterial3D::~QgsMaterial3D() = default;

void QgsMaterial3D::setTopology( QRhiGraphicsPipeline::Topology topology )
{
  mTopology = topology;
  mStateKey = 0;
}

void QgsMaterial3D::setCullMode( QRhiGraphicsPipeline::CullMode mode )
{
  mCullMode = mode;
  mStateKey = 0;
}

void QgsMaterial3D::setFrontFace( QRhiGraphicsPipeline::FrontFace face )
{
  mFrontFace = face;
  mStateKey = 0;
}

void QgsMaterial3D::setDepthTest( bool enabled )
{
  mDepthTest = enabled;
  mStateKey = 0;
}

void QgsMaterial3D::setDepthWrite( bool enabled )
{
  mDepthWrite = enabled;
  mStateKey = 0;
}

void QgsMaterial3D::setDepthOp( QRhiGraphicsPipeline::CompareOp op )
{
  mDepthOp = op;
  mStateKey = 0;
}

void QgsMaterial3D::setBlend( const QRhiGraphicsPipeline::TargetBlend &blend )
{
  mBlend = blend;
  mStateKey = 0;
}

void QgsMaterial3D::setPolygonMode( QRhiGraphicsPipeline::PolygonMode mode )
{
  mPolygonMode = mode;
  mStateKey = 0;
}

void QgsMaterial3D::setUniformData( const void *data, int size )
{
  mUniformBuffer.setData( QByteArray( static_cast<const char *>( data ), size ) );
}

void QgsMaterial3D::setTexture( const TextureBinding &texture )
{
  const auto it = std::find_if( mTextures.begin(), mTextures.end(), [&texture]( const TextureBinding &existing ) { return existing.binding == texture.binding; } );

  if ( !texture.texture )
  {
    if ( it != mTextures.end() )
      mTextures.erase( it );
    return;
  }

  if ( it != mTextures.end() )
  {
    *it = texture;
    return;
  }

  mTextures.append( texture );
  std::sort( mTextures.begin(), mTextures.end(), []( const TextureBinding &a, const TextureBinding &b ) { return a.binding < b.binding; } );
}

size_t QgsMaterial3D::stateKey() const
{
  if ( mStateKey )
    return mStateKey;

  const size_t key = qHashMulti(
    0,
    static_cast<int>( mTopology ),
    static_cast<int>( mCullMode ),
    static_cast<int>( mFrontFace ),
    mDepthTest,
    mDepthWrite,
    static_cast<int>( mDepthOp ),
    static_cast<int>( mPolygonMode ),
    mBlend.enable,
    static_cast<int>( mBlend.colorWrite ),
    static_cast<int>( mBlend.srcColor ),
    static_cast<int>( mBlend.dstColor ),
    static_cast<int>( mBlend.opColor ),
    static_cast<int>( mBlend.srcAlpha ),
    static_cast<int>( mBlend.dstAlpha ),
    static_cast<int>( mBlend.opAlpha )
  );

  mStateKey = key ? key : 1;
  return mStateKey;
}

bool QgsMaterial3D::appendResourceBindings( QRhi *rhi, QRhiResourceUpdateBatch *updates, QgsGraphicsPipelineCache *cache, QVector<QRhiShaderResourceBinding> *bindings )
{
  // the material block is optional: a material with no tweakable values does not declare one
  if ( !mUniformBuffer.isEmpty() )
  {
    QRhiBuffer *buffer = mUniformBuffer.rhiBuffer( rhi, updates );
    if ( !buffer )
      return false;

    bindings->append( QRhiShaderResourceBinding::uniformBuffer( QgsRhiBinding::Material, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, buffer ) );
  }

  for ( const TextureBinding &texture : std::as_const( mTextures ) )
  {
    QRhiTexture *rhiTexture = texture.texture->rhiTexture( rhi, updates );
    if ( !rhiTexture )
    {
      QgsDebugError( u"Material texture at binding %1 could not be uploaded"_s.arg( texture.binding ) );
      return false;
    }

    QRhiSampler *sampler = cache->sampler( rhi, texture.magFilter, texture.minFilter, texture.mipmapMode, texture.addressU, texture.addressV );
    if ( !sampler )
      return false;

    bindings->append( QRhiShaderResourceBinding::sampledTexture( texture.binding, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, rhiTexture, sampler ) );
  }

  return true;
}

void QgsMaterial3D::releaseGpuResources()
{
  mUniformBuffer.releaseGpuResources();
}
