/***************************************************************************
  qgsoverlaytexturematerial3d.h
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

#ifndef QGSOVERLAYTEXTUREMATERIAL3D_H
#define QGSOVERLAYTEXTUREMATERIAL3D_H

#include "qgis_3d.h"
#include "qgsgputexture.h"
#include "qgsmaterial3d.h"

#include <QSizeF>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief The overlay texture material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsOverlayTextureBlock in
 * shaders/rhi/qgsoverlaytexturematerial.glsl.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsOverlayTextureUniforms
{
    float centerTexCoords[2] = { 0.5f, 0.5f }; // offset 0
    float sizeTexCoords[2] = { 0.5f, 0.5f };   // offset 8
    int isDepth = 0;                           // offset 16
    int flipTextureY = 0;                      // offset 20
};

/**
 * \ingroup qgis_3d
 * \brief A texture shown over the finished scene, in a corner or across the whole view.
 *
 * The port of QgsOverlayTextureEntity, which covers both the 2D map overlay and the depth
 * map preview used when debugging it.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsOverlayTextureMaterial3D : public QgsMaterial3D
{
  public:
    QgsOverlayTextureMaterial3D();

    //! Sets the texture to show
    void setTexture( QRhiTexture *texture );

    /**
     * Places the quad with its centre at \a center and half-extents \a halfSize, both in
     * texture coordinates measured from the top left of the view.
     */
    void setViewport( const QPointF &center, const QSizeF &halfSize );

    /**
     * Places the quad in \a corner, \a size across and \a offset in from the edges, all as
     * fractions of the view.
     */
    void setPosition( Qt::Corner corner, const QSizeF &size, const QSizeF &offset );

    //! Sets whether the texture's red channel is shown as grey, for previewing a depth map
    void setIsDepth( bool isDepth );

    //! Sets whether the texture is sampled bottom up
    void setFlipTextureY( bool flip );

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsOverlayTextureUniforms &uniforms() const { return mUniforms; }

  private:
    QgsOverlayTextureUniforms mUniforms;
    QgsGpuTexture mTexture;
};

#endif // QGSOVERLAYTEXTUREMATERIAL3D_H
