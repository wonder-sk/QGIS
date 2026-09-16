/***************************************************************************
  qgstexturematerial3d.cpp
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

#include "qgstexturematerial3d.h"

#include "qgsgputexture.h"
#include "qgslogger.h"
#include "qgsmaterial3dutils.h"
#include "qgsrenderuniforms.h"

#include <QQuaternion>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsTexturedUniforms ) == 112, "QgsTexturedUniforms must match its std140 layout" );

std::shared_ptr<QgsRhiShader> QgsTextureMaterial3D::shaderFor( bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
{
  const QString variant = QgsMaterial3DUtils::shaderVariantSuffix( false, instanced, instanceFlags );

  return QgsRhiShader::load( u":/shaders/rhi/textured%1.vert.qsb"_s.arg( variant ), u":/shaders/rhi/textured.frag.qsb"_s );
}

QgsTextureMaterial3D::QgsTextureMaterial3D( bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
  : QgsMaterial3D( shaderFor( instanced, instanceFlags ) )
  , mInstanced( instanced )
{
#ifdef QGISDEBUG
  // the C++ struct and the GLSL block are written by hand in two different files, so check
  // once that they agree rather than let a bad offset corrupt everything after it
  const QVector<QgsUniformMember> members {
    { u"opacity"_s, offsetof( QgsTexturedUniforms, opacity ), 4 },
    { u"decodeSrgb"_s, offsetof( QgsTexturedUniforms, decodeSrgb ), 4 },
    { u"meshMatrix"_s, offsetof( QgsTexturedUniforms, meshMatrix ), 64 },
    { u"symbolRotation"_s, offsetof( QgsTexturedUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsTexturedUniforms, symbolScale ), 16 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsTexturedUniforms does not match QgsTexturedBlock: %1"_s.arg( error ) );
#endif

  updateUniforms();
}

void QgsTextureMaterial3D::updateUniforms()
{
  setUniforms( mUniforms );
}

void QgsTextureMaterial3D::setDiffuseTexture( QgsGpuTexture *texture )
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

void QgsTextureMaterial3D::setOpacity( float opacity )
{
  mUniforms.opacity = opacity;
  updateUniforms();
}

void QgsTextureMaterial3D::setMeshTransform( const QMatrix4x4 &transform )
{
  QgsMaterial3DUtils::setMatrix4( mUniforms.meshMatrix, transform );
  updateUniforms();
}

void QgsTextureMaterial3D::setSymbolScale( const QVector3D &scale )
{
  QgsMaterial3DUtils::setVector3( mUniforms.symbolScale, scale );
  updateUniforms();
}

void QgsTextureMaterial3D::setSymbolRotation( const QQuaternion &rotation )
{
  QgsMaterial3DUtils::setQuaternion( mUniforms.symbolRotation, rotation );
  updateUniforms();
}
