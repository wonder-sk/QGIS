/***************************************************************************
  qgsgoochmaterial3d.cpp
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

#include "qgsgoochmaterial3d.h"

#include "qgslogger.h"
#include "qgsmaterial3dutils.h"
#include "qgsrenderuniforms.h"

#include <QQuaternion>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsGoochUniforms ) == 224, "QgsGoochUniforms must match its std140 layout" );

std::shared_ptr<QgsRhiShader> QgsGoochMaterial3D::shaderFor( bool dataDefined, bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
{
  const QString variant = QgsMaterial3DUtils::shaderVariantSuffix( dataDefined, instanced, instanceFlags );

  // only where the colours come from differs in the fragment stage, so the instanced
  // variants share the plain fragment shader
  const QString fragmentVariant = dataDefined ? variant : QString();

  return QgsRhiShader::load( u":/shaders/rhi/gooch%1.vert.qsb"_s.arg( variant ), u":/shaders/rhi/gooch%1.frag.qsb"_s.arg( fragmentVariant ) );
}

QgsGoochMaterial3D::QgsGoochMaterial3D( bool dataDefined, bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
  : QgsMaterial3D( shaderFor( dataDefined, instanced, instanceFlags ) )
  , mDataDefined( dataDefined )
  , mInstanced( instanced )
{
#ifdef QGISDEBUG
  // the C++ struct and the GLSL block are written by hand in two different files, so check
  // once that they agree rather than let a bad offset corrupt everything after it
  const QVector<QgsUniformMember> members {
    { u"diffuseColor"_s, offsetof( QgsGoochUniforms, diffuseColor ), 16 },
    { u"specularColor"_s, offsetof( QgsGoochUniforms, specularColor ), 16 },
    { u"warmColor"_s, offsetof( QgsGoochUniforms, warmColor ), 16 },
    { u"coolColor"_s, offsetof( QgsGoochUniforms, coolColor ), 16 },
    { u"shininess"_s, offsetof( QgsGoochUniforms, shininess ), 4 },
    { u"alpha"_s, offsetof( QgsGoochUniforms, alpha ), 4 },
    { u"beta"_s, offsetof( QgsGoochUniforms, beta ), 4 },
    { u"meshMatrix"_s, offsetof( QgsGoochUniforms, meshMatrix ), 64 },
    { u"meshNormalMatrix"_s, offsetof( QgsGoochUniforms, meshNormalMatrix ), 48 },
    { u"symbolRotation"_s, offsetof( QgsGoochUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsGoochUniforms, symbolScale ), 16 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsGoochUniforms does not match QgsGoochBlock: %1"_s.arg( error ) );
#endif

  // the same defaults QgsGoochMaterial had
  setDiffuse( QColor::fromRgbF( 0.7f, 0.7f, 0.7f ) );
  setSpecular( QColor::fromRgbF( 1.0f, 1.0f, 1.0f ) );
  setWarm( QColor::fromRgbF( 0.42f, 0.0f, 0.42f ) );
  setCool( QColor::fromRgbF( 1.0f, 0.51f, 0.0f ) );

  updateUniforms();
}

void QgsGoochMaterial3D::updateUniforms()
{
  setUniforms( mUniforms );
}

void QgsGoochMaterial3D::setDiffuse( const QColor &diffuse )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.diffuseColor, diffuse );
  updateUniforms();
}

void QgsGoochMaterial3D::setSpecular( const QColor &specular )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.specularColor, specular );
  updateUniforms();
}

void QgsGoochMaterial3D::setWarm( const QColor &warm )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.warmColor, warm );
  updateUniforms();
}

void QgsGoochMaterial3D::setCool( const QColor &cool )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.coolColor, cool );
  updateUniforms();
}

void QgsGoochMaterial3D::setShininess( float shininess )
{
  mUniforms.shininess = shininess;
  updateUniforms();
}

void QgsGoochMaterial3D::setAlpha( float alpha )
{
  mUniforms.alpha = alpha;
  updateUniforms();
}

void QgsGoochMaterial3D::setBeta( float beta )
{
  mUniforms.beta = beta;
  updateUniforms();
}

void QgsGoochMaterial3D::setMeshTransform( const QMatrix4x4 &transform )
{
  QgsMaterial3DUtils::setMatrix4( mUniforms.meshMatrix, transform );
  QgsMaterial3DUtils::setNormalMatrix( mUniforms.meshNormalMatrix, transform );
  updateUniforms();
}

void QgsGoochMaterial3D::setSymbolScale( const QVector3D &scale )
{
  QgsMaterial3DUtils::setVector3( mUniforms.symbolScale, scale );
  updateUniforms();
}

void QgsGoochMaterial3D::setSymbolRotation( const QQuaternion &rotation )
{
  QgsMaterial3DUtils::setQuaternion( mUniforms.symbolRotation, rotation );
  updateUniforms();
}
