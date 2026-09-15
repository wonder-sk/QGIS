/***************************************************************************
  qgsphongmaterial3d.h
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

#ifndef QGSPHONGMATERIAL3D_H
#define QGSPHONGMATERIAL3D_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsmaterial3d.h"

#include <QColor>
#include <QMatrix4x4>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief The Phong material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsPhongBlock in shaders/rhi/qgsphongmaterial.glsl, which
 * QgsPhongMaterial3D checks once when it is constructed.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsPhongUniforms
{
    float ambientColor[4] = {};      // offset 0, rgb used
    float diffuseColor[4] = {};      // offset 16, rgb used
    float specularColor[4] = {};     // offset 32, rgb used
    float shininess = 0.0f;          // offset 48
    float opacity = 1.0f;            // offset 52
    float unused0 = 0.0f;            // offset 56
    float unused1 = 0.0f;            // offset 60
    float meshMatrix[16] = {};       // offset 64
    float meshNormalMatrix[12] = {}; // offset 128, a mat3 as three padded columns
    float symbolRotation[4] = {};    // offset 176
    float symbolScale[4] = {};       // offset 192
};

/**
 * \ingroup qgis_3d
 * \brief Phong shading for the QRhi engine, with uniform or per-vertex colours.
 *
 * The direct port of QgsPhongMaterial. What was a QEffect holding five QParameters and a
 * QShaderProgram whose source was swapped at runtime is now one uniform struct and one
 * shader chosen up front, because the variants are known at build time and are baked then.
 *
 * Which variant is used is fixed when the material is constructed: the data-defined and
 * instanced forms read different vertex attributes, so they are genuinely different
 * shaders rather than a flag.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsPhongMaterial3D : public QgsMaterial3D
{
  public:
    /**
     * Constructs a Phong material.
     *
     * When \a dataDefined is TRUE the colours come from per-vertex attributes rather than
     * from setAmbient(), setDiffuse() and setSpecular(). When \a instanced is TRUE the
     * geometry is drawn once per instance, and \a instanceFlags says which of scale and
     * rotation each instance carries; whatever an instance does not carry falls back to
     * setSymbolScale() and setSymbolRotation().
     */
    explicit QgsPhongMaterial3D( bool dataDefined = false, bool instanced = false, Qgis::InstancedMaterialFlags instanceFlags = Qgis::InstancedMaterialFlags() );

    //! Returns TRUE if the material takes its colours from per-vertex attributes
    bool isDataDefined() const { return mDataDefined; }

    //! Returns TRUE if the material draws instanced geometry
    bool isInstanced() const { return mInstanced; }

    //! Sets the ambient colour, which must be an sRGB colour
    void setAmbient( const QColor &ambient, float scaleFactor = 1.0f );

    //! Sets the diffuse colour, which must be an sRGB colour
    void setDiffuse( const QColor &diffuse, float scaleFactor = 1.0f );

    //! Sets the specular colour, which must be an sRGB colour
    void setSpecular( const QColor &specular, float scaleFactor = 1.0f );

    //! Sets how tight the specular highlight is. Zero switches specular off.
    void setShininess( float shininess );

    //! Sets the opacity applied to the shaded colour
    void setOpacity( float opacity );

    /**
     * Sets the transform from mesh space, the raw input vertex coordinates, to object space.
     *
     * Often an identity matrix, but it can be an axis flip or the transform of the glTF
     * node a mesh came from. Only has an effect when the material is instanced.
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
    const QgsPhongUniforms &uniforms() const { return mUniforms; }

  private:
    //! Returns the baked shader for the given variant, or NULLPTR if it is missing
    static std::shared_ptr<QgsRhiShader> shaderFor( bool dataDefined, bool instanced, Qgis::InstancedMaterialFlags instanceFlags );

    void updateUniforms();

    bool mDataDefined = false;
    bool mInstanced = false;
    QgsPhongUniforms mUniforms;
};

#endif // QGSPHONGMATERIAL3D_H
