/***************************************************************************
  qgsrenderpass.h
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

#ifndef QGSRENDERPASS_H
#define QGSRENDERPASS_H

#include <rhi/qrhi.h>
#include <vector>

#include "qgis_3d.h"
#include "qgsgpubuffer.h"
#include "qgsrendercommand.h"
#include "qgsrenderuniforms.h"
#include "qgsuniformring.h"

#include <QColor>
#include <QMatrix4x4>
#include <QString>

#define SIP_NO_FILE

class QgsGraphicsPipelineCache;
class QgsRhiEngine;

/**
 * \ingroup qgis_3d
 * \brief One QRhi render pass: a target, a camera, and an array of things to draw.
 *
 * This replaces the part of Qt3D's frame graph QGIS actually used. A frame is a sequence of
 * passes executed in order, written as plain code, so the order things happen in is the
 * order they appear in the source rather than a property of a node tree.
 *
 * A pass owns the uniform buffer holding its camera and the per-frame ring buffer holding
 * one transform block per command, so nothing outside needs to know how uniforms are fed.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsRenderPass
{
  public:
    //! Constructs a pass called \a name, which is used for GPU debug markers
    explicit QgsRenderPass( const QString &name = QString() );
    ~QgsRenderPass();

    QgsRenderPass( const QgsRenderPass & ) = delete;
    QgsRenderPass &operator=( const QgsRenderPass & ) = delete;

    //! Returns the pass name, as it appears in GPU debug captures
    QString name() const { return mName; }

    /**
     * Sets the camera the pass renders from.
     *
     * \a projection is the plain projection matrix; the clip space correction the backend
     * needs is applied by the pass, so callers never have to think about whether the
     * current API puts Y up or down or maps depth to [0,1] or [-1,1].
     */
    void setCamera( const QMatrix4x4 &view, const QMatrix4x4 &projection );

    //! Sets the eye position in world coordinates, as lighting shaders expect it
    void setEyePosition( const QVector3D &position );

    //! Returns the view matrix set by setCamera()
    QMatrix4x4 viewMatrix() const { return mView; }

    //! Returns the projection matrix set by setCamera(), without clip space correction
    QMatrix4x4 projectionMatrix() const { return mProjection; }

    //! Sets whether the colour attachment is cleared at the start of the pass, and to what
    void setClearColor( const QColor &color ) { mClearColor = color; }

    //! Returns the colour the pass clears to
    QColor clearColor() const { return mClearColor; }

    //! Sets the value the depth buffer is cleared to at the start of the pass
    void setDepthClearValue( float depth ) { mDepthClearValue = depth; }

    //! Appends a command to draw this frame
    void addCommand( const QgsRenderCommand &command ) { mCommands.push_back( command ); }

    //! Removes every command. Called at the start of each frame, before the scene is walked.
    void clearCommands() { mCommands.clear(); }

    //! Returns the commands appended so far
    const std::vector<QgsRenderCommand> &commands() const { return mCommands; }

    /**
     * Sets whether commands are sorted by QgsRenderCommand::sortKey before drawing.
     *
     * On by default. Turn it off for passes whose order is already meaningful, such as a
     * chain of full screen post processing quads.
     */
    void setSortEnabled( bool enabled ) { mSortEnabled = enabled; }

    /**
     * Records the pass into the engine's current command buffer, drawing into \a target.
     *
     * Must be called between QgsRhiEngine::beginFrame() and QgsRhiEngine::endFrame().
     * Passing NULLPTR for \a target draws into the engine's own output.
     * Returns FALSE if a resource the pass needed could not be built, in which case
     * nothing was drawn.
     */
    bool execute( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, QRhiRenderTarget *target = nullptr );

    //! Drops the GPU resources the pass owns. Must be called while the device is alive.
    void releaseGpuResources();

  private:
    //! A command with everything needed to record it resolved
    struct ResolvedCommand
    {
        QRhiGraphicsPipeline *pipeline = nullptr;
        QRhiShaderResourceBindings *shaderResourceBindings = nullptr;
        quint32 drawUniformOffset = 0;
        QVarLengthArray<QRhiCommandBuffer::VertexInput, 8> vertexInputs;
        QRhiBuffer *indexBuffer = nullptr;
        QRhiCommandBuffer::IndexFormat indexFormat = QRhiCommandBuffer::IndexUInt32;
        quint32 elementCount = 0;
        quint32 instanceCount = 1;
        quint32 firstElement = 0;
    };

    QString mName;
    QMatrix4x4 mView;
    QMatrix4x4 mProjection;
    QVector3D mEyePosition;
    QColor mClearColor = Qt::black;
    float mDepthClearValue = 1.0f;
    bool mSortEnabled = true;

    std::vector<QgsRenderCommand> mCommands;
    std::vector<ResolvedCommand> mResolved;

    QgsGpuBuffer mViewBuffer { QgsGpuBuffer::Role::Uniform, QgsGpuBuffer::Usage::Dynamic };
    QgsUniformRing mDrawUniforms;
};

#endif // QGSRENDERPASS_H
