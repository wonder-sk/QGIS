
#ifndef QGSTILEDMESHLAYER3DRENDERER_H
#define QGSTILEDMESHLAYER3DRENDERER_H

#include "qgis_3d.h"

#include "qgsabstract3drenderer.h"
#include "3dtiles.h"


class _3D_EXPORT QgsTiledMeshLayer3DRenderer : public QgsAbstract3DRenderer
{
  public:
    QgsTiledMeshLayer3DRenderer( QString uri );

    virtual QString type() const override { return "tiledmesh"; }
    virtual QgsAbstract3DRenderer *clone() const override { Q_ASSERT( false ); return nullptr; }
    virtual Qt3DCore::QEntity *createEntity( const Qgs3DMapSettings &map ) const override;
    virtual void writeXml( QDomElement &, const QgsReadWriteContext & ) const override {}
    virtual void readXml( const QDomElement &, const QgsReadWriteContext & ) override {}

    QString mUri;
    mutable SceneContext mCtx;
};

#endif
