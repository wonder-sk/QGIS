/***************************************************************************
  qgsambientocclusionmaterial3d.h
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

#ifndef QGSAMBIENTOCCLUSIONMATERIAL3D_H
#define QGSAMBIENTOCCLUSIONMATERIAL3D_H

#include "qgis_3d.h"
#include "qgsgputexture.h"
#include "qgsmaterial3d.h"

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief The ambient occlusion material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsAmbientOcclusionBlock in
 * shaders/rhi/qgsambientocclusionmaterial.glsl.
 *
 * The sample kernel and the noise pattern are vec4 arrays although only three components
 * are used: std140 gives a vec3 array a sixteen byte stride anyway, so spelling the
 * padding out is what lets this be a plain C++ array.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsAmbientOcclusionUniforms
{
    //! How many sample directions the occlusion of a pixel is estimated from
    static constexpr int KERNEL_SIZE = 64;

    //! Side of the square of random rotations tiled over the screen, squared
    static constexpr int NOISE_SIZE = 16;

    float kernel[KERNEL_SIZE][4] = {}; // offset 0
    float noise[NOISE_SIZE][4] = {};   // offset 1024

    float intensity = 0.5f;  // offset 1280
    float radius = 25.0f;    // offset 1284
    float threshold = 0.5f;  // offset 1288
    float tanHalfFov = 0.0f; // offset 1292
};

/**
 * \ingroup qgis_3d
 * \brief Estimates how much of the sky each pixel can see, from the depth buffer alone.
 *
 * The port of QgsAmbientOcclusionRenderEntity. Creases and the insides of corners see less
 * of their surroundings than a flat wall does, and darkening them is most of what makes a
 * building look solid rather than pasted on.
 *
 * The sample kernel and the noise pattern are generated once here rather than uploaded as
 * a texture, which is what the Qt3D version did through a QParameter list.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsAmbientOcclusionMaterial3D : public QgsMaterial3D
{
  public:
    QgsAmbientOcclusionMaterial3D();

    //! Sets the depth texture the occlusion is estimated from
    void setDepthTexture( QRhiTexture *texture );

    //! Sets how strongly the occlusion darkens a pixel
    void setIntensity( float intensity );

    //! Sets the radius of the neighbourhood a pixel is compared against, in world units
    void setRadius( float radius );

    //! Sets how much occlusion there has to be before anything is darkened at all
    void setThreshold( float threshold );

    //! Sets the tangent of half the camera's vertical field of view, in radians
    void setTanHalfFov( float tanHalfFov );

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsAmbientOcclusionUniforms &uniforms() const { return mUniforms; }

  private:
    QgsAmbientOcclusionUniforms mUniforms;
    QgsGpuTexture mDepthTexture;
};

/**
 * \ingroup qgis_3d
 * \brief Smooths out the noise pattern the ambient occlusion material samples with.
 *
 * The port of QgsAmbientOcclusionBlurEntity. The occlusion pass rotates its kernel by a
 * 4x4 pattern of random angles, which trades banding for noise; a 4x4 box blur is exactly
 * wide enough to average that pattern back out.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsAmbientOcclusionBlurMaterial3D : public QgsMaterial3D
{
  public:
    QgsAmbientOcclusionBlurMaterial3D();

    //! Sets the occlusion texture to blur
    void setOcclusionTexture( QRhiTexture *texture );

  private:
    QgsGpuTexture mOcclusionTexture;
};

#endif // QGSAMBIENTOCCLUSIONMATERIAL3D_H
