/***************************************************************************
  qgsobjgeometry3d.h
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

#ifndef QGSOBJGEOMETRY3D_H
#define QGSOBJGEOMETRY3D_H

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
 * \brief One material's worth of an OBJ model, as geometry for the QRhi engine.
 *
 * The QRhi counterpart of the geometry QgsObj3DUtils assembles per material. tinyobj
 * gives back separate triangles rather than an indexed mesh, so this is never indexed.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsObjGeometry3D
{
  public:
    /**
     * Describes an OBJ mesh's interleaved buffer layout.
     *
     * The flags must match how the vertex data was written: they are what fixes the
     * stride and each attribute's offset within it.
     */
    QgsObjGeometry3D( bool withNormals, bool withTextureCoords );

    QgsObjGeometry3D( const QgsObjGeometry3D & ) = delete;
    QgsObjGeometry3D &operator=( const QgsObjGeometry3D & ) = delete;

    //! Sets the interleaved vertex data; the vertex count follows from its size
    void setVertexData( const QByteArray &vertexData );

    //! Returns the geometry to put in a render command
    const QgsGeometry3D *geometry() const { return &mGeometry; }

    //! Returns the number of bytes one vertex occupies
    int stride() const { return mStride; }

    //! Drops the GPU buffer, keeping the data. Must be called while the device is alive.
    void releaseGpuResources();

  private:
    QgsGpuBuffer mVertexBuffer { QgsGpuBuffer::Role::Vertex };
    QgsGeometry3D mGeometry;
    int mStride = 0;
};

/// @endcond

#endif // QGSOBJGEOMETRY3D_H
