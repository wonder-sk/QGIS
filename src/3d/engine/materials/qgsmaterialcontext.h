/***************************************************************************
  qgsmaterialcontext.h
  --------------------------------------
  Date                 : March 2026
  Copyright            : (C) 2026 by Nyall Dawson
  Email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMATERIALCONTEXT_H
#define QGSMATERIALCONTEXT_H

#include "qgis.h"
#include "qgis_3d.h"

#include <QColor>

#define SIP_NO_FILE

class Qgs3DRenderContext;


/**
 * \ingroup qgis_3d
 * \brief Context settings for a material.
 *
 * \warning This is not considered stable API, and may change in future QGIS releases. It is
 * exposed to the Python bindings as a tech preview only.
 *
 * \since QGIS 3.16
 */
class _3D_EXPORT QgsMaterialContext
{
  public:
    /**
     * Constructs a material context from the settings in a 3D render \a context.
     *
     * \since QGIS 4.2
    */
    static QgsMaterialContext fromRenderContext( const Qgs3DRenderContext &context );

    /**
     * Returns TRUE if the material should represent a selected state.
     *
     * \see setIsSelected()
     */
    bool isSelected() const { return mIsSelected; }

    /**
     * Sets whether the material should represent a selected state.
     *
     * \see isSelected()
     */
    void setIsSelected( bool isSelected ) { mIsSelected = isSelected; }

    /**
     * Returns the color for representing materials in a selected state.
     *
     * \see setSelectionColor()
     */
    QColor selectionColor() const { return mSelectedColor; }

    /**
     * Sets the color for representing materials in a selected state.
     *
     * \see selectionColor()
     */
    void setSelectionColor( const QColor &color ) { mSelectedColor = color; }

    /**
     * Returns TRUE if the material should represent a highlighted state.
     *
     * \see setIsHighlighted()
     * \since QGIS 4.0
     */
    bool isHighlighted() const { return mIsHighlighted; }

    /**
     * Sets whether the material should represent a highlighted state.
     *
     * \see isHighlighted()
     * \since QGIS 4.0
     */
    void setIsHighlighted( bool isHighlighted ) { mIsHighlighted = isHighlighted; }

    /**
     * Returns the texture filtering quality.
     *
     * \see setTextureFilterQuality()
     * \since QGIS 4.2
     */
    Qgis::TextureFilterQuality textureFilterQuality() const { return mTextureFilterQuality; }

    /**
     * Sets the texture filtering \a quality.
     *
     * \see textureFilterQuality()
     * \since QGIS 4.2
     */
    void setTextureFilterQuality( Qgis::TextureFilterQuality quality ) { mTextureFilterQuality = quality; }

    /**
     * Returns TRUE if the material is being shown in a preview widget.
     *
     * \see setIsPreview()
     * \since QGIS 4.2
     */
    bool isPreview() const;

    /**
     * Sets whether the material is being shown in a preview widget.
     *
     * \see isPreview()
     * \since QGIS 4.2
     */
    void setIsPreview( bool isPreview );

  private:
    bool mIsSelected = false;
    bool mIsHighlighted = false;
    bool mIsPreview = false;

    QColor mSelectedColor;
    Qgis::TextureFilterQuality mTextureFilterQuality = Qgis::TextureFilterQuality::Trilinear;
};


#endif // QGSMATERIALCONTEXT_H
