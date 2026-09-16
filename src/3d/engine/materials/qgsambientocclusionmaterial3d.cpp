/***************************************************************************
  qgsambientocclusionmaterial3d.cpp
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

#include "qgsambientocclusionmaterial3d.h"

#include <random>

#include "qgslogger.h"
#include "qgsrenderuniforms.h"

#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsAmbientOcclusionUniforms ) == 1296, "QgsAmbientOcclusionUniforms must match its std140 layout" );

QgsAmbientOcclusionMaterial3D::QgsAmbientOcclusionMaterial3D()
  : QgsMaterial3D( QgsRhiShader::load( u":/shaders/rhi/ambientocclusion.vert.qsb"_s, u":/shaders/rhi/ambientocclusion.frag.qsb"_s ) )
{
#ifdef QGISDEBUG
  const QVector<QgsUniformMember> members {
    { u"kernel"_s, offsetof( QgsAmbientOcclusionUniforms, kernel ), 1024 },
    { u"noise"_s, offsetof( QgsAmbientOcclusionUniforms, noise ), 256 },
    { u"intensity"_s, offsetof( QgsAmbientOcclusionUniforms, intensity ), 4 },
    { u"radius"_s, offsetof( QgsAmbientOcclusionUniforms, radius ), 4 },
    { u"threshold"_s, offsetof( QgsAmbientOcclusionUniforms, threshold ), 4 },
    { u"tanHalfFov"_s, offsetof( QgsAmbientOcclusionUniforms, tanHalfFov ), 4 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsAmbientOcclusionUniforms does not match QgsAmbientOcclusionBlock: %1"_s.arg( error ) );
#endif

  setDepthTest( false );
  setDepthWrite( false );
  setCullMode( QRhiGraphicsPipeline::None );

  // A fixed seed, so that two runs of the same scene produce the same image. The pattern
  // only has to be free of structure, not unpredictable.
  std::default_random_engine generator;
  std::uniform_real_distribution<float> randomFloats( 0.0f, 1.0f );

  for ( int i = 0; i < QgsAmbientOcclusionUniforms::KERNEL_SIZE; ++i )
  {
    QVector3D sample( randomFloats( generator ) * 2.0f - 1.0f, randomFloats( generator ) * 2.0f - 1.0f, randomFloats( generator ) * 2.0f - 1.0f );
    sample.normalize();

    // bunch the samples up towards the pixel being shaded, where they matter most
    const float t = static_cast<float>( i ) / static_cast<float>( QgsAmbientOcclusionUniforms::KERNEL_SIZE );
    sample *= 0.1f + 0.9f * t * t;

    mUniforms.kernel[i][0] = sample.x();
    mUniforms.kernel[i][1] = sample.y();
    mUniforms.kernel[i][2] = sample.z();
  }

  for ( int i = 0; i < QgsAmbientOcclusionUniforms::NOISE_SIZE; ++i )
  {
    mUniforms.noise[i][0] = randomFloats( generator );
    mUniforms.noise[i][1] = randomFloats( generator );
  }

  setUniforms( mUniforms );
}

void QgsAmbientOcclusionMaterial3D::setDepthTexture( QRhiTexture *texture )
{
  mDepthTexture.setExternalTexture( texture );

  TextureBinding binding;
  binding.binding = QgsRhiBinding::FirstSampler;
  binding.texture = texture ? &mDepthTexture : nullptr;
  binding.magFilter = QRhiSampler::Nearest;
  binding.minFilter = QRhiSampler::Nearest;
  setTexture( binding );
}

void QgsAmbientOcclusionMaterial3D::setIntensity( float intensity )
{
  mUniforms.intensity = intensity;
  setUniforms( mUniforms );
}

void QgsAmbientOcclusionMaterial3D::setRadius( float radius )
{
  mUniforms.radius = radius;
  setUniforms( mUniforms );
}

void QgsAmbientOcclusionMaterial3D::setThreshold( float threshold )
{
  mUniforms.threshold = threshold;
  setUniforms( mUniforms );
}

void QgsAmbientOcclusionMaterial3D::setTanHalfFov( float tanHalfFov )
{
  mUniforms.tanHalfFov = tanHalfFov;
  setUniforms( mUniforms );
}

QgsAmbientOcclusionBlurMaterial3D::QgsAmbientOcclusionBlurMaterial3D()
  : QgsMaterial3D( QgsRhiShader::load( u":/shaders/rhi/screen.vert.qsb"_s, u":/shaders/rhi/ambientocclusionblur.frag.qsb"_s ) )
{
  setDepthTest( false );
  setDepthWrite( false );
  setCullMode( QRhiGraphicsPipeline::None );
}

void QgsAmbientOcclusionBlurMaterial3D::setOcclusionTexture( QRhiTexture *texture )
{
  mOcclusionTexture.setExternalTexture( texture );

  TextureBinding binding;
  binding.binding = QgsRhiBinding::FirstSampler;
  binding.texture = texture ? &mOcclusionTexture : nullptr;
  // the shader uses texelFetch, so the filters only matter to the sampler object itself
  binding.magFilter = QRhiSampler::Nearest;
  binding.minFilter = QRhiSampler::Nearest;
  setTexture( binding );
}
