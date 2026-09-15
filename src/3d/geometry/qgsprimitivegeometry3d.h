/***************************************************************************
  qgsprimitivegeometry3d.h
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

#ifndef QGSPRIMITIVEGEOMETRY3D_H
#define QGSPRIMITIVEGEOMETRY3D_H

#include <memory>

#include "qgis_3d.h"
#include "qgsgeometry3d.h"
#include "qgsgpubuffer.h"

#include <QSize>
#include <QString>

#define SIP_NO_FILE

class QFont;

/**
 * \ingroup qgis_3d
 * \brief Parametric shapes for the QRhi engine: spheres, boxes, cones and the like.
 *
 * These replace the Qt3DExtras geometry generators QGIS used for point symbol shapes, the
 * 3D axis, light and camera markers and the material preview. Parameters, defaults and
 * orientations follow Qt3DExtras, so a shape looks the same as it did, but the meshes are
 * generated here and the vertex layout is QGIS's own.
 *
 * Every shape but extruded text is centred on the origin. Spheres, cylinders and cones
 * stand along the Y axis, a plane lies in the XZ plane facing up, and a torus lies in the
 * XY plane; extruded text sits on its baseline at the origin and extrudes along +Z, the
 * way Qt3DExtras placed it. Vertex data is interleaved as position, normal, optional
 * tangent, texture coordinate, the same order QgsTessellator writes, so the same shaders
 * can draw both.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsPrimitiveGeometry3D
{
  public:
    QgsPrimitiveGeometry3D();
    ~QgsPrimitiveGeometry3D();

    QgsPrimitiveGeometry3D( const QgsPrimitiveGeometry3D & ) = delete;
    QgsPrimitiveGeometry3D &operator=( const QgsPrimitiveGeometry3D & ) = delete;

    /**
     * Builds a sphere of the given \a radius, divided into \a rings of latitude and
     * \a slices of longitude.
     */
    static std::unique_ptr<QgsPrimitiveGeometry3D> sphere( float radius = 1.0f, int rings = 16, int slices = 16, bool withTangents = false );

    //! Builds a box centred on the origin with the given extents along each axis
    static std::unique_ptr<QgsPrimitiveGeometry3D> cuboid( float xExtent = 1.0f, float yExtent = 1.0f, float zExtent = 1.0f, bool withTangents = false );

    /**
     * Builds a cylinder of the given \a radius and \a length standing along the Y axis,
     * with flat caps at both ends.
     */
    static std::unique_ptr<QgsPrimitiveGeometry3D> cylinder( float radius = 1.0f, float length = 1.0f, int rings = 16, int slices = 16, bool withTangents = false );

    /**
     * Builds a cone standing along the Y axis, tapering from \a bottomRadius to
     * \a topRadius over \a length.
     *
     * A \a topRadius of zero gives a sharp point, and equal radii give a cylinder.
     * The end caps are optional because a cone with an open end is used for arrow heads.
     */
    static std::unique_ptr<QgsPrimitiveGeometry3D> cone(
      float bottomRadius = 1.0f, float topRadius = 0.0f, float length = 1.0f, bool hasBottomEndcap = true, bool hasTopEndcap = true, int rings = 16, int slices = 16, bool withTangents = false
    );

    /**
     * Builds a torus lying in the XY plane, a ring of \a radius whose tube has
     * \a minorRadius.
     */
    static std::unique_ptr<QgsPrimitiveGeometry3D> torus( float radius = 1.0f, float minorRadius = 1.0f, int rings = 16, int slices = 16, bool withTangents = false );

    /**
     * Builds a flat rectangle in the XZ plane facing up, \a width along X and \a height
     * along Z, divided into a grid of \a resolution vertices.
     */
    static std::unique_ptr<QgsPrimitiveGeometry3D> plane( float width = 1.0f, float height = 1.0f, QSize resolution = QSize( 2, 2 ), bool withTangents = false );

    /**
     * Builds \a text extruded to \a depth along the Z axis, laid out in \a font.
     *
     * The glyph outlines are triangulated for the front and back faces and walled in
     * between. The result is scaled by the font's point size, so a size change alters the
     * resolution of the curves rather than the size of the text.
     *
     * Returns a geometry with no triangles when the text is empty or the font renders
     * nothing for it.
     */
    static std::unique_ptr<QgsPrimitiveGeometry3D> extrudedText( const QString &text, const QFont &font, float depth = 1.0f );

    //! Returns the geometry to put in a render command
    const QgsGeometry3D *geometry() const { return &mGeometry; }

    //! Returns the number of vertices the shape was built from
    int vertexCount() const { return mGeometry.vertexCount(); }

    //! Returns the number of indices the shape is drawn with
    int indexCount() const { return mGeometry.indexCount(); }

    //! Returns the number of bytes one vertex occupies
    int stride() const { return mStride; }

    //! Returns TRUE if the shape carries tangent vectors
    bool hasTangents() const { return mHasTangents; }

    //! Drops the GPU buffers, keeping the data. Must be called while the device is alive.
    void releaseGpuResources();

  private:
    //! Describes the attribute layout and hands the built data to the buffers
    void setData( const QByteArray &vertexData, int vertexCount, const QByteArray &indexData, int indexCount, bool withTangents );

    QgsGpuBuffer mVertexBuffer { QgsGpuBuffer::Role::Vertex };
    QgsGpuBuffer mIndexBuffer { QgsGpuBuffer::Role::Index };
    QgsGeometry3D mGeometry;
    int mStride = 0;
    bool mHasTangents = false;
};

#endif // QGSPRIMITIVEGEOMETRY3D_H
