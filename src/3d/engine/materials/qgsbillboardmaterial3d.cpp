/***************************************************************************
  qgsbillboardmaterial3d.cpp
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
 *                                                                         *
 ***************************************************************************/

#include "qgsbillboardmaterial3d.h"

#include "qgslogger.h"
#include "qgsrenderuniforms.h"

#include <QString>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsBillboardUniforms ) == 16, "QgsBillboardUniforms must match its std140 layout" );

QString QgsBillboardMaterial3D::shaderVariant( InstanceAttributes attributes )
{
  QString variant;
  if ( attributes.testFlag( InstanceAttribute::Size ) )
    variant += "_size"_L1;
  if ( attributes.testFlag( InstanceAttribute::TextureData ) )
    variant += "_atlas"_L1;
  if ( attributes.testFlag( InstanceAttribute::PixelOffsets ) )
    variant += "_pixeloffset"_L1;
  return variant;
}

QgsBillboardMaterial3D::QgsBillboardMaterial3D( InstanceAttributes attributes )
  : QgsMaterial3D( QgsRhiShader::load( u":/shaders/rhi/billboard%1.vert.qsb"_s.arg( shaderVariant( attributes ) ), u":/shaders/rhi/billboard.frag.qsb"_s ) )
  , mInstanceAttributes( attributes )
{
#ifdef QGISDEBUG
  // the C++ struct and the GLSL block are written by hand in two different files, so check
  // once that they agree rather than let a bad offset corrupt everything after it
  const QVector<QgsUniformMember> members {
    { u"billboardSize"_s, offsetof( QgsBillboardUniforms, billboardSize ), 8 },
    { u"verticalOffset"_s, offsetof( QgsBillboardUniforms, verticalOffset ), 4 },
    { u"featureFlags"_s, offsetof( QgsBillboardUniforms, featureFlags ), 4 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsBillboardUniforms does not match QgsBillboardBlock: %1"_s.arg( error ) );
#endif

  // the quad is four corners of a strip, and it faces the camera by construction rather
  // than by winding, so neither face may be culled
  setTopology( QRhiGraphicsPipeline::TriangleStrip );
  setCullMode( QRhiGraphicsPipeline::None );

  // symbols are rarely rectangular, so what the alpha test does not discard still has to
  // blend at the edges
  QRhiGraphicsPipeline::TargetBlend blend;
  blend.enable = true;
  blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
  blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
  setBlend( blend );

  updateUniforms();
}

void QgsBillboardMaterial3D::updateUniforms()
{
  setUniforms( mUniforms );
}

void QgsBillboardMaterial3D::setFeature( Feature feature, bool enabled )
{
  const quint32 bit = static_cast<quint32>( feature );
  if ( enabled )
    mUniforms.featureFlags |= bit;
  else
    mUniforms.featureFlags &= ~bit;

  updateUniforms();
}

void QgsBillboardMaterial3D::setSize( QSizeF size )
{
  mUniforms.billboardSize[0] = static_cast<float>( size.width() );
  mUniforms.billboardSize[1] = static_cast<float>( size.height() );
  updateUniforms();
}

void QgsBillboardMaterial3D::setVerticalOffset( float offset )
{
  mUniforms.verticalOffset = offset;
  updateUniforms();
}

void QgsBillboardMaterial3D::setVerticalOffsetEnabled( bool enabled )
{
  setFeature( Feature::VerticalOffset, enabled );
}

void QgsBillboardMaterial3D::setPerspectiveScaleEnabled( bool enabled )
{
  setFeature( Feature::PerspectiveScale, enabled );
}

void QgsBillboardMaterial3D::setSpriteTexture( QgsGpuTexture *texture )
{
  mSpriteTexture = texture;

  TextureBinding binding;
  binding.binding = SpriteTextureBinding;
  binding.texture = texture;
  setTexture( binding );
}
