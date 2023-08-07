#include "qgstiledscenechunkloader.h"

#include "qgs3dmapsettings.h"
#include "qgsgltf3dutils.h"
#include "qgstiledsceneboundingvolume.h"
#include "qgstiledscenetile.h"


// TODO:
// - loading gltf in bg thread
// - saner tile IDs  (why do we need the IDs?)
// - request tiles via network


size_t qHash( const QgsChunkNodeId &n )
{
  return n.d ^ n.x ^ n.y ^ n.z;
}

// converts box from map coordinates to world coords (also flips [X,Y] to [X,-Z])
QgsAABB aabbConvert( const QgsBox3D &b0, const QgsVector3D &sceneOriginTargetCrs )
{
  QgsBox3D b = b0 - sceneOriginTargetCrs;
  return QgsAABB( b.xMinimum(), b.zMinimum(), -b.yMaximum(), b.xMaximum(), b.zMaximum(), -b.yMinimum() );
}

static bool isOBBTooBig( const QgsOrientedBox3D &box )
{
  QgsVector3D size = box.size();
  return size.x() > 1e5 || size.y() > 1e5 || size.z() > 1e5;
}

bool hasLargeBounds( const QgsTiledSceneTile &t )
{
  if ( t.geometricError() > 1e6 )
    return true;

  switch ( t.boundingVolume()->type() )
  {
    case Qgis::TiledSceneBoundingVolumeType::Region:
    {
      // TODO: is region always lat/lon?
      QgsBox3D region = static_cast<const QgsTiledSceneBoundingVolumeRegion *>( t.boundingVolume() )->region();
      qDebug() << "region" << region.toString( 1 );
      return region.width() > 15 || region.height() > 15;
    }

    case Qgis::TiledSceneBoundingVolumeType::OrientedBox:
      return isOBBTooBig( static_cast<const QgsTiledSceneBoundingVolumeBox *>( t.boundingVolume() )->box() );

    case Qgis::TiledSceneBoundingVolumeType::Sphere:
      return static_cast<const QgsTiledSceneBoundingVolumeSphere *>( t.boundingVolume() )->sphere().diameter() > 1e5;
  }

  return false;
}


// B3DM is section 10.1  (pages 66 - 73)
// 10.1.3  (page 68)
struct b3dmHeader
{
  unsigned char magic[4];
  quint32 version;
  quint32 byteLength;
  quint32 featureTableJsonByteLength;
  quint32 featureTableBinaryByteLength;
  quint32 batchTableJsonByteLength;
  quint32 batchTableBinaryByteLength;

  void dump()
  {
    qDebug() << "b3dm version " << version;
    qDebug() << "len " << byteLength;
    qDebug() << "feat table  " << featureTableJsonByteLength << featureTableBinaryByteLength;
    qDebug() << "batch table " << batchTableJsonByteLength << batchTableBinaryByteLength;
  }
};


QgsTiledSceneChunkLoader::QgsTiledSceneChunkLoader( QgsChunkNode *node, const QgsTiledSceneChunkLoaderFactory *factory, const QgsTiledSceneTile &t )
  : QgsChunkLoader( node ), mFactory( factory ), mTile( t )
{
  qDebug() << "chunk loader " << node->tileId().text();
  // start async (actually just do deferred finish()!)
  QMetaObject::invokeMethod( this, "finished", Qt::QueuedConnection );
}

