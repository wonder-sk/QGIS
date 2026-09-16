/***************************************************************************
  qgstexturematerial3d.h
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

#ifndef QGSTEXTUREMATERIAL3D_H
#define QGSTEXTUREMATERIAL3D_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsmaterial3d.h"

#include <QMatrix4x4>

#define SIP_NO_FILE

class QgsGpuTexture;

/**
 * \ingroup qgis_3d
 * \brief The unlit textured material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsTexturedBlock in shaders/rhi/qgstexturedmaterial.glsl, which
 * QgsTextureMaterial3D checks once when it is constructed.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsTexturedUniforms
{
    float opacity = 1.0f;                                                                                                      // offset 0
    float unused0 = 0.0f;                                                                                                      // offset 4
    float unused1 = 0.0f;                                                                                                      // offset 8
    float unused2 = 0.0f;                                                                                                      // offset 12
    float meshMatrix[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }; // offset 16, identity by default
    float symbolRotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };                                                                      // offset 80, no rotation
    float symbolScale[4] = { 1.0f, 1.0f, 1.0f, 0.0f };                                                                         // offset 96, unit scale
};

/**
 * \ingroup qgis_3d
 * \brief An unlit textured material for the QRhi engine.
 *
 * The port of QgsTextureMaterial: the texture supplies the colour and nothing else
 * affects it. Its Qt3D counterpart also applied the shadow map, which belongs to the
 * shadow pass in P4 and is not wired up here.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsTextureMaterial3D : public QgsMaterial3D
{
  public:
    //! The sampler binding the diffuse texture is bound at
    static constexpr int DiffuseTextureBinding = 4;

    /**
     * Constructs an unlit textured material.
     *
     * When \a instanced is TRUE the geometry is drawn once per instance, and
     * \a instanceFlags says which of scale and rotation each instance carries; whatever an
     * instance does not carry falls back to setSymbolScale() and setSymbolRotation().
     */
    explicit QgsTextureMaterial3D( bool instanced = false, Qgis::InstancedMaterialFlags instanceFlags = Qgis::InstancedMaterialFlags() );

    //! Returns TRUE if the material draws instanced geometry
    bool isInstanced() const { return mInstanced; }

    /**
     * Sets the texture the material samples its colour from.
     *
     * The texture is not owned by the material and has to outlive it. Passing NULLPTR
     * leaves the material with no texture, which cannot be drawn.
     */
    void setDiffuseTexture( QgsGpuTexture *texture );

    //! Returns the texture the material samples, or NULLPTR if none was set
    QgsGpuTexture *diffuseTexture() const { return mDiffuseTexture; }

    //! Sets the opacity the sampled alpha is multiplied by
    void setOpacity( float opacity );

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
    const QgsTexturedUniforms &uniforms() const { return mUniforms; }

  private:
    //! Returns the baked shader for the given variant, or NULLPTR if it is missing
    static std::shared_ptr<QgsRhiShader> shaderFor( bool instanced, Qgis::InstancedMaterialFlags instanceFlags );

    void updateUniforms();

    bool mInstanced = false;
    QgsGpuTexture *mDiffuseTexture = nullptr;
    QgsTexturedUniforms mUniforms;
};

#endif // QGSTEXTUREMATERIAL3D_H
