/***************************************************************************
  qgsrenderuniforms.h
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

#ifndef QGSRENDERUNIFORMS_H
#define QGSRENDERUNIFORMS_H

#include "qgis_3d.h"

#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QVector3D>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief Uniform block bindings shared by every QGIS shader.
 *
 * The low bindings are reserved so that any shader can be drawn in any pass without
 * knowing which pass it is in. This mirrors the split Qt3D used, which keeps ported
 * shaders recognisable, but the layouts are QGIS's own and are declared in
 * shaders/rhi/view.glsl and shaders/rhi/draw.glsl.
 *
 * \since QGIS 3.46
 */
namespace QgsRhiBinding
{
  //! Per-view data: camera matrices, eye position, viewport. Constant for a whole pass.
  constexpr int View = 0;

  //! Per-draw data: model matrix and its derivatives. Bound with a dynamic offset.
  constexpr int Draw = 1;

  //! Per-material data: whatever the material's own uniform struct holds.
  constexpr int Material = 2;

  //! First binding a material may use for a texture sampler.
  constexpr int FirstSampler = 3;
} // namespace QgsRhiBinding

/**
 * \ingroup qgis_3d
 * \brief The per-view uniform block, laid out for std140.
 *
 * Matrices are stored column major, exactly as QMatrix4x4 keeps them, so they can be
 * memcpy'd straight out of constData().
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsViewUniforms
{
    float viewMatrix[16] = {};              // offset 0
    float projectionMatrix[16] = {};        // offset 64
    float viewProjectionMatrix[16] = {};    // offset 128
    float inverseViewMatrix[16] = {};       // offset 192
    float inverseProjectionMatrix[16] = {}; // offset 256
    float eyePosition[3] = {};              // offset 320, vec3
    float nearPlane = 0.0f;                 // offset 332
    float farPlane = 0.0f;                  // offset 336
    float aspectRatio = 1.0f;               // offset 340
    float viewportWidth = 0.0f;             // offset 344
    float viewportHeight = 0.0f;            // offset 348

    /**
   * Fills the block from a view and a projection matrix.
   *
   * \a projectionMatrix must already include the backend's clip space correction, see
   * QRhi::clipSpaceCorrMatrix().
   */
    void setCamera( const QMatrix4x4 &view, const QMatrix4x4 &projection );

    //! Sets the eye position in world coordinates
    void setEyePosition( const QVector3D &position );

    //! Sets the viewport size in pixels, and the aspect ratio derived from it
    void setViewportSize( const QSize &size );
};

/**
 * \ingroup qgis_3d
 * \brief The per-draw uniform block, laid out for std140.
 *
 * One of these is written per render command into the frame's uniform ring, and bound
 * with a dynamic offset.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsDrawUniforms
{
    float modelMatrix[16] = {};               // offset 0
    float modelViewProjectionMatrix[16] = {}; // offset 64

    /**
   * The normal matrix, as std140 lays out a mat3: three columns of four floats each,
   * the fourth of every column being padding.
   */
    float modelNormalMatrix[12] = {}; // offset 128, size 48

    /**
   * Fills the block from a world transform and the view projection matrix it is drawn with.
   *
   * The normal matrix is derived from \a model, so non-uniform scaling is handled.
   */
    void setTransform( const QMatrix4x4 &model, const QMatrix4x4 &viewProjection );
};

#endif // QGSRENDERUNIFORMS_H
