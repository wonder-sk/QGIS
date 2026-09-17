/***************************************************************************
  qgsmaterial3dfactory.cpp
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

#include "qgsmaterial3dfactory.h"

#include "qgs3dutils.h"
#include "qgsapplication.h"
#include "qgsgoochmaterial3d.h"
#include "qgsgoochmaterialsettings.h"
#include "qgsgputexture.h"
#include "qgsimagecache.h"
#include "qgslogger.h"
#include "qgsmaterial3dutils.h"
#include "qgsmaterialcontext.h"
#include "qgsmetalroughmaterial3d.h"
#include "qgsmetalroughmaterialsettings.h"
#include "qgsmetalroughtexturedmaterialsettings.h"
#include "qgsphongmaterial3d.h"
#include "qgsphongmaterialsettings.h"
#include "qgsphongtexturedmaterial3d.h"
#include "qgsphongtexturedmaterialsettings.h"
#include "qgssceneobject3d.h"
#include "qgssettings.h"
#include "qgsunlitmaterial3d.h"
#include "qgsunlitmaterialsettings.h"

#include <QColor>
#include <QString>

using namespace Qt::StringLiterals;

///@cond PRIVATE

namespace
{

  //! Returns TRUE if the technique draws a surface, which is all any of these materials can draw
  bool isTriangleTechnique( Qgis::MaterialRenderingTechnique technique )
  {
    switch ( technique )
    {
      case Qgis::MaterialRenderingTechnique::Triangles:
      case Qgis::MaterialRenderingTechnique::TrianglesDataDefined:
      case Qgis::MaterialRenderingTechnique::TrianglesWithFixedTexture:
      case Qgis::MaterialRenderingTechnique::TrianglesFromModel:
      case Qgis::MaterialRenderingTechnique::Points:
        return true;

      case Qgis::MaterialRenderingTechnique::Lines:
      case Qgis::MaterialRenderingTechnique::InstancedPoints:
      case Qgis::MaterialRenderingTechnique::Billboards:
        return false;
    }
    return false;
  }

  /**
   * Loads the image at \a path and hands a texture for it to \a object, or returns
   * NULLPTR when there is no such image.
   *
   * Anisotropic filtering, which the texture filter quality setting asks for, has no
   * QRhiSampler equivalent, so every texture gets a mip chain filtered trilinearly.
   */
  QgsGpuTexture *loadTexture( QgsSceneObject3D *object, const QString &path, bool isSrgb )
  {
    if ( path.isEmpty() || !object )
      return nullptr;

    bool fitsInCache = false;
    const QImage image = QgsApplication::imageCache()->pathAsImage( path, QSize(), true, 1.0, fitsInCache );
    if ( image.isNull() )
      return nullptr;

    auto texture = std::make_unique<QgsGpuTexture>();
    texture->setImage( image );
    texture->setMipmapsEnabled( true );
    texture->setSrgb( isSrgb );
    return object->addTexture( std::move( texture ) );
  }

  //! Binds \a texture at \a binding so that texture coordinates outside the unit square repeat
  void bindRepeating( QgsMaterial3D *material, int binding, QgsGpuTexture *texture )
  {
    QgsMaterial3D::TextureBinding bound;
    bound.binding = binding;
    bound.texture = texture;
    bound.mipmapMode = QRhiSampler::Linear;
    bound.addressU = QRhiSampler::Repeat;
    bound.addressV = QRhiSampler::Repeat;
    material->setTexture( bound );
  }

  //! Returns TRUE if the texture transform of \a settings comes from a data defined property
  bool hasDataDefinedTextureTransform( const QgsAbstractMaterialSettings *settings )
  {
    const QgsPropertyCollection &properties = settings->dataDefinedProperties();
    return properties.isActive( QgsAbstractMaterialSettings::Property::TextureOffset )
           || properties.isActive( QgsAbstractMaterialSettings::Property::TextureScale )
           || properties.isActive( QgsAbstractMaterialSettings::Property::TextureRotation );
  }

  std::unique_ptr<QgsMaterial3D> createPhongMaterial( const QgsPhongMaterialSettings *settings, const QgsMaterialContext &context, bool instanced, Qgis::InstancedMaterialFlags flags )
  {
    // the data defined and instanced forms read different vertex attributes, so which one
    // this is has to be decided before the material exists rather than set on it
    const bool dataDefined = !instanced && QgsMaterial3DFactory::hasDataDefinedColors( settings );

    auto material = std::make_unique<QgsPhongMaterial3D>( dataDefined, instanced, flags );
    if ( !dataDefined )
    {
      const QColor ambient = context.isSelected() ? context.selectionColor().darker() : settings->ambient();
      const QColor diffuse = context.isSelected() ? context.selectionColor() : settings->diffuse();
      material->setAmbient( ambient, static_cast<float>( settings->ambientCoefficient() ) );
      material->setDiffuse( diffuse, static_cast<float>( settings->diffuseCoefficient() ) );
      material->setSpecular( settings->specular(), static_cast<float>( settings->specularCoefficient() ) );
    }
    material->setShininess( static_cast<float>( settings->shininess() ) );
    material->setOpacity( static_cast<float>( settings->opacity() ) );
    return material;
  }

  std::unique_ptr<QgsMaterial3D> createGoochMaterial( const QgsGoochMaterialSettings *settings, const QgsMaterialContext &context, bool instanced, Qgis::InstancedMaterialFlags flags )
  {
    const bool dataDefined = !instanced && QgsMaterial3DFactory::hasDataDefinedColors( settings );

    auto material = std::make_unique<QgsGoochMaterial3D>( dataDefined, instanced, flags );
    material->setDiffuse( context.isSelected() ? context.selectionColor() : settings->diffuse() );
    material->setSpecular( settings->specular() );
    material->setCool( settings->cool() );
    material->setWarm( settings->warm() );
    material->setShininess( static_cast<float>( settings->shininess() ) );
    material->setAlpha( static_cast<float>( settings->alpha() ) );
    material->setBeta( static_cast<float>( settings->beta() ) );
    return material;
  }

  std::unique_ptr<QgsMaterial3D> createUnlitMaterial( const QgsUnlitMaterialSettings *settings, const QgsMaterialContext &context, bool instanced, Qgis::InstancedMaterialFlags flags )
  {
    const bool dataDefined = !instanced && QgsMaterial3DFactory::hasDataDefinedColors( settings );

    auto material = std::make_unique<QgsUnlitMaterial3D>( dataDefined, instanced, flags );
    if ( !dataDefined )
      material->setColor( context.isSelected() ? context.selectionColor() : settings->color() );
    return material;
  }

  std::unique_ptr<QgsMaterial3D> createPhongTexturedMaterial(
    const QgsPhongTexturedMaterialSettings *settings, const QgsMaterialContext &context, bool instanced, Qgis::InstancedMaterialFlags flags, QgsSceneObject3D *object
  )
  {
    QgsGpuTexture *texture = loadTexture( object, settings->diffuseTexturePath(), true );
    if ( !texture )
    {
      // with no texture there is nothing this material can do that the plain Phong one
      // cannot, and its shader would sample an unbound texture
      QgsPhongMaterialSettings fallback;
      fallback.setDiffuse( QColor::fromRgbF( 0.7f, 0.7f, 0.7f, 1.0f ) ); // the Qt3D default diffuse colour
      fallback.setAmbient( settings->ambient() );
      fallback.setSpecular( settings->specular() );
      fallback.setShininess( settings->shininess() );
      fallback.setOpacity( settings->opacity() );
      return createPhongMaterial( &fallback, context, instanced, flags );
    }

    const bool dataDefinedTextureTransform = !instanced && hasDataDefinedTextureTransform( settings );

    auto material = std::make_unique<QgsPhongTexturedMaterial3D>( dataDefinedTextureTransform, instanced, flags );
    material->setAmbient( context.isSelected() ? context.selectionColor().darker() : settings->ambient() );
    material->setSpecular( settings->specular() );
    material->setShininess( static_cast<float>( settings->shininess() ) );
    material->setOpacity( static_cast<float>( settings->opacity() ) );
    material->setDiffuseTexture( texture );
    material->setDiffuseTextureScale( static_cast<float>( settings->textureScale() ) );
    material->setDiffuseTextureRotation( static_cast<float>( settings->textureRotation() ) );
    material->setDiffuseTextureOffset( static_cast<float>( settings->textureOffset().x() ), static_cast<float>( settings->textureOffset().y() ) );

    // setDiffuseTexture() clamps, which is right for a terrain tile's map image and wrong
    // here: the texture scale exists so that a symbol's texture tiles across a surface
    bindRepeating( material.get(), QgsPhongTexturedMaterial3D::DiffuseTextureBinding, texture );
    return material;
  }

  void applyMetalRoughSettings( QgsMetalRoughMaterial3D *material, const QgsMetalRoughMaterialSettings *settings, const QgsMaterialContext &context )
  {
    material->setBaseColor( context.isSelected() ? context.selectionColor() : settings->baseColor() );
    material->setEmissionColor( settings->emissionColor().isValid() ? settings->emissionColor() : QColor( 0, 0, 0 ) );
    material->setEmissionFactor( static_cast<float>( settings->emissionFactor() ) );
    material->setClearCoatFactor( static_cast<float>( settings->clearCoatFactor() ) );
    material->setClearCoatRoughness( static_cast<float>( settings->clearCoatRoughness() ) );
    material->setMetalness( static_cast<float>( settings->metalness() ) );
    material->setRoughness( static_cast<float>( settings->roughness() ) );
    material->setReflectance( static_cast<float>( settings->reflectance() ) );
    material->setAnisotropy( static_cast<float>( settings->anisotropy() ) );
    material->setAnisotropyRotation( static_cast<float>( settings->anisotropyRotation() ) );
    material->setOpacity( static_cast<float>( settings->opacity() ) );
  }

  void applyMetalRoughTexturedSettings( QgsMetalRoughMaterial3D *material, const QgsMetalRoughTexturedMaterialSettings *settings, QgsSceneObject3D *object )
  {
    material->setTextureScale( static_cast<float>( settings->textureScale() ) );
    material->setTextureRotation( static_cast<float>( settings->textureRotation() ) );
    material->setTextureOffset( static_cast<float>( settings->textureOffset().x() ), static_cast<float>( settings->textureOffset().y() ) );

    // each map falls back to the constant the settings would otherwise supply, which is
    // what switches the corresponding shader feature off
    if ( QgsGpuTexture *texture = loadTexture( object, settings->baseColorTexturePath(), true ) )
      material->setBaseColorTexture( texture );
    else
      material->setBaseColor( QColor( "grey" ) );

    if ( QgsGpuTexture *texture = loadTexture( object, settings->metalnessTexturePath(), false ) )
      material->setMetalnessTexture( texture );
    else
      material->setMetalness( 0.0f );

    if ( QgsGpuTexture *texture = loadTexture( object, settings->roughnessTexturePath(), false ) )
      material->setRoughnessTexture( texture );
    else
      material->setRoughness( 0.5f );

    material->setNormalTexture( loadTexture( object, settings->normalTexturePath(), false ) );
    material->setHeightTexture( loadTexture( object, settings->heightTexturePath(), false ) );
    material->setParallaxScale( static_cast<float>( settings->parallaxScale() ) );
    material->setAmbientOcclusionTexture( loadTexture( object, settings->ambientOcclusionTexturePath(), false ) );
    material->setEmissionTexture( loadTexture( object, settings->emissionTexturePath(), true ) );
    material->setEmissionFactor( static_cast<float>( settings->emissionFactor() ) );
    material->setOpacity( static_cast<float>( settings->opacity() ) );
  }

  QgsMetalRoughMaterial3D::Mode metalRoughMode( const QgsAbstractMaterialSettings *settings, bool instanced )
  {
    if ( instanced )
      return QgsMetalRoughMaterial3D::Mode::Instanced;
    if ( QgsMaterial3DFactory::hasDataDefinedColors( settings ) )
      return QgsMetalRoughMaterial3D::Mode::DataDefinedColors;
    if ( hasDataDefinedTextureTransform( settings ) )
      return QgsMetalRoughMaterial3D::Mode::DataDefinedTextureTransform;
    return QgsMetalRoughMaterial3D::Mode::Uniform;
  }

  //! Builds the material for \a settings, instanced or not, which is all that differs between the two entry points
  std::unique_ptr<QgsMaterial3D> createMaterialForSettings(
    const QgsAbstractMaterialSettings *settings, const QgsMaterialContext &context, bool instanced, Qgis::InstancedMaterialFlags flags, QgsSceneObject3D *object
  )
  {
    const QString type = settings->type();

    if ( type == "phong"_L1 )
      return createPhongMaterial( static_cast<const QgsPhongMaterialSettings *>( settings ), context, instanced, flags );

    if ( type == "gooch"_L1 )
      return createGoochMaterial( static_cast<const QgsGoochMaterialSettings *>( settings ), context, instanced, flags );

    if ( type == "unlit"_L1 )
      return createUnlitMaterial( static_cast<const QgsUnlitMaterialSettings *>( settings ), context, instanced, flags );

    if ( type == "phongtextured"_L1 )
      return createPhongTexturedMaterial( static_cast<const QgsPhongTexturedMaterialSettings *>( settings ), context, instanced, flags, object );

    if ( type == "metalrough"_L1 )
    {
      const auto *metalRough = static_cast<const QgsMetalRoughMaterialSettings *>( settings );
      auto material = std::make_unique<QgsMetalRoughMaterial3D>( metalRoughMode( settings, instanced ), QgsMetalRoughMaterial3D::VertexAttributes(), flags );
      applyMetalRoughSettings( material.get(), metalRough, context );
      return material;
    }

    if ( type == "metalroughtextured"_L1 )
    {
      const auto *textured = static_cast<const QgsMetalRoughTexturedMaterialSettings *>( settings );
      // every map is sampled at the primitive's own texture coordinates, and the normal
      // and height maps are only usable with a tangent frame to read them in
      QgsMetalRoughMaterial3D::VertexAttributes attributes = QgsMetalRoughMaterial3D::VertexAttribute::TextureCoords;
      if ( textured->requiresTangents() )
        attributes |= QgsMetalRoughMaterial3D::VertexAttribute::Tangents;

      auto material = std::make_unique<QgsMetalRoughMaterial3D>( metalRoughMode( settings, instanced ), attributes, flags );
      applyMetalRoughTexturedSettings( material.get(), textured, object );
      return material;
    }

    // "null" has no material of its own - it is what a model symbol uses to say that the
    // model's own materials should be kept - and "simpleline" only ever had one for the
    // Lines technique, which is P6's thick line work
    if ( type != "null"_L1 && type != "simpleline"_L1 )
      QgsDebugError( u"No QRhi material for material settings type: %1"_s.arg( type ) );

    return nullptr;
  }

} // namespace

