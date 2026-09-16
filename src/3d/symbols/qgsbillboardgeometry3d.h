/***************************************************************************
  qgsbillboardgeometry3d.h
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

#ifndef QGSBILLBOARDGEOMETRY3D_H
#define QGSBILLBOARDGEOMETRY3D_H

#include "qgis_3d.h"
#include "qgsgeometry3d.h"
#include "qgsgpubuffer.h"

#include <QPoint>
#include <QVector2D>
#include <QVector3D>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief Screen facing quads for point symbols, as instanced geometry for the QRhi engine.
 *
 * The QRhi counterpart of QgsBillboardGeometry, drawn with QgsBillboardMaterial3D. One
 * unit quad is drawn as a four vertex triangle strip, instanced once per billboard, with
 * everything that varies between billboards supplied as instanced attributes.
 *
 * Which instanced attributes are present depends on which of the three setters was used,
 * and has to agree with the material's shader variant: a shader that reads an attribute
 * the geometry does not supply fails to build a pipeline and draws nothing at all. Use
 * attributes() to pick the matching variant.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsBillboardGeometry3D
{
  public:
    //! Per-instance data a billboard geometry can carry beyond the position
    enum class Attribute
    {
      Position = 1 << 0,     //!< Billboard position, always present
      Size = 1 << 1,         //!< Per-billboard size, when billboards differ in size
      TextureData = 1 << 2,  //!< Texture atlas offset and size, when billboards differ in texture
      PixelOffsets = 1 << 3, //!< Per-billboard offset in pixels
    };
    Q_DECLARE_FLAGS( Attributes, Attribute )

    //! Contains the billboard positions and texture information
    struct BillboardAtlasData
    {
        //! Vertex position for billboard placement
        QVector3D position;
        //! Texture atlas offset for associated billboard texture
        QVector2D textureAtlasOffset;
        //! Texture atlas size for associated billboard texture
        QVector2D textureAtlasSize;
        //! Optional pixel offset for billboard
        QPoint pixelOffset;
    };

    QgsBillboardGeometry3D();

    QgsBillboardGeometry3D( const QgsBillboardGeometry3D & ) = delete;
    QgsBillboardGeometry3D &operator=( const QgsBillboardGeometry3D & ) = delete;

    /**
     * Sets the positions of billboards that all share one texture and one size.
     *
     * \see setPositionsAndSizes()
     * \see setBillboardData()
     */
    void setPositions( const QVector<QVector3D> &positions );

    /**
     * Sets per-billboard \a positions and \a sizes for billboards sharing one texture.
     *
     * \see setPositions()
     */
    void setPositionsAndSizes( const QVector<QVector3D> &positions, const QVector<QSizeF> &sizes );

    /**
     * Sets positions and texture atlas coordinates for billboards drawn from one atlas.
     *
     * Set \a includePixelOffsets to TRUE when the billboard data carries the optional
     * per-billboard pixel offsets.
     *
     * \see setPositions()
     */
    void setBillboardData( const QVector<BillboardAtlasData> &billboards, bool includePixelOffsets = false );

    //! Returns which per-instance attributes the geometry currently supplies
    Attributes attributes() const { return mAttributes; }

    //! Returns the number of billboards, which is the number of instances to draw
    int count() const { return mCount; }

    //! Returns the geometry to put in a render command, drawn as a triangle strip
    const QgsGeometry3D *geometry() const { return &mGeometry; }

    //! Returns the number of bytes one billboard occupies in the instance buffer
    int stride() const { return mStride; }

    //! Drops the GPU buffers, keeping the data. Must be called while the device is alive.
    void releaseGpuResources();

  private:
    //! Rebuilds the attribute list for the given set of per-instance attributes
    void setAttributes( Attributes attributes );

    QgsGpuBuffer mQuadBuffer { QgsGpuBuffer::Role::Vertex };
    QgsGpuBuffer mInstanceBuffer { QgsGpuBuffer::Role::Vertex, QgsGpuBuffer::Usage::Dynamic };
    QgsGeometry3D mGeometry;

    Attributes mAttributes;
    int mStride = 0;
    int mCount = 0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS( QgsBillboardGeometry3D::Attributes )

#endif // QGSBILLBOARDGEOMETRY3D_H
