/***************************************************************************
  qgsmaterial3dutils.h
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

#ifndef QGSMATERIAL3DUTILS_H
#define QGSMATERIAL3DUTILS_H

#include "qgis.h"
#include "qgis_3d.h"

#define SIP_NO_FILE

class QColor;
class QMatrix4x4;
class QQuaternion;
class QString;
class QVector3D;


/**
 * \ingroup qgis_3d
 * \brief Helpers shared by the QRhi materials for filling their std140 uniform structs.
 *
 * Every material writes plain floats into a struct that has to match a GLSL block exactly,
 * so the handful of conversions that need care - sRGB to linear, a mat3 padded out to
 * three vec4 columns, a quaternion in xyzw order - live here rather than once per material.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
namespace QgsMaterial3DUtils
{
  /**
   * Converts one sRGB channel to linear.
   *
   * Deliberately a copy of srgbFloatToLinear() in qgs3dutils.cpp rather than a call to it:
   * Qgs3DUtils lives behind a header that pulls in most of Qt3D, and the whole point of
   * this engine is not to depend on that. The two must stay in step until the Qt3D side
   * is gone and the conversion can move somewhere neutral.
   */
  _3D_EXPORT float srgbToLinear( float srgb );

  /**
   * Writes the sRGB colour \a color into the vec4 at \a destination, converted to linear
   * and multiplied by \a scaleFactor.
   *
   * Only the colour channels are converted; the alpha channel is copied across as it is,
   * which is what Qgs3DUtils::srgbToLinear() does too. Most materials ignore it and take
   * their transparency from a separate opacity uniform instead.
   */
  _3D_EXPORT void setLinearColor( float *destination, const QColor &color, float scaleFactor = 1.0f );

  //! Writes the column major contents of \a matrix into the mat4 at \a destination
  _3D_EXPORT void setMatrix4( float *destination, const QMatrix4x4 &matrix );

  /**
   * Writes the normal matrix of \a transform into the mat3 at \a destination.
   *
   * std140 stores a mat3 as three columns of four floats, the fourth of each being
   * padding, so \a destination must have room for twelve floats rather than nine.
   */
  _3D_EXPORT void setNormalMatrix( float *destination, const QMatrix4x4 &transform );

  //! Writes \a value into the vec4 at \a destination, with \a w as its fourth component
  _3D_EXPORT void setVector3( float *destination, const QVector3D &value, float w = 0.0f );

  //! Writes \a rotation into the vec4 at \a destination, in xyzw order as GLSL expects it
  _3D_EXPORT void setQuaternion( float *destination, const QQuaternion &rotation );

  /**
   * Returns the suffix identifying a baked shader variant, such as "_instanced_scale".
   *
   * Materials build their shader paths from a base name plus this, so that the naming the
   * CMake variant lists use is decided in one place. \a dataDefined and \a instanced are
   * mutually exclusive; data defined wins if both are set, matching what the Qt3D
   * materials did when their shaders were swapped at runtime.
   */
  _3D_EXPORT QString shaderVariantSuffix( bool dataDefined, bool instanced, Qgis::InstancedMaterialFlags instanceFlags );
} // namespace QgsMaterial3DUtils

#endif // QGSMATERIAL3DUTILS_H
