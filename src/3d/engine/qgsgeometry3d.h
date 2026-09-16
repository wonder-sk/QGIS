/***************************************************************************
  qgsgeometry3d.h
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

#ifndef QGSGEOMETRY3D_H
#define QGSGEOMETRY3D_H

#include <rhi/qrhi.h>

#include "qgis_3d.h"
#include "qgsaabb.h"
#include "qgsvertexattribute.h"

#include <QList>

#define SIP_NO_FILE

class QgsGpuBuffer;

/**
 * \ingroup qgis_3d
 * \brief A set of vertex attributes plus optional indices, ready to be drawn.
 *
 * This is the direct replacement for Qt3DCore::QGeometry together with
 * Qt3DRender::QGeometryRenderer: it carries both the attribute layout and the vertex or
 * index count to draw, because keeping the two apart never bought QGIS anything.
 *
 * The geometry does not own its buffers. Chunk loaders typically keep the buffers next to
 * the geometry and destroy both together.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsGeometry3D
{
  public:
    QgsGeometry3D() = default;

    //! Adds a vertex attribute. Attribute names must be unique within a geometry.
    void addAttribute( const QgsVertexAttribute &attribute );

    /**
     * Removes every vertex attribute.
     *
     * Geometries whose attribute set changes with the data they are given, such as
     * billboards, rebuild the list rather than patching it.
     */
    void clearAttributes();

    //! Returns all vertex attributes
    const QList<QgsVertexAttribute> &attributes() const { return mAttributes; }

    //! Returns the attribute called \a name, or NULLPTR if the geometry has no such attribute
    const QgsVertexAttribute *attribute( const QString &name ) const;

    /**
     * Sets the index buffer to draw with.
     *
     * Passing NULLPTR makes the geometry non-indexed, in which case vertexCount() vertices
     * are drawn straight from the attribute buffers.
     */
    void setIndexBuffer( QgsGpuBuffer *buffer, QRhiCommandBuffer::IndexFormat format = QRhiCommandBuffer::IndexUInt32 );

    //! Returns the index buffer, or NULLPTR if the geometry is not indexed
    QgsGpuBuffer *indexBuffer() const { return mIndexBuffer; }

    //! Returns the element type of the index buffer
    QRhiCommandBuffer::IndexFormat indexFormat() const { return mIndexFormat; }

    //! Returns TRUE if the geometry is drawn through an index buffer
    bool isIndexed() const { return mIndexBuffer != nullptr; }

    //! Sets how many vertices to draw when the geometry is not indexed
    void setVertexCount( int count ) { mVertexCount = count; }

    //! Returns how many vertices are drawn when the geometry is not indexed
    int vertexCount() const { return mVertexCount; }

    //! Sets how many indices to draw when the geometry is indexed
    void setIndexCount( int count ) { mIndexCount = count; }

    //! Returns how many indices are drawn when the geometry is indexed
    int indexCount() const { return mIndexCount; }

    //! Returns the number of elements a draw call for this geometry issues
    int drawCount() const { return mIndexBuffer ? mIndexCount : mVertexCount; }

    //! Sets the bounding box of the geometry, in the coordinates the vertex data uses
    void setBoundingBox( const QgsAABB &box ) { mBoundingBox = box; }

    //! Returns the bounding box of the geometry
    QgsAABB boundingBox() const { return mBoundingBox; }

    /**
     * Returns a value identifying the attribute layout of this geometry.
     *
     * Two geometries with the same layout key can share a graphics pipeline, which is what
     * the pipeline cache uses this for. The key covers attribute names, formats, strides
     * and instancing divisors, but deliberately not the buffers or the data in them.
     */
    size_t layoutKey() const;

  private:
    QList<QgsVertexAttribute> mAttributes;
    QgsGpuBuffer *mIndexBuffer = nullptr;
    QRhiCommandBuffer::IndexFormat mIndexFormat = QRhiCommandBuffer::IndexUInt32;
    int mVertexCount = 0;
    int mIndexCount = 0;
    QgsAABB mBoundingBox;
    mutable size_t mLayoutKey = 0;
};

#endif // QGSGEOMETRY3D_H
