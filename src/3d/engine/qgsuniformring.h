/***************************************************************************
  qgsuniformring.h
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

#ifndef QGSUNIFORMRING_H
#define QGSUNIFORMRING_H

#include <memory>
#include <rhi/qrhi.h>

#include "qgis_3d.h"

#include <QByteArray>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief A per-frame arena of equally sized uniform slices bound with dynamic offsets.
 *
 * This is what replaces Qt3D's QParameter for per-draw data. Instead of one uniform buffer
 * per object, a pass writes one slice per render command into a single buffer and binds it
 * at the right offset for each draw, so a scene with thousands of chunks needs one buffer
 * rather than thousands.
 *
 * Use is two phased, which is what keeps growth safe: every slice is staged with append()
 * while the pass walks its commands, then upload() sizes the GPU buffer once and uploads
 * everything. Because no draw has been recorded yet at that point, growing the buffer can
 * never invalidate a buffer a recorded command already refers to.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsUniformRing
{
  public:
    QgsUniformRing();
    ~QgsUniformRing();

    QgsUniformRing( const QgsUniformRing & ) = delete;
    QgsUniformRing &operator=( const QgsUniformRing & ) = delete;

    /**
     * Discards everything staged so far and starts a new frame with slices of
     * \a elementSize bytes.
     *
     * The slice size is rounded up to the device's uniform buffer offset alignment,
     * because that is the granularity dynamic offsets work at.
     */
    void reset( QRhi *rhi, int elementSize );

    /**
     * Stages one slice holding the bytes at \a data, and returns the dynamic offset it
     * will be bound at.
     *
     * Exactly the element size passed to reset() is read from \a data; any alignment
     * padding beyond it is zero filled and never reaches the shader.
     */
    quint32 append( const void *data );

    //! Returns the number of slices staged since the last reset()
    int count() const { return mCount; }

    /**
     * Returns the stride between slices, which is the element size passed to reset()
     * rounded up to the device's uniform buffer offset alignment.
     */
    quint32 elementSize() const { return mElementSize; }

    /**
     * Makes sure the GPU buffer is large enough and queues the upload of everything staged.
     *
     * Returns the buffer to bind, or NULLPTR if nothing was staged or the buffer could not
     * be created. Must be called before any draw using these offsets is recorded.
     */
    QRhiBuffer *upload( QRhi *rhi, QRhiResourceUpdateBatch *updates );

    //! Drops the GPU buffer. Must be called while the device is still alive.
    void releaseGpuResources();

  private:
    QByteArray mStaging;
    quint32 mDataSize = 0;
    quint32 mElementSize = 0;
    int mCount = 0;
    std::unique_ptr<QRhiBuffer> mBuffer;
};

#endif // QGSUNIFORMRING_H
