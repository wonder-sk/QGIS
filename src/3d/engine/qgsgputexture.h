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

    //! How one texel is stored
    enum class Format
    {
      //! Eight bits per channel, what an ordinary image holds
      RGBA8,

      /**
       * A float per channel, for textures that carry values rather than colours, such as
       * a colour ramp's breakpoints or a grid of vectors. QRhi has no two channel float
       * format, so a texture that only needs two of the four still uses this one.
       */
      RGBA32F,
    };

    /**
     * Sets the image the texture holds, and marks it for re-upload.
     *
     * The image is converted to RGBA8888 if it is in some other format, because that is
     * the one colour format every QRhi backend is required to support.
     */
    void setImage( const QImage &image );

    /**
     * Sets raw texel data of \a size in \a format, and marks it for re-upload.
     *
     * \a data has to hold exactly one texel per pixel, tightly packed, row by row from
     * the top. This is how a texture that is not a picture is built: the mesh layer's
     * colour ramp, for instance, is a row of floats rather than anything displayable.
     */
    void setData( const QByteArray &data, const QSize &size, Format format );

    //! Returns the image the texture was built from, which is null when setData() was used
    QImage image() const { return mImage; }

    //! Returns how one texel is stored
    Format format() const { return mFormat; }

    //! Returns the size of the texture in pixels
    QSize size() const { return mSize; }

    //! Returns TRUE if the texture holds nothing to upload and stands for nothing external
    bool isEmpty() const { return !mExternalTexture && mSize.isEmpty(); }

    /**
     * Makes this stand for \a texture, which somebody else owns and keeps alive.
     *
     * This is how a material samples the output of an earlier pass: the post processing
     * chain binds render target attachments, which live on the GPU only and have no CPU
     * side image to upload. Everything about the CPU side is ignored while an external
     * texture is set; passing NULLPTR goes back to the ordinary uploaded form.
     */
    void setExternalTexture( QRhiTexture *texture );

    //! Returns the texture set by setExternalTexture(), or NULLPTR if there is none
    QRhiTexture *externalTexture() const { return mExternalTexture; }

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
     * Sets whether the texels hold sRGB encoded colours rather than linear values.
     *
     * Shading is done in linear space, so a texture that carries a picture - a terrain
     * tile, a glTF base colour map - has to be decoded when it is sampled, which the
     * hardware does for free given an sRGB format. A texture that carries values rather
     * than colours, such as a colour ramp read with texelFetch() or a grid of vectors,
     * must not be, and neither must one that is already linear.
     */
    void setSrgb( bool srgb );

    //! Returns whether the texels hold sRGB encoded colours
    bool isSrgb() const { return mSrgb; }

    /**
     * Returns TRUE if a material sampling this texture has to convert it to linear itself.
     *
     * An sRGB texture is decoded by the hardware on every backend but OpenGL, where
     * QRhi ignores the flag for uncompressed formats - see
     * QgsRhiEngine::decodesSrgbTextures(). A material that samples colours asks this and
     * switches its shader's conversion on, so that the same texture looks the same
     * whichever backend the view is running on.
     */
    bool needsShaderSrgbDecode() const;

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
    QByteArray mData;
    QSize mSize;
    Format mFormat = Format::RGBA8;
    bool mMipmaps = false;
    bool mSrgb = false;
    bool mDirty = true;
    std::unique_ptr<QRhiTexture> mTexture;
    QRhiTexture *mExternalTexture = nullptr;
};

#endif // QGSGPUTEXTURE_H
