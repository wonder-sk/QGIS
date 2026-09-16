/***************************************************************************
  qgsmeshmaterial3d.h
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

#ifndef QGSMESHMATERIAL3D_H
#define QGSMESHMATERIAL3D_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsmaterial3d.h"
#include "qgsvector.h"

#include <QColor>
#include <QPointF>
#include <QSize>

#define SIP_NO_FILE

class QgsColorRampShader;
class QgsGpuTexture;

/**
 * \ingroup qgis_3d
 * \brief The mesh layer material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsMeshBlock in shaders/rhi/qgsmeshmaterial.glsl, which
 * QgsMeshMaterial3D checks once when it is constructed.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsMeshUniforms
{
    float meshColor[4] = {};   // offset 0
    float lineColor[4] = {};   // offset 16
    float arrowsColor[4] = {}; // offset 32

    float arrowsMinCorner[2] = {}; // offset 48
    float arrowsSpacing = 1.0f;    // offset 56
    float lineWidth = 1.0f;        // offset 60

    int textureType = 0;      // offset 64
    int colorRampType = 0;    // offset 68
    int colorRampCount = 0;   // offset 72
    quint32 featureFlags = 0; // offset 76
};

/**
 * \ingroup qgis_3d
 * \brief The material a mesh layer's surface is drawn with, for the QRhi engine.
 *
 * The port of QgsMesh3DMaterial. A fragment takes its colour either from one flat colour
 * or from a colour ramp applied to a magnitude, which is the vertex's own height for
 * terrain and a dataset value otherwise. Over that the material can draw the triangle
 * wireframe, worked out from barycentric coordinates rather than from extra geometry, and
 * a field of arrows sampled from a grid of vectors.
 *
 * Whether the geometry carries a scalar magnitude is baked, because it is a vertex
 * attribute; everything else is a run time flag. The colour ramp, which Qt3D held in a 1D
 * texture that QRhi has no equivalent of, is a float texture one texel high: the break
 * point's value in the red channel and its colour in the other three.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsMeshMaterial3D : public QgsMaterial3D
{
  public:
    //! Sampler bindings, in the order shaders/rhi/mesh.frag declares them
    enum Binding : int
    {
      ColorRampTextureBinding = 4,
      ArrowsGridTextureBinding = 5,
      ArrowTextureBinding = 6,
    };

    //! What a fragment takes its colour from, matching QgsMesh3DSymbol::RenderingStyle
    enum class Style : int
    {
      SingleColor = 0, //!< One colour for the whole surface
      ColorRamp = 1,   //!< The terrain's colour ramp, applied to the magnitude
      ColorRamp2D = 2, //!< The layer's 2D colour ramp, applied to the magnitude
    };

    //! A part of the material that is switched on at run time
    enum class Feature : int
    {
      FlatTriangles = 1 << 0, //!< Shade with each triangle's own normal rather than the interpolated one
      Wireframe = 1 << 1,     //!< Draw the triangle edges over the surface
      Arrows = 1 << 2,        //!< Draw an arrow per grid cell over the surface
    };
    Q_DECLARE_FLAGS( Features, Feature )

    /**
     * Constructs the material for a mesh surface.
     *
     * When \a hasScalarMagnitude is TRUE the colour ramp is applied to a per-vertex
     * "scalarMagnitude" attribute, which the geometry then has to supply; otherwise it is
     * applied to the vertex's height. This cannot be changed afterwards, because it is
     * what picks the baked vertex shader.
     */
    explicit QgsMeshMaterial3D( bool hasScalarMagnitude = false );

    //! Returns TRUE if the magnitude comes from a per-vertex attribute rather than from the height
    bool hasScalarMagnitude() const { return mHasScalarMagnitude; }

    //! Returns which parts of the material are switched on
    Features features() const;

    //! Sets what a fragment takes its colour from
    void setStyle( Style style );

    //! Sets the colour used by Style::SingleColor, which must be an sRGB colour
    void setMeshColor( const QColor &color );

    /**
     * Builds the colour ramp texture from \a shader, multiplying each break point's value
     * by \a valueScale.
     *
     * The scale is what lets a ramp defined in map units follow the symbol's vertical
     * exaggeration when the magnitude is a height; pass 1 when it is a dataset value.
     * The texture is owned by the material.
     */
    void setColorRamp( const QgsColorRampShader &shader, double valueScale = 1.0 );

    //! Sets the colour the wireframe is drawn in, which must be an sRGB colour
    void setWireframeColor( const QColor &color );

    //! Sets how wide the wireframe's lines are, in pixels
    void setWireframeLineWidth( float width );

    //! Sets whether the wireframe is drawn
    void setWireframeEnabled( bool enabled );

    //! Sets whether each triangle is shaded with its own normal
    void setFlatTrianglesEnabled( bool enabled );

    /**
     * Sets the grid of vectors the arrows are drawn from.
     *
     * Each \a vectors entry is one cell of a \a gridSize grid, laid out row by row. When
     * \a fixedSize is TRUE every arrow is drawn at full length, otherwise each is scaled
     * by its own length against \a maxVectorLength. The texture is owned by the material.
     */
    void setArrowsGrid( const QVector<QgsVector> &vectors, const QSize &gridSize, bool fixedSize, double maxVectorLength );

    /**
     * Sets the picture an arrow is drawn with, of which only the alpha channel is used.
     *
     * The texture is not owned by the material and has to outlive it.
     */
    void setArrowTexture( QgsGpuTexture *texture );

    //! Sets the colour the arrows are drawn in, which must be an sRGB colour
    void setArrowsColor( const QColor &color );

    //! Sets where the arrow grid's first cell sits and how far apart its cells are, in map units
    void setArrowsPlacement( const QPointF &minCorner, float spacing );

    //! Sets whether the arrows are drawn
    void setArrowsEnabled( bool enabled );

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsMeshUniforms &uniforms() const { return mUniforms; }

  private:
    void updateUniforms();
    void setFeature( Feature feature, bool enabled );

    bool mHasScalarMagnitude = false;
    QgsMeshUniforms mUniforms;

    /**
     * One white pixel, bound wherever the material has no real texture.
     *
     * Every sampler the shader declares has to have something behind it, whether or not
     * the feature that reads it is switched on.
     */
    std::unique_ptr<QgsGpuTexture> mPlaceholderTexture;
    std::unique_ptr<QgsGpuTexture> mColorRampTexture;
    std::unique_ptr<QgsGpuTexture> mArrowsGridTexture;
};

Q_DECLARE_OPERATORS_FOR_FLAGS( QgsMeshMaterial3D::Features )

#endif // QGSMESHMATERIAL3D_H
