/***************************************************************************
  qgswiredmeshgeometry3d.h
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

#ifndef QGSWIREDMESHGEOMETRY3D_H
#define QGSWIREDMESHGEOMETRY3D_H

#include "qgis_3d.h"
#include "qgsgeometry3d.h"
#include "qgsgpubuffer.h"

#include <QList>
#include <QVector3D>

#define SIP_NO_FILE

class QgsAABB;

/**
 * \ingroup qgis_3d
 * \brief A set of unconnected line segments, used to draw chunk bounding boxes.
 *
 * The QRhi counterpart of Qgs3DWiredMesh. Vertices are taken in pairs, so the material
 * drawing this has to use QRhiGraphicsPipeline::Lines topology. These are debug overlays
 * one pixel wide, which is why they do not go through P6's thick line expansion.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsWiredMeshGeometry3D
{
  public:
    QgsWiredMeshGeometry3D();

    QgsWiredMeshGeometry3D( const QgsWiredMeshGeometry3D & ) = delete;
    QgsWiredMeshGeometry3D &operator=( const QgsWiredMeshGeometry3D & ) = delete;

    //! Replaces the line segments, taken as consecutive pairs of \a vertices
    void setVertices( const QList<QVector3D> &vertices );

    //! Replaces the line segments with the edges of each box in \a boxes
    void setVertices( const QList<QgsAABB> &boxes );

    //! Returns the number of vertices, which is twice the number of segments
    int vertexCount() const { return mGeometry.vertexCount(); }

    //! Returns the geometry to put in a render command, drawn as lines
    const QgsGeometry3D *geometry() const { return &mGeometry; }

    //! Drops the GPU buffer, keeping the data. Must be called while the device is alive.
    void releaseGpuResources();

  private:
    QgsGpuBuffer mVertexBuffer { QgsGpuBuffer::Role::Vertex, QgsGpuBuffer::Usage::Dynamic };
    QgsGeometry3D mGeometry;
};

#endif // QGSWIREDMESHGEOMETRY3D_H
