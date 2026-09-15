/***************************************************************************
  qgsrendercommand.h
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

#ifndef QGSRENDERCOMMAND_H
#define QGSRENDERCOMMAND_H

#include "qgis_3d.h"

#include <QMatrix4x4>

#define SIP_NO_FILE

class QgsGeometry3D;
class QgsMaterial3D;

/**
 * \ingroup qgis_3d
 * \brief One thing to draw: a geometry, what to draw it with, and where to put it.
 *
 * Commands are cheap value types rebuilt every frame by whatever walks the scene, which is
 * why there is no entity tree here. An object is drawn in a pass because a command for it
 * was appended to that pass, rather than because a layer filter happened to match it.
 *
 * Neither the geometry nor the material is owned by the command; both have to outlive the
 * frame they were appended in.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsRenderCommand
{
    //! Geometry to draw
    const QgsGeometry3D *geometry = nullptr;

    //! Material to draw it with
    QgsMaterial3D *material = nullptr;

    /**
   * Transform from the geometry's own coordinates to world coordinates.
   *
   * Chunked entities put their chunk origin here, which is what keeps vertex data in a
   * small range of single precision floats no matter where on the globe the chunk is.
   */
    QMatrix4x4 worldTransform;

    //! Number of instances to draw. Instanced attributes advance once per instance.
    int instanceCount = 1;

    //! Index of the first vertex, or of the first index for indexed geometry
    int firstElement = 0;

    //! How many vertices or indices to draw, or -1 to draw all of the geometry
    int elementCount = -1;

    /**
   * Value the pass sorts commands by, ascending.
   *
   * For opaque geometry this should group commands sharing a pipeline, to keep pipeline
   * switches down. For transparent geometry it should order them back to front, because
   * blending is not commutative.
   */
    quint32 sortKey = 0;
};

#endif // QGSRENDERCOMMAND_H
