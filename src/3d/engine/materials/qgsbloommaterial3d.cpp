/***************************************************************************
  qgsbloommaterial3d.cpp
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

#include "qgsbloommaterial3d.h"

#include "qgslogger.h"
#include "qgsrenderuniforms.h"

#include <QString>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsBloomUniforms ) == 8, "QgsBloomUniforms must match its std140 layout" );

QgsBloomMaterial3D::QgsBloomMaterial3D( Step step )
  : QgsMaterial3D( QgsRhiShader::load( u":/shaders/rhi/screen.vert.qsb"_s, step == Step::Downsample ? u":/shaders/rhi/bloomdownsample.frag.qsb"_s : u":/shaders/rhi/bloomupsample.frag.qsb"_s ) )
  , mStep( step )
{
#ifdef QGISDEBUG
  const QVector<QgsUniformMember> members {
    { u"filterRadius"_s, offsetof( QgsBloomUniforms, filterRadius ), 4 },
    { u"aspectRatio"_s, offsetof( QgsBloomUniforms, aspectRatio ), 4 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsBloomUniforms does not match QgsBloomBlock: %1"_s.arg( error ) );
#endif

  setDepthTest( false );
  setDepthWrite( false );
  setCullMode( QRhiGraphicsPipeline::None );

  if ( step == Step::Upsample )
  {
    // each level is added onto the one above it, which is what turns a chain of blurs into
    // a single wide halo rather than a set of concentric rings
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::One;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::One;
    setBlend( blend );
  }

  setUniforms( mUniforms );
}

void QgsBloomMaterial3D::setSourceTexture( QRhiTexture *texture )
{
  mSourceTexture.setExternalTexture( texture );

  TextureBinding binding;
  binding.binding = QgsRhiBinding::FirstSampler;
  binding.texture = texture ? &mSourceTexture : nullptr;
  // linear filtering and edge clamping on every mip, which is what makes the chain smooth
  binding.magFilter = QRhiSampler::Linear;
  binding.minFilter = QRhiSampler::Linear;
  setTexture( binding );
}

void QgsBloomMaterial3D::setFilterRadius( float radius )
{
  mUniforms.filterRadius = radius;
  setUniforms( mUniforms );
}

void QgsBloomMaterial3D::setAspectRatio( float ratio )
{
  mUniforms.aspectRatio = ratio;
  setUniforms( mUniforms );
}
