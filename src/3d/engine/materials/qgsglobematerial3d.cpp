/***************************************************************************
  qgsglobematerial3d.cpp
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

#include "qgsglobematerial3d.h"

#include "qgslogger.h"
#include "qgsrenderuniforms.h"

#include <QString>

using namespace Qt::StringLiterals;

static_assert( sizeof( QgsGlobeUniforms ) == 16, "QgsGlobeUniforms must match its std140 layout" );

QgsGlobeMaterial3D::QgsGlobeMaterial3D()
  : QgsMaterial3D( QgsRhiShader::load( u":/shaders/rhi/globe.vert.qsb"_s, u":/shaders/rhi/globe.frag.qsb"_s ) )
{
#ifdef QGISDEBUG
  // the C++ struct and the GLSL block are written by hand in two different files, so check
  // once that they agree rather than let a bad offset corrupt everything after it
  const QVector<QgsUniformMember> members {
    { u"texCoordScale"_s, offsetof( QgsGlobeUniforms, texCoordScale ), 4 },
  };
  QString error;
  if ( !shader()->checkUniformBlockLayout( QgsRhiBinding::Material, members, &error ) )
    QgsDebugError( u"QgsGlobeUniforms does not match QgsGlobeBlock: %1"_s.arg( error ) );
#endif

  updateUniforms();
}

void QgsGlobeMaterial3D::updateUniforms()
{
  setUniforms( mUniforms );
}

void QgsGlobeMaterial3D::setDiffuseTexture( QgsGpuTexture *texture )
{
  mDiffuseTexture = texture;

  TextureBinding binding;
  binding.binding = DiffuseTextureBinding;
  binding.texture = texture;
  setTexture( binding );
}

void QgsGlobeMaterial3D::setTextureScale( float scale )
{
  mUniforms.texCoordScale = scale;
  updateUniforms();
}
