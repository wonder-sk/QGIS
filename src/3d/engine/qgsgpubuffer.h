/***************************************************************************
  qgsgpubuffer.h
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

#ifndef QGSGPUBUFFER_H
#define QGSGPUBUFFER_H

#include <memory>
#include <rhi/qrhi.h>

#include "qgis_3d.h"

#include <QByteArray>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief A block of data that lives on the CPU and is mirrored to the GPU on demand.
 *
 * The CPU side copy is kept deliberately: ray casting and scene export both walk vertex
 * data on the CPU, and the geometry generating code in symbols, terrain and mesh builds
 * its data as a QByteArray anyway.
 *
 * The GPU side buffer is created lazily the first time the buffer is used for drawing,
 * and re-uploaded whenever setData() marked it dirty. Because creating and uploading both
 * need a live device, they happen in rhiBuffer(), not in setData().
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsGpuBuffer
{
  public:
    //! What the buffer will be bound as
    enum class Role
    {
      Vertex,  //!< Vertex attribute data
      Index,   //!< Index data
      Uniform, //!< Uniform block data
    };

    //! How often the content is expected to change
    enum class Usage
    {
      Static,  //!< Uploaded once, or rarely
      Dynamic, //!< Changes most frames
    };

    //! Constructs an empty buffer which will be bound in the given \a role
    explicit QgsGpuBuffer( Role role = Role::Vertex, Usage usage = Usage::Static );
    ~QgsGpuBuffer();

    QgsGpuBuffer( const QgsGpuBuffer & ) = delete;
    QgsGpuBuffer &operator=( const QgsGpuBuffer & ) = delete;

    //! Returns the role the buffer is bound in
    Role role() const { return mRole; }

    //! Replaces the buffer content, and marks it for re-upload
    void setData( const QByteArray &data );

    //! Returns the CPU side copy of the buffer content
    const QByteArray &data() const { return mData; }

    //! Returns the size of the buffer content in bytes
    int size() const { return static_cast<int>( mData.size() ); }

    //! Returns TRUE if the buffer holds no data
    bool isEmpty() const { return mData.isEmpty(); }

    /**
     * Returns the GPU buffer holding this data, creating and uploading it if needed.
     *
     * Any upload needed is appended to \a updates, which the caller must submit to the
     * command buffer. Returns NULLPTR if the buffer is empty or could not be created.
     */
    QRhiBuffer *rhiBuffer( QRhi *rhi, QRhiResourceUpdateBatch *updates );

    /**
     * Drops the GPU side buffer, keeping the CPU side copy.
     *
     * Must be called while the device is still alive. The buffer is recreated on the next
     * rhiBuffer() call.
     */
    void releaseGpuResources();

  private:
    Role mRole = Role::Vertex;
    Usage mUsage = Usage::Static;
    QByteArray mData;
    bool mDirty = true;
    std::unique_ptr<QRhiBuffer> mBuffer;
};

#endif // QGSGPUBUFFER_H
