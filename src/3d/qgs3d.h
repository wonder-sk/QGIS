/***************************************************************************
                         qgs3d.h
                         --------
    begin                : July 2020
    copyright            : (C) 2020 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGS3D_H
#define QGS3D_H

#include <memory>

#include "qgis.h"
#include "qgis_3d.h"
#include "qgis_sip.h"

class QgsMaterialRegistry;
class Qgs3DTerrainRegistry;
class QgsAbstractMaterialSettings;
class QgsExpressionContext;
class QgsSettingsEntryBool;
template<class T> class QgsSettingsEntryEnumFlag;

/**
 * \ingroup gui
 * \brief A singleton class containing various registries and other global members
 * related to 3D classes.
 * \since QGIS 3.16
 */
class _3D_EXPORT Qgs3D
{
  public:
    static const QgsSettingsEntryBool *settingMsaaEnabled SIP_SKIP;
    static const QgsSettingsEntryEnumFlag<Qgis::TextureFilterQuality> *settingTextureFilterQuality SIP_SKIP;
    static const QgsSettingsEntryEnumFlag<Qgis::ShadowQuality> *settingShadowQuality SIP_SKIP;

    Qgs3D( const Qgs3D &other ) = delete;
    Qgs3D &operator=( const Qgs3D &other ) = delete;

    /**
     * Returns a pointer to the singleton instance.
     */
    static Qgs3D *instance();

    ~Qgs3D();

    /**
     * Initializes the 3D framework.
     */
    static void initialize();

    /**
     * Returns the material registry, used for managing 3D materials.
     */
    static QgsMaterialRegistry *materialRegistry();

    /**
     * Returns the terrain registry, used for managing 3D terrains.
     */
    static Qgs3DTerrainRegistry *terrainRegistry();

#ifndef SIP_RUN
    // if you change this, also update the corresponding constant in shadows.inc.frag!
    //! Number of shadow map cascades
    static constexpr int NUM_SHADOW_CASCADES = 4;

    /**
     * Default value of the map3d/gpuMemoryLimit setting, in megabytes.
     *
     * The budget a chunked entity unloads its least recently used chunks to stay within.
     * Textures are counted as well as geometry, and on a textured scene they are the
     * larger half, which is why this is several times the pre-QRhi number.
     *
     * \since QGIS 4.2
     */
    static constexpr double DEFAULT_GPU_MEMORY_LIMIT_MB = 2000.0;

#endif

  private:
    Qgs3D();

#ifdef SIP_RUN
    Qgs3D( const Qgs3D &other );
#endif

    bool mInitialized = false;

    Qgs3DTerrainRegistry *mTerrainRegistry = nullptr;
};

#endif // QGS3D_H
