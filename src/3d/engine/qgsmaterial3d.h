/***************************************************************************
  qgsmaterial3d.h
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

#ifndef QGSMATERIAL3D_H
#define QGSMATERIAL3D_H

#include <memory>
#include <rhi/qrhi.h>

#include "qgis_3d.h"
#include "qgsgpubuffer.h"
#include "qgsrhishader.h"

#include <QVector>

#define SIP_NO_FILE

class QgsGpuTexture;
class QgsGraphicsPipelineCache;

/**
 * \ingroup qgis_3d
 * \brief What to draw something with: a shader, some pipeline state, uniforms and textures.
 *
 * This replaces Qt3D's QMaterial / QEffect / QTechnique / QRenderPass stack, which QGIS
 * only ever used to say those four things. There is no technique matching here: a material
 * has exactly one shader, and if the shader is missing that is an error at load time rather
 * than an entity that silently fails to draw.
 *
 * Subclasses are expected to own a plain C++ struct matching their GLSL uniform block, fill
 * it, and hand it to setUniformData(). QgsRhiShader::checkUniformBlockLayout() verifies the
 * two agree.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsMaterial3D
{
  public:
    //! A texture bound to one of the material's sampler bindings
    struct TextureBinding
    {
        //! Binding index, at or above QgsRhiBinding::FirstSampler
        int binding = 0;
        //! Texture to sample. Not owned by the material.
        QgsGpuTexture *texture = nullptr;
        //! Filter used when the texture is magnified
        QRhiSampler::Filter magFilter = QRhiSampler::Linear;
        //! Filter used when the texture is minified
        QRhiSampler::Filter minFilter = QRhiSampler::Linear;
        //! How to filter between mip levels, or QRhiSampler::None for no mipmapping
        QRhiSampler::Filter mipmapMode = QRhiSampler::None;
        //! Wrapping mode along the horizontal axis
        QRhiSampler::AddressMode addressU = QRhiSampler::ClampToEdge;
        //! Wrapping mode along the vertical axis
        QRhiSampler::AddressMode addressV = QRhiSampler::ClampToEdge;
    };

    //! Constructs a material drawing with \a shader, which must not be NULLPTR
    explicit QgsMaterial3D( std::shared_ptr<QgsRhiShader> shader );
    virtual ~QgsMaterial3D();

    QgsMaterial3D( const QgsMaterial3D & ) = delete;
    QgsMaterial3D &operator=( const QgsMaterial3D & ) = delete;

    //! Returns the shader the material draws with
    QgsRhiShader *shader() const { return mShader.get(); }

    //! Returns the primitive topology the material draws with
    QRhiGraphicsPipeline::Topology topology() const { return mTopology; }
    //! Sets the primitive topology the material draws with
    void setTopology( QRhiGraphicsPipeline::Topology topology );

    //! Returns which faces are culled
    QRhiGraphicsPipeline::CullMode cullMode() const { return mCullMode; }
    //! Sets which faces are culled
    void setCullMode( QRhiGraphicsPipeline::CullMode mode );

    //! Returns which winding order counts as front facing
    QRhiGraphicsPipeline::FrontFace frontFace() const { return mFrontFace; }
    //! Sets which winding order counts as front facing
    void setFrontFace( QRhiGraphicsPipeline::FrontFace face );

    //! Returns whether fragments are depth tested
    bool hasDepthTest() const { return mDepthTest; }
    //! Sets whether fragments are depth tested
    void setDepthTest( bool enabled );

    //! Returns whether fragments write to the depth buffer
    bool hasDepthWrite() const { return mDepthWrite; }
    //! Sets whether fragments write to the depth buffer
    void setDepthWrite( bool enabled );

    //! Returns the depth comparison used when depth testing
    QRhiGraphicsPipeline::CompareOp depthOp() const { return mDepthOp; }
    //! Sets the depth comparison used when depth testing
    void setDepthOp( QRhiGraphicsPipeline::CompareOp op );

    //! Returns the colour blending configuration
    QRhiGraphicsPipeline::TargetBlend blend() const { return mBlend; }
    //! Sets the colour blending configuration
    void setBlend( const QRhiGraphicsPipeline::TargetBlend &blend );

    //! Returns whether polygons are filled or drawn as lines
    QRhiGraphicsPipeline::PolygonMode polygonMode() const { return mPolygonMode; }
    //! Sets whether polygons are filled or drawn as lines
    void setPolygonMode( QRhiGraphicsPipeline::PolygonMode mode );

    //! Returns the constant depth offset applied to fragments, in depth buffer units
    int depthBias() const { return mDepthBias; }

    /**
     * Sets a constant and a slope scaled offset added to every fragment's depth.
     *
     * This is what Qt3D's QPolygonOffset was. The shadow pass uses it to push shadow
     * casters away from the light, so that a surface does not shadow itself.
     */
    void setDepthBias( int constant, float slopeScaled );

    //! Returns the depth offset applied in proportion to the polygon's slope
    float slopeScaledDepthBias() const { return mSlopeScaledDepthBias; }

    /**
     * Replaces the content of the material's uniform block.
     *
     * The data is expected to match the block the shader declares at
     * QgsRhiBinding::Material; use QgsRhiShader::checkUniformBlockLayout() to confirm that
     * once, when the material is set up.
     */
    void setUniformData( const void *data, int size );

    //! Convenience overload of setUniformData() taking the material's uniform struct
    template<typename T> void setUniforms( const T &uniforms ) { setUniformData( &uniforms, static_cast<int>( sizeof( T ) ) ); }

    //! Returns the content of the material's uniform block
    const QByteArray &uniformData() const { return mUniformBuffer.data(); }

    /**
     * Binds \a texture at the given sampler binding, replacing whatever was bound there.
     *
     * Passing a NULLPTR texture removes the binding.
     */
    void setTexture( const TextureBinding &texture );

    //! Returns the material's texture bindings, in binding order
    const QVector<TextureBinding> &textures() const { return mTextures; }

    /**
     * Returns a value identifying the pipeline state of this material.
     *
     * Two materials with the same shader and the same state key can share a graphics
     * pipeline. Textures and uniform values are deliberately not part of the key, since
     * they do not affect the pipeline.
     */
    size_t stateKey() const;

    /**
     * Appends the material's own shader resource bindings, those from
     * QgsRhiBinding::Material upwards, to \a bindings.
     *
     * Any upload the uniform buffer or the textures need is queued on \a updates.
     * Returns FALSE if a bound texture could not be uploaded.
     */
    bool appendResourceBindings( QRhi *rhi, QRhiResourceUpdateBatch *updates, QgsGraphicsPipelineCache *cache, QVector<QRhiShaderResourceBinding> *bindings );

    //! Drops every GPU resource the material owns. Must be called while the device is alive.
    void releaseGpuResources();

  private:
    std::shared_ptr<QgsRhiShader> mShader;

    QRhiGraphicsPipeline::Topology mTopology = QRhiGraphicsPipeline::Triangles;
    QRhiGraphicsPipeline::CullMode mCullMode = QRhiGraphicsPipeline::Back;
    QRhiGraphicsPipeline::FrontFace mFrontFace = QRhiGraphicsPipeline::CCW;
    bool mDepthTest = true;
    bool mDepthWrite = true;
    QRhiGraphicsPipeline::CompareOp mDepthOp = QRhiGraphicsPipeline::Less;
    QRhiGraphicsPipeline::TargetBlend mBlend;
    QRhiGraphicsPipeline::PolygonMode mPolygonMode = QRhiGraphicsPipeline::Fill;
    int mDepthBias = 0;
    float mSlopeScaledDepthBias = 0.0f;

    QgsGpuBuffer mUniformBuffer { QgsGpuBuffer::Role::Uniform, QgsGpuBuffer::Usage::Dynamic };
    QVector<TextureBinding> mTextures;

    mutable size_t mStateKey = 0;
};

#endif // QGSMATERIAL3D_H
