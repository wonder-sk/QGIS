/***************************************************************************
  qgsbloommaterial3d.h
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

#ifndef QGSBLOOMMATERIAL3D_H
#define QGSBLOOMMATERIAL3D_H

#include "qgis_3d.h"
#include "qgsgputexture.h"
#include "qgsmaterial3d.h"

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief The bloom material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsBloomBlock in shaders/rhi/qgsbloommaterial.glsl.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsBloomUniforms
{
    float filterRadius = 0.005f; // offset 0
    float aspectRatio = 1.0f;    // offset 4
};

/**
 * \ingroup qgis_3d
 * \brief One step of the bloom mip chain, either halving the image or blending it back up.
 *
 * The port of QgsBloomDownsampleEntity and QgsBloomUpsampleEntity, which differed only in
 * which fragment shader they loaded and whether they blended, so they are one class here.
 *
 * Which step this is has to be fixed at construction because it picks a shader, but since
 * neither shader reads any vertex attribute the other does not, that is a matter of taste
 * rather than of the attribute set rule the scene materials live by.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsBloomMaterial3D : public QgsMaterial3D
{
  public:
    //! Which direction along the mip chain this material takes
    enum class Step
    {
      //! Halves the image with a 13 tap filter, writing over whatever was there
      Downsample,

      //! Blurs the image with a 9 tap tent filter and adds it to the level above
      Upsample,
    };

    explicit QgsBloomMaterial3D( Step step );

    //! Returns which direction along the mip chain this material takes
    Step step() const { return mStep; }

    //! Sets the mip level this step reads from
    void setSourceTexture( QRhiTexture *texture );

    //! Sets how far the upsampling tent filter reaches, in texture coordinates
    void setFilterRadius( float radius );

    //! Sets the aspect ratio of the view, which keeps the filter round rather than oval
    void setAspectRatio( float ratio );

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsBloomUniforms &uniforms() const { return mUniforms; }

  private:
    Step mStep;
    QgsBloomUniforms mUniforms;
    QgsGpuTexture mSourceTexture;
};

#endif // QGSBLOOMMATERIAL3D_H
