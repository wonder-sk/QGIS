#include "qgstiledmeshchunkloader.h"

#include "qgs3dmapsettings.h"


// TODO:
// - caching script: start from root
// - lazy load hierarchy
// - loading gltf in bg thread
// - saner tile IDs  (why do we need the IDs?)
// - request tiles via network


size_t qHash( const QgsChunkNodeId &n )
{
  return n.d ^ n.x ^ n.y ^ n.z;
}

// also flips to world coords
QgsAABB aabbConvert( const QgsBox3d &b )
{
  return QgsAABB( b.xMinimum(), b.zMinimum(), -b.yMaximum(), b.xMaximum(), b.zMaximum(), -b.yMinimum() );
}


QgsTiledMeshChunkLoader::QgsTiledMeshChunkLoader( QgsChunkNode *node, TiledMeshData &data, Tile &t )
  : QgsChunkLoader( node ), mData( data ), mT( t )
{
  qDebug() << "chunk loader " << node->tileId().text();
  // start async (actually just do deferred finish()!)
  QMetaObject::invokeMethod( this, "finished", Qt::QueuedConnection );
}

Qt3DCore::QEntity *QgsTiledMeshChunkLoader::createEntity( Qt3DCore::QEntity *parent )
{
  qDebug() << "create entity!" << mNode->tileId().text();

  // load content + convert
  QString uri = mT.contentUri;

  // nothing to show for this tile
  // TODO: can we skip loading it at all?
  if ( uri.isEmpty() )
  {
    return new Qt3DCore::QEntity( parent );
  }

  // we do not load tiles that are too big - at least for the time being
  // the problem is that their 3D bounding boxes with ECEF coordinates are huge
  // and we are unable to turn them into planar bounding boxes
  if ( mT.obb.isTooBig() )
  {
    return new Qt3DCore::QEntity( parent );
  }

  // home-made relative path support
  if ( uri.startsWith( "./" ) )
  {
    uri.replace( "./", mData.relativePathBase );
  }

  qDebug() << "loading: " << uri;

  Qt3DCore::QEntity *gltfEntity = gltfToEntity( uri, mData.coords );
  gltfEntity->setParent( parent );
  return gltfEntity;
}

///

QgsTiledMeshChunkLoaderFactory::QgsTiledMeshChunkLoaderFactory( const Qgs3DMapSettings &map, TiledMeshData &data )
  : mMap( map ), mData( data )
{
}

QgsChunkLoader *QgsTiledMeshChunkLoaderFactory::createChunkLoader( QgsChunkNode *node ) const
{
  Q_ASSERT( mNodeIdToTile.contains( node->tileId() ) );
  Tile *t = mNodeIdToTile[node->tileId()];
  return new QgsTiledMeshChunkLoader( node, mData, *t );
}

QgsChunkNode *QgsTiledMeshChunkLoaderFactory::createRootNode() const
{
  QgsChunkNodeId nodeId( 0, 0, 0, 0 );
  mNodeIdToTile[nodeId] = &mData.rootTile;
  if ( mData.rootTile.geomError > 1e6 || mData.rootTile.obb.isTooBig() )
  {
    // use the full extent of the scene
    QgsVector3D v0 = mMap.mapToWorldCoordinates( QgsVector3D( mMap.extent().xMinimum(), mMap.extent().yMinimum(), -100 ) );
    QgsVector3D v1 = mMap.mapToWorldCoordinates( QgsVector3D( mMap.extent().xMaximum(), mMap.extent().yMaximum(), +100 ) );
    QgsAABB aabb( v0.x(), v0.y(), v0.z(), v1.x(), v1.y(), v1.z() );
    float err = std::min( 1e6, mData.rootTile.geomError );
    qDebug() << "root" << nodeId.text() << aabb.toString() << err;
    return new QgsChunkNode( nodeId, aabb, err );
  }
  else
  {
    QgsAABB aabb = aabbConvert( mData.rootTile.obb.aabb( mData.coords ) );
    qDebug() << "root" << nodeId.text() << aabb.toString() << mData.rootTile.geomError;
    return new QgsChunkNode( nodeId, aabb, mData.rootTile.geomError );
  }
}

QVector<QgsChunkNode *> QgsTiledMeshChunkLoaderFactory::createChildren( QgsChunkNode *node ) const
{
  qDebug() << "create children" << node->tileId().text();
  QVector<QgsChunkNode *> children;
  Q_ASSERT( mNodeIdToTile.contains( node->tileId() ) );
  Tile *t = mNodeIdToTile[node->tileId()];
  for ( Tile &ch : t->children )
  {
    QgsChunkNodeId chId( node->tileId().d + 1, rand(), 0, 0 ); // TODO: some more reasonable IDs?

    // first check if this node should be even considered
    if ( ch.obb.isTooBig() )
    {
      // if the tile is huge, let's try to see if our scene is actually inside
      // (if not, let' skip this child altogether!)
      // TODO: make OBB of our scene in ECEF rather than just using center of the scene?
      QgsPointXY c = mMap.extent().center();
      QgsVector3D cEcef = reproject( *mData.coords.ecefToTargetCrs, QgsVector3D( c.x(), c.y(), 0 ), true );
      QgsVector3D ecef2 = cEcef - ch.obb.center;
      QVector3D aaa = ch.obb.rot.inverted().map( ecef2.toVector3D() );
      if ( aaa.x() > 1 || aaa.y() > 1 || aaa.z() > 1 ||
           aaa.x() < -1 || aaa.y() < -1 || aaa.z() < -1 )
      {
        qDebug() << "skipping child" << chId.text();
        continue;
      }
    }

    mNodeIdToTile[chId] = &ch;
    QgsChunkNode *nChild;
    if ( ch.geomError > 1e6 || ch.obb.isTooBig() )
    {
      // use the full extent of the scene
      QgsVector3D v0 = mMap.mapToWorldCoordinates( QgsVector3D( mMap.extent().xMinimum(), mMap.extent().yMinimum(), -100 ) );
      QgsVector3D v1 = mMap.mapToWorldCoordinates( QgsVector3D( mMap.extent().xMaximum(), mMap.extent().yMaximum(), +100 ) );
      QgsAABB aabb( v0.x(), v0.y(), v0.z(), v1.x(), v1.y(), v1.z() );
      float err = std::min( 1e6, ch.geomError );
      qDebug() << "child" << chId.text() << aabb.toString() << err;
      nChild = new QgsChunkNode( chId, aabb, err );
    }
    else
    {
      QgsAABB aabb( aabbConvert( ch.obb.aabb( mData.coords ) ) );
      qDebug() << "child" << chId.text() << aabb.toString() << ch.geomError;
      nChild = new QgsChunkNode( chId, aabb, ch.geomError );
    }
    qDebug() << "is too big? " << ch.obb.isTooBig();
    children.append( nChild );
  }
  return children;
}
