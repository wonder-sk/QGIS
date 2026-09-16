/***************************************************************************
  qgspointcloudgeometry3d.h
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

#ifndef QGSPOINTCLOUDGEOMETRY3D_H
#define QGSPOINTCLOUDGEOMETRY3D_H

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
#include <QVector3D>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief One chunk of a point cloud, as geometry for the QRhi engine.
 *
 * Replaces the four QgsPointCloud3DGeometry subclasses, which under Qt3D differed only in
 * which per-vertex attributes they declared, with one class and a layout enum. That is
 * the same dividing line the materials already use: what changes which attributes a
 * shader reads has to be a baked variant, and everything else is a run time flag.
 *
 * Points are drawn either as points, where the material is responsible for giving them a
 * size, or as a triangulated surface once setTriangles() has supplied an index buffer and
 * per-vertex normals.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsPointCloudGeometry3D
{
  public:
    //! What each point carries beyond its position, which is what picks the shader variant
    enum class VertexLayout
    {
      //! Position only, for the single colour renderer
      PositionOnly,
      //! Position and a "vertexParameter" the colour ramp is sampled with
      PositionAndParameter,
      //! Position and a "vertexColor" read straight from the point cloud
      PositionAndColor,
      //! Position, a "vertexParameter" and a per-point "vertexSize", for the classification renderer
      PositionParameterAndSize,
    };

    /**
     * Describes a point cloud chunk's buffer layout.
     *
     * \a byteStride is the symbol's own stride, which may leave room beyond the
     * attributes this layout uses; passing zero packs the attributes tightly.
     */
    explicit QgsPointCloudGeometry3D( VertexLayout layout, int byteStride = 0 );

    QgsPointCloudGeometry3D( const QgsPointCloudGeometry3D & ) = delete;
    QgsPointCloudGeometry3D &operator=( const QgsPointCloudGeometry3D & ) = delete;

    /**
     * Builds the interleaved vertex buffer from the per-point arrays.
     *
     * Only the arrays the layout uses are read, and each of those must be as long as
     * \a positions.
     */
    void setPoints(
      const QVector<QVector3D> &positions, const QVector<float> &parameters = QVector<float>(), const QVector<QVector3D> &colors = QVector<QVector3D>(), const QVector<float> &pointSizes = QVector<float>()
    );

    /**
     * Turns the chunk into a triangulated surface rather than a cloud of points.
     *
     * \a indices holds 32 bit unsigned indices into the points, and \a normals three
     * floats per point. Passing empty data returns the chunk to being drawn as points.
     */
    void setTriangles( const QByteArray &indices, const QByteArray &normals );

    //! Returns the layout of the vertex buffer
    VertexLayout layout() const { return mLayout; }

    //! Returns TRUE if the chunk is drawn as a triangulated surface
    bool isTriangulated() const { return mGeometry.isIndexed(); }

    //! Returns the number of points
    int pointCount() const { return mGeometry.vertexCount(); }

    //! Returns the geometry to put in a render command
    const QgsGeometry3D *geometry() const { return &mGeometry; }

    //! Returns the number of bytes one point occupies in the vertex buffer
    int stride() const { return mStride; }

    //! Drops the GPU buffers, keeping the data. Must be called while the device is alive.
    void releaseGpuResources();

  private:
    //! Returns how many floats of the stride the layout actually fills
    static int floatsPerPoint( VertexLayout layout );

    QgsGpuBuffer mVertexBuffer { QgsGpuBuffer::Role::Vertex };
    QgsGpuBuffer mIndexBuffer { QgsGpuBuffer::Role::Index };
    QgsGpuBuffer mNormalBuffer { QgsGpuBuffer::Role::Vertex };
    QgsGeometry3D mGeometry;

    VertexLayout mLayout = VertexLayout::PositionOnly;
    int mStride = 0;
};

/// @endcond

#endif // QGSPOINTCLOUDGEOMETRY3D_H
