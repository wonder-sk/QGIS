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

#include <cmath>

#include "qgslogger.h"
#include "qgsrenderuniforms.h"

#include <QMatrix3x3>
#include <QQuaternion>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsPhongUniforms ) == 208, "QgsPhongUniforms must match its std140 layout" );

namespace
{
  /**
   * Converts one sRGB channel to linear.
   *
   * Deliberately a copy of srgbFloatToLinear() in qgs3dutils.cpp rather than a call to it:
   * Qgs3DUtils lives behind a header that pulls in most of Qt3D, and the whole point of
   * this engine is not to depend on that. The two must stay in step until the Qt3D side
   * is gone and the conversion can move somewhere neutral.
   */
  float srgbToLinear( float srgb )
  {
    // from https://www.w3.org/TR/WCAG21/#dfn-relative-luminance
    return srgb <= 0.04045f ? srgb / 12.92f : std::pow( ( srgb + 0.055f ) / 1.055f, 2.4f );
  }

  //! Writes an sRGB colour into a vec4, converted to linear and scaled
  void setLinearColor( float *destination, const QColor &color, float scaleFactor )
  {
    destination[0] = srgbToLinear( static_cast<float>( color.redF() ) ) * scaleFactor;
    destination[1] = srgbToLinear( static_cast<float>( color.greenF() ) ) * scaleFactor;
    destination[2] = srgbToLinear( static_cast<float>( color.blueF() ) ) * scaleFactor;
    destination[3] = 1.0f;
  }
} // namespace

std::shared_ptr<QgsRhiShader> QgsPhongMaterial3D::shaderFor( bool dataDefined, bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
{
  QString vertex = u":/shaders/rhi/phong"_s;
  QString fragment = u":/shaders/rhi/phong"_s;

  if ( dataDefined )
  {
    vertex += "_datadefined"_L1;
    fragment += "_datadefined"_L1;
  }
  else if ( instanced )
  {
    vertex += "_instanced"_L1;
    if ( instanceFlags.testFlag( Qgis::InstancedMaterialFlag::DataDefinedScale ) )
      vertex += "_scale"_L1;
    if ( instanceFlags.testFlag( Qgis::InstancedMaterialFlag::DataDefinedRotation ) )
      vertex += "_rotation"_L1;
  }

  return QgsRhiShader::load( vertex + ".vert.qsb"_L1, fragment + ".frag.qsb"_L1 );
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

  // the identity mesh transform, so that an instanced material works before setMeshTransform()
  const QMatrix4x4 identity;
  setMeshTransform( identity );
  setSymbolScale( QVector3D( 1.0f, 1.0f, 1.0f ) );
  setSymbolRotation( QQuaternion() );

  setAmbient( QColor::fromRgbF( 0.1f, 0.1f, 0.1f ) );
  setDiffuse( QColor::fromRgbF( 0.7f, 0.7f, 0.7f ) );
  setSpecular( QColor::fromRgbF( 1.0f, 1.0f, 1.0f ) );
}

void QgsPhongMaterial3D::updateUniforms()
{
  setUniforms( mUniforms );
}

void QgsPhongMaterial3D::setAmbient( const QColor &ambient, float scaleFactor )
{
  setLinearColor( mUniforms.ambientColor, ambient, scaleFactor );
  updateUniforms();
}

void QgsPhongMaterial3D::setDiffuse( const QColor &diffuse, float scaleFactor )
{
  setLinearColor( mUniforms.diffuseColor, diffuse, scaleFactor );
  updateUniforms();
}

void QgsPhongMaterial3D::setSpecular( const QColor &specular, float scaleFactor )
{
  setLinearColor( mUniforms.specularColor, specular, scaleFactor );
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
  std::memcpy( mUniforms.meshMatrix, transform.constData(), 16 * sizeof( float ) );

  // std140 stores a mat3 as three columns of four floats
  const QMatrix3x3 normalMatrix = transform.normalMatrix();
  const float *source = normalMatrix.constData();
  for ( int column = 0; column < 3; ++column )
  {
    mUniforms.meshNormalMatrix[column * 4 + 0] = source[column * 3 + 0];
    mUniforms.meshNormalMatrix[column * 4 + 1] = source[column * 3 + 1];
    mUniforms.meshNormalMatrix[column * 4 + 2] = source[column * 3 + 2];
    mUniforms.meshNormalMatrix[column * 4 + 3] = 0.0f;
  }

  updateUniforms();
}

void QgsPhongMaterial3D::setSymbolScale( const QVector3D &scale )
{
  mUniforms.symbolScale[0] = scale.x();
  mUniforms.symbolScale[1] = scale.y();
  mUniforms.symbolScale[2] = scale.z();
  mUniforms.symbolScale[3] = 0.0f;
  updateUniforms();
}

void QgsPhongMaterial3D::setSymbolRotation( const QQuaternion &rotation )
{
  mUniforms.symbolRotation[0] = rotation.x();
  mUniforms.symbolRotation[1] = rotation.y();
  mUniforms.symbolRotation[2] = rotation.z();
  mUniforms.symbolRotation[3] = rotation.scalar();
  updateUniforms();
}
