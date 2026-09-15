/***************************************************************************
  qgsvertexattribute.h
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

#ifndef QGSVERTEXATTRIBUTE_H
#define QGSVERTEXATTRIBUTE_H

#include <rhi/qrhi.h>

#include "qgis_3d.h"

#include <QString>

#define SIP_NO_FILE

class QgsGpuBuffer;

/**
 * \ingroup qgis_3d
 * \brief Describes one vertex attribute stored in a QgsGpuBuffer.
 *
 * Attributes are identified by name, the same way they were with Qt3D. QRhi itself binds
 * vertex inputs by location rather than by name, so the name is resolved to a location
 * once, when a graphics pipeline is built, using the reflection data of the vertex shader.
 * Geometry generating code therefore never has to know about shader locations.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsVertexAttribute
{
    //! Name of the attribute, matched against the vertex shader input of the same name
    QString name;

    //! Buffer holding the attribute data. Several attributes may share one buffer.
    QgsGpuBuffer *buffer = nullptr;

    //! Element type of the attribute
    QRhiVertexInputAttribute::Format format = QRhiVertexInputAttribute::Float3;

    //! Byte offset of the first element within the buffer
    quint32 byteOffset = 0;

    //! Byte distance between consecutive elements. Zero means tightly packed.
    quint32 byteStride = 0;

    /**
   * Rate at which the attribute advances for instanced drawing.
   *
   * Zero means the attribute advances per vertex, one means it advances once per instance,
   * and larger values advance once every N instances.
   */
    quint32 instanceDivisor = 0;

    //! Returns the size of one element in bytes
    int elementSize() const;
};

#endif // QGSVERTEXATTRIBUTE_H