std::unique_ptr<QgsMaterial3D> QgsMaterial3DFactory::createMaterial(
  const QgsAbstractMaterialSettings *settings, Qgis::MaterialRenderingTechnique technique, const QgsMaterialContext &context, QgsSceneObject3D *object
)
{
  if ( !settings || !isTriangleTechnique( technique ) )
    return nullptr;

  if ( context.isHighlighted() )
    return createHighlightMaterial();

  return createMaterialForSettings( settings, context, false, Qgis::InstancedMaterialFlags(), object );
}

std::unique_ptr<QgsMaterial3D> QgsMaterial3DFactory::createInstancedMaterial(
  const QgsAbstractMaterialSettings *settings,
  const QgsMaterialContext &context,
  Qgis::InstancedMaterialFlags flags,
  const QMatrix4x4 &meshTransform,
  const QVector3D &symbolScale,
  const QQuaternion &symbolRotation,
  QgsSceneObject3D *object
)
{
  std::unique_ptr<QgsMaterial3D> material;
  if ( context.isHighlighted() )
    material = createHighlightMaterial( true, flags, meshTransform );
  else if ( settings )
    material = createMaterialForSettings( settings, context, true, flags, object );

  if ( !material )
    return nullptr;

  // these three belong to the instanced shader variants only, and every material that has
  // them spells them the same way while sharing no base class that could say so
  const auto applyInstanceUniforms = [&meshTransform, &symbolScale, &symbolRotation]( auto *typed ) {
    typed->setMeshTransform( meshTransform );
    typed->setSymbolScale( symbolScale );
    typed->setSymbolRotation( symbolRotation );
  };

  if ( auto *phong = dynamic_cast<QgsPhongMaterial3D *>( material.get() ) )
    applyInstanceUniforms( phong );
  else if ( auto *gooch = dynamic_cast<QgsGoochMaterial3D *>( material.get() ) )
    applyInstanceUniforms( gooch );
  else if ( auto *unlit = dynamic_cast<QgsUnlitMaterial3D *>( material.get() ) )
    applyInstanceUniforms( unlit );
  else if ( auto *phongTextured = dynamic_cast<QgsPhongTexturedMaterial3D *>( material.get() ) )
    applyInstanceUniforms( phongTextured );
  else if ( auto *metalRough = dynamic_cast<QgsMetalRoughMaterial3D *>( material.get() ) )
    applyInstanceUniforms( metalRough );

  return material;
}

