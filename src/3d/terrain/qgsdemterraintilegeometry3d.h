/***************************************************************************
  qgsdemterraintilegeometry3d.h
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

#ifndef QGSDEMTERRAINTILEGEOMETRY3D_H
#define QGSDEMTERRAINTILEGEOMETRY3D_H

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

class QMatrix4x4;
class QgsRay3D;
class QgsRayCastContext;

/**
 * \ingroup qgis_3d
 * \brief One DEM terrain tile, as a grid of vertices the QRhi engine can draw.
 *
 * The QRhi counterpart of DemTerrainTileGeometry. Vertices are laid out as a regular
 * (resolution + 2) square grid: the extra ring around the edge is the skirt that hides
 * the seam between neighbouring tiles at different levels of detail.
 *
 * The two data generators are static and public because the Qt3D geometry uses them too
 * while both engines coexist. Neither was ever Qt3D's work.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsDemTerrainTileGeometry3D
{
  public:
    /**
     * Builds the geometry for one tile.
     *
     * \a resolution is the number of vertices along one side of the tile before the skirt
     * ring is added, \a side its width in world units, \a vertScale the factor heights are
     * multiplied by and \a skirtHeight how far the skirt hangs below the tile edge.
     * \a heightMap holds one float per vertex, row by row, and may contain NaN for
     * no-data, in which case the affected triangles are collapsed rather than drawn.
     */
    QgsDemTerrainTileGeometry3D( int resolution, float side, float vertScale, float skirtHeight, const QByteArray &heightMap );

    QgsDemTerrainTileGeometry3D( const QgsDemTerrainTileGeometry3D & ) = delete;
    QgsDemTerrainTileGeometry3D &operator=( const QgsDemTerrainTileGeometry3D & ) = delete;

    /**
     * Builds the interleaved vertex data for a tile: position, texture coordinate, normal.
     *
     * The result has ( \a res + 2 )² vertices of eight floats each.
     */
    static QByteArray createVertexData( int res, float side, float vertScale, float skirtHeight, const QByteArray &heights );

    //! Builds the 32 bit index data for a tile, collapsing triangles that touch no-data
    static QByteArray createIndexData( int res, const QByteArray &heightMap );

    /**
     * Intersects \a ray with the tile's triangles, returning the intersection closest to
     * the ray origin in \a intersectionPoint.
     *
     * \a worldTransform places the tile's vertices in the same space as the ray.
     * Returns FALSE when the ray misses the tile.
     */
    static bool rayIntersection(
      const QByteArray &vertexData, const QByteArray &indexData, const QgsRay3D &ray, const QgsRayCastContext &context, const QMatrix4x4 &worldTransform, QVector3D &intersectionPoint
    );

    //! Convenience overload of rayIntersection() using this tile's own data
    bool rayIntersection( const QgsRay3D &ray, const QgsRayCastContext &context, const QMatrix4x4 &worldTransform, QVector3D &intersectionPoint ) const;

    //! Returns the geometry to put in a render command
    const QgsGeometry3D *geometry() const { return &mGeometry; }

    //! Returns the number of bytes one vertex occupies
    static constexpr int stride() { return 8 * sizeof( float ); }

    //! Drops the GPU buffers, keeping the data. Must be called while the device is alive.
    void releaseGpuResources();

  private:
    QgsGpuBuffer mVertexBuffer { QgsGpuBuffer::Role::Vertex };
    QgsGpuBuffer mIndexBuffer { QgsGpuBuffer::Role::Index };
    QgsGeometry3D mGeometry;
};

/// @endcond

#endif // QGSDEMTERRAINTILEGEOMETRY3D_H
