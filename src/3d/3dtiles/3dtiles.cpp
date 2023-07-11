
#include "3dtiles.h"

#include <fstream>


Tile parseTile( json &tileJson, QString relativePathBase )
{
  json box = tileJson["boundingVolume"]["box"];

  Tile t;
  t.obb = OBB::fromJson( box );
  t.geomError = tileJson["geometricError"];
  if ( tileJson.contains( "content" ) )
  {
    t.contentUri = QString::fromStdString( tileJson["content"]["uri"] );

    // instead of graphical content, this could lead to another tileset JSON
    // TODO: does not need to be JSON extension - we should check file content
    if ( t.contentUri.endsWith( ".json" ) )
    {
      // home-made relative path support
      if ( t.contentUri.startsWith( "./" ) )
      {
        t.contentUri.replace( "./", relativePathBase );
      }

      if ( QFile::exists( t.contentUri ) )
      {
        qDebug() << "loading extra tileset:" << t.contentUri;
        t = loadTilesetJson( t.contentUri, relativePathBase );
      }
      else
      {
        //qDebug() << "skipped missing tileset:" << t.contentUri;
      }
    }
  }
  if ( tileJson.contains( "children" ) )
  {
    for ( json &childTileJson : tileJson["children"] )
    {
      t.children.append( parseTile( childTileJson, relativePathBase ) );
    }
  }
  //t.obb.dump();
  //qDebug() << "content:" << t.contentUri;
  //qDebug() << "children:" << t.children.count();

  return t;
}


Tile loadTilesetJson( QString tilesetPath, QString relativePathBase )
{
  std::ifstream f( tilesetPath.toStdString() );
  json data = json::parse( f );

  Tile rootTile = parseTile( data["root"], relativePathBase );

  return rootTile;
}


QStringList collectTilesAtLevel( const Tile &tile, int level, QString relativePathBase )
{
  QStringList files;

  if ( !tile.contentUri.isEmpty() && level == 0 )
  {
    QString uri = tile.contentUri;

    // home-made relative path support
    if ( uri.startsWith( "./" ) )
    {
      uri.replace( "./", relativePathBase );
    }

    files << uri;
  }

  for ( const Tile &child : tile.children )
  {
    files += collectTilesAtLevel( child, level - 1, relativePathBase );
  }
  return files;
}


bool init3DTilesScene( SceneContext &ctx )
{
  if ( !ctx.targetCrs.isEmpty() )
  {
    // TODO: trasnform context
    ctx.ecefToTargetCrs.reset( new QgsCoordinateTransform( QgsCoordinateReferenceSystem( "EPSG:4978" ), QgsCoordinateReferenceSystem( ctx.targetCrs ), QgsCoordinateTransformContext() ) );
    if ( !ctx.ecefToTargetCrs->isValid() )
    {
      qDebug() << "Failed to create transformation object";
      return false;
    }
  }
  else
  {
    ctx.ecefToTargetCrs = nullptr;
  }

  if ( !ctx.tilesetJsonPath.isEmpty() )
  {
    ctx.rootTile = loadTilesetJson( ctx.tilesetJsonPath, ctx.relativePathBase );
    ctx.files << collectTilesAtLevel( ctx.rootTile, ctx.tilesetLevel, ctx.relativePathBase );

    //        QVector<VEC3D> vv = root.children[0].obb.cornersSceneCoords(ctx);
    //        qDebug() << "OBB" << vv;
    //        AABB aabb = root.children[0].obb.aabb(ctx);
    //        qDebug() << "AABB" << aabb.v0 << aabb.v1;
    //        for (int i = 0; i < 1; ++i)
    //        {
    //            Qt3DCore::QEntity *eBox = entityForOBB(root.children[i].obb, ctx);
    //            eBox->setParent(rootEntity);
    //            Qt3DCore::QEntity *aBox = entityForAABB(root.children[i].obb, ctx);
    //            aBox->setParent(rootEntity);
    //        }
  }
  return true;
}


Qt3DCore::QEntity *loadAllSceneTiles( SceneContext &ctx )
{
  Qt3DCore::QEntity *tilesEntity = new Qt3DCore::QEntity;
  for ( int i = 0; i < ctx.files.size(); ++i )
  {
    Qt3DCore::QEntity *gltfEntity = gltfToEntity( ctx.files[i], ctx );
    gltfEntity->setParent( tilesEntity );
  }

  return tilesEntity;
}

QVector<QgsVector3D> OBB::cornersSceneCoords( SceneContext &ctx )
{
  QVector<QgsVector3D> c = corners();
  for ( int i = 0; i < c.count(); ++i )
  {
    c[i] = reproject( *ctx.ecefToTargetCrs, c[i] ) - ctx.sceneOriginTargetCrs;
  }
  return c;
}

QgsBox3d OBB::aabb( SceneContext &ctx )
{
  const QVector<QgsVector3D> c = cornersSceneCoords( ctx );
  QgsVector3D v0 = c[0], v1 = c[0];
  for ( const QgsVector3D &v : c )
  {
    if ( v.x() < v0.x() ) v0.setX( v.x() );
    if ( v.y() < v0.y() ) v0.setY( v.y() );
    if ( v.z() < v0.z() ) v0.setZ( v.z() );
    if ( v.x() > v1.x() ) v1.setX( v.x() );
    if ( v.y() > v1.y() ) v1.setY( v.y() );
    if ( v.z() > v1.z() ) v1.setZ( v.z() );
  }
  return QgsBox3d( v0.x(), v0.y(), v0.z(), v1.x(), v1.y(), v1.z() );
}
