/***************************************************************************
  qgsdepthrendermaterial3d.cpp
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

#include "qgsdepthrendermaterial3d.h"

#include "qgsrenderuniforms.h"

#include <QString>

using namespace Qt::StringLiterals;

QgsDepthRenderMaterial3D::QgsDepthRenderMaterial3D()
  : QgsMaterial3D( QgsRhiShader::load( u":/shaders/rhi/screen.vert.qsb"_s, u":/shaders/rhi/depthrender.frag.qsb"_s ) )
{
  // a full screen quad covering everything: nothing to test against, nothing to cull
  setDepthTest( false );
  setDepthWrite( false );
  setCullMode( QRhiGraphicsPipeline::None );
}

void QgsDepthRenderMaterial3D::setDepthTexture( QRhiTexture *texture )
{
  mDepthTexture.setExternalTexture( texture );

  TextureBinding binding;
  binding.binding = QgsRhiBinding::FirstSampler;
  binding.texture = texture ? &mDepthTexture : nullptr;
  // nearest, because a depth value halfway between two surfaces is a point in mid air
  binding.magFilter = QRhiSampler::Nearest;
  binding.minFilter = QRhiSampler::Nearest;
  setTexture( binding );
}
