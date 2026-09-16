/***************************************************************************
  qgshighlightsrenderpass.h
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

#ifndef QGSHIGHLIGHTSRENDERPASS_H
#define QGSHIGHLIGHTSRENDERPASS_H

#include "qgis_3d.h"
#include "qgsgpurendertarget.h"
#include "qgshighlightmaterial3d.h"
#include "qgsrenderpass.h"

#include <QString>

#define SIP_NO_FILE

using namespace Qt::StringLiterals;


class QgsGraphicsPipelineCache;
class QgsRhiEngine;
class QgsScreenQuad;

/**
 * \ingroup qgis_3d
 * \brief Draws identified features as a translucent fill with a band around them.
 *
 * The port of QgsHighlightsRenderView, in two steps:
 *
 * - the fill, drawn straight over the scene by whatever material the highlight handler
 *   built for it, through fillPass();
 * - the band, which needs to know where the fill went. Its geometry goes to maskPass(),
 *   which renders coverage into a small texture, and a full screen pass then grows a band
 *   out of it.
 *
 * Qt3D got the band by drawing the geometry four more times, each a few pixels off, with
 * a stencil test keeping only what fell outside the original. Under QRhi a render target
 * cannot have both a stencil buffer and a depth texture the post processing chain can
 * sample - on the Metal backend a depth texture carries no stencil aspect at all - so the
 * mask replaces it. It is also cheaper: one small pass over the geometry and one full
 * screen quad, rather than four passes over the geometry.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsHighlightsRenderPass
{
  public:
    QgsHighlightsRenderPass();
    ~QgsHighlightsRenderPass();

    QgsHighlightsRenderPass( const QgsHighlightsRenderPass & ) = delete;
    QgsHighlightsRenderPass &operator=( const QgsHighlightsRenderPass & ) = delete;

    //! Creates the coverage mask at \a size. Returns TRUE on success.
    bool prepare( QRhi *rhi, QSize size );

    //! Drops every GPU resource. Must be called while the device is still alive.
    void releaseGpuResources();

    //! Returns the pass the translucent fill is drawn in, over the scene
    QgsRenderPass &fillPass() { return mFillPass; }

    /**
     * Returns the pass that marks where the highlighted features are.
     *
     * Its commands should be the same geometry as fillPass()'s, drawn with a
     * QgsHighlightMaskMaterial3D.
     */
    QgsRenderPass &maskPass() { return mMaskPass; }

    //! Sets the colour of the band drawn around a highlighted feature
    void setOutlineColor( const QColor &color ) { mOutlineMaterial.setOutlineColor( color ); }

    //! Sets how far the band reaches out from the edge of a feature, in pixels
    void setOutlineWidth( int pixels ) { mOutlineMaterial.setOutlineWidth( pixels ); }

    //! Removes every command from both passes
    void clearCommands();

    //! Returns TRUE if there is anything to draw this frame
    bool isEmpty() const { return mFillPass.commands().empty() && mMaskPass.commands().empty(); }

    /**
     * Draws the fill and the band into \a target, which is normally the forward pass's.
     *
     * \a target must be the variant that keeps its contents, since everything here is
     * drawn over a scene that has already been rendered. Returns FALSE if nothing was drawn.
     */
    bool render( QgsRhiEngine *engine, QgsGraphicsPipelineCache *cache, const QgsScreenQuad *quad, QRhiRenderTarget *target );

  private:
    QgsGpuRenderTarget mMaskTarget;
    QgsRenderPass mFillPass { u"highlight fill"_s };
    QgsRenderPass mMaskPass { u"highlight mask"_s };
    QgsRenderPass mOutlinePass { u"highlight outline"_s };
    QgsHighlightOutlineMaterial3D mOutlineMaterial;
};

#endif // QGSHIGHLIGHTSRENDERPASS_H
