

#include "qgstiledmeshlayer3drenderer.h"

#include "qgstiledmeshchunkloader.h"
#include "qgschunkedentity_p.h"

#include "qgs3dmapsettings.h"
#include "3dtiles.h"


QgsTiledMeshLayer3DRenderer::QgsTiledMeshLayer3DRenderer( QString uri )
  : mUri( uri )
{
}

Qt3DCore::QEntity *QgsTiledMeshLayer3DRenderer::createEntity( const Qgs3DMapSettings &map ) const
{
  int tilesetLevel = -1;  // 1-4  (or negative for chunk loader)

  mData.relativePathBase = QFileInfo( mUri ).path() + "/";
  mData.coords.sceneOriginTargetCrs = QgsVector3D( map.origin().x(), map.origin().y(), -40 );
  QString targetCrs = map.crs().authid();

  if ( !targetCrs.isEmpty() )
  {
    mData.coords.ecefToTargetCrs.reset( new QgsCoordinateTransform(
                                          QgsCoordinateReferenceSystem( "EPSG:4978" ), QgsCoordinateReferenceSystem( targetCrs ), map.transformContext() ) );
    if ( !mData.coords.ecefToTargetCrs->isValid() )
    {
      qDebug() << "Failed to create transformation object";
    }
  }

  mData.rootTile = loadTilesetJson( mUri, mData.relativePathBase );

  if ( tilesetLevel < 0 )
  {
    QgsTiledMeshChunkLoaderFactory *factory = new QgsTiledMeshChunkLoaderFactory( map, mData );
    QgsChunkedEntity *e = new QgsChunkedEntity( 50.0, factory, true );  // TODO: why so high?
    e->setShowBoundingBoxes( true );
    return e;
  }
  else
  {
    // add 3D Tiles stuff directly as a QEntity! (at one zoom level)
    Qt3DCore::QEntity *e = loadAllSceneTiles( mData.rootTile, tilesetLevel, mData.relativePathBase, mData.coords );
    return e;
  }
}