std::unique_ptr<QgsMaterial3D> QgsMaterial3DFactory::createHighlightMaterial( bool instanced, Qgis::InstancedMaterialFlags flags, const QMatrix4x4 &meshTransform )
{
  const QgsSettings settings;
  const float alpha = settings.value( u"Map/highlight/colorAlpha"_s, Qgis::DEFAULT_HIGHLIGHT_COLOR.alpha() ).toFloat() / 255.f;
  QColor color = QColor( settings.value( u"Map/highlight/color"_s, Qgis::DEFAULT_HIGHLIGHT_COLOR.name() ).toString() );
  color.setAlphaF( alpha );

  auto material = std::make_unique<QgsUnlitMaterial3D>( false, instanced, flags );
  material->setColor( color );
  material->setMeshTransform( meshTransform );
  return material;
}

bool QgsMaterial3DFactory::hasDataDefinedColors( const QgsAbstractMaterialSettings *settings )
{
  if ( !settings )
    return false;

  const QgsPropertyCollection &properties = settings->dataDefinedProperties();
  const QStringList names = dataDefinedColorAttributes( settings );
  if ( names.isEmpty() )
    return false;

  const QString type = settings->type();
  if ( type == "phong"_L1 )
  {
    return properties.isActive( QgsAbstractMaterialSettings::Property::Ambient )
           || properties.isActive( QgsAbstractMaterialSettings::Property::Diffuse )
           || properties.isActive( QgsAbstractMaterialSettings::Property::Specular );
  }
  if ( type == "gooch"_L1 )
  {
    return properties.isActive( QgsAbstractMaterialSettings::Property::Warm )
           || properties.isActive( QgsAbstractMaterialSettings::Property::Cool )
           || properties.isActive( QgsAbstractMaterialSettings::Property::Diffuse )
           || properties.isActive( QgsAbstractMaterialSettings::Property::Specular );
  }
  if ( type == "unlit"_L1 )
    return properties.isActive( QgsAbstractMaterialSettings::Property::BaseColor );

  if ( type == "metalrough"_L1 || type == "metalroughtextured"_L1 )
  {
    return properties.isActive( QgsAbstractMaterialSettings::Property::BaseColor ) || properties.isActive( QgsAbstractMaterialSettings::Property::EmissionColor );
  }

  return false;
}

