/***************************************************************************
  qgsoverlayquads.h
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

#ifndef QGSOVERLAYQUADS_H
#define QGSOVERLAYQUADS_H

#include <memory>
#include <vector>

#include "qgis_3d.h"
#include "qgsoverlaytexturematerial3d.h"

#define SIP_NO_FILE

class QgsRenderPass;
class QgsScreenQuad;

/**
 * \ingroup qgis_3d
 * \brief The textures drawn on top of the finished scene, such as the 2D map overlay.
 *
 * The port of QgsOverlayTextureRenderView and the entities that populated it. Under Qt3D
 * an overlay was an entity carrying a layer that a filter in its own frame graph branch
 * matched; here it is a quad added to the command list of the pass that already draws the
 * final image, which is all the layer filtering ever amounted to.
 *
 * It is deliberately not a pass of its own. The final image goes to the window's
 * swapchain or to an export texture, and neither can be drawn into twice in one frame
 * without either clearing what is there or paying for a copy, so the overlays ride along
 * with the post processing quad instead.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsOverlayQuads
{
  public:
    QgsOverlayQuads();
    ~QgsOverlayQuads();

    QgsOverlayQuads( const QgsOverlayQuads & ) = delete;
    QgsOverlayQuads &operator=( const QgsOverlayQuads & ) = delete;

    /**
     * Adds a quad showing \a texture and returns the material that places and configures it.
     *
     * This object owns the material; the returned pointer stays valid until clear().
     */
    QgsOverlayTextureMaterial3D *addOverlay( QRhiTexture *texture );

    //! Removes every overlay, destroying their materials
    void clear();

    //! Returns how many overlays there are
    int count() const { return static_cast<int>( mOverlays.size() ); }

    //! Returns the material of the overlay at \a index, or NULLPTR if there is no such overlay
    QgsOverlayTextureMaterial3D *overlay( int index ) const;

    /**
     * Appends one command per overlay to \a pass, drawing \a quad.
     *
     * The overlays go in the order they were added, so \a pass must not sort.
     */
    void appendCommands( QgsRenderPass *pass, const QgsScreenQuad *quad ) const;

    //! Drops every GPU resource. Must be called while the device is still alive.
    void releaseGpuResources();

  private:
    std::vector<std::unique_ptr<QgsOverlayTextureMaterial3D>> mOverlays;
};

#endif // QGSOVERLAYQUADS_H
