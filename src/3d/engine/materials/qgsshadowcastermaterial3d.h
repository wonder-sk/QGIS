/***************************************************************************
  qgsshadowcastermaterial3d.h
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

#ifndef QGSSHADOWCASTERMATERIAL3D_H
#define QGSSHADOWCASTERMATERIAL3D_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsmaterial3d.h"

#include <QMatrix4x4>

#define SIP_NO_FILE

class QQuaternion;
class QVector3D;

/**
 * \ingroup qgis_3d
 * \brief The shadow caster material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsShadowCasterBlock in shaders/rhi/qgsshadowcastermaterial.glsl.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsShadowCasterUniforms
{
    float meshMatrix[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }; // offset 0, identity
    float symbolRotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };                                                                      // offset 64
    float symbolScale[4] = { 1.0f, 1.0f, 1.0f, 0.0f };                                                                         // offset 80
};

/**
 * \ingroup qgis_3d
 * \brief What geometry is drawn with while the shadow map is being filled.
 *
 * Qt3D re-drew every caster with its own full material and threw the colour away, which
 * meant lighting a fragment that was about to be discarded. Here the shadow pass gets a
 * material of its own that reads nothing but the position, so a caster costs a vertex
 * transform and a depth write.
 *
 * Using a different material than the forward pass does mean the two have to agree about
 * where the geometry ends up: an instanced symbol needs the same mesh transform and the
 * same fallback scale and rotation here as in its own material, or its shadow will not
 * line up with it.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsShadowCasterMaterial3D : public QgsMaterial3D
{
  public:
    /**
     * Constructs a shadow caster material.
     *
     * When \a instanced is TRUE the geometry is drawn once per instance, and
     * \a instanceFlags says which of scale and rotation each instance carries, exactly as
     * for the forward materials.
     */
    explicit QgsShadowCasterMaterial3D( bool instanced = false, Qgis::InstancedMaterialFlags instanceFlags = Qgis::InstancedMaterialFlags() );

    //! Returns TRUE if the material draws instanced geometry
    bool isInstanced() const { return mInstanced; }

    //! Sets the transform from mesh space to object space. Instanced materials only.
    void setMeshTransform( const QMatrix4x4 &transform );

    //! Sets the scale used for instances that do not carry their own
    void setSymbolScale( const QVector3D &scale );

    //! Sets the rotation used for instances that do not carry their own
    void setSymbolRotation( const QQuaternion &rotation );

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsShadowCasterUniforms &uniforms() const { return mUniforms; }

  private:
    static std::shared_ptr<QgsRhiShader> shaderFor( bool instanced, Qgis::InstancedMaterialFlags instanceFlags );

    bool mInstanced = false;
    QgsShadowCasterUniforms mUniforms;
};

#endif // QGSSHADOWCASTERMATERIAL3D_H
