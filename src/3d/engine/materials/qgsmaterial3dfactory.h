/***************************************************************************
  qgsmaterial3dfactory.h
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

#ifndef QGSMATERIAL3DFACTORY_H
#define QGSMATERIAL3DFACTORY_H

#include <memory>

#include "qgis.h"
#include "qgis_3d.h"

#include <QMatrix4x4>
#include <QQuaternion>
#include <QStringList>
#include <QVector3D>

#define SIP_NO_FILE

class QByteArray;
class QgsAbstractMaterialSettings;
class QgsExpressionContext;
class QgsMaterial3D;
class QgsMaterialContext;
class QgsSceneObject3D;

/**
 * \ingroup qgis_3d
 * \brief Turns a QgsAbstractMaterialSettings into the QRhi material that draws it.
 *
 * This is what QgsAbstractMaterial3DHandler and Qgs3D::toMaterial() were under Qt3D, and
 * it is deliberately a plain switch on the settings type rather than a registry: the
 * eight material settings classes all live in core and the QRhi materials that pair with
 * them all live next to this file, so there was never anything for a third party to
 * register, and the indirection only made the mapping harder to read.
 *
 * Textures are owned by the scene object the material is built for, because that is what
 * is destroyed when a chunk is unloaded.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
namespace QgsMaterial3DFactory
{
  /**
   * Returns the material drawing \a settings, or NULLPTR when the settings have no
   * material for \a technique.
   *
   * Any texture the material needs is loaded and handed to \a object, which must outlive
   * the material.
   */
  _3D_EXPORT std::unique_ptr<QgsMaterial3D> createMaterial(
    const QgsAbstractMaterialSettings *settings, Qgis::MaterialRenderingTechnique technique, const QgsMaterialContext &context, QgsSceneObject3D *object
  );

  /**
   * Returns the material drawing \a settings on geometry drawn once per instance.
   *
   * \a flags says which of scale and rotation each instance carries, and \a meshTransform
   * is applied to the mesh before it is instanced, which is where a model's axis
   * convention is corrected. \a symbolScale and \a symbolRotation are what instances that
   * do not carry their own are drawn with.
   */
  _3D_EXPORT std::unique_ptr<QgsMaterial3D> createInstancedMaterial(
    const QgsAbstractMaterialSettings *settings,
    const QgsMaterialContext &context,
    Qgis::InstancedMaterialFlags flags,
    const QMatrix4x4 &meshTransform,
    const QVector3D &symbolScale,
    const QQuaternion &symbolRotation,
    QgsSceneObject3D *object
  );

  /**
   * Returns the flat material an identified feature is drawn with, in the colour the
   * Map/highlight settings give.
   */
  _3D_EXPORT std::unique_ptr<QgsMaterial3D> createHighlightMaterial(
    bool instanced = false, Qgis::InstancedMaterialFlags flags = Qgis::InstancedMaterialFlags(), const QMatrix4x4 &meshTransform = QMatrix4x4()
  );

  /**
   * Returns TRUE if any of \a settings' colours comes from a data defined property, and
   * so has to be evaluated per feature and supplied per vertex.
   */
  _3D_EXPORT bool hasDataDefinedColors( const QgsAbstractMaterialSettings *settings );

  /**
   * Returns the vertex shader inputs the data defined colours of \a settings feed, in the
   * order dataDefinedColors() packs them.
   *
   * Pass these to QgsMaterial3DUtils::addDataDefinedColorAttributes() to describe the
   * buffer the packed colours were written into.
   */
  _3D_EXPORT QStringList dataDefinedColorAttributes( const QgsAbstractMaterialSettings *settings );

  /**
   * Returns TRUE if \a settings' data defined colours have to be stored as floats.
   *
   * A byte per channel is enough unless the material scales its colours by a coefficient
   * below one, where the loss of precision is visible.
   */
  _3D_EXPORT bool dataDefinedColorsUseFloats( const QgsAbstractMaterialSettings *settings );

  /**
   * Returns one vertex's worth of \a settings' data defined colours, evaluated against
   * \a expressionContext and packed the way the material's shader reads them.
   *
   * The result is QgsMaterial3DUtils::dataDefinedColorStride() bytes long and is repeated
   * once per vertex of the feature it was evaluated for.
   */
  _3D_EXPORT QByteArray dataDefinedColors( const QgsAbstractMaterialSettings *settings, const QgsExpressionContext &expressionContext );
} // namespace QgsMaterial3DFactory

#endif // QGSMATERIAL3DFACTORY_H
