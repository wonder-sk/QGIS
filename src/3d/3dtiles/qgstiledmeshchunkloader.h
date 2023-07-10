#ifndef QGSTILEDMESHCHUNKLOADER_H
#define QGSTILEDMESHCHUNKLOADER_H

#include "qgschunkloader_p.h"
#include "qgschunknode_p.h"
#include "3dtiles.h"

class Qgs3DMapSettings;


class QgsTiledMeshChunkLoader : public QgsChunkLoader
{
    //    Q_OBJECT
  public:
    QgsTiledMeshChunkLoader( QgsChunkNode *node, SceneContext &ctx, Tile &t );

    virtual Qt3DCore::QEntity *createEntity( Qt3DCore::QEntity *parent );

    SceneContext &mCtx;
    Tile &mT;
};


class QgsTiledMeshChunkLoaderFactory : public QgsChunkLoaderFactory
{
  public:
    QgsTiledMeshChunkLoaderFactory( const Qgs3DMapSettings &map, SceneContext &ctx );

    virtual QgsChunkLoader *createChunkLoader( QgsChunkNode *node ) const override;
    virtual QgsChunkNode *createRootNode() const override;
    virtual QVector<QgsChunkNode *> createChildren( QgsChunkNode *node ) const override;

    const Qgs3DMapSettings &mMap;
    SceneContext &mCtx;
    mutable QHash<QgsChunkNodeId, Tile *> mNodeIdToTile;
};


#endif // QGSTILEDMESHCHUNKLOADER_H
