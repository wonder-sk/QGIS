/***************************************************************************
  qgstessellatedpolygongeometry3d.h
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

#ifndef QGSTESSELLATEDPOLYGONGEOMETRY3D_H
#define QGSTESSELLATEDPOLYGONGEOMETRY3D_H

#include "qgis_3d.h"
#include "qgsfeatureid.h"
#include "qgsgeometry3d.h"
#include "qgsgpubuffer.h"

#include <QVector>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief Polygons tessellated into triangles, ready for the QRhi engine to draw.
 *
 * The QRhi counterpart of QgsTessellatedPolygonGeometry. It takes the interleaved vertex
 * buffer QgsTessellator produces and describes it as a QgsGeometry3D, which is the whole
 * of the porting work for this kind of class: the tessellation itself, the buffer layout
 * and the feature id bookkeeping were never Qt3D's doing.
 *
 * Attribute names match the ones the Qt3D version used, so shaders and symbol code can
 * keep referring to "vertexPosition", "vertexNormal" and so on.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsTessellatedPolygonGeometry3D
{
  public:
    /**
     * Constructs the geometry description for a tessellator configured the same way.
     *
     * The flags must match the QgsTessellator that produced the data, because they are
     * what fixes the stride and the offset of each attribute within it.
     */
    explicit QgsTessellatedPolygonGeometry3D( bool withNormals = true, bool withTextureCoords = false, bool withTangents = false );

    QgsTessellatedPolygonGeometry3D( const QgsTessellatedPolygonGeometry3D & ) = delete;
    QgsTessellatedPolygonGeometry3D &operator=( const QgsTessellatedPolygonGeometry3D & ) = delete;

    /**
     * Sets the interleaved vertex data, normally QgsTessellator::data().
     *
     * \a vertexCount is the number of vertices in it, not its size in bytes.
     */
    void setVertexBufferData( const QByteArray &vertexData, int vertexCount );

    //! Sets the index data, which must be 32 bit unsigned integers
    void setIndexBufferData( const QByteArray &indexData, int indexCount );

    /**
     * Records which feature each triangle came from, for picking.
     *
     * For the feature at featureIds()[i], its triangles start at
     * triangleIndexStartingIndices()[i].
     */
    void setTriangleIndex( const QVector<QgsFeatureId> &featureIds, const QVector<uint> &startingIndices );

    /**
     * Returns the id of the feature the triangle at \a triangleIndex belongs to, or
     * FID_NULL when it belongs to none.
     */
    QgsFeatureId triangleIndexToFeatureId( uint triangleIndex ) const;

    //! Returns the ids of the features in this geometry
    QVector<QgsFeatureId> featureIds() const { return mFeatureIds; }

    //! Returns where each feature's triangles start
    QVector<uint> triangleIndexStartingIndices() const { return mStartingIndices; }

    //! Returns the geometry to put in a render command
    const QgsGeometry3D *geometry() const { return &mGeometry; }

    //! Returns the number of bytes one vertex occupies in the vertex buffer
    int stride() const { return mStride; }

    //! Drops the GPU buffers, keeping the data. Must be called while the device is alive.
    void releaseGpuResources();

  private:
    QgsGpuBuffer mVertexBuffer { QgsGpuBuffer::Role::Vertex };
    QgsGpuBuffer mIndexBuffer { QgsGpuBuffer::Role::Index };
    QgsGeometry3D mGeometry;
    int mStride = 0;

    QVector<QgsFeatureId> mFeatureIds;
    QVector<uint> mStartingIndices;
};

#endif // QGSTESSELLATEDPOLYGONGEOMETRY3D_H
