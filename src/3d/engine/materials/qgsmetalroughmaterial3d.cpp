/***************************************************************************
  qgsmetalroughmaterial3d.cpp
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

#include "qgsmetalroughmaterial3d.h"

#include <cmath>

#include "qgslogger.h"
#include "qgsmaterial3dutils.h"
#include "qgsrenderuniforms.h"

#include <QImage>
#include <QQuaternion>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsMetalRoughUniforms ) == 240, "QgsMetalRoughUniforms must match its std140 layout" );

QString QgsMetalRoughMaterial3D::shaderVariant( Mode mode, VertexAttributes attributes, Qgis::InstancedMaterialFlags instanceFlags )
{
  // which optional attributes the geometry supplies, as CMake names them
  QString attributeSuffix;
  if ( attributes.testFlag( VertexAttribute::Tangents ) )
    attributeSuffix = u"_texcoord_tangent"_s;
  else if ( attributes.testFlag( VertexAttribute::TextureCoords ) )
    attributeSuffix = u"_texcoord"_s;

  switch ( mode )
  {
    case Mode::DataDefinedColors:
      // per-vertex colours instead of texture coordinates, so no attribute suffix
      return u"_datadefined"_s;

    case Mode::Instanced:
      // instanced geometry varies in both axes: a glTF tile drawn through
      // EXT_mesh_gpu_instancing may carry texture coordinates, tangents or neither,
      // where the primitive generators the point symbols use always carry both
      return QgsMaterial3DUtils::shaderVariantSuffix( false, true, instanceFlags ) + attributeSuffix;

    case Mode::DataDefinedTextureTransform:
      return attributes.testFlag( VertexAttribute::Tangents ) ? u"_ddtexture_tangent"_s : u"_ddtexture"_s;

    case Mode::Uniform:
      break;
  }

  return attributeSuffix;
}

std::shared_ptr<QgsRhiShader> QgsMetalRoughMaterial3D::shaderFor( Mode mode, VertexAttributes attributes, Qgis::InstancedMaterialFlags instanceFlags )
{
  const QString variant = shaderVariant( mode, attributes, instanceFlags );

  // the fragment stage only differs in whether it reads the data defined colours
  const QString fragmentVariant = mode == Mode::DataDefinedColors ? u"_datadefined"_s : QString();

  return QgsRhiShader::load( u":/shaders/rhi/metalrough%1.vert.qsb"_s.arg( variant ), u":/shaders/rhi/metalrough%1.frag.qsb"_s.arg( fragmentVariant ) );
}

QgsMetalRoughMaterial3D::QgsMetalRoughMaterial3D( Mode mode, VertexAttributes attributes, Qgis::InstancedMaterialFlags instanceFlags )
  : QgsMaterial3D( shaderFor( mode, attributes, instanceFlags ) )
  , mMode( mode )
  , mVertexAttributes( attributes )
{
#ifdef QGISDEBUG
  // the C++ struct and the GLSL block are written by hand in two different files, so check
  // once that they agree rather than let a bad offset corrupt everything after it
  const QVector<QgsUniformMember> members {
    { u"baseColor"_s, offsetof( QgsMetalRoughUniforms, baseColor ), 16 },
    { u"emissiveColor"_s, offsetof( QgsMetalRoughUniforms, emissiveColor ), 16 },
    { u"metalness"_s, offsetof( QgsMetalRoughUniforms, metalness ), 4 },
    { u"roughness"_s, offsetof( QgsMetalRoughUniforms, roughness ), 4 },
    { u"reflectance"_s, offsetof( QgsMetalRoughUniforms, reflectance ), 4 },
    { u"opacity"_s, offsetof( QgsMetalRoughUniforms, opacity ), 4 },
    { u"anisotropy"_s, offsetof( QgsMetalRoughUniforms, anisotropy ), 4 },
    { u"anisotropyRotation"_s, offsetof( QgsMetalRoughUniforms, anisotropyRotation ), 4 },
    { u"clearCoatFactor"_s, offsetof( QgsMetalRoughUniforms, clearCoatFactor ), 4 },
    { u"clearCoatRoughness"_s, offsetof( QgsMetalRoughUniforms, clearCoatRoughness ), 4 },
    { u"parallaxScale"_s, offsetof( QgsMetalRoughUniforms, parallaxScale ), 4 },
    { u"emissiveFactor"_s, offsetof( QgsMetalRoughUniforms, emissiveFactor ), 4 },
    { u"texCoordScale"_s, offsetof( QgsMetalRoughUniforms, texCoordScale ), 4 },
    { u"texCoordRotation"_s, offsetof( QgsMetalRoughUniforms, texCoordRotation ), 4 },
    { u"texCoordOffset"_s, offsetof( QgsMetalRoughUniforms, texCoordOffset ), 8 },
    { u"featureFlags"_s, offsetof( QgsMetalRoughUniforms, featureFlags ), 4 },
    { u"meshMatrix"_s, offsetof( QgsMetalRoughUniforms, meshMatrix ), 64 },
    { u"meshNormalMatrix"_s, offsetof( QgsMetalRoughUniforms, meshNormalMatrix ), 48 },
    { u"symbolRotation"_s, offsetof( QgsMetalRoughUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsMetalRoughUniforms, symbolScale ), 16 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsMetalRoughUniforms does not match QgsMetalRoughBlock: %1"_s.arg( error ) );
#endif

  QImage placeholder( 1, 1, QImage::Format_RGBA8888 );
  placeholder.fill( Qt::white );
  mPlaceholderTexture.setImage( placeholder );

  // every sampler the shader declares has to be bound, so start with the placeholder
  // behind all of them and let the setters replace the ones that get a real map
  for ( const Binding binding : { BaseColorMapBinding, MetalnessMapBinding, RoughnessMapBinding, AmbientOcclusionMapBinding, NormalMapBinding, HeightMapBinding, EmissionMapBinding } )
  {
    TextureBinding texture;
    texture.binding = binding;
    texture.texture = &mPlaceholderTexture;
    setTexture( texture );
  }

  // the same defaults QgsMetalRoughMaterial had
  setBaseColor( QColor( "grey" ) );
  setEmissionColor( QColor( 0, 0, 0 ) );

  updateUniforms();
}

QgsMetalRoughMaterial3D::Features QgsMetalRoughMaterial3D::features() const
{
  return Features( static_cast<int>( mUniforms.featureFlags ) );
}

void QgsMetalRoughMaterial3D::updateUniforms()
{
  setUniforms( mUniforms );
}

void QgsMetalRoughMaterial3D::setFeature( Feature feature, bool enabled )
{
  const quint32 bit = static_cast<quint32>( feature );
  if ( enabled )
    mUniforms.featureFlags |= bit;
  else
    mUniforms.featureFlags &= ~bit;

  updateUniforms();
}

void QgsMetalRoughMaterial3D::setMapTexture( Binding binding, Feature feature, QgsGpuTexture *texture )
{
  TextureBinding bound;
  bound.binding = binding;
  bound.texture = texture ? texture : &mPlaceholderTexture;
  bound.addressU = QRhiSampler::Repeat;
  bound.addressV = QRhiSampler::Repeat;
  setTexture( bound );

  setFeature( feature, texture != nullptr );
}

void QgsMetalRoughMaterial3D::setBaseColor( const QColor &color )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.baseColor, color );
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setBaseColorTexture( QgsGpuTexture *texture )
{
  setMapTexture( BaseColorMapBinding, Feature::BaseColorMap, texture );
}

void QgsMetalRoughMaterial3D::setMetalness( float metalness )
{
  mUniforms.metalness = metalness;
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setMetalnessTexture( QgsGpuTexture *texture )
{
  setMapTexture( MetalnessMapBinding, Feature::MetalnessMap, texture );
}

void QgsMetalRoughMaterial3D::setRoughness( float roughness )
{
  mUniforms.roughness = roughness;
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setRoughnessTexture( QgsGpuTexture *texture )
{
  setMapTexture( RoughnessMapBinding, Feature::RoughnessMap, texture );
}

void QgsMetalRoughMaterial3D::setReflectance( float reflectance )
{
  mUniforms.reflectance = reflectance;
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setAnisotropy( float anisotropy )
{
  mUniforms.anisotropy = anisotropy;
  setFeature( Feature::Anisotropy, anisotropy > 0.0f );
}

void QgsMetalRoughMaterial3D::setAnisotropyRotation( float rotation )
{
  // the shader takes the sine and cosine of this directly, so it is stored in radians
  mUniforms.anisotropyRotation = static_cast<float>( M_PI ) * rotation / 180.0f;
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setAmbientOcclusionTexture( QgsGpuTexture *texture )
{
  setMapTexture( AmbientOcclusionMapBinding, Feature::AmbientOcclusionMap, texture );
}

void QgsMetalRoughMaterial3D::setNormalTexture( QgsGpuTexture *texture )
{
  setMapTexture( NormalMapBinding, Feature::NormalMap, texture );
}

void QgsMetalRoughMaterial3D::setHeightTexture( QgsGpuTexture *texture )
{
  setMapTexture( HeightMapBinding, Feature::HeightMap, texture );
}

void QgsMetalRoughMaterial3D::setParallaxScale( float scale )
{
  mUniforms.parallaxScale = scale;
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setEmissionTexture( QgsGpuTexture *texture )
{
  setMapTexture( EmissionMapBinding, Feature::EmissionMap, texture );
}

void QgsMetalRoughMaterial3D::setEmissionColor( const QColor &color )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.emissiveColor, color );
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setEmissionFactor( float factor )
{
  mUniforms.emissiveFactor = factor;
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setClearCoatFactor( float factor )
{
  mUniforms.clearCoatFactor = factor;
  setFeature( Feature::ClearCoat, factor > 0.0f );
}

void QgsMetalRoughMaterial3D::setClearCoatRoughness( float roughness )
{
  mUniforms.clearCoatRoughness = roughness;
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setFlatShadingEnabled( bool enabled )
{
  setFeature( Feature::FlatShading, enabled );
}

void QgsMetalRoughMaterial3D::setOpacity( float opacity )
{
  mUniforms.opacity = opacity;
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setTextureScale( float scale )
{
  mUniforms.texCoordScale = scale;
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setTextureRotation( float rotation )
{
  mUniforms.texCoordRotation = rotation;
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setTextureOffset( float offsetX, float offsetY )
{
  mUniforms.texCoordOffset[0] = offsetX;
  mUniforms.texCoordOffset[1] = offsetY;
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setMeshTransform( const QMatrix4x4 &transform )
{
  QgsMaterial3DUtils::setMatrix4( mUniforms.meshMatrix, transform );
  QgsMaterial3DUtils::setNormalMatrix( mUniforms.meshNormalMatrix, transform );
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setSymbolScale( const QVector3D &scale )
{
  QgsMaterial3DUtils::setVector3( mUniforms.symbolScale, scale );
  updateUniforms();
}

void QgsMetalRoughMaterial3D::setSymbolRotation( const QQuaternion &rotation )
{
  QgsMaterial3DUtils::setQuaternion( mUniforms.symbolRotation, rotation );
  updateUniforms();
}
