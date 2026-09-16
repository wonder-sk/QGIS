/***************************************************************************
  qgsmeshgeometry3d.h
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

#ifndef QGSMESHGEOMETRY3D_H
#define QGSMESHGEOMETRY3D_H

///@cond PRIVATE

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QGIS API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//

#include "qgis_3d.h"
#include "qgsgeometry3d.h"
#include "qgsgpubuffer.h"

#include <QByteArray>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief A triangulated mesh layer surface, as geometry for the QRhi engine.
 *
 * The QRhi counterpart of QgsMesh3DGeometry and QgsMeshDataset3DGeometry, which under
 * Qt3D differed only in whether they declared a scalar magnitude attribute. Neither the
 * triangulation nor the worker threads that build the vertex data were Qt3D's doing, so
 * QgsMesh3DGeometryBuilder is reused unchanged: it hands over two byte arrays, and this
 * class says what is in them.
 *
 * Wireframe rendering is why a mesh may be non-indexed. QgsMesh3DMaterial draws the
 * wireframe from barycentric coordinates it derives from a vertex's position within its
 * triangle, which only works while each triangle has its own three vertices.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsMeshGeometry3D
{
  public:
    /**
     * Describes a mesh surface's buffer layout.
     *
     * Set \a withScalarMagnitude for a dataset mesh, whose vertices carry the value the
     * colour ramp is sampled with, and leave it FALSE for a terrain mesh. Set \a indexed
     * to FALSE for wireframe rendering, where each triangle owns its three vertices and
     * there is no index buffer.
     */
    explicit QgsMeshGeometry3D( bool withScalarMagnitude, bool indexed = true );

    QgsMeshGeometry3D( const QgsMeshGeometry3D & ) = delete;
    QgsMeshGeometry3D &operator=( const QgsMeshGeometry3D & ) = delete;

    /**
     * Sets the interleaved vertex data, as QgsMesh3DGeometryBuilder::vertexData() returns.
     *
     * The vertex count follows from the size of the data, since the builder writes one
     * vertex per mesh vertex when indexed and three per triangle when not.
     */
    void setVertexData( const QByteArray &vertexData );

    //! Sets the index data, as QgsMesh3DGeometryBuilder::indexData() returns
    void setIndexData( const QByteArray &indexData );

    //! Returns TRUE if the mesh carries a scalar magnitude per vertex
    bool hasScalarMagnitude() const { return mHasScalarMagnitude; }

    //! Returns the geometry to put in a render command
    const QgsGeometry3D *geometry() const { return &mGeometry; }

    //! Returns the number of bytes one vertex occupies
    int stride() const { return mStride; }

    //! Drops the GPU buffers, keeping the data. Must be called while the device is alive.
    void releaseGpuResources();

  private:
    QgsGpuBuffer mVertexBuffer { QgsGpuBuffer::Role::Vertex };
    QgsGpuBuffer mIndexBuffer { QgsGpuBuffer::Role::Index };
    QgsGeometry3D mGeometry;

    bool mHasScalarMagnitude = false;
    int mStride = 0;
};

/// @endcond

#endif // QGSMESHGEOMETRY3D_H
