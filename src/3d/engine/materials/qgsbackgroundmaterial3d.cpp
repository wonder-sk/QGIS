/***************************************************************************
  qgsbackgroundmaterial3d.cpp
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

#include "qgsbackgroundmaterial3d.h"

#include "qgslogger.h"
#include "qgsmaterial3dutils.h"
#include "qgsrenderuniforms.h"

#include <QString>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsBackgroundUniforms ) == 32, "QgsBackgroundUniforms must match its std140 layout" );

QgsBackgroundMaterial3D::QgsBackgroundMaterial3D()
  : QgsMaterial3D( QgsRhiShader::load( u":/shaders/rhi/background.vert.qsb"_s, u":/shaders/rhi/background.frag.qsb"_s ) )
{
#ifdef QGISDEBUG
  const QVector<QgsUniformMember> members {
    { u"topColor"_s, offsetof( QgsBackgroundUniforms, topColor ), 16 },
    { u"bottomColor"_s, offsetof( QgsBackgroundUniforms, bottomColor ), 16 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsBackgroundUniforms does not match QgsBackgroundBlock: %1"_s.arg( error ) );
#endif

  // sits at the far plane, so it passes only where nothing has been drawn, and it must not
  // write depth or it would occlude the transparent objects drawn after it
  setDepthOp( QRhiGraphicsPipeline::LessOrEqual );
  setDepthWrite( false );
  setCullMode( QRhiGraphicsPipeline::None );

  setUniforms( mUniforms );
}

void QgsBackgroundMaterial3D::setTopColor( const QColor &color )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.topColor, color );
  setUniforms( mUniforms );
}

void QgsBackgroundMaterial3D::setBottomColor( const QColor &color )
{
  QgsMaterial3DUtils::setLinearColor( mUniforms.bottomColor, color );
  setUniforms( mUniforms );
}
