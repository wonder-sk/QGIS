/***************************************************************************
  qgsunlitmaterial3d.h
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

#ifndef QGSUNLITMATERIAL3D_H
#define QGSUNLITMATERIAL3D_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsmaterial3d.h"

#include <QColor>
#include <QMatrix4x4>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief The unlit material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsUnlitBlock in shaders/rhi/qgsunlitmaterial.glsl, which
 * QgsUnlitMaterial3D checks once when it is constructed.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsUnlitUniforms
{
    float materialColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };                                                                       // offset 0
    float meshMatrix[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }; // offset 16, identity by default
    float symbolRotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };                                                                      // offset 80, no rotation
    float symbolScale[4] = { 1.0f, 1.0f, 1.0f, 0.0f };                                                                         // offset 96, unit scale
};

/**
 * \ingroup qgis_3d
 * \brief A single colour, unlit material for the QRhi engine.
 *
 * The port of QgsUnlitMaterial. Unlit materials are not affected by scene lighting, so
 * this has no normals and no light block at all: the colour is resolved in the vertex
 * stage and the fragment stage does nothing but write it out.
 *
 * Which variant is used is fixed when the material is constructed, because the data
 * defined and instanced forms read different vertex attributes.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsUnlitMaterial3D : public QgsMaterial3D
{
  public:
    /**
     * Constructs an unlit material.
     *
     * When \a dataDefined is TRUE the colour comes from the per-vertex
     * "dataDefinedBaseColor" attribute rather than from setColor(). When \a instanced is
     * TRUE the geometry is drawn once per instance, and \a instanceFlags says which of
     * scale and rotation each instance carries; whatever an instance does not carry falls
     * back to setSymbolScale() and setSymbolRotation().
     */
    explicit QgsUnlitMaterial3D( bool dataDefined = false, bool instanced = false, Qgis::InstancedMaterialFlags instanceFlags = Qgis::InstancedMaterialFlags() );

    //! Returns TRUE if the material takes its colour from a per-vertex attribute
    bool isDataDefined() const { return mDataDefined; }

    //! Returns TRUE if the material draws instanced geometry
    bool isInstanced() const { return mInstanced; }

    //! Sets the material colour, which must be an sRGB colour. Alpha is kept as it is.
    void setColor( const QColor &color );

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
    const QgsUnlitUniforms &uniforms() const { return mUniforms; }

  private:
    //! Returns the baked shader for the given variant, or NULLPTR if it is missing
    static std::shared_ptr<QgsRhiShader> shaderFor( bool dataDefined, bool instanced, Qgis::InstancedMaterialFlags instanceFlags );

    void updateUniforms();

    bool mDataDefined = false;
    bool mInstanced = false;
    QgsUnlitUniforms mUniforms;
};

#endif // QGSUNLITMATERIAL3D_H
