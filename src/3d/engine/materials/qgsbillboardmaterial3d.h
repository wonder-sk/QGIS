/***************************************************************************
  qgsbillboardmaterial3d.h
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

#ifndef QGSBILLBOARDMATERIAL3D_H
#define QGSBILLBOARDMATERIAL3D_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsmaterial3d.h"

#include <QSizeF>

#define SIP_NO_FILE

class QgsGpuTexture;

/**
 * \ingroup qgis_3d
 * \brief The billboard material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsBillboardBlock in shaders/rhi/qgsbillboardmaterial.glsl,
 * which QgsBillboardMaterial3D checks once when it is constructed.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsBillboardUniforms
{
    float billboardSize[2] = { 1.0f, 1.0f }; // offset 0
    float verticalOffset = 0.0f;             // offset 8
    quint32 featureFlags = 0;                // offset 12
};

/**
 * \ingroup qgis_3d
 * \brief Draws a flat image at each of a set of points, always facing the camera.
 *
 * The port of QgsPoint3DBillboardMaterial. One unit quad is drawn once per point, with
 * the point's position coming from a per-instance attribute; the quad is offset either
 * in view space, so that it grows and shrinks with distance, or after the perspective
 * divide, so that it keeps a fixed size in pixels.
 *
 * Which of those two it is, and whether the quad is shifted along its own vertical axis,
 * are run time flags. What is baked is what the geometry supplies per instance: its own
 * size, its place in a texture atlas, and a pixel offset.
 *
 * Unlike the Qt3D material this takes the viewport size from the pass rather than having
 * to be told it, because the engine's view block carries it already.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsBillboardMaterial3D : public QgsMaterial3D
{
  public:
    //! The sampler binding the sprite image is bound at
    static constexpr int SpriteTextureBinding = 4;

    //! A per-instance attribute the geometry can supply, each of which needs its own shader
    enum class InstanceAttribute : int
    {
      Size = 1 << 0,         //!< An "instanceSize" attribute, overriding setSize()
      TextureData = 1 << 1,  //!< "atlasOffset" and "atlasSize" attributes, placing the sprite in an atlas
      PixelOffsets = 1 << 2, //!< A "pixelOffset" attribute, shifting the quad on screen
    };
    Q_DECLARE_FLAGS( InstanceAttributes, InstanceAttribute )

    /**
     * Constructs a billboard material for geometry supplying \a attributes per instance.
     *
     * This cannot be changed afterwards, because it is what picks the baked shader.
     */
    explicit QgsBillboardMaterial3D( InstanceAttributes attributes = InstanceAttributes() );

    //! Returns which attributes the geometry supplies per instance
    InstanceAttributes instanceAttributes() const { return mInstanceAttributes; }

    /**
     * Sets the size every billboard is drawn at.
     *
     * In pixels normally, and in world units once setPerspectiveScaleEnabled() is on.
     * Ignored when the geometry supplies InstanceAttribute::Size.
     */
    void setSize( QSizeF size );

    /**
     * Sets how far the quad is shifted along its own vertical axis, as a fraction of its
     * height: 0.5 anchors its bottom edge to the point, -0.5 its top edge.
     */
    void setVerticalOffset( float offset );

    //! Sets whether the vertical offset is applied at all
    void setVerticalOffsetEnabled( bool enabled );

    /**
     * Sets whether a billboard is sized in world units and so shrinks with distance,
     * rather than keeping a fixed size in pixels.
     */
    void setPerspectiveScaleEnabled( bool enabled );

    /**
     * Sets the image the billboards are drawn from.
     *
     * The texture is not owned by the material and has to outlive it. Passing NULLPTR
     * leaves the material with no texture, which cannot be drawn.
     */
    void setSpriteTexture( QgsGpuTexture *texture );

    //! Returns the image the billboards are drawn from, or NULLPTR if none was set
    QgsGpuTexture *spriteTexture() const { return mSpriteTexture; }

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsBillboardUniforms &uniforms() const { return mUniforms; }

    //! Returns the name of the baked shader variant \a attributes select
    static QString shaderVariant( InstanceAttributes attributes );

  private:
    //! What is switched on at run time rather than baked
    enum class Feature : int
    {
      VerticalOffset = 1 << 0,
      PerspectiveScale = 1 << 1,
    };

    void updateUniforms();
    void setFeature( Feature feature, bool enabled );

    InstanceAttributes mInstanceAttributes;
    QgsGpuTexture *mSpriteTexture = nullptr;
    QgsBillboardUniforms mUniforms;
};

Q_DECLARE_OPERATORS_FOR_FLAGS( QgsBillboardMaterial3D::InstanceAttributes )

#endif // QGSBILLBOARDMATERIAL3D_H
