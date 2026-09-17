/***************************************************************************
  qgsterraintileloader.h
  --------------------------------------
  Date                 : July 2017
  Copyright            : (C) 2017 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSTERRAINTILELOADER_H
#define QGSTERRAINTILELOADER_H

/// @cond PRIVATE

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QGIS API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//


#include <memory>

#include "qgschunkloader.h"
#include "qgsrectangle.h"

#include <QImage>

#define SIP_NO_FILE

class QgsMaterial3D;
class QgsPhongMaterialSettings;
class QgsSceneObject3D;
class QgsTerrainEntity;


/**
 * \ingroup qgis_3d
 * \brief Base class for chunk loaders for terrain tiles.
 *
 * Adds functionality for asynchronous rendering of terrain tile map texture and access to the terrain entity.
 */
class QgsTerrainTileLoader : public QgsChunkLoader
{
    Q_OBJECT

  public:
    //! Constructs loader for a chunk node
    QgsTerrainTileLoader( QgsTerrainEntity *terrain, QgsChunkNode *mNode );

  protected:
    //! Starts asynchronous rendering of map texture
    void loadTexture();

    /**
     * Builds the material a terrain tile is drawn with in the QRhi engine, taking the
     * rendered map image as its texture where there is one.
     *
     * The texture is added to \a object, which owns it, so that it is released together
     * with the rest of the chunk. Terrain is drawn with no backface culling, so that it
     * can be looked at from underground.
     */
    std::unique_ptr<QgsMaterial3D> createTileMaterial( QgsSceneObject3D *object, bool isShadingEnabled, const QgsPhongMaterialSettings &shadingMaterial, bool useTexture );
    //! Gives access to the terain entity
    QgsTerrainEntity *terrain() { return mTerrain; }
    // Emit finished by default when texture is loaded.
    // Subclasses can override this and emit finish later.
    virtual void onTextureLoaded();

  protected slots:
    void onImageReady( int jobId, const QImage &image );

  private:
    QgsTerrainEntity *mTerrain = nullptr;
    QgsRectangle mExtentMapCrs;
    QString mTileDebugText;
    int mTextureJobId = -1;
    QImage mTextureImage;
};

/// @endcond

#endif // QGSTERRAINTILELOADER_H
