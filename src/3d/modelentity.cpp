#include "modelentity.h"

#include <QUrl>

#include <Qt3DCore/QTransform>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QSceneLoader>
#include <Qt3DRender/QMesh>
#include <Qt3DExtras/QPhongMaterial>

#include "abstract3dsymbol.h"
#include "map3d.h"


static Qt3DExtras::QPhongMaterial* phongMaterial(const Point3DSymbol &symbol) {
    Qt3DExtras::QPhongMaterial* phong = new Qt3DExtras::QPhongMaterial;

    phong->setAmbient(symbol.material.ambient());
    phong->setDiffuse(symbol.material.diffuse());
    phong->setSpecular(symbol.material.specular());
    phong->setShininess(symbol.material.shininess());

    return phong;
}

ModelEntity::ModelEntity(const QVector3D& position, const Point3DSymbol &symbol, Qt3DCore::QNode *parent )
  : Qt3DCore::QEntity( parent )
{
  Q_ASSERT(symbol.shapeProperties["shape"].toString() == "model" ); // others handled in PointEntity
  QString src = symbol.shapeProperties["model"].toString();
  QUrl url = QUrl::fromLocalFile(src);

  if (symbol.shapeProperties["overwriteMaterial"].toBool()) {
      Qt3DRender::QMesh * mesh = new Qt3DRender::QMesh;
      mesh->setSource(url);
      addComponent( mesh );

      Qt3DExtras::QPhongMaterial* material = phongMaterial(symbol);
      addComponent( material );

  } else {
      Qt3DRender::QSceneLoader * modelLoader = new Qt3DRender::QSceneLoader;
      modelLoader->setSource(url);
      addComponent( modelLoader );
  }

  Qt3DCore::QTransform* tr = new Qt3DCore::QTransform;
  tr->setMatrix(symbol.transform);
  tr->setTranslation(position + tr->translation());
  addComponent( tr );
}
