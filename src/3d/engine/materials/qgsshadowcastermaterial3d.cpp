/***************************************************************************
  qgsshadowcastermaterial3d.cpp
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

#include "qgsshadowcastermaterial3d.h"

#include "qgslogger.h"
#include "qgsmaterial3dutils.h"
#include "qgsrenderuniforms.h"

#include <QQuaternion>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsShadowCasterUniforms ) == 96, "QgsShadowCasterUniforms must match its std140 layout" );

std::shared_ptr<QgsRhiShader> QgsShadowCasterMaterial3D::shaderFor( bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
{
  const QString variant = QgsMaterial3DUtils::shaderVariantSuffix( false, instanced, instanceFlags );
  return QgsRhiShader::load( u":/shaders/rhi/shadowcaster%1.vert.qsb"_s.arg( variant ), u":/shaders/rhi/shadowcaster.frag.qsb"_s );
}

QgsShadowCasterMaterial3D::QgsShadowCasterMaterial3D( bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
  : QgsMaterial3D( shaderFor( instanced, instanceFlags ) )
  , mInstanced( instanced )
{
#ifdef QGISDEBUG
  const QVector<QgsUniformMember> members {
    { u"meshMatrix"_s, offsetof( QgsShadowCasterUniforms, meshMatrix ), 64 },
    { u"symbolRotation"_s, offsetof( QgsShadowCasterUniforms, symbolRotation ), 16 },
    { u"symbolScale"_s, offsetof( QgsShadowCasterUniforms, symbolScale ), 16 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsShadowCasterUniforms does not match QgsShadowCasterBlock: %1"_s.arg( error ) );
#endif

  // The light looks along the cascade's axis, so the surfaces facing it are the ones whose
  // depth matters. Culling the front faces instead means what lands in the map is the back
  // of each object, which moves self-shadowing acne to the side facing away from the light
  // where nothing can see it.
  setCullMode( QRhiGraphicsPipeline::Front );

  // Qt3D's QPolygonOffset with the same numbers: push casters a little further from the
  // light so that a flat surface does not shadow itself
  setDepthBias( 4, 1.1f );

  setUniforms( mUniforms );
}

void QgsShadowCasterMaterial3D::setMeshTransform( const QMatrix4x4 &transform )
{
  QgsMaterial3DUtils::setMatrix4( mUniforms.meshMatrix, transform );
  setUniforms( mUniforms );
}

void QgsShadowCasterMaterial3D::setSymbolScale( const QVector3D &scale )
{
  QgsMaterial3DUtils::setVector3( mUniforms.symbolScale, scale );
  setUniforms( mUniforms );
}

void QgsShadowCasterMaterial3D::setSymbolRotation( const QQuaternion &rotation )
{
  QgsMaterial3DUtils::setQuaternion( mUniforms.symbolRotation, rotation );
  setUniforms( mUniforms );
}
