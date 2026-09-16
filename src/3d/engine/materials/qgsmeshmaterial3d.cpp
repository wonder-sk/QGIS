/***************************************************************************
  qgsmeshmaterial3d.cpp
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

#include "qgsmeshmaterial3d.h"

#include "qgscolorrampshader.h"
#include "qgsgputexture.h"
#include "qgslogger.h"
#include "qgsmaterial3dutils.h"
#include "qgsrenderuniforms.h"
#include "qgsvector.h"

#include <QImage>
#include <QPointF>
#include <QString>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsMeshUniforms ) == 80, "QgsMeshUniforms must match its std140 layout" );

QgsMeshMaterial3D::QgsMeshMaterial3D( bool hasScalarMagnitude )
  : QgsMaterial3D( QgsRhiShader::load( hasScalarMagnitude ? u":/shaders/rhi/mesh_scalar.vert.qsb"_s : u":/shaders/rhi/mesh.vert.qsb"_s, u":/shaders/rhi/mesh.frag.qsb"_s ) )
  , mHasScalarMagnitude( hasScalarMagnitude )
{
#ifdef QGISDEBUG
  // the C++ struct and the GLSL block are written by hand in two different files, so check
  // once that they agree rather than let a bad offset corrupt everything after it
  const QVector<QgsUniformMember> members {
    { u"meshColor"_s, offsetof( QgsMeshUniforms, meshColor ), 16 },
    { u"lineColor"_s, offsetof( QgsMeshUniforms, lineColor ), 16 },
    { u"arrowsColor"_s, offsetof( QgsMeshUniforms, arrowsColor ), 16 },
    { u"arrowsMinCorner"_s, offsetof( QgsMeshUniforms, arrowsMinCorner ), 8 },
    { u"arrowsSpacing"_s, offsetof( QgsMeshUniforms, arrowsSpacing ), 4 },
    { u"lineWidth"_s, offsetof( QgsMeshUniforms, lineWidth ), 4 },
    { u"textureType"_s, offsetof( QgsMeshUniforms, textureType ), 4 },
    { u"colorRampType"_s, offsetof( QgsMeshUniforms, colorRampType ), 4 },
    { u"colorRampCount"_s, offsetof( QgsMeshUniforms, colorRampCount ), 4 },
    { u"featureFlags"_s, offsetof( QgsMeshUniforms, featureFlags ), 4 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsMeshUniforms does not match QgsMeshBlock: %1"_s.arg( error ) );
#endif

  mPlaceholderTexture = std::make_unique<QgsGpuTexture>();
  QImage placeholder( 1, 1, QImage::Format_RGBA8888 );
  placeholder.fill( Qt::white );
  mPlaceholderTexture->setImage( placeholder );

  // every sampler the shader declares has to be bound, whether or not the feature that
  // reads it is switched on
  for ( const Binding binding : { ColorRampTextureBinding, ArrowsGridTextureBinding, ArrowTextureBinding } )
  {
    TextureBinding texture;
    texture.binding = binding;
    texture.texture = mPlaceholderTexture.get();

    // the ramp and the grid are read with texelFetch(), which takes no filtering, and the
    // arrow was drawn at its own size, so nothing here wants interpolating
    texture.magFilter = QRhiSampler::Nearest;
    texture.minFilter = QRhiSampler::Nearest;
    setTexture( texture );
  }

  setMeshColor( QColor( 0, 0, 0 ) );
  setWireframeColor( QColor( 0, 0, 0 ) );
  setArrowsColor( QColor( 0, 0, 0 ) );

  updateUniforms();
}

QgsMeshMaterial3D::Features QgsMeshMaterial3D::features() const
{
  return Features( static_cast<int>( mUniforms.featureFlags ) );
}

void QgsMeshMaterial3D::updateUniforms()
{
  setUniforms( mUniforms );
}

void QgsMeshMaterial3D::setFeature( Feature feature, bool enabled )
{
  const quint32 bit = static_cast<quint32>( feature );
  if ( enabled )
    mUniforms.featureFlags |= bit;
  else
    mUniforms.featureFlags &= ~bit;

  updateUniforms();
}

void QgsMeshMaterial3D::setStyle( Style style )
{
  mUniforms.textureType = static_cast<int>( style );
  updateUniforms();
}

void QgsMeshMaterial3D::setMeshColor( const QColor &color )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.meshColor, color );
  mUniforms.meshColor[3] = 1.0f;
  updateUniforms();
}

void QgsMeshMaterial3D::setColorRamp( const QgsColorRampShader &shader, double valueScale )
{
  const QList<QgsColorRampShader::ColorRampItem> items = shader.colorRampItemList();

  // one texel per break point: the value it applies from in the red channel, the colour
  // it maps to in the other three. Fully transparent entries are dropped rather than
  // drawn as a hole, which is what the Qt3D texture generator did too.
  QByteArray data;
  int count = 0;
  for ( const QgsColorRampShader::ColorRampItem &item : items )
  {
    if ( item.color.alphaF() == 0.0 )
      continue;

    const float texel[4] = {
      static_cast<float>( item.value * valueScale ),
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
    // the shader still samples the ramp, so leave something valid behind it
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

void QgsMeshMaterial3D::setWireframeColor( const QColor &color )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.lineColor, color );
  mUniforms.lineColor[3] = 1.0f;
  updateUniforms();
}

void QgsMeshMaterial3D::setWireframeLineWidth( float width )
{
  mUniforms.lineWidth = width;
  updateUniforms();
}

void QgsMeshMaterial3D::setWireframeEnabled( bool enabled )
{
  setFeature( Feature::Wireframe, enabled );
}

void QgsMeshMaterial3D::setFlatTrianglesEnabled( bool enabled )
{
  setFeature( Feature::FlatTriangles, enabled );
}

void QgsMeshMaterial3D::setArrowsGrid( const QVector<QgsVector> &vectors, const QSize &gridSize, bool fixedSize, double maxVectorLength )
{
  TextureBinding binding;
  binding.binding = ArrowsGridTextureBinding;
  binding.magFilter = QRhiSampler::Nearest;
  binding.minFilter = QRhiSampler::Nearest;

  const int cells = gridSize.width() * gridSize.height();
  if ( !gridSize.isValid() || cells <= 0 || vectors.size() < cells )
  {
    mArrowsGridTexture.reset();
    binding.texture = mPlaceholderTexture.get();
    setTexture( binding );
    return;
  }

  // the length in the red channel and the direction in the green one. QRhi has no two
  // channel float format, so the other two channels go unused rather than unallocated.
  QByteArray data;
  data.reserve( cells * 4 * int( sizeof( float ) ) );
  for ( int i = 0; i < cells; ++i )
  {
    const float texel[4] = {
      fixedSize || maxVectorLength == 0.0 ? 1.0f : static_cast<float>( vectors.at( i ).length() / maxVectorLength ),
      static_cast<float>( vectors.at( i ).angle() ),
      0.0f,
      0.0f,
    };
    data.append( reinterpret_cast<const char *>( texel ), sizeof( texel ) );
  }

  mArrowsGridTexture = std::make_unique<QgsGpuTexture>();
  mArrowsGridTexture->setData( data, gridSize, QgsGpuTexture::Format::RGBA32F );
  binding.texture = mArrowsGridTexture.get();
  setTexture( binding );
}

void QgsMeshMaterial3D::setArrowTexture( QgsGpuTexture *texture )
{
  TextureBinding binding;
  binding.binding = ArrowTextureBinding;
  binding.texture = texture ? texture : mPlaceholderTexture.get();
  binding.magFilter = QRhiSampler::Nearest;
  binding.minFilter = QRhiSampler::Nearest;
  setTexture( binding );
}

void QgsMeshMaterial3D::setArrowsColor( const QColor &color )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.arrowsColor, color );
  mUniforms.arrowsColor[3] = 1.0f;
  updateUniforms();
}

void QgsMeshMaterial3D::setArrowsPlacement( const QPointF &minCorner, float spacing )
{
  mUniforms.arrowsMinCorner[0] = static_cast<float>( minCorner.x() );
  mUniforms.arrowsMinCorner[1] = static_cast<float>( minCorner.y() );
  mUniforms.arrowsSpacing = spacing;
  updateUniforms();
}

void QgsMeshMaterial3D::setArrowsEnabled( bool enabled )
{
  setFeature( Feature::Arrows, enabled );
}
