/***************************************************************************
  qgsterraintileloader.cpp
  --------------------------------------
  Date                 : July 2017
  Copyright            : (C) 2017 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsterraintileloader.h"

#include "qgs3d.h"
#include "qgs3dmapsettings.h"
#include "qgs3dutils.h"
#include "qgschunknode.h"
#include "qgscoordinatetransform.h"
#include "qgsgputexture.h"
#include "qgsphongmaterial3d.h"
#include "qgsphongtexturedmaterial3d.h"
#include "qgssceneobject3d.h"
#include "qgsterrainentity.h"
#include "qgsterraingenerator.h"
#include "qgsterraintexturegenerator_p.h"
#include "qgstexturematerial3d.h"

#include "moc_qgsterraintileloader.cpp"

/// @cond PRIVATE

QgsTerrainTileLoader::QgsTerrainTileLoader( QgsTerrainEntity *terrain, QgsChunkNode *node )
  : QgsChunkLoader( node )
  , mTerrain( terrain )
{
  const Qgs3DMapSettings *map = mTerrain->mapSettings();
  const QgsChunkNodeId nodeId = node->tileId();
  const QgsRectangle extentTerrainCrs = map->terrainGenerator()->tilingScheme().tileToExtent( nodeId );
  mExtentMapCrs = Qgs3DUtils::tryReprojectExtent2D( extentTerrainCrs, map->terrainGenerator()->crs(), map->crs(), map->transformContext() );
  mTileDebugText = nodeId.text();
}

void QgsTerrainTileLoader::loadTexture()
{
  connect( mTerrain->textureGenerator(), &QgsTerrainTextureGenerator::tileReady, this, &QgsTerrainTileLoader::onImageReady );
  mTextureJobId = mTerrain->textureGenerator()->render( mExtentMapCrs, mNode->tileId(), mTileDebugText );
}

std::unique_ptr<QgsMaterial3D> QgsTerrainTileLoader::createTileMaterial( QgsSceneObject3D *object, bool isShadingEnabled, const QgsPhongMaterialSettings &shadingMaterial, bool useTexture )
{
  std::unique_ptr<QgsMaterial3D> material;

  if ( ( useTexture || !isShadingEnabled ) && !mTextureImage.isNull() )
  {
    auto texture = std::make_unique<QgsGpuTexture>();
    texture->setImage( mTextureImage );
    // the rendered map image is sRGB encoded, and shading happens in linear space
    texture->setSrgb( true );
    QgsGpuTexture *texturePtr = object->addTexture( std::move( texture ) );

    if ( isShadingEnabled )
    {
      auto phongTextured = std::make_unique<QgsPhongTexturedMaterial3D>();
      phongTextured->setAmbient( shadingMaterial.ambient() );
      phongTextured->setSpecular( shadingMaterial.specular() );
      phongTextured->setShininess( static_cast<float>( shadingMaterial.shininess() ) );
      phongTextured->setOpacity( static_cast<float>( shadingMaterial.opacity() ) );
      phongTextured->setDiffuseTexture( texturePtr );
      material = std::move( phongTextured );
    }
    else
    {
      auto textured = std::make_unique<QgsTextureMaterial3D>();
      textured->setDiffuseTexture( texturePtr );
      material = std::move( textured );
    }
  }
  else
  {
    auto phong = std::make_unique<QgsPhongMaterial3D>();
    phong->setAmbient( shadingMaterial.ambient() );
    phong->setDiffuse( shadingMaterial.diffuse() );
    phong->setSpecular( shadingMaterial.specular() );
    phong->setShininess( static_cast<float>( shadingMaterial.shininess() ) );
    phong->setOpacity( static_cast<float>( shadingMaterial.opacity() ) );
    material = std::move( phong );
  }

  // no backface culling on terrain, to allow terrain to be viewed from underground
  material->setCullMode( QRhiGraphicsPipeline::None );
  return material;
}

void QgsTerrainTileLoader::onTextureLoaded()
{
  emit finished();
}

void QgsTerrainTileLoader::onImageReady( int jobId, const QImage &image )
{
  if ( mTextureJobId == jobId )
  {
    mTextureImage = image;
    mTextureJobId = -1;
    onTextureLoaded();
  }
}

/// @endcond
