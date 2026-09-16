/***************************************************************************
  qgsbackgroundmaterial3d.h
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

#ifndef QGSBACKGROUNDMATERIAL3D_H
#define QGSBACKGROUNDMATERIAL3D_H

#include "qgis_3d.h"
#include "qgsmaterial3d.h"

#include <QColor>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief The gradient background material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsBackgroundBlock in shaders/rhi/qgsbackgroundmaterial.glsl.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsBackgroundUniforms
{
    float topColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };    // offset 0
    float bottomColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // offset 16
};

/**
 * \ingroup qgis_3d
 * \brief The vertical gradient drawn behind everything else in the scene.
 *
 * The port of QgsGradientBackgroundEntity's material. It draws QgsScreenQuad pinned to the
 * far plane with depth writes off, so it fills whatever the scene did not cover and never
 * hides anything.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsBackgroundMaterial3D : public QgsMaterial3D
{
  public:
    QgsBackgroundMaterial3D();

    //! Sets the sRGB colour at the top of the view
    void setTopColor( const QColor &color );

    //! Sets the sRGB colour at the bottom of the view
    void setBottomColor( const QColor &color );

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsBackgroundUniforms &uniforms() const { return mUniforms; }

  private:
    QgsBackgroundUniforms mUniforms;
};

#endif // QGSBACKGROUNDMATERIAL3D_H
