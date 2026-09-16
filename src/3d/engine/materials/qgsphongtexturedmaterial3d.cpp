/***************************************************************************
  qgsphongtexturedmaterial3d.cpp
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

#include "qgsphongtexturedmaterial3d.h"

#include "qgsgputexture.h"
#include "qgslogger.h"
#include "qgsmaterial3dutils.h"
#include "qgsrenderuniforms.h"

#include <QQuaternion>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsPhongTexturedUniforms ) == 208, "QgsPhongTexturedUniforms must match its std140 layout" );

std::shared_ptr<QgsRhiShader> QgsPhongTexturedMaterial3D::shaderFor( bool dataDefinedTextureTransform, bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
{
  const QString variant = QgsMaterial3DUtils::shaderVariantSuffix( dataDefinedTextureTransform, instanced, instanceFlags );

  // only the texture coordinates differ between the variants, so there is one fragment stage
  return QgsRhiShader::load( u":/shaders/rhi/phongtextured%1.vert.qsb"_s.arg( variant ), u":/shaders/rhi/phongtextured.frag.qsb"_s );
}

QgsPhongTexturedMaterial3D::QgsPhongTexturedMaterial3D( bool dataDefinedTextureTransform, bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
  : QgsMaterial3D( shaderFor( dataDefinedTextureTransform, instanced, instanceFlags ) )
  , mDataDefinedTextureTransform( dataDefinedTextureTransform )
  , mInstanced( instanced )
{
#ifdef QGISDEBUG
  // the C++ struct and the GLSL block are written by hand in two different files, so check
  // once that they agree rather than let a bad offset corrupt everything after it
  const QVector<QgsUniformMember> members {
    { u"ambientColor"_s, offsetof( QgsPhongTexturedUniforms, ambientColor ), 16 },
    { u"specularColor"_s, offsetof( QgsPhongTexturedUniforms, specularColor ), 16 },
    { u"shininess"_s, offsetof( QgsPhongTexturedUniforms, shininess ), 4 },
    { u"opacity"_s, offsetof( QgsPhongTexturedUniforms, opacity ), 4 },
    { u"texCoordScale"_s, offsetof( QgsPhongTexturedUniforms, texCoordScale ), 4 },
    { u"texCoordRotation"_s, offsetof( QgsPhongTexturedUniforms, texCoordRotation ), 4 },
    { u"texCoordOffset"_s, offsetof( QgsPhongTexturedUniforms, texCoordOffset ), 8 },
    { u"decodeSrgb"_s, offsetof( QgsPhongTexturedUniforms, decodeSrgb ), 4 },
    { u"meshMatrix"_s, offsetof( QgsPhongTexturedUniforms, meshMatrix ), 64 },
    { u"meshNormalMatrix"_s, offsetof( QgsPhongTexturedUniforms, meshNormalMatrix ), 48 },
    { u"symbolRotation"_s, offsetof( QgsPhongTexturedUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsPhongTexturedUniforms, symbolScale ), 16 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsPhongTexturedUniforms does not match QgsPhongTexturedBlock: %1"_s.arg( error ) );
#endif

  // the same defaults QgsPhongTexturedMaterial had
  setAmbient( QColor::fromRgbF( 0.05f, 0.05f, 0.05f ) );
  setSpecular( QColor::fromRgbF( 0.01f, 0.01f, 0.01f ) );

  updateUniforms();
}

void QgsPhongTexturedMaterial3D::updateUniforms()
{
  setUniforms( mUniforms );
}

void QgsPhongTexturedMaterial3D::setAmbient( const QColor &ambient )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.ambientColor, ambient );
  updateUniforms();
}

void QgsPhongTexturedMaterial3D::setSpecular( const QColor &specular )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.specularColor, specular );
  updateUniforms();
}

void QgsPhongTexturedMaterial3D::setShininess( float shininess )
{
  mUniforms.shininess = shininess;
  updateUniforms();
}

void QgsPhongTexturedMaterial3D::setOpacity( float opacity )
{
  mUniforms.opacity = opacity;
  updateUniforms();
}

void QgsPhongTexturedMaterial3D::setDiffuseTexture( QgsGpuTexture *texture )
{
  mDiffuseTexture = texture;

  // a picture is sRGB encoded and shading happens in linear space; the shader only has to
  // convert on a backend that did not give the texture an sRGB format
  mUniforms.decodeSrgb = texture && texture->needsShaderSrgbDecode() ? 1.0f : 0.0f;
  updateUniforms();

  TextureBinding binding;
  binding.binding = DiffuseTextureBinding;
  binding.texture = texture;
  setTexture( binding );
}

void QgsPhongTexturedMaterial3D::setDiffuseTextureScale( float scale )
{
  mUniforms.texCoordScale = scale;
  updateUniforms();
}

void QgsPhongTexturedMaterial3D::setDiffuseTextureRotation( float rotation )
{
  mUniforms.texCoordRotation = rotation;
  updateUniforms();
}

void QgsPhongTexturedMaterial3D::setDiffuseTextureOffset( float offsetX, float offsetY )
{
  mUniforms.texCoordOffset[0] = offsetX;
  mUniforms.texCoordOffset[1] = offsetY;
  updateUniforms();
}

void QgsPhongTexturedMaterial3D::setMeshTransform( const QMatrix4x4 &transform )
{
  QgsMaterial3DUtils::setMatrix4( mUniforms.meshMatrix, transform );
  QgsMaterial3DUtils::setNormalMatrix( mUniforms.meshNormalMatrix, transform );
  updateUniforms();
}

void QgsPhongTexturedMaterial3D::setSymbolScale( const QVector3D &scale )
{
  QgsMaterial3DUtils::setVector3( mUniforms.symbolScale, scale );
  updateUniforms();
}

void QgsPhongTexturedMaterial3D::setSymbolRotation( const QQuaternion &rotation )
{
  QgsMaterial3DUtils::setQuaternion( mUniforms.symbolRotation, rotation );
  updateUniforms();
}
