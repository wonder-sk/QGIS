
#include "3dtiles.h"

#include <fstream>


Tile parseTile( json &tileJson, QString relativePathBase, CoordsContext &coordsCtx, QgsMatrix4x4 trParent )
{
  json bounds = tileJson["boundingVolume"];

  Tile t;

  if ( bounds.contains( "box" ) )
  {
    t.boundsType = Tile::BoundsOBB;
    OBB obb = OBB::fromJson( bounds["box"] );
    t.obb = obb;
    t.largeBounds = t.geomError > 1e6 || obb.isTooBig();
    if ( !t.largeBounds )
    {
      // convert to axis aligned bbox
      t.region = obb.aabb( coordsCtx );
    }
  }
  else if ( bounds.contains( "region" ) )
  {
    t.boundsType = Tile::BoundsRegion;
    json region = bounds["region"];
    double west = region[0].get<double>() * 180 / M_PI;
    double south = region[1].get<double>() * 180 / M_PI;
    double east = region[2].get<double>() * 180 / M_PI;
    double north = region[3].get<double>() * 180 / M_PI;
    double minHeight = region[4].get<double>();
    double maxHeight = region[5].get<double>();
    coordsCtx.regionToTargetCrs->transformInPlace( west, south, minHeight );
    coordsCtx.regionToTargetCrs->transformInPlace( east, north, maxHeight );
    west -= coordsCtx.sceneOriginTargetCrs.x();
    east -= coordsCtx.sceneOriginTargetCrs.x();
    north -= coordsCtx.sceneOriginTargetCrs.y();
    south -= coordsCtx.sceneOriginTargetCrs.y();
    //qDebug() << "region" << west << east << "|" << south << north << "|" << minHeight << maxHeight;
    t.region = QgsBox3d( west, south, minHeight, east, north, maxHeight );
  }

  // TODO: 3d tiles support any combination of replacement/additive strategies
  // but chunked entity only allows one or the other for the whole entity
  if ( tileJson.contains( "refine" ) && tileJson["refine"] == "ADD" )
  {
    t.additiveStrategy = true;
  }

  if ( tileJson.contains( "transform" ) )
  {
    json transform = tileJson["transform"];
    double *ptr = t.transform.data();
    for ( int i = 0; i < 16; ++i )
      ptr[i] = transform[i].get<double>();
  }

  t.transform = trParent * t.transform;

  // update bounds based on transform
  // TODO: also apply to sphere bounds
  if ( t.boundsType == Tile::BoundsOBB && !t.transform.isIdentity() )
  {
    t.obb.transform( t.transform );
    t.region = t.obb.aabb( coordsCtx );
  }

  t.geomError = tileJson["geometricError"];
  if ( tileJson.contains( "content" ) )
  {
    // spec says "uri" but e.g. new york uses "url"
    bool isUri = tileJson["content"].contains( "uri" );

    t.contentUri = QString::fromStdString( isUri ? tileJson["content"]["uri"] : tileJson["content"]["url"] );

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
        t = loadTilesetJson( t.contentUri, relativePathBase, coordsCtx );
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
      t.children.append( parseTile( childTileJson, relativePathBase, coordsCtx, t.transform ) );
    }
  }
  //t.obb.dump();
  //qDebug() << "content:" << t.contentUri;
  //qDebug() << "children:" << t.children.count();

  return t;
}


Tile loadTilesetJson( QString tilesetPath, QString relativePathBase, CoordsContext &coordsCtx )
{
  std::ifstream f( tilesetPath.toStdString() );
  json data = json::parse( f );

  Tile rootTile = parseTile( data["root"], relativePathBase, coordsCtx, QgsMatrix4x4() );

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


Qt3DCore::QEntity *loadAllSceneTiles( const Tile &rootTile, int tilesetLevel, QString relativePathBase, CoordsContext &coordsCtx )
{
  Qt3DCore::QEntity *tilesEntity = new Qt3DCore::QEntity;

  const QStringList files = collectTilesAtLevel( rootTile, tilesetLevel, relativePathBase );
  for ( const QString &file : files )
  {
    Qt3DCore::QEntity *gltfEntity = gltfToEntity( file, coordsCtx );
    gltfEntity->setParent( tilesEntity );
  }

  return tilesEntity;
}

QVector<QgsVector3D> OBB::cornersSceneCoords( CoordsContext &coordsCtx )
{
  QVector<QgsVector3D> c = corners();
  for ( int i = 0; i < c.count(); ++i )
  {
    c[i] = reproject( *coordsCtx.ecefToTargetCrs, c[i] ) - coordsCtx.sceneOriginTargetCrs;
  }
  return c;
}

QgsBox3d OBB::aabb( CoordsContext &coordsCtx )
{
  const QVector<QgsVector3D> c = cornersSceneCoords( coordsCtx );
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
