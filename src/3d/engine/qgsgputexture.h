/***************************************************************************
  qgsgputexture.h
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

#ifndef QGSGPUTEXTURE_H
#define QGSGPUTEXTURE_H

#include <memory>
#include <rhi/qrhi.h>

#include "qgis_3d.h"

#include <QImage>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief An image that is uploaded to the GPU on demand.
 *
 * Like QgsGpuBuffer, the source data stays on the CPU and the GPU texture is created the
 * first time it is needed for drawing, so textures can be built on a chunk loading thread
 * and only touch the device once they reach the render thread.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsGpuTexture
{
  public:
    QgsGpuTexture() = default;
    ~QgsGpuTexture();

    QgsGpuTexture( const QgsGpuTexture & ) = delete;
    QgsGpuTexture &operator=( const QgsGpuTexture & ) = delete;

    /**
     * Sets the image the texture holds, and marks it for re-upload.
     *
     * The image is converted to RGBA8888 if it is in some other format, because that is
     * the one colour format every QRhi backend is required to support.
     */
    void setImage( const QImage &image );

    //! Returns the image the texture was built from
    QImage image() const { return mImage; }

    //! Returns the size of the texture in pixels
    QSize size() const { return mImage.size(); }

    //! Returns TRUE if the texture holds no image
    bool isEmpty() const { return mImage.isNull(); }

    /**
     * Sets whether a mip chain should be generated and used.
     *
     * Mipmaps are worth it for textures that are minified, such as terrain tiles, and are
     * a waste for textures sampled at their native size, such as full screen passes.
     */
    void setMipmapsEnabled( bool enabled );

    //! Returns whether mipmaps are generated for this texture
    bool mipmapsEnabled() const { return mMipmaps; }

    /**
     * Returns the GPU texture, creating and uploading it if needed.
     *
     * Any upload needed is appended to \a updates, which the caller must submit.
     * Returns NULLPTR if the texture is empty or could not be created.
     */
    QRhiTexture *rhiTexture( QRhi *rhi, QRhiResourceUpdateBatch *updates );

    //! Drops the GPU texture, keeping the image. Must be called while the device is alive.
    void releaseGpuResources();

  private:
    QImage mImage;
    bool mMipmaps = false;
    bool mDirty = true;
    std::unique_ptr<QRhiTexture> mTexture;
};

#endif // QGSGPUTEXTURE_H