QStringList QgsMaterial3DFactory::dataDefinedColorAttributes( const QgsAbstractMaterialSettings *settings )
{
  if ( !settings )
    return {};

  // the order is the order the colours are packed in, and each name has to be an input
  // the material's data defined shader variant actually reads
  const QString type = settings->type();
  if ( type == "phong"_L1 )
    return { u"dataDefinedAmbientColor"_s, u"dataDefinedDiffuseColor"_s, u"dataDefinedSpecularColor"_s };
  if ( type == "gooch"_L1 )
    return { u"dataDefinedDiffuseColor"_s, u"dataDefinedWarmColor"_s, u"dataDefinedCoolColor"_s, u"dataDefinedSpecularColor"_s };
  if ( type == "unlit"_L1 )
    return { u"dataDefinedBaseColor"_s };
  if ( type == "metalrough"_L1 || type == "metalroughtextured"_L1 )
    return { u"dataDefinedBaseColor"_s, u"dataDefinedEmissionColor"_s };

  return {};
}

bool QgsMaterial3DFactory::dataDefinedColorsUseFloats( const QgsAbstractMaterialSettings *settings )
{
  if ( !settings )
    return false;

  const QString type = settings->type();
  if ( type == "phong"_L1 )
  {
    // a coefficient below one scales the colour down before it is stored, and a byte per
    // channel is not enough precision for that
    const auto *phong = static_cast<const QgsPhongMaterialSettings *>( settings );
    return phong->diffuseCoefficient() < 1 || phong->ambientCoefficient() < 1 || phong->specularCoefficient() < 1;
  }

  // the unlit and metal rough shaders take their colours already in the 0 to 1 range the
  // material works in, so they were floats under Qt3D too
  return type == "unlit"_L1 || type == "metalrough"_L1 || type == "metalroughtextured"_L1;
}

