/***************************************************************************
  qgspostprocessingmaterial3d.h
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

#ifndef QGSPOSTPROCESSINGMATERIAL3D_H
#define QGSPOSTPROCESSINGMATERIAL3D_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsgputexture.h"
#include "qgsmaterial3d.h"

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief The post processing material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsPostprocessingBlock in
 * shaders/rhi/qgspostprocessingmaterial.glsl.
 *
 * Everything here is a run time flag rather than a baked shader variant, following the
 * rule the scene materials settled on: only something that changes which vertex attributes
 * are read has to be baked, and a full screen quad always has exactly one.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsPostprocessingUniforms
{
    int edlEnabled = 0;              // offset 0
    float edlStrength = 1000.0f;     // offset 4
    float edlDistance = 2.0f;        // offset 8
    int ambientOcclusionEnabled = 0; // offset 12

    int bloomEnabled = 0;            // offset 16
    float bloomFactor = 0.05f;       // offset 20
    float exposureAdjustment = 0.0f; // offset 24
    int toneMapping = 0;             // offset 28
};

/**
 * \ingroup qgis_3d
 * \brief Turns the forward pass's HDR output into the image the user sees.
 *
 * The port of QgsPostprocessingEntity. It applies eye dome lighting, ambient occlusion and
 * bloom, then exposure, tone mapping and gamma.
 *
 * Shadows are the one thing it no longer does: in both the Qt3D version and this one they
 * are resolved in the lighting shaders, and the only shadow related thing left here is the
 * cascade tint used when testing.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsPostprocessingMaterial3D : public QgsMaterial3D
{
  public:
    QgsPostprocessingMaterial3D();

    //! Sets the colour the forward pass rendered
    void setColorTexture( QRhiTexture *texture );

    //! Sets the depth the forward pass rendered, which eye dome lighting reads
    void setDepthTexture( QRhiTexture *texture );

    //! Sets the blurred occlusion factor the ambient occlusion pass produced
    void setAmbientOcclusionTexture( QRhiTexture *texture );

    //! Sets the halo the bloom pass produced
    void setBloomTexture( QRhiTexture *texture );

    //! Sets whether eye dome lighting shades pixels by how much nearer they are than their neighbours
    void setEyeDomeLightingEnabled( bool enabled );

    //! Sets how strongly eye dome lighting darkens an edge
    void setEyeDomeLightingStrength( float strength );

    //! Sets how far from a pixel eye dome lighting looks for a depth difference, in pixels
    void setEyeDomeLightingDistance( float distance );

    //! Sets whether the ambient occlusion texture is applied
    void setAmbientOcclusionEnabled( bool enabled );

    //! Sets whether the bloom texture is added
    void setBloomEnabled( bool enabled );

    //! Sets how much of the bloom texture is added
    void setBloomFactor( float factor );

    //! Sets the exposure adjustment in stops, applied before tone mapping
    void setExposureAdjustment( float stops );

    //! Sets which tone mapping curve brings the image into displayable range
    void setToneMapping( Qgis::ToneMappingMethod method );

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsPostprocessingUniforms &uniforms() const { return mUniforms; }

  private:
    void setPassTexture( int binding, QgsGpuTexture *holder, QRhiTexture *texture );

    QgsPostprocessingUniforms mUniforms;
    QgsGpuTexture mColorTexture;
    QgsGpuTexture mDepthTexture;
    QgsGpuTexture mAmbientOcclusionTexture;
    QgsGpuTexture mBloomTexture;
};

#endif // QGSPOSTPROCESSINGMATERIAL3D_H
