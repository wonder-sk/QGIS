/***************************************************************************
  qgspointcloudmaterial3d.h
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

#ifndef QGSPOINTCLOUDMATERIAL3D_H
#define QGSPOINTCLOUDMATERIAL3D_H

#include <memory>

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsmaterial3d.h"

#include <QColor>

#define SIP_NO_FILE

class QgsColorRampShader;
class QgsGpuTexture;

/**
 * \ingroup qgis_3d
 * \brief The point cloud material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsPointCloudBlock in shaders/rhi/qgspointcloudmaterial.glsl,
 * which QgsPointCloudMaterial3D checks once when it is constructed.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsPointCloudUniforms
{
    float singleColor[4] = {}; // offset 0

    float pointSize = 1.0f; // offset 16
    int style = 0;          // offset 20
    int colorRampType = 0;  // offset 24
    int colorRampCount = 0; // offset 28

    quint32 featureFlags = 0; // offset 32
    float unused0 = 0.0f;     // offset 36
    float unused1 = 0.0f;     // offset 40
    float unused2 = 0.0f;     // offset 44
};

/**
 * \ingroup qgis_3d
 * \brief The material a point cloud chunk is drawn with, for the QRhi engine.
 *
 * The port of what QgsPointCloud3DSymbolHandler::makeEntity() used to assemble by hand
 * out of pointcloud.vert / pointcloud.frag, a QgsMaterial and the symbol's QParameters.
 *
 * Which style a chunk is drawn with is a baked variant rather than a run time flag,
 * because each style reads a different set of vertex attributes - a colour ramp needs a
 * per-point parameter, RGB needs a per-point colour - and a shader input that no attribute
 * feeds leaves the pipeline unbuildable and the chunk undrawn. Whether the chunk was
 * triangulated is baked for the same reason: that is what adds the normal. The styles line
 * up one for one with QgsPointCloudGeometry3D::VertexLayout.
 *
 * Everything else - which colour the single colour style uses, how the ramp interpolates,
 * whether the surface is lit - is a run time flag on the uniform block.
 *
 * \note Points drawn as points get their size from gl_PointSize, as they did under Qt3D.
 * P6 replaces that path with instanced quads, which is also what will make point size
 * work on the backends that ignore it.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsPointCloudMaterial3D : public QgsMaterial3D
{
  public:
    //! The sampler binding the colour ramp is bound at
    static constexpr int ColorRampTextureBinding = 4;

    //! Where a point takes its colour from
    enum class Style : int
    {
      SingleColor = 0,    //!< One colour for every point
      ColorRamp = 1,      //!< A ramp applied to a per-point value
      Rgb = 2,            //!< A colour read straight from the point cloud
      Classification = 3, //!< A ramp indexed by the point's class code
    };

    //! A part of the material that is switched on at run time
    enum class Feature : int
    {
      //! The chunk is a triangulated surface rather than a cloud of points, and so is lit
      Triangulated = 1 << 0,
    };
    Q_DECLARE_FLAGS( Features, Feature )

    /**
     * Constructs the material for a point cloud chunk drawn with \a style.
     *
     * When \a triangulated is TRUE the chunk is drawn as a lit surface of triangles, which
     * needs a per-vertex normal; otherwise it is drawn as points sized by setPointSize().
     * Neither can be changed afterwards, because together they pick the baked shader.
     */
    explicit QgsPointCloudMaterial3D( Style style = Style::SingleColor, bool triangulated = false );

    //! Returns where a point takes its colour from
    Style style() const { return mStyle; }

    //! Returns TRUE if the chunk is drawn as a triangulated surface rather than as points
    bool isTriangulated() const { return mTriangulated; }

    //! Returns which parts of the material are switched on
    Features features() const;

    //! Sets the colour used by Style::SingleColor, which must be an sRGB colour
    void setSingleColor( const QColor &color );

    //! Sets how wide a point is drawn, in pixels. Ignored for a triangulated chunk.
    void setPointSize( float size );

    /**
     * Builds the colour ramp texture from \a shader.
     *
     * Used by Style::ColorRamp, which looks a value up in it, and by
     * Style::Classification, whose class codes index it directly. The texture is owned by
     * the material.
     */
    void setColorRamp( const QgsColorRampShader &shader );

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsPointCloudUniforms &uniforms() const { return mUniforms; }

  private:
    //! Returns the baked shader for the given variant, or NULLPTR if it is missing
    static std::shared_ptr<QgsRhiShader> shaderFor( Style style, bool triangulated );

    void updateUniforms();

    Style mStyle = Style::SingleColor;
    bool mTriangulated = false;
    QgsPointCloudUniforms mUniforms;

    /**
     * One white pixel, bound while the material has no colour ramp.
     *
     * The shader declares the sampler whatever the style is, and every sampler a shader
     * declares has to have something behind it.
     */
    std::unique_ptr<QgsGpuTexture> mPlaceholderTexture;
    std::unique_ptr<QgsGpuTexture> mColorRampTexture;
};

Q_DECLARE_OPERATORS_FOR_FLAGS( QgsPointCloudMaterial3D::Features )

#endif // QGSPOINTCLOUDMATERIAL3D_H
