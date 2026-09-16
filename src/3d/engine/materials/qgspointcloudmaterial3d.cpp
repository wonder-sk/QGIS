/***************************************************************************
  qgspointcloudmaterial3d.cpp
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

#include "qgspointcloudmaterial3d.h"

#include "qgscolorrampshader.h"
#include "qgsgputexture.h"
#include "qgslogger.h"
#include "qgsmaterial3dutils.h"
#include "qgsrenderuniforms.h"

#include <QImage>
#include <QString>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsPointCloudUniforms ) == 48, "QgsPointCloudUniforms must match its std140 layout" );

std::shared_ptr<QgsRhiShader> QgsPointCloudMaterial3D::shaderFor( Style style, bool triangulated )
{
  QString variant;
  switch ( style )
  {
    case Style::SingleColor:
      variant = u"singlecolor"_s;
      break;
    case Style::ColorRamp:
      variant = u"colorramp"_s;
      break;
    case Style::Rgb:
      variant = u"rgb"_s;
      break;
    case Style::Classification:
      variant = u"classification"_s;
      break;
  }

  if ( triangulated )
    variant += "_triangulated"_L1;

  // the fragment stage has no variants: every vertex variant writes the same varyings, so
  // which style applies is a branch on the uniform block rather than a different shader
  return QgsRhiShader::load( u":/shaders/rhi/pointcloud_%1.vert.qsb"_s.arg( variant ), u":/shaders/rhi/pointcloud.frag.qsb"_s );
}

QgsPointCloudMaterial3D::QgsPointCloudMaterial3D( Style style, bool triangulated )
  : QgsMaterial3D( shaderFor( style, triangulated ) )
  , mStyle( style )
  , mTriangulated( triangulated )
{
#ifdef QGISDEBUG
  // the C++ struct and the GLSL block are written by hand in two different files, so check
  // once that they agree rather than let a bad offset corrupt everything after it
  const QVector<QgsUniformMember> members {
    { u"singleColor"_s, offsetof( QgsPointCloudUniforms, singleColor ), 16 },
    { u"pointSize"_s, offsetof( QgsPointCloudUniforms, pointSize ), 4 },
    { u"style"_s, offsetof( QgsPointCloudUniforms, style ), 4 },
    { u"colorRampType"_s, offsetof( QgsPointCloudUniforms, colorRampType ), 4 },
    { u"colorRampCount"_s, offsetof( QgsPointCloudUniforms, colorRampCount ), 4 },
    { u"featureFlags"_s, offsetof( QgsPointCloudUniforms, featureFlags ), 4 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsPointCloudUniforms does not match QgsPointCloudBlock: %1"_s.arg( error ) );
#endif

  mPlaceholderTexture = std::make_unique<QgsGpuTexture>();
  QImage placeholder( 1, 1, QImage::Format_RGBA8888 );
  placeholder.fill( Qt::white );
  mPlaceholderTexture->setImage( placeholder );

  TextureBinding ramp;
  ramp.binding = ColorRampTextureBinding;
  ramp.texture = mPlaceholderTexture.get();
  // the ramp is read with texelFetch(), which takes no filtering at all
  ramp.magFilter = QRhiSampler::Nearest;
  ramp.minFilter = QRhiSampler::Nearest;
  setTexture( ramp );

  // points are their own topology; a triangulated chunk is an ordinary surface, drawn
  // without culling because it is single sided and may be looked at from underneath
  setTopology( triangulated ? QRhiGraphicsPipeline::Triangles : QRhiGraphicsPipeline::Points );
  setCullMode( QRhiGraphicsPipeline::None );

  mUniforms.style = static_cast<int>( style );
  if ( triangulated )
    mUniforms.featureFlags |= static_cast<quint32>( Feature::Triangulated );

  setSingleColor( QColor( 255, 255, 255 ) );
}

QgsPointCloudMaterial3D::Features QgsPointCloudMaterial3D::features() const
{
  return Features( static_cast<int>( mUniforms.featureFlags ) );
}

void QgsPointCloudMaterial3D::updateUniforms()
{
  setUniforms( mUniforms );
}

void QgsPointCloudMaterial3D::setSingleColor( const QColor &color )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.singleColor, color );
  mUniforms.singleColor[3] = 1.0f;
  updateUniforms();
}

void QgsPointCloudMaterial3D::setPointSize( float size )
{
  mUniforms.pointSize = size;
  updateUniforms();
}

void QgsPointCloudMaterial3D::setColorRamp( const QgsColorRampShader &shader )
{
  const QList<QgsColorRampShader::ColorRampItem> items = shader.colorRampItemList();

  // one texel per break point: the value it applies from in the red channel, the colour
  // it maps to in the other three. This is the layout QgsColorRampTexture built for the
  // Qt3D 1D texture, which is what lets the ramp lookups port line for line.
  //
  // Unlike that generator, a fully transparent break point is kept rather than skipped.
  // It skipped them while still declaring the texture's full width, which left every
  // later texel fetch reading past the end of the data; and the classification style
  // indexes the ramp by position, so dropping one miscolours every class after it.
  QByteArray data;
  int count = 0;
  for ( const QgsColorRampShader::ColorRampItem &item : items )
  {
    const float texel[4] = {
      static_cast<float>( item.value ),
      static_cast<float>( item.color.redF() ),
      static_cast<float>( item.color.greenF() ),
      static_cast<float>( item.color.blueF() ),
    };
    data.append( reinterpret_cast<const char *>( texel ), sizeof( texel ) );
    ++count;
  }

  mUniforms.colorRampCount = count;
  mUniforms.colorRampType = static_cast<int>( shader.colorRampType() );
  updateUniforms();

  TextureBinding binding;
  binding.binding = ColorRampTextureBinding;
  binding.magFilter = QRhiSampler::Nearest;
  binding.minFilter = QRhiSampler::Nearest;

  if ( count == 0 )
  {
    // the shader samples the ramp whatever colorRampCount says, so leave something valid
    // behind it rather than an unbound sampler
    mColorRampTexture.reset();
    binding.texture = mPlaceholderTexture.get();
  }
  else
  {
    mColorRampTexture = std::make_unique<QgsGpuTexture>();
    mColorRampTexture->setData( data, QSize( count, 1 ), QgsGpuTexture::Format::RGBA32F );
    binding.texture = mColorRampTexture.get();
  }

  setTexture( binding );
}
