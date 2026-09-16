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

QMatrix4x4 QgsSceneObject3D::worldTransform( const QgsVector3D &origin ) const
{
  // an object that was never placed in map coordinates is positioned by its transform
  // alone, in world coordinates, and the origin does not enter into where it is drawn
  if ( !mHasGeoTranslation )
    return mTransform;

  // the subtraction happens here, on every frame, rather than once when the object was
  // built: the scene's origin moves as the camera travels, and an object that had baked
  // the old one in would be drawn a whole origin shift away from where it belongs
  QMatrix4x4 world;
  world.translate( ( mGeoTranslation - origin ).toVector3D() );
  return world * mTransform;
}

void QgsSceneObject3D::buildCommands( bool shadowPass, const QgsVector3D &origin, quint32 baseSortKey, std::vector<QgsRenderCommand> &commands ) const
{
  if ( !mEnabled )
    return;

  const QMatrix4x4 world = worldTransform( origin );

  for ( const std::unique_ptr<Part> &part : mParts )
  {
    if ( shadowPass && !part->castsShadows )
      continue;

    QgsRenderCommand command;
    command.geometry = part->geometry;
    command.material = part->material.get();
    command.worldTransform = world * part->transform;
    command.instanceCount = part->instanceCount;
    command.sortKey = baseSortKey;
    commands.push_back( command );
  }
}

void QgsSceneObject3D::collectCommands( QgsRenderPass &pass, const QgsVector3D &origin, quint32 baseSortKey ) const
{
  std::vector<QgsRenderCommand> commands;
  buildCommands( false, origin, baseSortKey, commands );
  for ( const QgsRenderCommand &command : commands )
    pass.addCommand( command );
}

void QgsSceneObject3D::collectShadowCommands( QgsShadowRenderPass &pass, const QgsVector3D &origin ) const
{
  std::vector<QgsRenderCommand> commands;
  buildCommands( true, origin, 0, commands );
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
