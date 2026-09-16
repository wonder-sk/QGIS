/***************************************************************************
  qgspostprocessingmaterial3d.cpp
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

#include "qgspostprocessingmaterial3d.h"

#include "qgslogger.h"
#include "qgsrenderuniforms.h"

#include <QString>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsPostprocessingUniforms ) == 32, "QgsPostprocessingUniforms must match its std140 layout" );

QgsPostprocessingMaterial3D::QgsPostprocessingMaterial3D()
  : QgsMaterial3D( QgsRhiShader::load( u":/shaders/rhi/screen.vert.qsb"_s, u":/shaders/rhi/postprocessing.frag.qsb"_s ) )
{
#ifdef QGISDEBUG
  const QVector<QgsUniformMember> members {
    { u"edlEnabled"_s, offsetof( QgsPostprocessingUniforms, edlEnabled ), 4 },
    { u"edlStrength"_s, offsetof( QgsPostprocessingUniforms, edlStrength ), 4 },
    { u"edlDistance"_s, offsetof( QgsPostprocessingUniforms, edlDistance ), 4 },
    { u"ambientOcclusionEnabled"_s, offsetof( QgsPostprocessingUniforms, ambientOcclusionEnabled ), 4 },
    { u"bloomEnabled"_s, offsetof( QgsPostprocessingUniforms, bloomEnabled ), 4 },
    { u"bloomFactor"_s, offsetof( QgsPostprocessingUniforms, bloomFactor ), 4 },
    { u"exposureAdjustment"_s, offsetof( QgsPostprocessingUniforms, exposureAdjustment ), 4 },
    { u"toneMapping"_s, offsetof( QgsPostprocessingUniforms, toneMapping ), 4 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsPostprocessingUniforms does not match QgsPostprocessingBlock: %1"_s.arg( error ) );
#endif

  setDepthTest( false );
  setDepthWrite( false );
  setCullMode( QRhiGraphicsPipeline::None );

  setUniforms( mUniforms );
}

void QgsPostprocessingMaterial3D::setPassTexture( int binding, QgsGpuTexture *holder, QRhiTexture *texture )
{
  holder->setExternalTexture( texture );

  TextureBinding textureBinding;
  textureBinding.binding = binding;
  textureBinding.texture = texture ? holder : nullptr;
  // one to one with the screen, so there is nothing to interpolate between
  textureBinding.magFilter = QRhiSampler::Nearest;
  textureBinding.minFilter = QRhiSampler::Nearest;
  setTexture( textureBinding );
}

void QgsPostprocessingMaterial3D::setColorTexture( QRhiTexture *texture )
{
  setPassTexture( QgsRhiBinding::FirstSampler, &mColorTexture, texture );
}

void QgsPostprocessingMaterial3D::setDepthTexture( QRhiTexture *texture )
{
  setPassTexture( QgsRhiBinding::FirstSampler + 1, &mDepthTexture, texture );
}

void QgsPostprocessingMaterial3D::setAmbientOcclusionTexture( QRhiTexture *texture )
{
  setPassTexture( QgsRhiBinding::FirstSampler + 2, &mAmbientOcclusionTexture, texture );
}

void QgsPostprocessingMaterial3D::setBloomTexture( QRhiTexture *texture )
{
  setPassTexture( QgsRhiBinding::FirstSampler + 3, &mBloomTexture, texture );
}

void QgsPostprocessingMaterial3D::setEyeDomeLightingEnabled( bool enabled )
{
  mUniforms.edlEnabled = enabled ? 1 : 0;
  setUniforms( mUniforms );
}

void QgsPostprocessingMaterial3D::setEyeDomeLightingStrength( float strength )
{
  mUniforms.edlStrength = strength;
  setUniforms( mUniforms );
}

void QgsPostprocessingMaterial3D::setEyeDomeLightingDistance( float distance )
{
  mUniforms.edlDistance = distance;
  setUniforms( mUniforms );
}

void QgsPostprocessingMaterial3D::setAmbientOcclusionEnabled( bool enabled )
{
  mUniforms.ambientOcclusionEnabled = enabled ? 1 : 0;
  setUniforms( mUniforms );
}

void QgsPostprocessingMaterial3D::setBloomEnabled( bool enabled )
{
  mUniforms.bloomEnabled = enabled ? 1 : 0;
  setUniforms( mUniforms );
}

void QgsPostprocessingMaterial3D::setBloomFactor( float factor )
{
  mUniforms.bloomFactor = factor;
  setUniforms( mUniforms );
}

void QgsPostprocessingMaterial3D::setExposureAdjustment( float stops )
{
  mUniforms.exposureAdjustment = stops;
  setUniforms( mUniforms );
}

void QgsPostprocessingMaterial3D::setToneMapping( Qgis::ToneMappingMethod method )
{
  mUniforms.toneMapping = method == Qgis::ToneMappingMethod::Aces ? 1 : 0;
  setUniforms( mUniforms );
}
