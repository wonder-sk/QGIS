/***************************************************************************
  qgsgltfgeometry3d.h
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

#ifndef QGSGLTFGEOMETRY3D_H
#define QGSGLTFGEOMETRY3D_H

///@cond PRIVATE

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QGIS API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//

#include <map>
#include <memory>

#include "qgis_3d.h"
#include "qgsgeometry3d.h"
#include "qgsgpubuffer.h"

#include <QQuaternion>
#include <QVector3D>

#define SIP_NO_FILE

namespace tinygltf
{
  class Model;
}

/**
 * \ingroup qgis_3d
 * \brief One glTF mesh primitive, as geometry for the QRhi engine.
 *
 * The QRhi counterpart of the Qt3DCore::QGeometry QgsGltf3DUtils used to assemble per
 * primitive. Loading, reprojection and material parsing stay in QgsGltf3DUtils: what
 * lives here is turning glTF accessors into vertex attributes, which is the part Qt3D
 * was doing.
 *
 * Two things differ from the Qt3D version, both deliberate:
 *
 * - **Buffers are shared.** A glTF buffer is uploaded once no matter how many accessors
 *   read from it, rather than deep copied per attribute as Qt3D's version did. This is
 *   the TODO that version carried.
 * - **Formats QRhi cannot express are converted.** glTF allows normalised shorts and
 *   three component bytes, which QRhi has no vertex format for; those accessors are
 *   unpacked to floats on the CPU instead of being rejected.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsGltfGeometry3D
{
  public:
    //! Where one instance of an instanced primitive goes
    struct InstanceTransform
    {
        QVector3D translation;
        QQuaternion rotation;
        QVector3D scale { 1.0f, 1.0f, 1.0f };
    };

    QgsGltfGeometry3D();
    ~QgsGltfGeometry3D();

    QgsGltfGeometry3D( const QgsGltfGeometry3D & ) = delete;
    QgsGltfGeometry3D &operator=( const QgsGltfGeometry3D & ) = delete;

    /**
     * Sets the vertex positions, as \a count tightly packed triples of floats.
     *
     * The positions come from QgsGltf3DUtils, which has already reprojected them into
     * the chunk's coordinates; a glTF position accessor is in the model's own space.
     */
    void setPositions( const QByteArray &positions, int count );

    /**
     * Adds the attribute the accessor at \a accessorIndex describes, under \a name.
     *
     * Returns FALSE and, when \a error is given, says why, if the accessor cannot be
     * turned into a vertex attribute at all.
     */
    bool addAttribute( const QString &name, const tinygltf::Model &model, int accessorIndex, QString *error = nullptr );

    /**
     * Sets the index buffer from the accessor at \a accessorIndex.
     *
     * glTF index accessors may be bytes, shorts or ints; QRhi only draws 16 or 32 bit
     * indices, so byte indices are widened to 16 bit.
     */
    bool setIndices( const tinygltf::Model &model, int accessorIndex, QString *error = nullptr );

    /**
     * Adds the per-instance translation, rotation and scale attributes.
     *
     * These are what the instanced shader variants read, so a primitive that has them
     * has to be drawn with an instanced material.
     */
    void setInstances( const QVector<InstanceTransform> &instances );

    //! Returns how many instances to draw, which is 1 for a primitive that is not instanced
    int instanceCount() const { return mInstanceCount; }

    //! Returns the geometry to put in a render command
    const QgsGeometry3D *geometry() const { return &mGeometry; }

    //! Drops the GPU buffers, keeping the data. Must be called while the device is alive.
    void releaseGpuResources();

  private:
    /**
     * Returns the buffer holding glTF buffer \a index, uploading it the first time.
     *
     * Sharing one buffer between every accessor that reads it is what keeps a model with
     * one interleaved buffer from being copied once per attribute.
     */
    QgsGpuBuffer *bufferFor( const tinygltf::Model &model, int index );

    //! Returns a buffer holding \a data, owned by this geometry
    QgsGpuBuffer *ownBuffer( const QByteArray &data, QgsGpuBuffer::Role role );

    QgsGeometry3D mGeometry;
    std::map<int, std::unique_ptr<QgsGpuBuffer>> mModelBuffers;
    std::vector<std::unique_ptr<QgsGpuBuffer>> mOwnBuffers;
    int mInstanceCount = 1;
};

/// @endcond

#endif // QGSGLTFGEOMETRY3D_H
