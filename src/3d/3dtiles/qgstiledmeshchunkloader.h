#ifndef QGSTILEDMESHCHUNKLOADER_H
#define QGSTILEDMESHCHUNKLOADER_H

#include "qgschunkloader_p.h"
#include "qgschunknode_p.h"
#include "3dtiles.h"

class Qgs3DMapSettings;


struct TiledMeshData
{
  Tile rootTile;

  CoordsContext coords;

  QString relativePathBase;  // how to resolve URIs starting with "./"
};


class QgsTiledMeshChunkLoader : public QgsChunkLoader
{
    //    Q_OBJECT
  public:
    QgsTiledMeshChunkLoader( QgsChunkNode *node, TiledMeshData &ctx, Tile &t );

    virtual Qt3DCore::QEntity *createEntity( Qt3DCore::QEntity *parent );

    TiledMeshData &mData;
    Tile &mT;
};


class QgsTiledMeshChunkLoaderFactory : public QgsChunkLoaderFactory
{
  public:
    QgsTiledMeshChunkLoaderFactory( const Qgs3DMapSettings &map, TiledMeshData &data );

    virtual QgsChunkLoader *createChunkLoader( QgsChunkNode *node ) const override;
    virtual QgsChunkNode *createRootNode() const override;
    virtual QVector<QgsChunkNode *> createChildren( QgsChunkNode *node ) const override;

    const Qgs3DMapSettings &mMap;
    TiledMeshData &mData;
    mutable QHash<QgsChunkNodeId, Tile *> mNodeIdToTile;
};


#endif // QGSTILEDMESHCHUNKLOADER_H
