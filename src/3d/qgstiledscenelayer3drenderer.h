
#ifndef QGSTILEDSCENELAYER3DRENDERER_H
#define QGSTILEDSCENELAYER3DRENDERER_H

#include "qgis_3d.h"

#include "qgs3drendererregistry.h"
#include "qgsabstract3drenderer.h"
#include "qgsmaplayerref.h"

#include "qgstiledscenechunkloader.h"

class QgsTiledSceneLayer;


/**
 * \ingroup 3d
 * \brief Metadata for tiled scene layer 3D renderer to allow creation of its instances from XML
 *
 * \note Not available in Python bindings
 *
 * \since QGIS 3.34
 */
class _3D_EXPORT QgsTiledSceneLayer3DRendererMetadata : public Qgs3DRendererAbstractMetadata SIP_SKIP
{
  public:
    QgsTiledSceneLayer3DRendererMetadata();

    //! Creates an instance of a 3D renderer based on a DOM element with renderer configuration
    QgsAbstract3DRenderer *createRenderer( QDomElement &elem, const QgsReadWriteContext &context ) override SIP_FACTORY;
};


/**
 * \ingroup 3d
 * \brief 3D renderer that renders content of a tiled scene layer
 *
 * \since QGIS 3.34
 */
class _3D_EXPORT QgsTiledSceneLayer3DRenderer : public QgsAbstract3DRenderer
{
  public:
    QgsTiledSceneLayer3DRenderer();

    //! Sets tiled scene layer associated with the renderer
    void setLayer( QgsTiledSceneLayer *layer );
    //! Returns tiled scene layer associated with the renderer
    QgsTiledSceneLayer *layer() const;

    /**
     * Returns the maximum screen error allowed when rendering the tiled scene.
     *
     * Larger values result in a faster render with less content rendered.
     *
     * \see setMaximumScreenError()
     */
    double maximumScreenError() const;

    /**
     * Sets the maximum screen \a error allowed when rendering the tiled scene.
     *
     * Larger values result in a faster render with less content rendered.
     *
     * \see maximumScreenError()
     */
    void setMaximumScreenError( double error );

    /**
     * Returns whether bounding boxes will be visible when rendering the tiled scene.
     *
     * \see setShowBoundingBoxes()
     */
    bool showBoundingBoxes() const;

    /**
     * Sets whether bounding boxes will be visible when rendering the tiled scene.
     *
     * \see showBoundingBoxes()
     */
    void setShowBoundingBoxes( bool showBoundingBoxes );

    virtual QString type() const override { return "tiledscene"; }
    virtual QgsAbstract3DRenderer *clone() const override;
    virtual Qt3DCore::QEntity *createEntity( const Qgs3DMapSettings &map ) const override;
    virtual void writeXml( QDomElement &elem, const QgsReadWriteContext &context ) const override;
    virtual void readXml( const QDomElement &elem, const QgsReadWriteContext &context ) override;
    virtual void resolveReferences( const QgsProject &project ) override;

  private:
    QgsMapLayerRef mLayerRef; //!< Layer used to extract mesh data from
    double mMaximumScreenError = 50.0;
    bool mShowBoundingBoxes = false;
};

#endif
