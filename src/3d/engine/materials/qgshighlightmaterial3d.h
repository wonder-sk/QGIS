/***************************************************************************
  qgshighlightmaterial3d.h
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

#ifndef QGSHIGHLIGHTMATERIAL3D_H
#define QGSHIGHLIGHTMATERIAL3D_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsgputexture.h"
#include "qgsmaterial3d.h"
#include "qgsshadowcastermaterial3d.h"

#include <QColor>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief Marks the pixels a highlighted feature covers, so that an outline can be drawn.
 *
 * Part of the port of QgsHighlightsRenderView. It shares the shadow caster's vertex
 * shaders, and so its uniform block, because both want exactly one thing from a vertex:
 * where it ends up. What differs is the fragment stage, which writes coverage rather than
 * depth.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsHighlightMaskMaterial3D : public QgsMaterial3D
{
  public:
    /**
     * Constructs a mask material.
     *
     * \a instanced and \a instanceFlags describe the geometry exactly as they do for the
     * material the feature is drawn with, since the mask has to land where the feature does.
     */
    explicit QgsHighlightMaskMaterial3D( bool instanced = false, Qgis::InstancedMaterialFlags instanceFlags = Qgis::InstancedMaterialFlags() );

    //! Sets the transform from mesh space to object space. Instanced materials only.
    void setMeshTransform( const QMatrix4x4 &transform );

    //! Sets the scale used for instances that do not carry their own
    void setSymbolScale( const QVector3D &scale );

    //! Sets the rotation used for instances that do not carry their own
    void setSymbolRotation( const QQuaternion &rotation );

  private:
    static std::shared_ptr<QgsRhiShader> shaderFor( bool instanced, Qgis::InstancedMaterialFlags instanceFlags );

    QgsShadowCasterUniforms mUniforms;
};

/**
 * \ingroup qgis_3d
 * \brief The highlight outline material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsHighlightOutlineBlock in
 * shaders/rhi/qgshighlightoutlinematerial.glsl.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsHighlightOutlineUniforms
{
    float outlineColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // offset 0
    int outlineWidth = 3;                               // offset 16
};

/**
 * \ingroup qgis_3d
 * \brief Draws a band around whatever the mask material covered.
 *
 * The other half of the QgsHighlightsRenderView port. Qt3D drew the highlighted geometry
 * four more times, each offset by a few pixels, and used a stencil test to keep only the
 * part that fell outside the original. Dilating a coverage mask gives the same picture for
 * one small texture and one full screen pass, and needs no stencil buffer - which matters,
 * because under QRhi a target cannot have both a stencil buffer and a depth texture the
 * post processing chain can sample.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsHighlightOutlineMaterial3D : public QgsMaterial3D
{
  public:
    QgsHighlightOutlineMaterial3D();

    //! Sets the coverage mask the band is grown from
    void setMaskTexture( QRhiTexture *texture );

    //! Sets the colour of the band
    void setOutlineColor( const QColor &color );

    //! Sets how far the band reaches out from the edge of a feature, in pixels
    void setOutlineWidth( int pixels );

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsHighlightOutlineUniforms &uniforms() const { return mUniforms; }

  private:
    QgsHighlightOutlineUniforms mUniforms;
    QgsGpuTexture mMaskTexture;
};

#endif // QGSHIGHLIGHTMATERIAL3D_H
