#ifndef QGSTILEDSCENECHUNKLOADER_H
#define QGSTILEDSCENECHUNKLOADER_H

#include "qgschunkloader_p.h"
#include "qgschunknode_p.h"
#include "qgstiledsceneindex.h"
#include "qgstiledscenetile.h"

class Qgs3DMapSettings;
class QgsCoordinateTransform;
class QgsTiledSceneChunkLoaderFactory;


class QgsTiledSceneChunkLoader : public QgsChunkLoader
{
    //    Q_OBJECT
  public:
    QgsTiledSceneChunkLoader( QgsChunkNode *node, const QgsTiledSceneChunkLoaderFactory *factory, const QgsTiledSceneTile &t );

    virtual Qt3DCore::QEntity *createEntity( Qt3DCore::QEntity *parent );

    const QgsTiledSceneChunkLoaderFactory *mFactory;
    QgsTiledSceneTile mTile;
};


class QgsTiledSceneChunkLoaderFactory : public QgsChunkLoaderFactory
{
  public:
    QgsTiledSceneChunkLoaderFactory( const Qgs3DMapSettings &map, QString relativePathBase, const QgsTiledSceneIndex &index );

    virtual QgsChunkLoader *createChunkLoader( QgsChunkNode *node ) const override;
    virtual QgsChunkNode *createRootNode() const override;
    virtual QVector<QgsChunkNode *> createChildren( QgsChunkNode *node ) const override;

    const Qgs3DMapSettings &mMap;
    QString mRelativePathBase;
    mutable QgsTiledSceneIndex mIndex;
    std::unique_ptr<QgsCoordinateTransform> mBoundsTransform;
    std::unique_ptr<QgsCoordinateTransform> mRegionTransform;
    mutable QHash<QgsChunkNodeId, QString> mNodeIdToIndexId;
};


#endif // QGSTILEDSCENECHUNKLOADER_H
