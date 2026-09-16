/***************************************************************************
  qgshighlightmaterial3d.cpp
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
 ***************************************************************************/

#include "qgshighlightmaterial3d.h"

#include "qgslogger.h"
#include "qgsmaterial3dutils.h"
#include "qgsrenderuniforms.h"

#include <QQuaternion>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsHighlightOutlineUniforms ) == 20, "QgsHighlightOutlineUniforms must match its std140 layout" );

std::shared_ptr<QgsRhiShader> QgsHighlightMaskMaterial3D::shaderFor( bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
{
  const QString variant = QgsMaterial3DUtils::shaderVariantSuffix( false, instanced, instanceFlags );
  return QgsRhiShader::load( u":/shaders/rhi/shadowcaster%1.vert.qsb"_s.arg( variant ), u":/shaders/rhi/highlightmask.frag.qsb"_s );
}

QgsHighlightMaskMaterial3D::QgsHighlightMaskMaterial3D( bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
  : QgsMaterial3D( shaderFor( instanced, instanceFlags ) )
{
  // the mask says where a feature is on screen, not which parts of it are visible, so it
  // ignores depth entirely and marks back faces too
  setDepthTest( false );
  setDepthWrite( false );
  setCullMode( QRhiGraphicsPipeline::None );

  setUniforms( mUniforms );
}

void QgsHighlightMaskMaterial3D::setMeshTransform( const QMatrix4x4 &transform )
{
  QgsMaterial3DUtils::setMatrix4( mUniforms.meshMatrix, transform );
  setUniforms( mUniforms );
}

void QgsHighlightMaskMaterial3D::setSymbolScale( const QVector3D &scale )
{
  QgsMaterial3DUtils::setVector3( mUniforms.symbolScale, scale );
  setUniforms( mUniforms );
}

void QgsHighlightMaskMaterial3D::setSymbolRotation( const QQuaternion &rotation )
{
  QgsMaterial3DUtils::setQuaternion( mUniforms.symbolRotation, rotation );
  setUniforms( mUniforms );
}

QgsHighlightOutlineMaterial3D::QgsHighlightOutlineMaterial3D()
  : QgsMaterial3D( QgsRhiShader::load( u":/shaders/rhi/screen.vert.qsb"_s, u":/shaders/rhi/highlightoutline.frag.qsb"_s ) )
{
#ifdef QGISDEBUG
  const QVector<QgsUniformMember> members {
    { u"outlineColor"_s, offsetof( QgsHighlightOutlineUniforms, outlineColor ), 16 },
    { u"outlineWidth"_s, offsetof( QgsHighlightOutlineUniforms, outlineWidth ), 4 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsHighlightOutlineUniforms does not match QgsHighlightOutlineBlock: %1"_s.arg( error ) );
#endif

  setDepthTest( false );
  setDepthWrite( false );
  setCullMode( QRhiGraphicsPipeline::None );

  // the shader discards everywhere but the band, and the band can be translucent
  QRhiGraphicsPipeline::TargetBlend blend;
  blend.enable = true;
  blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
  blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
  blend.srcAlpha = QRhiGraphicsPipeline::One;
  blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
  setBlend( blend );

  setUniforms( mUniforms );
}

void QgsHighlightOutlineMaterial3D::setMaskTexture( QRhiTexture *texture )
{
  mMaskTexture.setExternalTexture( texture );

  TextureBinding binding;
  binding.binding = QgsRhiBinding::FirstSampler;
  binding.texture = texture ? &mMaskTexture : nullptr;
  // the mask is either covered or not, so interpolating between the two would only blur
  // the edge the band is measured from
  binding.magFilter = QRhiSampler::Nearest;
  binding.minFilter = QRhiSampler::Nearest;
  setTexture( binding );
}

void QgsHighlightOutlineMaterial3D::setOutlineColor( const QColor &color )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.outlineColor, color );
  setUniforms( mUniforms );
}

void QgsHighlightOutlineMaterial3D::setOutlineWidth( int pixels )
{
  mUniforms.outlineWidth = pixels;
  setUniforms( mUniforms );
}
