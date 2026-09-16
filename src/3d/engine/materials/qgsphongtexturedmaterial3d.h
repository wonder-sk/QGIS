/***************************************************************************
  qgsphongtexturedmaterial3d.h
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

#ifndef QGSPHONGTEXTUREDMATERIAL3D_H
#define QGSPHONGTEXTUREDMATERIAL3D_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsmaterial3d.h"

#include <QColor>
#include <QMatrix4x4>

#define SIP_NO_FILE

class QgsGpuTexture;

/**
 * \ingroup qgis_3d
 * \brief The textured Phong material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsPhongTexturedBlock in
 * shaders/rhi/qgsphongtexturedmaterial.glsl, which QgsPhongTexturedMaterial3D checks once
 * when it is constructed.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsPhongTexturedUniforms
{
    float ambientColor[4] = {};                                                                                                // offset 0, rgb used
    float specularColor[4] = {};                                                                                               // offset 16, rgb used
    float shininess = 150.0f;                                                                                                  // offset 32
    float opacity = 1.0f;                                                                                                      // offset 36
    float texCoordScale = 1.0f;                                                                                                // offset 40
    float texCoordRotation = 0.0f;                                                                                             // offset 44
    float texCoordOffset[2] = {};                                                                                              // offset 48
    float decodeSrgb = 0.0f;                                                                                                   // offset 56
    float unused0 = 0.0f;                                                                                                      // offset 60
    float meshMatrix[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }; // offset 64, identity by default
    float meshNormalMatrix[12] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };                   // offset 128, a mat3 as three padded columns
    float symbolRotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };                                                                      // offset 176, no rotation
    float symbolScale[4] = { 1.0f, 1.0f, 1.0f, 0.0f };                                                                         // offset 192, unit scale
};

/**
 * \ingroup qgis_3d
 * \brief Phong shading with a diffuse texture, for the QRhi engine.
 *
 * The port of QgsPhongTexturedMaterial. The diffuse colour comes from a texture instead of
 * a uniform; everything else is the same shading model QgsPhongMaterial3D uses, and both
 * share shaders/rhi/qgsphong.glsl.
 *
 * The texture coordinates are scaled and rotated about setDiffuseTextureOffset(), either
 * by the material's own values or, in the data defined variant, by a per-vertex
 * "ddTextureTransform" attribute holding offset, scale and rotation. Instanced geometry
 * uses its own texture coordinates untransformed, as it did under Qt3D.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsPhongTexturedMaterial3D : public QgsMaterial3D
{
  public:
    //! The sampler binding the diffuse texture is bound at
    static constexpr int DiffuseTextureBinding = 4;

    /**
     * Constructs a textured Phong material.
     *
     * When \a dataDefinedTextureTransform is TRUE the texture transform comes from a
     * per-vertex attribute rather than from setDiffuseTextureScale() and friends. When
     * \a instanced is TRUE the geometry is drawn once per instance, and \a instanceFlags
     * says which of scale and rotation each instance carries; whatever an instance does
     * not carry falls back to setSymbolScale() and setSymbolRotation().
     */
    explicit QgsPhongTexturedMaterial3D( bool dataDefinedTextureTransform = false, bool instanced = false, Qgis::InstancedMaterialFlags instanceFlags = Qgis::InstancedMaterialFlags() );

    //! Returns TRUE if the texture transform comes from a per-vertex attribute
    bool isDataDefinedTextureTransform() const { return mDataDefinedTextureTransform; }

    //! Returns TRUE if the material draws instanced geometry
    bool isInstanced() const { return mInstanced; }

    //! Sets the ambient colour, which must be an sRGB colour
    void setAmbient( const QColor &ambient );

    //! Sets the specular colour, which must be an sRGB colour
    void setSpecular( const QColor &specular );

    //! Sets how tight the specular highlight is. Zero switches specular off.
    void setShininess( float shininess );

    //! Sets the opacity applied to the shaded colour
    void setOpacity( float opacity );

    /**
     * Sets the texture the diffuse colour is sampled from.
     *
     * The texture is not owned by the material and has to outlive it. Passing NULLPTR
     * leaves the material with no texture, which cannot be drawn.
     */
    void setDiffuseTexture( QgsGpuTexture *texture );

    //! Returns the texture the diffuse colour is sampled from, or NULLPTR if none was set
    QgsGpuTexture *diffuseTexture() const { return mDiffuseTexture; }

    //! Sets the factor the texture coordinates are scaled by about the texture offset
    void setDiffuseTextureScale( float scale );

    //! Sets how far the texture coordinates are rotated about the texture offset, in degrees
    void setDiffuseTextureRotation( float rotation );

    //! Sets the point in texture space the scale and the rotation are applied about
    void setDiffuseTextureOffset( float offsetX, float offsetY );

    /**
     * Sets the transform from mesh space, the raw input vertex coordinates, to object space.
     *
     * Only has an effect when the material is instanced.
     */
    void setMeshTransform( const QMatrix4x4 &transform );

    /**
     * Sets the scale used for instances that do not carry their own.
     *
     * Only has an effect when the material is instanced and instanceFlags did not include
     * Qgis::InstancedMaterialFlag::DataDefinedScale.
     */
    void setSymbolScale( const QVector3D &scale );

    /**
     * Sets the rotation quaternion used for instances that do not carry their own.
     *
     * Only has an effect when the material is instanced and instanceFlags did not include
     * Qgis::InstancedMaterialFlag::DataDefinedRotation.
     */
    void setSymbolRotation( const QQuaternion &rotation );

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsPhongTexturedUniforms &uniforms() const { return mUniforms; }

  private:
    //! Returns the baked shader for the given variant, or NULLPTR if it is missing
    static std::shared_ptr<QgsRhiShader> shaderFor( bool dataDefinedTextureTransform, bool instanced, Qgis::InstancedMaterialFlags instanceFlags );

    void updateUniforms();

    bool mDataDefinedTextureTransform = false;
    bool mInstanced = false;
    QgsGpuTexture *mDiffuseTexture = nullptr;
    QgsPhongTexturedUniforms mUniforms;
};

#endif // QGSPHONGTEXTUREDMATERIAL3D_H