Qt3DCore::QEntity *QgsTiledSceneChunkLoader::createEntity( Qt3DCore::QEntity *parent )
{
  qDebug() << "create entity!" << mNode->tileId().text();

  // load content + convert
  QString uri = mTile.resources().value( QStringLiteral( "content" ) ).toString();

  // nothing to show for this tile
  // TODO: can we skip loading it at all?
  if ( uri.isEmpty() )
  {
    return new Qt3DCore::QEntity( parent );
  }

  // we do not load tiles that are too big - at least for the time being
  // the problem is that their 3D bounding boxes with ECEF coordinates are huge
  // and we are unable to turn them into planar bounding boxes
  if ( hasLargeBounds( mTile ) )
  {
    return new Qt3DCore::QEntity( parent );
  }

  // home-made relative path support
  if ( uri.startsWith( "./" ) )
  {
    uri.replace( "./", mFactory->mRelativePathBase );
  }
  else if ( QFileInfo( uri ).isRelative() )
  {
    uri = mFactory->mRelativePathBase + uri;
  }

  qDebug() << "loading: " << uri;

  QgsGltf3DUtils::EntityTransform entityTransform;
  entityTransform.tileTransform = mTile.transform() ? *mTile.transform() : QgsMatrix4x4();
  entityTransform.sceneOriginTargetCrs = mFactory->mMap.origin();
  entityTransform.ecefToTargetCrs = mFactory->mBoundsTransform.get();

  // TODO: according to the spec we should actually find out from the content what is the file type
  if ( uri.endsWith( ".b3dm" ) )
  {
    QFile file( uri );
    if ( !file.open( QIODevice::ReadOnly ) )
      return new Qt3DCore::QEntity( parent );

    b3dmHeader hdr;
    file.read( ( char * )&hdr, sizeof( b3dmHeader ) );

    Q_ASSERT( hdr.featureTableBinaryByteLength == 0 ); // unsupported for now
    if ( hdr.featureTableJsonByteLength )
    {
      QByteArray ba = file.read( hdr.featureTableJsonByteLength );
//        qDebug() << "feature table:";
//        qDebug() << ba;
    }

    Q_ASSERT( hdr.batchTableBinaryByteLength == 0 ); // unsupported for now
    if ( hdr.batchTableJsonByteLength )
    {
      QByteArray ba = file.read( hdr.batchTableJsonByteLength );
//        qDebug() << "batch table:";
//        qDebug() << ba;
    }

    QByteArray baGLTF = file.readAll();
    Qt3DCore::QEntity *gltfEntity = QgsGltf3DUtils::gltfToEntity( baGLTF, entityTransform, uri );
    gltfEntity->setParent( parent );
    return gltfEntity;
  }
  else
  {
    // we assume it is a GLTF content

    QFile f( uri );
    if ( !f.open( QIODevice::ReadOnly ) )
    {
      qDebug() << "unable to open GLTF file: " << uri;
      return new Qt3DCore::QEntity( parent );
    }

    QByteArray data = f.readAll();

    Qt3DCore::QEntity *gltfEntity = QgsGltf3DUtils::gltfToEntity( data, entityTransform, uri );
    gltfEntity->setParent( parent );
    return gltfEntity;
  }
}

///

QgsTiledSceneChunkLoaderFactory::QgsTiledSceneChunkLoaderFactory( const Qgs3DMapSettings &map, QString relativePathBase, const QgsTiledSceneIndex &index )
  : mMap( map ), mRelativePathBase( relativePathBase ), mIndex( index )
{
  mBoundsTransform.reset( new QgsCoordinateTransform( QgsCoordinateReferenceSystem( "EPSG:4978" ), mMap.crs(), mMap.transformContext() ) );
  mRegionTransform.reset( new QgsCoordinateTransform( QgsCoordinateReferenceSystem( "EPSG:4979" ), mMap.crs(), mMap.transformContext() ) );
}

QgsChunkLoader *QgsTiledSceneChunkLoaderFactory::createChunkLoader( QgsChunkNode *node ) const
{
  Q_ASSERT( mNodeIdToIndexId.contains( node->tileId() ) );
  QgsTiledSceneTile t = mIndex.getTile( mNodeIdToIndexId[node->tileId()] );

  return new QgsTiledSceneChunkLoader( node, this, t );
}

QgsChunkNode *QgsTiledSceneChunkLoaderFactory::createRootNode() const
{
  QgsTiledSceneTile t = mIndex.rootTile();

  QgsChunkNodeId nodeId( 0, 0, 0, 0 );
  mNodeIdToIndexId[nodeId] = t.id();

  if ( hasLargeBounds( t ) )
  {
    // use the full extent of the scene
    QgsVector3D v0 = mMap.mapToWorldCoordinates( QgsVector3D( mMap.extent().xMinimum(), mMap.extent().yMinimum(), -100 ) );
    QgsVector3D v1 = mMap.mapToWorldCoordinates( QgsVector3D( mMap.extent().xMaximum(), mMap.extent().yMaximum(), +100 ) );
    QgsAABB aabb( v0.x(), v0.y(), v0.z(), v1.x(), v1.y(), v1.z() );
    float err = std::min( 1e6, t.geometricError() );
    qDebug() << "root" << nodeId.text() << aabb.toString() << err;
    return new QgsChunkNode( nodeId, aabb, err );
  }
  else
  {
    bool isRegion = t.boundingVolume()->type() == Qgis::TiledSceneBoundingVolumeType::Region;
    QgsBox3D box = t.boundingVolume()->bounds( isRegion ? *mRegionTransform.get() : *mBoundsTransform.get() );
    QgsAABB aabb = aabbConvert( box, mMap.origin() );
    qDebug() << "root" << nodeId.text() << aabb.toString() << t.geometricError();
    return new QgsChunkNode( nodeId, aabb, t.geometricError() );
  }
}

