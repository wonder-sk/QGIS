/***************************************************************************
  qgsphongmaterial3d.cpp
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

#include "qgsphongmaterial3d.h"

#include "qgslogger.h"
#include "qgsmaterial3dutils.h"
#include "qgsrenderuniforms.h"

#include <QQuaternion>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsPhongUniforms ) == 208, "QgsPhongUniforms must match its std140 layout" );

std::shared_ptr<QgsRhiShader> QgsPhongMaterial3D::shaderFor( bool dataDefined, bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
{
  const QString variant = QgsMaterial3DUtils::shaderVariantSuffix( dataDefined, instanced, instanceFlags );

  // only where the colour comes from differs in the fragment stage, so the instanced
  // variants share the plain fragment shader
  const QString fragmentVariant = dataDefined ? variant : QString();

  return QgsRhiShader::load( u":/shaders/rhi/phong%1.vert.qsb"_s.arg( variant ), u":/shaders/rhi/phong%1.frag.qsb"_s.arg( fragmentVariant ) );
}

QgsPhongMaterial3D::QgsPhongMaterial3D( bool dataDefined, bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
  : QgsMaterial3D( shaderFor( dataDefined, instanced, instanceFlags ) )
  , mDataDefined( dataDefined )
  , mInstanced( instanced )
{
#ifdef QGISDEBUG
  // the C++ struct and the GLSL block are written by hand in two different files, so check
  // once that they agree rather than let a bad offset corrupt everything after it
  const QVector<QgsUniformMember> members {
    { u"ambientColor"_s, offsetof( QgsPhongUniforms, ambientColor ), 16 },
    { u"diffuseColor"_s, offsetof( QgsPhongUniforms, diffuseColor ), 16 },
    { u"specularColor"_s, offsetof( QgsPhongUniforms, specularColor ), 16 },
    { u"shininess"_s, offsetof( QgsPhongUniforms, shininess ), 4 },
    { u"opacity"_s, offsetof( QgsPhongUniforms, opacity ), 4 },
    { u"meshMatrix"_s, offsetof( QgsPhongUniforms, meshMatrix ), 64 },
    { u"meshNormalMatrix"_s, offsetof( QgsPhongUniforms, meshNormalMatrix ), 48 },
    { u"symbolRotation"_s, offsetof( QgsPhongUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsPhongUniforms, symbolScale ), 16 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsPhongUniforms does not match QgsPhongBlock: %1"_s.arg( error ) );
#endif

  setAmbient( QColor::fromRgbF( 0.1f, 0.1f, 0.1f ) );
  setDiffuse( QColor::fromRgbF( 0.7f, 0.7f, 0.7f ) );
  setSpecular( QColor::fromRgbF( 1.0f, 1.0f, 1.0f ) );

  updateUniforms();
}

void QgsPhongMaterial3D::updateUniforms()
{
  setUniforms( mUniforms );
}

void QgsPhongMaterial3D::setAmbient( const QColor &ambient, float scaleFactor )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.ambientColor, ambient, scaleFactor );
  updateUniforms();
}

void QgsPhongMaterial3D::setDiffuse( const QColor &diffuse, float scaleFactor )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.diffuseColor, diffuse, scaleFactor );
  updateUniforms();
}

void QgsPhongMaterial3D::setSpecular( const QColor &specular, float scaleFactor )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.specularColor, specular, scaleFactor );
  updateUniforms();
}

void QgsPhongMaterial3D::setShininess( float shininess )
{
  mUniforms.shininess = shininess;
  updateUniforms();
}

void QgsPhongMaterial3D::setOpacity( float opacity )
{
  mUniforms.opacity = opacity;
  updateUniforms();
}

void QgsPhongMaterial3D::setMeshTransform( const QMatrix4x4 &transform )
{
  QgsMaterial3DUtils::setMatrix4( mUniforms.meshMatrix, transform );
  QgsMaterial3DUtils::setNormalMatrix( mUniforms.meshNormalMatrix, transform );
  updateUniforms();
}

void QgsPhongMaterial3D::setSymbolScale( const QVector3D &scale )
{
  QgsMaterial3DUtils::setVector3( mUniforms.symbolScale, scale );
  updateUniforms();
}

void QgsPhongMaterial3D::setSymbolRotation( const QQuaternion &rotation )
{
  QgsMaterial3DUtils::setQuaternion( mUniforms.symbolRotation, rotation );
  updateUniforms();
}
