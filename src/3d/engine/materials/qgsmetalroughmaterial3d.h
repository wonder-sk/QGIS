/***************************************************************************
  qgsmetalroughmaterial3d.h
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

#ifndef QGSMETALROUGHMATERIAL3D_H
#define QGSMETALROUGHMATERIAL3D_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgsgputexture.h"
#include "qgsmaterial3d.h"

#include <QColor>
#include <QMatrix4x4>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief The metal rough material's uniform block, laid out for std140.
 *
 * Must stay in step with QgsMetalRoughBlock in shaders/rhi/qgsmetalroughmaterial.glsl,
 * which QgsMetalRoughMaterial3D checks once when it is constructed.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct _3D_EXPORT QgsMetalRoughUniforms
{
    float baseColor[4] = {};     // offset 0
    float emissiveColor[4] = {}; // offset 16, rgb used

    float metalness = 0.0f;                                                                                                    // offset 32
    float roughness = 0.0f;                                                                                                    // offset 36
    float reflectance = 0.5f;                                                                                                  // offset 40
    float opacity = 1.0f;                                                                                                      // offset 44
    float anisotropy = 0.0f;                                                                                                   // offset 48
    float anisotropyRotation = 0.0f;                                                                                           // offset 52, in radians
    float clearCoatFactor = 0.0f;                                                                                              // offset 56
    float clearCoatRoughness = 0.0f;                                                                                           // offset 60
    float parallaxScale = 0.1f;                                                                                                // offset 64
    float emissiveFactor = 1.0f;                                                                                               // offset 68
    float texCoordScale = 1.0f;                                                                                                // offset 72
    float texCoordRotation = 0.0f;                                                                                             // offset 76
    float texCoordOffset[2] = {};                                                                                              // offset 80
    quint32 featureFlags = 0;                                                                                                  // offset 88
    float unused0 = 0.0f;                                                                                                      // offset 92
    float meshMatrix[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }; // offset 96, identity by default
    float meshNormalMatrix[12] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };                   // offset 160, a mat3 as three padded columns
    float symbolRotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };                                                                      // offset 208, no rotation
    float symbolScale[4] = { 1.0f, 1.0f, 1.0f, 0.0f };                                                                         // offset 224, unit scale
};

/**
 * \ingroup qgis_3d
 * \brief Physically based metal rough shading for the QRhi engine.
 *
 * The port of QgsMetalRoughMaterial, and the one material whose variants could not simply
 * be spelled out. Its Qt3D fragment shader was preprocessed with eleven independent
 * booleans - one per texture map, plus flat shading, anisotropy and the clear coat - which
 * is over two thousand combinations. They are run time flags on the uniform block here
 * instead, branched on inside a single shader. The branches take the same path for every
 * fragment of a draw, so they cost little, and a material can no longer be configured into
 * a combination that was never baked.
 *
 * What stayed a baked variant is anything that changes which vertex attributes are read,
 * because a shader input that no attribute feeds makes the graphics pipeline fail to build
 * and nothing is drawn at all. That is what mode() and vertexAttributes() select between:
 * the caller states what its geometry actually supplies.
 *
 * Every sampler the shader declares has to be bound whether the material uses it or not,
 * so the material keeps a one pixel white texture and binds it to each map it has none for.
 *
 * Image based lighting is deliberately not ported. It needs the environment light's cube
 * map and spherical harmonics, which reach the shader as scene wide state rather than as
 * material state, and neither a cube map texture nor a per-pass environment block exists
 * in the engine yet.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsMetalRoughMaterial3D : public QgsMaterial3D
{
  public:
    /**
     * Where the material's per-fragment inputs come from.
     *
     * These are mutually exclusive because each reads a different set of vertex
     * attributes, and so needs a different baked shader.
     */
    enum class Mode : int
    {
      Uniform,                     //!< Colours and the texture transform come from the material
      DataDefinedColors,           //!< Base and emission colour come from per-vertex attributes
      DataDefinedTextureTransform, //!< The texture transform comes from a per-vertex attribute
      Instanced,                   //!< The geometry is drawn once per instance
    };

    //! An optional vertex attribute the geometry drawn with this material supplies
    enum class VertexAttribute : int
    {
      TextureCoords = 1 << 0, //!< A "vertexTexCoord" attribute, needed by every map
      Tangents = 1 << 1,      //!< A "vertexTangent" attribute, which improves normal and height mapping
    };
    Q_DECLARE_FLAGS( VertexAttributes, VertexAttribute )

    //! A part of the shading model that is switched on at run time rather than baked
    enum class Feature : int
    {
      BaseColorMap = 1 << 0,        //!< The base colour is sampled rather than uniform
      MetalnessMap = 1 << 1,        //!< Metalness is sampled rather than uniform
      RoughnessMap = 1 << 2,        //!< Roughness is sampled rather than uniform
      AmbientOcclusionMap = 1 << 3, //!< Ambient occlusion is sampled
      NormalMap = 1 << 4,           //!< The normal is sampled in tangent space
      HeightMap = 1 << 5,           //!< Texture coordinates are parallax offset by a height map
      EmissionMap = 1 << 6,         //!< Emission is sampled rather than uniform
      FlatShading = 1 << 7,         //!< The normal is derived per fragment rather than interpolated
      Anisotropy = 1 << 8,          //!< The specular highlight is stretched along the tangent
      ClearCoat = 1 << 9,           //!< A second specular layer is added over the base one
    };
    Q_DECLARE_FLAGS( Features, Feature )

    //! Sampler bindings, in the order shaders/rhi/metalrough.frag declares them
    enum Binding : int
    {
      BaseColorMapBinding = 4,
      MetalnessMapBinding = 5,
      RoughnessMapBinding = 6,
      AmbientOcclusionMapBinding = 7,
      NormalMapBinding = 8,
      HeightMapBinding = 9,
      EmissionMapBinding = 10,
    };

    /**
     * Constructs a metal rough material.
     *
     * \a mode and \a attributes together say what the geometry supplies, and so which
     * baked shader is used; they cannot be changed afterwards. \a instanceFlags only
     * applies to Mode::Instanced, and says which of scale and rotation each instance
     * carries; whatever an instance does not carry falls back to setSymbolScale() and
     * setSymbolRotation().
     */
    explicit QgsMetalRoughMaterial3D( Mode mode = Mode::Uniform, VertexAttributes attributes = VertexAttributes(), Qgis::InstancedMaterialFlags instanceFlags = Qgis::InstancedMaterialFlags() );

    //! Returns where the material's per-fragment inputs come from
    Mode mode() const { return mMode; }

    //! Returns which optional vertex attributes the geometry supplies
    VertexAttributes vertexAttributes() const { return mVertexAttributes; }

    //! Returns which parts of the shading model are switched on
    Features features() const;

    //! Sets the base colour, which must be an sRGB colour. Ignored when a base colour map is set.
    void setBaseColor( const QColor &color );

    /**
     * Sets the texture the base colour is sampled from, or NULLPTR to use setBaseColor().
     *
     * No texture passed to this material is owned by it, and all of them have to outlive it.
     */
    void setBaseColorTexture( QgsGpuTexture *texture );

    //! Sets the constant metalness, between 0 and 1. Ignored when a metalness map is set.
    void setMetalness( float metalness );

    //! Sets the texture metalness is sampled from, or NULLPTR to use setMetalness()
    void setMetalnessTexture( QgsGpuTexture *texture );

    //! Sets the constant roughness, between 0 and 1. Ignored when a roughness map is set.
    void setRoughness( float roughness );

    //! Sets the texture roughness is sampled from, or NULLPTR to use setRoughness()
    void setRoughnessTexture( QgsGpuTexture *texture );

    //! Sets the reflectance of the dielectric part of the surface, between 0 and 1
    void setReflectance( float reflectance );

    //! Sets how far the specular highlight is stretched, between 0 and 1. Zero switches it off.
    void setAnisotropy( float anisotropy );

    //! Sets which way the stretched specular highlight points, in degrees
    void setAnisotropyRotation( float rotation );

    //! Sets the texture ambient occlusion is sampled from, or NULLPTR for none
    void setAmbientOcclusionTexture( QgsGpuTexture *texture );

    //! Sets the tangent space normal map, or NULLPTR for none
    void setNormalTexture( QgsGpuTexture *texture );

    //! Sets the height map used for parallax mapping, or NULLPTR for none
    void setHeightTexture( QgsGpuTexture *texture );

    //! Sets how far the height map displaces the texture coordinates
    void setParallaxScale( float scale );

    //! Sets the texture emission is sampled from, or NULLPTR to use setEmissionColor()
    void setEmissionTexture( QgsGpuTexture *texture );

    //! Sets the solid emissive colour, which must be an sRGB colour
    void setEmissionColor( const QColor &color );

    //! Sets the factor the emission is multiplied by
    void setEmissionFactor( float factor );

    //! Sets how strong the clear coat layer is, between 0 and 1. Zero switches it off.
    void setClearCoatFactor( float factor );

    //! Sets how rough the clear coat layer is, between 0 and 1
    void setClearCoatRoughness( float roughness );

    //! Sets whether the normal is derived per fragment instead of interpolated
    void setFlatShadingEnabled( bool enabled );

    //! Sets the opacity of the shaded colour
    void setOpacity( float opacity );

    //! Sets the factor the texture coordinates are scaled by about the texture offset
    void setTextureScale( float scale );

    //! Sets how far the texture coordinates are rotated about the texture offset, in degrees
    void setTextureRotation( float rotation );

    //! Sets the point in texture space the scale and the rotation are applied about
    void setTextureOffset( float offsetX, float offsetY );

    /**
     * Sets the transform from mesh space, the raw input vertex coordinates, to object space.
     *
     * Only has an effect in Mode::Instanced.
     */
    void setMeshTransform( const QMatrix4x4 &transform );

    /**
     * Sets the scale used for instances that do not carry their own.
     *
     * Only has an effect in Mode::Instanced, and when instanceFlags did not include
     * Qgis::InstancedMaterialFlag::DataDefinedScale.
     */
    void setSymbolScale( const QVector3D &scale );

    /**
     * Sets the rotation quaternion used for instances that do not carry their own.
     *
     * Only has an effect in Mode::Instanced, and when instanceFlags did not include
     * Qgis::InstancedMaterialFlag::DataDefinedRotation.
     */
    void setSymbolRotation( const QQuaternion &rotation );

    //! Returns the current uniform values, mostly so that tests can check them
    const QgsMetalRoughUniforms &uniforms() const { return mUniforms; }

    //! Returns the name of the baked shader variant \a mode and \a attributes select
    static QString shaderVariant( Mode mode, VertexAttributes attributes, Qgis::InstancedMaterialFlags instanceFlags );

  private:
    //! Returns the baked shader for the given variant, or NULLPTR if it is missing
    static std::shared_ptr<QgsRhiShader> shaderFor( Mode mode, VertexAttributes attributes, Qgis::InstancedMaterialFlags instanceFlags );

    void updateUniforms();

    /**
     * Binds \a texture at \a binding and turns \a feature on, or binds the placeholder
     * and turns it off when \a texture is NULLPTR.
     */
    void setMapTexture( Binding binding, Feature feature, QgsGpuTexture *texture );

    void setFeature( Feature feature, bool enabled );

    Mode mMode = Mode::Uniform;
    VertexAttributes mVertexAttributes;
    QgsMetalRoughUniforms mUniforms;

    /**
     * One white pixel, bound wherever the material has no real map.
     *
     * Every sampler the shader declares has to have something behind it, and a texture
     * this small costs nothing next to having to bake a shader per combination of maps.
     */
    QgsGpuTexture mPlaceholderTexture;
};

Q_DECLARE_OPERATORS_FOR_FLAGS( QgsMetalRoughMaterial3D::VertexAttributes )
Q_DECLARE_OPERATORS_FOR_FLAGS( QgsMetalRoughMaterial3D::Features )

#endif // QGSMETALROUGHMATERIAL3D_H
