/***************************************************************************
  qgsglobetilegeometry3d.h
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

#ifndef QGSGLOBETILEGEOMETRY3D_H
#define QGSGLOBETILEGEOMETRY3D_H

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
#include "qgsvector3d.h"

#define SIP_NO_FILE

class QgsCoordinateTransform;

/**
 * \ingroup qgis_3d
 * \brief One tile of the globe: a patch of a longitude / latitude grid, curved into ECEF.
 *
 * The QRhi counterpart of the geometry QgsGlobeChunkedEntity builds per tile. The grid is
 * laid out in longitude and latitude, transformed into the globe's geocentric CRS and
 * then written relative to the tile's own centre, which is what keeps the coordinates
 * small enough for single precision floats this far from the earth's centre. That centre
 * is origin(), and the caller puts it in the render command's world transform.
 *
 * Indices are 16 bit, so a tile may have at most 65536 vertices.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsGlobeTileGeometry3D
{
  public:
    /**
     * Builds the geometry for the tile spanning the given longitude and latitude range.
     *
     * \a globeCrsToLatLon transforms from the globe's geocentric CRS to longitude and
     * latitude; it is used in reverse. \a lonSliceCount and \a latSliceCount are the
     * numbers of vertices along each side, so both must be at least two.
     */
    QgsGlobeTileGeometry3D( const QgsCoordinateTransform &globeCrsToLatLon, double lonMin, double lonMax, double latMin, double latMax, int lonSliceCount, int latSliceCount );

    QgsGlobeTileGeometry3D( const QgsGlobeTileGeometry3D & ) = delete;
    QgsGlobeTileGeometry3D &operator=( const QgsGlobeTileGeometry3D & ) = delete;

    /**
     * Returns the point the vertex coordinates are relative to, in the globe's CRS.
     *
     * This is the centre of the tile, and belongs in the world transform of any render
     * command drawing it.
     */
    QgsVector3D origin() const { return mOrigin; }

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
    QgsVector3D mOrigin;
};

/// @endcond

#endif // QGSGLOBETILEGEOMETRY3D_H
