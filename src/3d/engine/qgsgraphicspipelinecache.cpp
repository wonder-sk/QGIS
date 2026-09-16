/***************************************************************************
  qgsgraphicspipelinecache.cpp
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

#include "qgsgraphicspipelinecache.h"

#include <algorithm>

#include "qgsgeometry3d.h"
#include "qgslogger.h"
#include "qgsmaterial3d.h"
#include "qgsrhishader.h"

#include <QHashFunctions>
#include <QString>
#include <QStringList>

using namespace Qt::StringLiterals;

QgsGraphicsPipelineCache::QgsGraphicsPipelineCache() = default;

QgsGraphicsPipelineCache::~QgsGraphicsPipelineCache()
{
  clear();
}

void QgsGraphicsPipelineCache::clear()
{
  for ( const Pipeline &entry : std::as_const( mPipelines ) )
    delete entry.pipeline;
  mPipelines.clear();

  qDeleteAll( mShaderResourceBindings );
  mShaderResourceBindings.clear();

  qDeleteAll( mSamplers );
  mSamplers.clear();
}

const QgsGraphicsPipelineCache::Pipeline *QgsGraphicsPipelineCache::pipeline(
  QRhi *rhi, const QgsMaterial3D *material, const QgsGeometry3D *geometry, QRhiShaderResourceBindings *srb, QRhiRenderPassDescriptor *renderPassDescriptor, int sampleCount
)
{
  const QgsRhiShader *shader = material->shader();

  const size_t key = qHashMulti( 0, reinterpret_cast<quintptr>( shader ), geometry->layoutKey(), material->stateKey(), reinterpret_cast<quintptr>( renderPassDescriptor ), sampleCount );

  const auto it = mPipelines.constFind( key );
  if ( it != mPipelines.constEnd() )
    return &it.value();

  // one vertex input binding per attribute. Attributes sharing a buffer simply bind that
  // buffer at different offsets, which costs nothing and keeps the mapping obvious.
  QVector<QRhiVertexInputBinding> inputBindings;
  QVector<QRhiVertexInputAttribute> inputAttributes;
  Pipeline entry;

  const QList<QgsVertexAttribute> &attributes = geometry->attributes();
  for ( int i = 0; i < attributes.size(); ++i )
  {
    const QgsVertexAttribute &attribute = attributes.at( i );
    const int location = shader->inputLocation( attribute.name );
    if ( location < 0 )
      continue; // the shader does not read this attribute, which is perfectly normal

    const int binding = inputBindings.size();
    const quint32 stride = attribute.byteStride > 0 ? attribute.byteStride : static_cast<quint32>( attribute.elementSize() );

    if ( attribute.instanceDivisor > 0 )
      inputBindings.append( QRhiVertexInputBinding( stride, QRhiVertexInputBinding::PerInstance, attribute.instanceDivisor ) );
    else
      inputBindings.append( QRhiVertexInputBinding( stride ) );

    // the attribute starts at the beginning of its binding, because the buffer is bound
    // at the attribute's own byte offset when the draw is recorded
    inputAttributes.append( QRhiVertexInputAttribute( binding, location, attribute.format, 0 ) );
    entry.attributeIndexes.append( i );
  }

  // a shader input with no attribute to feed it is fatal rather than merely unused: the
  // backend refuses to build the pipeline, and it does so with a message that varies by
  // API, so say what is actually wrong here instead
  if ( inputAttributes.size() != shader->inputNames().size() )
  {
    QStringList missing;
    const QStringList inputs = shader->inputNames();
    for ( const QString &input : inputs )
    {
      const auto found = std::find_if( attributes.cbegin(), attributes.cend(), [&input]( const QgsVertexAttribute &attribute ) { return attribute.name == input; } );
      if ( found == attributes.cend() )
        missing.append( input );
    }
    if ( !missing.isEmpty() )
    {
      QgsDebugError( u"%1 reads vertex attributes the geometry does not have: %2"_s.arg( shader->vertexPath(), missing.join( ", "_L1 ) ) );
      return nullptr;
    }
  }

  QRhiVertexInputLayout layout;
  layout.setBindings( inputBindings.cbegin(), inputBindings.cend() );
  layout.setAttributes( inputAttributes.cbegin(), inputAttributes.cend() );

  QRhiGraphicsPipeline *pipeline = rhi->newGraphicsPipeline();
  pipeline->setShaderStages( {
    { QRhiShaderStage::Vertex, shader->vertexShader() },
    { QRhiShaderStage::Fragment, shader->fragmentShader() },
  } );
  pipeline->setVertexInputLayout( layout );
  pipeline->setShaderResourceBindings( srb );
  pipeline->setRenderPassDescriptor( renderPassDescriptor );
  pipeline->setSampleCount( sampleCount );

  pipeline->setTopology( material->topology() );
  pipeline->setCullMode( material->cullMode() );
  pipeline->setFrontFace( material->frontFace() );
  pipeline->setDepthTest( material->hasDepthTest() );
  pipeline->setDepthWrite( material->hasDepthWrite() );
  pipeline->setDepthOp( material->depthOp() );
  pipeline->setTargetBlends( { material->blend() } );

  if ( material->polygonMode() != QRhiGraphicsPipeline::Fill && !rhi->isFeatureSupported( QRhi::NonFillPolygonMode ) )
    QgsDebugMsgLevel( u"Wireframe polygon mode is not supported by this backend, drawing filled"_s, 2 );
  else
    pipeline->setPolygonMode( material->polygonMode() );

  if ( !pipeline->create() )
  {
    QgsDebugError( u"Failed to build graphics pipeline for %1"_s.arg( shader->vertexPath() ) );
    delete pipeline;
    return nullptr;
  }

  entry.pipeline = pipeline;
  return &mPipelines.insert( key, entry ).value();
}

QRhiShaderResourceBindings *QgsGraphicsPipelineCache::shaderResourceBindings( QRhi *rhi, const QVector<QRhiShaderResourceBinding> &bindings )
{
  size_t key = qHash( bindings.size() );
  for ( const QRhiShaderResourceBinding &binding : bindings )
    key = qHashMulti( key, binding );

  const auto it = mShaderResourceBindings.constFind( key );
  if ( it != mShaderResourceBindings.constEnd() )
    return it.value();

  QRhiShaderResourceBindings *srb = rhi->newShaderResourceBindings();
  srb->setBindings( bindings.cbegin(), bindings.cend() );
  if ( !srb->create() )
  {
    QgsDebugError( u"Failed to build shader resource bindings"_s );
    delete srb;
    return nullptr;
  }

  mShaderResourceBindings.insert( key, srb );
  return srb;
}

QRhiSampler *QgsGraphicsPipelineCache::sampler(
  QRhi *rhi, QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter, QRhiSampler::Filter mipmapMode, QRhiSampler::AddressMode addressU, QRhiSampler::AddressMode addressV
)
{
  const size_t key = qHashMulti( 0, static_cast<int>( magFilter ), static_cast<int>( minFilter ), static_cast<int>( mipmapMode ), static_cast<int>( addressU ), static_cast<int>( addressV ) );

  const auto it = mSamplers.constFind( key );
  if ( it != mSamplers.constEnd() )
    return it.value();

  QRhiSampler *sampler = rhi->newSampler( magFilter, minFilter, mipmapMode, addressU, addressV );
  if ( !sampler->create() )
  {
    QgsDebugError( u"Failed to create sampler"_s );
    delete sampler;
    return nullptr;
  }

  mSamplers.insert( key, sampler );
  return sampler;
}
