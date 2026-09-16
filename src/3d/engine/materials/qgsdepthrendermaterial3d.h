/***************************************************************************
  qgsdepthrendermaterial3d.h
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

#ifndef QGSDEPTHRENDERMATERIAL3D_H
#define QGSDEPTHRENDERMATERIAL3D_H

#include "qgis_3d.h"
#include "qgsgputexture.h"
#include "qgsmaterial3d.h"

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief Spreads the depth buffer over the three colour channels so that it can be read back.
 *
 * The port of QgsDepthEntity. QGIS needs the depth under the cursor on the CPU to turn a
 * drag into a camera move, and reading a depth texture back is not portable, so the depth
 * pass draws this over the whole view and reads back an ordinary colour image.
 *
 * The material has no uniform block of its own: the depth texture is all it needs.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsDepthRenderMaterial3D : public QgsMaterial3D
{
  public:
    QgsDepthRenderMaterial3D();

    //! Sets the depth texture to encode, normally the forward pass's depth attachment
    void setDepthTexture( QRhiTexture *texture );

  private:
    QgsGpuTexture mDepthTexture;
};

#endif // QGSDEPTHRENDERMATERIAL3D_H
