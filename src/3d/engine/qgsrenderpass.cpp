/***************************************************************************
  qgsrenderpass.cpp
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

#include "qgsrenderpass.h"

#include <algorithm>

#include "qgsgeometry3d.h"
#include "qgsgputexture.h"
#include "qgsgraphicspipelinecache.h"
#include "qgslogger.h"
#include "qgsmaterial3d.h"
#include "qgsrhiengine.h"

#include <QString>

using namespace Qt::StringLiterals;

QgsRenderPass::QgsRenderPass( const QString &name )
  : mName( name )
{}

QgsRenderPass::~QgsRenderPass() = default;

void QgsRenderPass::setCamera( const QMatrix4x4 &view, const QMatrix4x4 &projection )
{
  mView = view;
  mProjection = projection;
}

void QgsRenderPass::setEyePosition( const QVector3D &position )
{
  mEyePosition = position;
}

bool QgsRenderPass::execute( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, QRhiRenderTarget *target )
{
  QRhi *rhi = engine->rhi();
  QRhiCommandBuffer *cb = engine->commandBuffer();
  if ( !rhi || !cb )
    return false;

  if ( !target )
    target = engine->renderTarget();
  if ( !target )
    return false;

  const QSize targetSize = target->pixelSize();

  if ( mSortEnabled )
  {
    std::stable_sort( mCommands.begin(), mCommands.end(), []( const QgsRenderCommand &a, const QgsRenderCommand &b ) { return a.sortKey < b.sortKey; } );
  }

  QRhiResourceUpdateBatch *updates = rhi->nextResourceUpdateBatch();

  // whatever the backend's notion of clip space is, it is dealt with here and nowhere else
  const QMatrix4x4 correctedProjection = rhi->clipSpaceCorrMatrix() * mProjection;
  const QMatrix4x4 viewProjection = correctedProjection * mView;

  QgsViewUniforms viewUniforms;
  viewUniforms.setCamera( mView, correctedProjection );
  viewUniforms.setEyePosition( mEyePosition );
  viewUniforms.setViewportSize( targetSize );
  mViewBuffer.setData( QByteArray( reinterpret_cast<const char *>( &viewUniforms ), sizeof( QgsViewUniforms ) ) );

  QRhiBuffer *viewBuffer = mViewBuffer.rhiBuffer( rhi, updates );
  if ( !viewBuffer )
    return false;

  // Phase one: stage the per draw uniforms for every command. Nothing is recorded yet, so
  // the ring buffer is free to grow and be recreated.
  mDrawUniforms.reset( rhi, sizeof( QgsDrawUniforms ) );
  mResolved.clear();
  mResolved.reserve( mCommands.size() );

  for ( const QgsRenderCommand &command : mCommands )
  {
    if ( !command.geometry || !command.material || command.instanceCount < 1 )
      continue;

    const int drawCount = command.elementCount >= 0 ? command.elementCount : command.geometry->drawCount();
    if ( drawCount <= 0 )
      continue;

    QgsDrawUniforms drawUniforms;
    drawUniforms.setTransform( command.worldTransform, viewProjection );

    ResolvedCommand resolved;
    resolved.drawUniformOffset = mDrawUniforms.append( &drawUniforms );
    resolved.elementCount = static_cast<quint32>( drawCount );
    resolved.instanceCount = static_cast<quint32>( command.instanceCount );
    resolved.firstElement = static_cast<quint32>( command.firstElement );
    resolved.indexFormat = command.geometry->indexFormat();
    mResolved.push_back( std::move( resolved ) );
  }

  QRhiBuffer *drawBuffer = mDrawUniforms.upload( rhi, updates );
  if ( !mResolved.empty() && !drawBuffer )
    return false;

  // Phase two: now that the buffers are final, resolve pipelines, bindings and vertex inputs
  size_t resolvedIndex = 0;
  for ( const QgsRenderCommand &command : mCommands )
  {
    if ( !command.geometry || !command.material || command.instanceCount < 1 )
      continue;

    const int drawCount = command.elementCount >= 0 ? command.elementCount : command.geometry->drawCount();
    if ( drawCount <= 0 )
      continue;

    ResolvedCommand &resolved = mResolved[resolvedIndex++];

    QVector<QRhiShaderResourceBinding> bindings;
    bindings.append( QRhiShaderResourceBinding::uniformBuffer( QgsRhiBinding::View, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, viewBuffer ) );
    bindings.append(
      QRhiShaderResourceBinding::uniformBufferWithDynamicOffset( QgsRhiBinding::Draw, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, drawBuffer, sizeof( QgsDrawUniforms ) )
    );

    if ( !command.material->appendResourceBindings( rhi, updates, cache, &bindings ) )
      return false;

    resolved.shaderResourceBindings = cache->shaderResourceBindings( rhi, bindings );
    if ( !resolved.shaderResourceBindings )
      return false;

    const QgsGraphicsPipelineCache::Pipeline *pipeline = cache->pipeline( rhi, command.material, command.geometry, resolved.shaderResourceBindings, target->renderPassDescriptor(), target->sampleCount() );
    if ( !pipeline )
      return false;

    resolved.pipeline = pipeline->pipeline;

    const QList<QgsVertexAttribute> &attributes = command.geometry->attributes();
    for ( int attributeIndex : pipeline->attributeIndexes )
    {
      const QgsVertexAttribute &attribute = attributes.at( attributeIndex );
      QRhiBuffer *buffer = attribute.buffer->rhiBuffer( rhi, updates );
      if ( !buffer )
      {
        QgsDebugError( u"Vertex attribute '%1' has no usable buffer"_s.arg( attribute.name ) );
        return false;
      }
      resolved.vertexInputs.append( { buffer, attribute.byteOffset } );
    }

    if ( command.geometry->isIndexed() )
    {
      resolved.indexBuffer = command.geometry->indexBuffer()->rhiBuffer( rhi, updates );
      if ( !resolved.indexBuffer )
        return false;
    }
  }

  // Phase three: record. Everything below only touches resources that already exist.
  if ( !mName.isEmpty() )
    cb->debugMarkBegin( mName.toUtf8() );

  cb->beginPass( target, mClearColor, { mDepthClearValue, 0 }, updates );
  cb->setViewport( QRhiViewport( 0, 0, targetSize.width(), targetSize.height() ) );

  QRhiGraphicsPipeline *boundPipeline = nullptr;
  for ( const ResolvedCommand &resolved : mResolved )
  {
    if ( resolved.pipeline != boundPipeline )
    {
      cb->setGraphicsPipeline( resolved.pipeline );
      boundPipeline = resolved.pipeline;
    }

    const QRhiCommandBuffer::DynamicOffset dynamicOffset( QgsRhiBinding::Draw, resolved.drawUniformOffset );
    cb->setShaderResources( resolved.shaderResourceBindings, 1, &dynamicOffset );

    if ( !resolved.vertexInputs.isEmpty() )
    {
      cb->setVertexInput( 0, resolved.vertexInputs.size(), resolved.vertexInputs.constData(), resolved.indexBuffer, 0, resolved.indexFormat );
    }

    if ( resolved.indexBuffer )
      cb->drawIndexed( resolved.elementCount, resolved.instanceCount, resolved.firstElement );
    else
      cb->draw( resolved.elementCount, resolved.instanceCount, resolved.firstElement );
  }

  cb->endPass();

  if ( !mName.isEmpty() )
    cb->debugMarkEnd();

  return true;
}

void QgsRenderPass::releaseGpuResources()
{
  mViewBuffer.releaseGpuResources();
  mDrawUniforms.releaseGpuResources();
  mResolved.clear();
}
