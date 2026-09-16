/***************************************************************************
  qgsmaterial3dutils.cpp
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

#include "qgsmaterial3dutils.h"

#include <cmath>
#include <cstring>

#include <QColor>
#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QString>
#include <QVector3D>

using namespace Qt::StringLiterals;

float QgsMaterial3DUtils::srgbToLinear( float srgb )
{
  // from https://www.w3.org/TR/WCAG21/#dfn-relative-luminance
  return srgb <= 0.04045f ? srgb / 12.92f : std::pow( ( srgb + 0.055f ) / 1.055f, 2.4f );
}

void QgsMaterial3DUtils::setLinearColor( float *destination, const QColor &color, float scaleFactor )
{
  destination[0] = srgbToLinear( static_cast<float>( color.redF() ) ) * scaleFactor;
  destination[1] = srgbToLinear( static_cast<float>( color.greenF() ) ) * scaleFactor;
  destination[2] = srgbToLinear( static_cast<float>( color.blueF() ) ) * scaleFactor;
  destination[3] = static_cast<float>( color.alphaF() );
}

void QgsMaterial3DUtils::setMatrix4( float *destination, const QMatrix4x4 &matrix )
{
  std::memcpy( destination, matrix.constData(), 16 * sizeof( float ) );
}

void QgsMaterial3DUtils::setNormalMatrix( float *destination, const QMatrix4x4 &transform )
{
  const QMatrix3x3 normalMatrix = transform.normalMatrix();
  const float *source = normalMatrix.constData();
  for ( int column = 0; column < 3; ++column )
  {
    destination[column * 4 + 0] = source[column * 3 + 0];
    destination[column * 4 + 1] = source[column * 3 + 1];
    destination[column * 4 + 2] = source[column * 3 + 2];
    destination[column * 4 + 3] = 0.0f;
  }
}

void QgsMaterial3DUtils::setVector3( float *destination, const QVector3D &value, float w )
{
  destination[0] = value.x();
  destination[1] = value.y();
  destination[2] = value.z();
  destination[3] = w;
}

void QgsMaterial3DUtils::setQuaternion( float *destination, const QQuaternion &rotation )
{
  destination[0] = rotation.x();
  destination[1] = rotation.y();
  destination[2] = rotation.z();
  destination[3] = rotation.scalar();
}

QString QgsMaterial3DUtils::shaderVariantSuffix( bool dataDefined, bool instanced, Qgis::InstancedMaterialFlags instanceFlags )
{
  if ( dataDefined )
    return u"_datadefined"_s;

  if ( !instanced )
    return QString();

  QString suffix = u"_instanced"_s;
  if ( instanceFlags.testFlag( Qgis::InstancedMaterialFlag::DataDefinedScale ) )
    suffix += "_scale"_L1;
  if ( instanceFlags.testFlag( Qgis::InstancedMaterialFlag::DataDefinedRotation ) )
    suffix += "_rotation"_L1;
  return suffix;
}
