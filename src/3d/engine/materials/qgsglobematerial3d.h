/***************************************************************************
  qgsglobematerial3d.h
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

#ifndef QGSGLOBEMATERIAL3D_H
#define QGSGLOBEMATERIAL3D_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsmaterial3d.h"

#define SIP_NO_FILE

class QgsGpuTexture;

/**
 * \ingroup qgis_3d
 * \brief The globe material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsGlobeBlock in shaders/rhi/qgsglobematerial.glsl, which
 * QgsGlobeMaterial3D checks once when it is constructed.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsGlobeUniforms
{
    float texCoordScale = 1.0f; // offset 0
    float unused0 = 0.0f;       // offset 4
    float unused1 = 0.0f;       // offset 8
    float unused2 = 0.0f;       // offset 12
};

/**
 * \ingroup qgis_3d
 * \brief The material globe terrain tiles are drawn with.
 *
 * The port of QgsGlobeMaterial. The map texture is darkened where the ellipsoid turns
 * away from the camera, and only once the camera is far enough out for that to read as
 * curvature rather than as a shadow. The scene's lights play no part in it.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsGlobeMaterial3D : public QgsMaterial3D
{
  public:
    //! The sampler binding the map texture is bound at
    static constexpr int DiffuseTextureBinding = 4;

    QgsGlobeMaterial3D();

    /**
     * Sets the texture the globe's surface is sampled from.
     *
     * The texture is not owned by the material and has to outlive it. Passing NULLPTR
     * leaves the material with no texture, which cannot be drawn.
     */
    void setDiffuseTexture( QgsGpuTexture *texture );

    //! Returns the texture the globe's surface is sampled from, or NULLPTR if none was set
    QgsGpuTexture *diffuseTexture() const { return mDiffuseTexture; }

    //! Sets the factor the texture coordinates are scaled by
    void setTextureScale( float scale );

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsGlobeUniforms &uniforms() const { return mUniforms; }

  private:
    void updateUniforms();

    QgsGpuTexture *mDiffuseTexture = nullptr;
    QgsGlobeUniforms mUniforms;
};

#endif // QGSGLOBEMATERIAL3D_H
