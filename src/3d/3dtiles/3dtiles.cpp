
#include "3dtiles.h"

#include <fstream>


QDebug operator<<( QDebug debug, const VEC3D &v )
{
  QDebugStateSaver saver( debug );
  debug.nospace() << '(' << v.x << ", " << v.y << ", " << v.z << ')';
  return debug;
}


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
  ctx.projContext = proj_context_create();

  if ( !ctx.targetCrs.isEmpty() )
  {
    ctx.ecefToTargetCrs = proj_create_crs_to_crs( ctx.projContext, "EPSG:4978", ctx.targetCrs.toUtf8().constData(), NULL );
    if ( !ctx.ecefToTargetCrs )
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

QVector<VEC3D> OBB::cornersSceneCoords( SceneContext &ctx )
{
  QVector<VEC3D> c = corners();
  for ( int i = 0; i < c.count(); ++i )
  {
    c[i] = reproject( ctx.ecefToTargetCrs, c[i] ) - ctx.sceneOriginTargetCrs;
  }
  return c;
}

AABB OBB::aabb( SceneContext &ctx )
{
  const QVector<VEC3D> c = cornersSceneCoords( ctx );
  AABB box;
  box.v0 = box.v1 = c[0];
  for ( const VEC3D &v : c )
  {
    if ( v.x < box.v0.x ) box.v0.x = v.x;
    if ( v.y < box.v0.y ) box.v0.y = v.y;
    if ( v.z < box.v0.z ) box.v0.z = v.z;
    if ( v.x > box.v1.x ) box.v1.x = v.x;
    if ( v.y > box.v1.y ) box.v1.y = v.y;
    if ( v.z > box.v1.z ) box.v1.z = v.z;
  }
  return box;
}
