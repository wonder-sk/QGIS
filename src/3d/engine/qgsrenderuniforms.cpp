/***************************************************************************
  qgsrenderuniforms.cpp
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

#include "qgsrenderuniforms.h"

#include <cstring>

#include <QSize>

static_assert( sizeof( QgsViewUniforms ) == 352, "QgsViewUniforms must match its std140 layout" );
static_assert( sizeof( QgsDrawUniforms ) == 176, "QgsDrawUniforms must match its std140 layout" );

namespace
{
  void copyMatrix4( float *destination, const QMatrix4x4 &matrix )
  {
    std::memcpy( destination, matrix.constData(), 16 * sizeof( float ) );
  }

  //! Writes a 3x3 matrix the way std140 stores one: three columns of four floats
  void copyMatrix3Padded( float *destination, const QMatrix3x3 &matrix )
  {
    const float *source = matrix.constData();
    for ( int column = 0; column < 3; ++column )
    {
      destination[column * 4 + 0] = source[column * 3 + 0];
      destination[column * 4 + 1] = source[column * 3 + 1];
      destination[column * 4 + 2] = source[column * 3 + 2];
      destination[column * 4 + 3] = 0.0f;
    }
  }
} // namespace

void QgsViewUniforms::setCamera( const QMatrix4x4 &view, const QMatrix4x4 &projection )
{
  copyMatrix4( viewMatrix, view );
  copyMatrix4( projectionMatrix, projection );
  copyMatrix4( viewProjectionMatrix, projection * view );
  copyMatrix4( inverseViewMatrix, view.inverted() );
  copyMatrix4( inverseProjectionMatrix, projection.inverted() );
}

void QgsViewUniforms::setEyePosition( const QVector3D &position )
{
  eyePosition[0] = position.x();
  eyePosition[1] = position.y();
  eyePosition[2] = position.z();
}

void QgsViewUniforms::setViewportSize( const QSize &size )
{
  viewportWidth = static_cast<float>( size.width() );
  viewportHeight = static_cast<float>( size.height() );
  aspectRatio = size.height() > 0 ? static_cast<float>( size.width() ) / static_cast<float>( size.height() ) : 1.0f;
}

void QgsDrawUniforms::setTransform( const QMatrix4x4 &model, const QMatrix4x4 &viewProjection )
{
  copyMatrix4( modelMatrix, model );
  copyMatrix4( modelViewProjectionMatrix, viewProjection * model );
  copyMatrix3Padded( modelNormalMatrix, model.normalMatrix() );
}
