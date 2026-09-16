/***************************************************************************
  qgsoverlaytexturematerial3d.cpp
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

#include "qgsoverlaytexturematerial3d.h"

#include "qgslogger.h"
#include "qgsrenderuniforms.h"

#include <QPointF>
#include <QString>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsOverlayTextureUniforms ) == 24, "QgsOverlayTextureUniforms must match its std140 layout" );

QgsOverlayTextureMaterial3D::QgsOverlayTextureMaterial3D()
  : QgsMaterial3D( QgsRhiShader::load( u":/shaders/rhi/overlaytexture.vert.qsb"_s, u":/shaders/rhi/overlaytexture.frag.qsb"_s ) )
{
#ifdef QGISDEBUG
  const QVector<QgsUniformMember> members {
    { u"centerTexCoords"_s, offsetof( QgsOverlayTextureUniforms, centerTexCoords ), 8 },
    { u"sizeTexCoords"_s, offsetof( QgsOverlayTextureUniforms, sizeTexCoords ), 8 },
    { u"isDepth"_s, offsetof( QgsOverlayTextureUniforms, isDepth ), 4 },
    { u"flipTextureY"_s, offsetof( QgsOverlayTextureUniforms, flipTextureY ), 4 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsOverlayTextureUniforms does not match QgsOverlayTextureBlock: %1"_s.arg( error ) );
#endif

  setDepthTest( false );
  setDepthWrite( false );
  setCullMode( QRhiGraphicsPipeline::None );

  setUniforms( mUniforms );
}

void QgsOverlayTextureMaterial3D::setTexture( QRhiTexture *texture )
{
  mTexture.setExternalTexture( texture );

  TextureBinding binding;
  binding.binding = QgsRhiBinding::FirstSampler;
  binding.texture = texture ? &mTexture : nullptr;
  QgsMaterial3D::setTexture( binding );
}

void QgsOverlayTextureMaterial3D::setViewport( const QPointF &center, const QSizeF &halfSize )
{
  mUniforms.centerTexCoords[0] = static_cast<float>( center.x() );
  mUniforms.centerTexCoords[1] = static_cast<float>( center.y() );
  mUniforms.sizeTexCoords[0] = static_cast<float>( halfSize.width() );
  mUniforms.sizeTexCoords[1] = static_cast<float>( halfSize.height() );
  setUniforms( mUniforms );
}

void QgsOverlayTextureMaterial3D::setPosition( Qt::Corner corner, const QSizeF &size, const QSizeF &offset )
{
  const double halfWidth = size.width() / 2;
  const double halfHeight = size.height() / 2;

  double centerX = 0.0;
  double centerY = 0.0;
  switch ( corner )
  {
    case Qt::Corner::TopLeftCorner:
      centerX = offset.width() + halfWidth;
      centerY = offset.height() + halfHeight;
      break;
    case Qt::Corner::TopRightCorner:
      centerX = 1.0 - halfWidth - offset.width();
      centerY = offset.height() + halfHeight;
      break;
    case Qt::Corner::BottomLeftCorner:
      centerX = offset.width() + halfWidth;
      centerY = 1.0 - halfHeight - offset.height();
      break;
    case Qt::Corner::BottomRightCorner:
      centerX = 1.0 - halfWidth - offset.width();
      centerY = 1.0 - halfHeight - offset.height();
      break;
  }

  setViewport( QPointF( centerX, centerY ), QSizeF( halfWidth, halfHeight ) );
}

void QgsOverlayTextureMaterial3D::setIsDepth( bool isDepth )
{
  mUniforms.isDepth = isDepth ? 1 : 0;
  setUniforms( mUniforms );
}

void QgsOverlayTextureMaterial3D::setFlipTextureY( bool flip )
{
  mUniforms.flipTextureY = flip ? 1 : 0;
  setUniforms( mUniforms );
}
