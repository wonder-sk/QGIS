/***************************************************************************
  qgssceneobject3d.cpp
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

#include "qgssceneobject3d.h"

#include "qgsgpubuffer.h"
#include "qgsrendercommand.h"
#include "qgsrenderpass.h"
#include "qgsshadowrenderpass.h"
#include "qgsvertexattribute.h"

#include <QSet>

QgsSceneObject3D::~QgsSceneObject3D() = default;

QgsSceneObject3D::Part *QgsSceneObject3D::addPart( const QgsGeometry3D *geometry, std::unique_ptr<QgsMaterial3D> material, const QMatrix4x4 &transform )
{
  auto part = std::make_unique<Part>();
  part->geometry = geometry;
  part->material = std::move( material );
  part->transform = transform;
  mParts.push_back( std::move( part ) );
  return mParts.back().get();
}

QgsGpuTexture *QgsSceneObject3D::addTexture( std::unique_ptr<QgsGpuTexture> texture )
{
  mTextures.push_back( std::move( texture ) );
  return mTextures.back().get();
}

void QgsSceneObject3D::buildCommands( bool shadowPass, quint32 baseSortKey, std::vector<QgsRenderCommand> &commands ) const
{
  if ( !mEnabled )
    return;

  for ( const std::unique_ptr<Part> &part : mParts )
  {
    if ( shadowPass && !part->castsShadows )
      continue;

    QgsRenderCommand command;
    command.geometry = part->geometry;
    command.material = part->material.get();
    command.worldTransform = mTransform * part->transform;
    command.instanceCount = part->instanceCount;
    command.sortKey = baseSortKey;
    commands.push_back( command );
  }
}

void QgsSceneObject3D::collectCommands( QgsRenderPass &pass, quint32 baseSortKey ) const
{
  std::vector<QgsRenderCommand> commands;
  buildCommands( false, baseSortKey, commands );
  for ( const QgsRenderCommand &command : commands )
    pass.addCommand( command );
}

void QgsSceneObject3D::collectShadowCommands( QgsShadowRenderPass &pass ) const
{
  std::vector<QgsRenderCommand> commands;
  buildCommands( true, 0, commands );
  for ( const QgsRenderCommand &command : commands )
    pass.addCommand( command );
}

qint64 QgsSceneObject3D::gpuMemorySize() const
{
  // a buffer may be shared between several attributes, and between the parts of one
  // object, so count each of them once
  QSet<const QgsGpuBuffer *> counted;
  qint64 total = 0;

  const auto addBuffer = [&counted, &total]( const QgsGpuBuffer *buffer ) {
    if ( !buffer || counted.contains( buffer ) )
      return;
    counted.insert( buffer );
    total += buffer->size();
  };

  for ( const std::unique_ptr<Part> &part : mParts )
  {
    if ( !part->geometry )
      continue;

    const QList<QgsVertexAttribute> attributes = part->geometry->attributes();
    for ( const QgsVertexAttribute &attribute : attributes )
      addBuffer( attribute.buffer );

    addBuffer( part->geometry->indexBuffer() );
  }

  for ( const std::unique_ptr<QgsGpuTexture> &texture : mTextures )
  {
    const QImage image = texture->image();
    total += static_cast<qint64>( image.sizeInBytes() );
  }

  return total;
}
