

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
  mCtx.tilesetJsonPath = mUri;
  mCtx.relativePathBase = QFileInfo( mUri ).path() + "/";
  mCtx.tilesetLevel = -1;  // 1-4  (or negative for chunk loader)
  mCtx.sceneOriginTargetCrs = QgsVector3D( map.origin().x(), map.origin().y(), -40 );
  mCtx.targetCrs = map.crs().authid();

  if ( !mCtx.targetCrs.isEmpty() )
  {
    mCtx.ecefToTargetCrs.reset( new QgsCoordinateTransform(
                                  QgsCoordinateReferenceSystem( "EPSG:4978" ), QgsCoordinateReferenceSystem( mCtx.targetCrs ), map.transformContext() ) );
    if ( !mCtx.ecefToTargetCrs->isValid() )
    {
      qDebug() << "Failed to create transformation object";
    }
  }

  mCtx.rootTile = loadTilesetJson( mCtx.tilesetJsonPath, mCtx.relativePathBase );

  if ( mCtx.tilesetLevel < 0 )
  {
    QgsTiledMeshChunkLoaderFactory *factory = new QgsTiledMeshChunkLoaderFactory( map, mCtx );
    QgsChunkedEntity *e = new QgsChunkedEntity( 50.0, factory, true );  // TODO: why so high?
    e->setShowBoundingBoxes( true );
    return e;
  }
  else
  {
    // add 3D Tiles stuff directly as a QEntity! (at one zoom level)
    Qt3DCore::QEntity *e = loadAllSceneTiles( mCtx );
    return e;
  }
}
