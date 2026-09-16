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

#include <QColor>
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

  //! The pass's light sources. Constant for a whole pass, like the view block.
  constexpr int Lights = 2;

  //! Per-material data: whatever the material's own uniform struct holds.
  constexpr int Material = 3;

  //! First binding a material may use for a texture sampler.
  constexpr int FirstSampler = 4;
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
   * +1 when the framebuffer's rows run the same way as world space, -1 when they run the
   * other way, which is what a fragment shader has to multiply any dFdy() by before using
   * it to build a direction. QRhi's clip space correction leaves this difference behind:
   * OpenGL's framebuffer is y up and every other backend's is y down, so a derivative
   * taken across y comes out with the opposite sign there. See QgsRenderPass::execute().
   */
    float framebufferYSign = 1.0f; // offset 352

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

    //! Sets framebufferYSign from whether the backend's framebuffer rows run bottom up
    void setFramebufferYUp( bool yUp ) { framebufferYSign = yUp ? 1.0f : -1.0f; }
};

/**
 * \ingroup qgis_3d
 * \brief The per-pass light block, laid out for std140.
 *
 * Qt3D built this array itself by collecting QAbstractLight components from the entity
 * tree. Without Qt3D the scene fills it in explicitly, which also let the packing be
 * chosen for std140 rather than worked around: each light is four vec4s, so the array
 * stride is a clean 64 bytes and there is no padding to get wrong.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsLightUniforms
{
    //! Largest number of lights a scene can have, matching MAX_LIGHTS in qgslights.glsl
    static constexpr int MAX_LIGHTS = 8;

    //! How a light's direction and position are interpreted
    enum class Type : int
    {
      Point = 0,       //!< Shines in all directions from a position, with distance attenuation
      Directional = 1, //!< Shines in one direction from infinitely far away, no attenuation
      Spot = 2,        //!< Shines in one direction from a position, within a cone
    };

    //! One light source, packed so that every member lands on a vec4 boundary
    struct Light
    {
        //! World position in xyz, Type in w
        float positionAndType[4] = {};
        //! Colour in rgb, intensity in a
        float colorAndIntensity[4] = {};
        //! Direction in xyz, spot cut-off half angle in degrees in w
        float directionAndCutOff[4] = {};
        //! Constant, linear and quadratic distance attenuation in xyz
        float attenuation[4] = {};
    };

    int lightCount = 0;       // offset 0
    int padding[3] = {};      // the array below has to start on a vec4 boundary
    Light lights[MAX_LIGHTS]; // offset 16, stride 64

    //! Removes every light
    void clear() { lightCount = 0; }

    /**
   * Adds a light shining in all directions from \a position, attenuated by distance.
   *
   * Returns FALSE, and adds nothing, if the block is already full.
   */
    bool addPointLight( const QVector3D &position, const QColor &color, float intensity, float constantAttenuation, float linearAttenuation, float quadraticAttenuation );

    /**
   * Adds a light shining along \a direction from infinitely far away.
   *
   * This is what a sun is: the caller works out the direction, the block does not care
   * where it came from. Returns FALSE, and adds nothing, if the block is already full.
   */
    bool addDirectionalLight( const QVector3D &direction, const QColor &color, float intensity );

    /**
   * Adds a light shining along \a direction from \a position, within a cone of
   * \a cutOffAngle degrees.
   *
   * Returns FALSE, and adds nothing, if the block is already full.
   */
    bool addSpotLight(
      const QVector3D &position, const QVector3D &direction, float cutOffAngle, const QColor &color, float intensity, float constantAttenuation, float linearAttenuation, float quadraticAttenuation
    );
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