QByteArray QgsMaterial3DFactory::dataDefinedColors( const QgsAbstractMaterialSettings *settings, const QgsExpressionContext &expressionContext )
{
  const QStringList names = dataDefinedColorAttributes( settings );
  if ( names.isEmpty() )
    return {};

  const QgsPropertyCollection &properties = settings->dataDefinedProperties();
  const bool useFloats = dataDefinedColorsUseFloats( settings );

  // the shaders read these colours as they are, so the sRGB to linear conversion the
  // uniform blocks get from setLinearColor() has to happen here instead
  const auto evaluate = [&properties, &expressionContext]( QgsAbstractMaterialSettings::Property property, const QColor &defaultColor, double coefficient = 1.0 ) {
    QColor color = Qgs3DUtils::srgbToLinear( properties.valueAsColor( property, expressionContext, defaultColor ) );
    if ( coefficient < 1.0 )
      color = QColor::fromRgbF( color.redF() * coefficient, color.greenF() * coefficient, color.blueF() * coefficient, color.alphaF() );
    return color;
  };

  QList<QColor> colors;
  const QString type = settings->type();
  if ( type == "phong"_L1 )
  {
    const auto *phong = static_cast<const QgsPhongMaterialSettings *>( settings );
    colors
      << evaluate( QgsAbstractMaterialSettings::Property::Ambient, phong->ambient(), phong->ambientCoefficient() )
      << evaluate( QgsAbstractMaterialSettings::Property::Diffuse, phong->diffuse(), phong->diffuseCoefficient() )
      << evaluate( QgsAbstractMaterialSettings::Property::Specular, phong->specular(), phong->specularCoefficient() );
  }
  else if ( type == "gooch"_L1 )
  {
    const auto *gooch = static_cast<const QgsGoochMaterialSettings *>( settings );
    colors
      << evaluate( QgsAbstractMaterialSettings::Property::Diffuse, gooch->diffuse() )
      << evaluate( QgsAbstractMaterialSettings::Property::Warm, gooch->warm() )
      << evaluate( QgsAbstractMaterialSettings::Property::Cool, gooch->cool() )
      << evaluate( QgsAbstractMaterialSettings::Property::Specular, gooch->specular() );
  }
  else if ( type == "unlit"_L1 )
  {
    const auto *unlit = static_cast<const QgsUnlitMaterialSettings *>( settings );
    colors << evaluate( QgsAbstractMaterialSettings::Property::BaseColor, unlit->color() );
  }
  else if ( type == "metalrough"_L1 || type == "metalroughtextured"_L1 )
  {
    const QColor baseColor = type == "metalrough"_L1 ? static_cast<const QgsMetalRoughMaterialSettings *>( settings )->baseColor() : QColor( "grey" );
    const QColor emissionColor = type == "metalrough"_L1 ? static_cast<const QgsMetalRoughMaterialSettings *>( settings )->emissionColor() : QColor();

    colors << evaluate( QgsAbstractMaterialSettings::Property::BaseColor, baseColor );

    // an invalid emission colour means no emission at all rather than black shifted
    // through the colour curve, so it is not evaluated the same way
    const QColor rawEmission = properties.valueAsColor( QgsAbstractMaterialSettings::Property::EmissionColor, expressionContext, emissionColor );
    colors << ( rawEmission.isValid() ? Qgs3DUtils::srgbToLinear( rawEmission ) : QColor( 0, 0, 0 ) );
  }

  QByteArray packed;
  packed.resize( QgsMaterial3DUtils::dataDefinedColorStride( static_cast<int>( colors.size() ), useFloats ) );
  QgsMaterial3DUtils::writeDataDefinedColors( packed.data(), colors, useFloats );
  return packed;
}

/// @endcond
