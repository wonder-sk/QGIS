/***************************************************************************
  qgsgoochmaterial3d.h
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

#ifndef QGSGOOCHMATERIAL3D_H
#define QGSGOOCHMATERIAL3D_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsmaterial3d.h"

#include <QColor>
#include <QMatrix4x4>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief The Gooch material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsGoochBlock in shaders/rhi/qgsgoochmaterial.glsl, which
 * QgsGoochMaterial3D checks once when it is constructed.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsGoochUniforms
{
    float diffuseColor[4] = {};                                                                                                // offset 0, rgb used
    float specularColor[4] = {};                                                                                               // offset 16, rgb used
    float warmColor[4] = {};                                                                                                   // offset 32, rgb used
    float coolColor[4] = {};                                                                                                   // offset 48, rgb used
    float shininess = 100.0f;                                                                                                  // offset 64
    float alpha = 0.25f;                                                                                                       // offset 68
    float beta = 0.5f;                                                                                                         // offset 72
    float unused0 = 0.0f;                                                                                                      // offset 76
    float meshMatrix[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }; // offset 80, identity by default
    float meshNormalMatrix[12] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };                   // offset 144, a mat3 as three padded columns
    float symbolRotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };                                                                      // offset 192, no rotation
    float symbolScale[4] = { 1.0f, 1.0f, 1.0f, 0.0f };                                                                         // offset 208, unit scale
};

/**
 * \ingroup qgis_3d
 * \brief Gooch shading for the QRhi engine, with uniform or per-vertex colours.
 *
 * The port of QgsGoochMaterial. Gooch shading is a technical illustration model: a
 * surface is toned between a cool and a warm colour by how it faces each light, which
 * keeps shape readable where a Phong surface would go flat black. Light colour,
 * intensity and attenuation are deliberately ignored, as they were under Qt3D.
 *
 * Which variant is used is fixed when the material is constructed, because the data
 * defined and instanced forms read different vertex attributes.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsGoochMaterial3D : public QgsMaterial3D
{
  public:
    /**
     * Constructs a Gooch material.
     *
     * When \a dataDefined is TRUE the colours come from per-vertex attributes rather than
     * from setDiffuse(), setSpecular(), setWarm() and setCool(). When \a instanced is TRUE
     * the geometry is drawn once per instance, and \a instanceFlags says which of scale
     * and rotation each instance carries; whatever an instance does not carry falls back
     * to setSymbolScale() and setSymbolRotation().
     */
    explicit QgsGoochMaterial3D( bool dataDefined = false, bool instanced = false, Qgis::InstancedMaterialFlags instanceFlags = Qgis::InstancedMaterialFlags() );

    //! Returns TRUE if the material takes its colours from per-vertex attributes
    bool isDataDefined() const { return mDataDefined; }

    //! Returns TRUE if the material draws instanced geometry
    bool isInstanced() const { return mInstanced; }

    //! Sets the diffuse colour, which must be an sRGB colour
    void setDiffuse( const QColor &diffuse );

    //! Sets the specular colour, which must be an sRGB colour
    void setSpecular( const QColor &specular );

    //! Sets the warm tone surfaces facing a light are shaded towards, as an sRGB colour
    void setWarm( const QColor &warm );

    //! Sets the cool tone surfaces facing away from a light are shaded towards, as an sRGB colour
    void setCool( const QColor &cool );

    //! Sets how tight the specular highlight is
    void setShininess( float shininess );

    //! Sets what fraction of the diffuse colour is mixed into the cool tone
    void setAlpha( float alpha );

    //! Sets what fraction of the diffuse colour is mixed into the warm tone
    void setBeta( float beta );

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
    const QgsGoochUniforms &uniforms() const { return mUniforms; }

  private:
    //! Returns the baked shader for the given variant, or NULLPTR if it is missing
    static std::shared_ptr<QgsRhiShader> shaderFor( bool dataDefined, bool instanced, Qgis::InstancedMaterialFlags instanceFlags );

    void updateUniforms();

    bool mDataDefined = false;
    bool mInstanced = false;
    QgsGoochUniforms mUniforms;
};

#endif // QGSGOOCHMATERIAL3D_H