QVector<QgsChunkNode *> QgsTiledSceneChunkLoaderFactory::createChildren( QgsChunkNode *node ) const
{
  qDebug() << "create children" << node->tileId().text();
  QVector<QgsChunkNode *> children;
  Q_ASSERT( mNodeIdToIndexId.contains( node->tileId() ) );

  const QStringList childIds = mIndex.childTileIds( mNodeIdToIndexId[node->tileId()] );
  for ( const QString &childId : childIds )
  {
    QgsChunkNodeId chId( node->tileId().d + 1, rand(), 0, 0 ); // TODO: some more reasonable IDs?

    QgsTiledSceneTile t = mIndex.getTile( childId );

    // first check if this node should be even considered
    if ( hasLargeBounds( t ) )
    {
      // if the tile is huge, let's try to see if our scene is actually inside
      // (if not, let' skip this child altogether!)
      // TODO: make OBB of our scene in ECEF rather than just using center of the scene?
      if ( t.boundingVolume()->type() == Qgis::TiledSceneBoundingVolumeType::OrientedBox )
      {
        QgsOrientedBox3D obb = static_cast<const QgsTiledSceneBoundingVolumeBox *>( t.boundingVolume() )->box();

        QgsPointXY c = mMap.extent().center();
        QgsVector3D cEcef = mBoundsTransform->transform( QgsVector3D( c.x(), c.y(), 0 ), Qgis::TransformDirection::Reverse );
        QgsVector3D ecef2 = cEcef - obb.center();

        const double *half = obb.halfAxes();

        // this is an approximate check anyway, no need for double precision matrix/vector
        QMatrix4x4 rot(
          half[0], half[3], half[6], 0,
          half[1], half[4], half[7], 0,
          half[2], half[5], half[8], 0,
          0, 0, 0, 1 );
        QVector3D aaa = rot.inverted().map( ecef2.toVector3D() );

        if ( aaa.x() > 1 || aaa.y() > 1 || aaa.z() > 1 ||
             aaa.x() < -1 || aaa.y() < -1 || aaa.z() < -1 )
        {
          qDebug() << "skipping child because our scene is not in it" << chId.text();
          continue;
        }
      }
    }

    mNodeIdToIndexId[chId] = childId;

    QgsChunkNode *nChild;
    if ( hasLargeBounds( t ) )
    {
      // use the full extent of the scene
      QgsVector3D v0 = mMap.mapToWorldCoordinates( QgsVector3D( mMap.extent().xMinimum(), mMap.extent().yMinimum(), -100 ) );
      QgsVector3D v1 = mMap.mapToWorldCoordinates( QgsVector3D( mMap.extent().xMaximum(), mMap.extent().yMaximum(), +100 ) );
      QgsAABB aabb( v0.x(), v0.y(), v0.z(), v1.x(), v1.y(), v1.z() );
      float err = std::min( 1e6, t.geometricError() );
      qDebug() << "child" << chId.text() << aabb.toString() << err;
      nChild = new QgsChunkNode( chId, aabb, err );
    }
    else
    {
      bool isRegion = t.boundingVolume()->type() == Qgis::TiledSceneBoundingVolumeType::Region;
      QgsBox3D box = t.boundingVolume()->bounds( isRegion ? *mRegionTransform.get() : *mBoundsTransform.get() );
      QgsAABB aabb = aabbConvert( box, mMap.origin() );
      qDebug() << "child" << chId.text() << aabb.toString() << t.geometricError();
      nChild = new QgsChunkNode( chId, aabb, t.geometricError() );
    }

    children.append( nChild );
  }
  return children;
}
