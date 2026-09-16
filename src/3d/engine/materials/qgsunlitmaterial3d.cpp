/***************************************************************************
  qgsunlitmaterial3d.cpp
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

#include "qgsunlitmaterial3d.h"

#include "qgslogger.h"
#include "qgsmaterial3dutils.h"
#include "qgsrenderuniforms.h"

#include <QQuaternion>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsUnlitUniforms ) == 112, "QgsUnlitUniforms must match its std140 layout" );

std::shared_ptr<QgsRhiShader> QgsUnlitMaterial3D::shaderFor( bool dataDefined, bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
{
  const QString variant = QgsMaterial3DUtils::shaderVariantSuffix( dataDefined, instanced, instanceFlags );

  // the fragment stage has no variants: the vertex stage has already worked out the colour
  return QgsRhiShader::load( u":/shaders/rhi/unlit%1.vert.qsb"_s.arg( variant ), u":/shaders/rhi/unlit.frag.qsb"_s );
}

QgsUnlitMaterial3D::QgsUnlitMaterial3D( bool dataDefined, bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
  : QgsMaterial3D( shaderFor( dataDefined, instanced, instanceFlags ) )
  , mDataDefined( dataDefined )
  , mInstanced( instanced )
{
#ifdef QGISDEBUG
  // the C++ struct and the GLSL block are written by hand in two different files, so check
  // once that they agree rather than let a bad offset corrupt everything after it
  const QVector<QgsUniformMember> members {
    { u"materialColor"_s, offsetof( QgsUnlitUniforms, materialColor ), 16 },
    { u"meshMatrix"_s, offsetof( QgsUnlitUniforms, meshMatrix ), 64 },
    { u"symbolRotation"_s, offsetof( QgsUnlitUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsUnlitUniforms, symbolScale ), 16 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsUnlitUniforms does not match QgsUnlitBlock: %1"_s.arg( error ) );
#endif

  setColor( QColor( 255, 255, 255 ) );

  updateUniforms();
}

void QgsUnlitMaterial3D::updateUniforms()
{
  setUniforms( mUniforms );
}

void QgsUnlitMaterial3D::setColor( const QColor &color )
{
  // unlike the lit materials this one has no separate opacity uniform, so the colour's
  // own alpha is what a caller sets transparency with
  QgsMaterial3DUtils::setLinearColor( mUniforms.materialColor, color );
  updateUniforms();
}

void QgsUnlitMaterial3D::setMeshTransform( const QMatrix4x4 &transform )
{
  QgsMaterial3DUtils::setMatrix4( mUniforms.meshMatrix, transform );
  updateUniforms();
}

void QgsUnlitMaterial3D::setSymbolScale( const QVector3D &scale )
{
  QgsMaterial3DUtils::setVector3( mUniforms.symbolScale, scale );
  updateUniforms();
}

void QgsUnlitMaterial3D::setSymbolRotation( const QQuaternion &rotation )
{
  QgsMaterial3DUtils::setQuaternion( mUniforms.symbolRotation, rotation );
  updateUniforms();
}
