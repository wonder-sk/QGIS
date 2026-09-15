/***************************************************************************
  qgsgraphicspipelinecache.h
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

#ifndef QGSGRAPHICSPIPELINECACHE_H
#define QGSGRAPHICSPIPELINECACHE_H

#include <rhi/qrhi.h>

#include "qgis_3d.h"

#include <QHash>
#include <QVector>

#define SIP_NO_FILE

class QgsGeometry3D;
class QgsMaterial3D;

/**
 * \ingroup qgis_3d
 * \brief Keeps graphics pipelines, shader resource binding sets and samplers alive and shared.
 *
 * Building a graphics pipeline means compiling and linking shaders for the target API, so
 * a scene that built one per draw would be unusable. Everything here is keyed on what the
 * object actually depends on, so a few hundred chunks drawn with the same symbol share a
 * single pipeline.
 *
 * The cache owns QRhi resources, so it must be destroyed before the QgsRhiEngine that
 * created them. Declaring it after the engine in the owning class is enough, since members
 * are destroyed in reverse order.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsGraphicsPipelineCache
{
  public:
    /**
     * \ingroup qgis_3d
     * \brief A pipeline together with the attribute mapping it was built for.
     */
    struct Pipeline
    {
        //! The pipeline to draw with
        QRhiGraphicsPipeline *pipeline = nullptr;

        /**
       * Indexes into QgsGeometry3D::attributes(), in vertex input binding order.
       *
       * Only attributes the vertex shader actually reads appear here, so the same geometry
       * can be drawn by a full shading pass and by a position-only depth pass.
       */
        QVector<int> attributeIndexes;
    };

    QgsGraphicsPipelineCache();
    ~QgsGraphicsPipelineCache();

    QgsGraphicsPipelineCache( const QgsGraphicsPipelineCache & ) = delete;
    QgsGraphicsPipelineCache &operator=( const QgsGraphicsPipelineCache & ) = delete;

    /**
     * Returns the pipeline for drawing \a geometry with \a material into a render target
     * described by \a renderPassDescriptor, building it if this is the first time.
     *
     * \a srb must be a binding set laid out the way the draw will bind it; the pipeline
     * only takes its layout from it, so any material using the same shader will do.
     * Returns NULLPTR if the pipeline could not be built.
     */
    const Pipeline *pipeline( QRhi *rhi, const QgsMaterial3D *material, const QgsGeometry3D *geometry, QRhiShaderResourceBindings *srb, QRhiRenderPassDescriptor *renderPassDescriptor, int sampleCount );

    /**
     * Returns a shader resource binding set holding exactly \a bindings, building it if
     * this is the first time. Returns NULLPTR if it could not be built.
     */
    QRhiShaderResourceBindings *shaderResourceBindings( QRhi *rhi, const QVector<QRhiShaderResourceBinding> &bindings );

    //! Returns a sampler with the given filtering and wrapping, building it if needed
    QRhiSampler *sampler( QRhi *rhi, QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter, QRhiSampler::Filter mipmapMode, QRhiSampler::AddressMode addressU, QRhiSampler::AddressMode addressV );

    //! Destroys everything cached. Must be called while the device is still alive.
    void clear();

    //! Returns how many pipelines are currently cached, for diagnostics and tests
    int pipelineCount() const { return static_cast<int>( mPipelines.size() ); }

    //! Returns how many binding sets are currently cached, for diagnostics and tests
    int shaderResourceBindingsCount() const { return static_cast<int>( mShaderResourceBindings.size() ); }

  private:
    QHash<size_t, Pipeline> mPipelines;
    QHash<size_t, QRhiShaderResourceBindings *> mShaderResourceBindings;
    QHash<size_t, QRhiSampler *> mSamplers;
};

#endif // QGSGRAPHICSPIPELINECACHE_H
