/***************************************************************************
  qgsscreenquad.cpp
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

#include "qgsscreenquad.h"

#include <QByteArray>
#include <QString>

using namespace Qt::StringLiterals;

QgsScreenQuad::QgsScreenQuad()
{
  // clang-format off
  static const float vertices[] = {
    -1.0f, -1.0f, 0.0f,
     1.0f, -1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f,
     1.0f, -1.0f, 0.0f,
     1.0f,  1.0f, 0.0f,
  };
  // clang-format on

  mBuffer.setData( QByteArray( reinterpret_cast<const char *>( vertices ), sizeof( vertices ) ) );

  QgsVertexAttribute position;
  position.name = u"vertexPosition"_s;
  position.buffer = &mBuffer;
  position.format = QRhiVertexInputAttribute::Float3;
  position.byteStride = 3 * sizeof( float );
  mGeometry.addAttribute( position );
  mGeometry.setVertexCount( 6 );
}
