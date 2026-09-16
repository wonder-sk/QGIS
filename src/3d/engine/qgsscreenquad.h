/***************************************************************************
  qgsscreenquad.h
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

#ifndef QGSSCREENQUAD_H
#define QGSSCREENQUAD_H

#include "qgis_3d.h"
#include "qgsgeometry3d.h"
#include "qgsgpubuffer.h"

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief Two triangles covering the whole of clip space, for full screen passes.
 *
 * The direct replacement for QgsRenderPassQuad's geometry. Every post processing pass
 * draws exactly this and does its work in the fragment shader, so one of these is shared
 * by the whole chain rather than rebuilt per pass.
 *
 * The single attribute is "vertexPosition", positions already in clip space, which is why
 * the vertex shaders of these passes ignore the camera entirely.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsScreenQuad
{
  public:
    QgsScreenQuad();

    //! Returns the geometry to draw
    const QgsGeometry3D *geometry() const { return &mGeometry; }

    //! Drops the GPU buffer. Must be called while the device is still alive.
    void releaseGpuResources() { mBuffer.releaseGpuResources(); }

  private:
    QgsGpuBuffer mBuffer { QgsGpuBuffer::Role::Vertex, QgsGpuBuffer::Usage::Static };
    QgsGeometry3D mGeometry;
};

#endif // QGSSCREENQUAD_H
