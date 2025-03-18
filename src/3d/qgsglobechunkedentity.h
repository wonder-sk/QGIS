#ifndef QGSGLOBECHUNKEDENTITY_H
#define QGSGLOBECHUNKEDENTITY_H

#include "qgis_3d.h"


#include "qgschunkedentity.h"

class QgsTerrainTextureGenerator;


class _3D_EXPORT QgsGlobeEntity : public QgsChunkedEntity
{
  public:
    QgsGlobeEntity( Qgs3DMapSettings *mapSettings, float maximumScreenSpaceError );
    ~QgsGlobeEntity();

    QgsTerrainTextureGenerator *mTextureGenerator = nullptr;

};



#endif // QGSGLOBECHUNKEDENTITY_H
