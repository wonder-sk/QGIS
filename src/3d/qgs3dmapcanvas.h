/***************************************************************************
  qgs3dmapcanvas.h
  --------------------------------------
  Date                 : July 2017
  Copyright            : (C) 2017 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGS3DMAPCANVAS_H
#define QGS3DMAPCANVAS_H

#include "qgis.h"
#include "qgis_3d.h"
#include "qgscrosssection.h"
#include "qgsrange.h"
#include "qgsraycastresult.h"

#include <QElapsedTimer>
#include <QtGui/QWindow>

class Qgs3DMapScene;
class Qgs3DMapSceneEntity;
class Qgs3DMapSettings;
class Qgs3DMapTool;
class QgsRhi3DEngine;
class QgsCameraController;
class QgsFeature;
class QgsMapLayer;
class QgsPointXY;
class QgsRayCastContext;
class QgsRectangle;
class QgsRubberBand3D;
class QgsTemporalController;


/**
 * \ingroup qgis_3d
 * \brief Convenience wrapper to simplify the creation of a 3D window ready to be used with QGIS.
 *
 * \since QGIS 3.36
 */
class _3D_EXPORT Qgs3DMapCanvas : public QWindow
{
    Q_OBJECT
  public:
    Qgs3DMapCanvas();
    ~Qgs3DMapCanvas() override;

    //! Returns access to the 3D scene configuration
    Qgs3DMapSettings *mapSettings() { return mMapSettings; }

    //! Returns access to the 3D scene (root 3D entity)
    Qgs3DMapScene *scene() { return mScene; }

    //! Returns access to the view's camera controller. Returns NULLPTR if the scene has not been initialized yet with setMapSettings()
    QgsCameraController *cameraController();

    /**
     * Sets the active map \a tool that will receive events from the 3D canvas. Does not transfer ownership.
     * If the tool is NULLPTR, events will be used for camera manipulation.
     * \since QGIS 4.0
     */
    void setMapTool( Qgs3DMapTool *tool );

    /**
     * Returns the active map tool that will receive events from the 3D canvas.
     * If the tool is NULLPTR, events will be used for camera manipulation.
     * \since QGIS 4.0
     */
    Qgs3DMapTool *mapTool() const { return mMapTool; }

    /**
     * Casts a ray towards the 3d scene and returns information about the intersected 3d entities.
     * \param screenPoint The ray starts from the current camera center and goes through this point (in pixel coordinates, originating at top left corner).
     * \param context A context object defining parameters for the ray casting.
     * \since QGIS 4.0
     */
    QgsRayCastResult castRay( const QPoint &screenPoint, QgsRayCastContext context );

    /**
     * Returns TRUE if the cross section mode is enabled or the 3d scene has other clipping planes applied
     *
     * \since QGIS 4.0
     */
    bool crossSectionEnabled() const;

    /**
     * Sets the cross section definition for the 3D map canvas.
     * The 3D scene will be clipped by four clipping planes, defined by a cross section line segment and
     * two parallel segments at cross section half width to each side.
     * Passing an invalid cross section will disable the clipping.
     * \param crossSection The cross section definition
     * \since QGIS 4.0
     */
    void setCrossSection( const QgsCrossSection &crossSection );

    /**
     * Returns the current cross section definition for the 3D map canvas.
     * \since QGIS 4.0
     */
    QgsCrossSection crossSection() const { return mCrossSection; }

#ifndef SIP_RUN

    //! Configure map scene being displayed. Takes ownership.
    void setMapSettings( Qgs3DMapSettings *mapSettings );

    //! Resets camera position to the default: looking down at the origin of world coordinates
    void resetView();

    //! Sets camera position to look down at the given point (in map coordinates) in given distance from plane with zero elevation
    void setViewFromTop( const QgsPointXY &center, float distance, float rotation = 0 );

    //! Saves the current scene as an image
    void saveAsImage( const QString &fileName, const QString &fileFormat );

    //! Returns the 3D engine
    QgsRhi3DEngine *engine() const { return mEngine; }

    /**
     * Sets the temporal controller
     */
    void setTemporalController( QgsTemporalController *temporalController );

    /**
     * Resets camera view to show the \a extent (top view)
     */
    void setViewFrom2DExtent( const QgsRectangle &extent );

    /**
     * Calculates the 2D extent viewed by the 3D camera as the vertices of the viewed trapezoid
     */
    QVector<QgsPointXY> viewFrustum2DExtent();

  signals:
    //! Emitted when the 3D map canvas was successfully saved as image
    void savedAsImage( const QString &fileName );

    //! Emitted when the map setting is changed
    void mapSettingsChanged();

    //! Emitted when the FPS count changes (at most every frame)
    void fpsCountChanged( float fpsCount );

    //! Emitted when the FPS counter is enabled or disabeld
    void fpsCounterEnabledChanged( bool enabled );

    //! Emitted when the viewed 2D extent seen by the 3D camera has changed
    void viewed2DExtentFrom3DChanged( QVector<QgsPointXY> extent );

    //! Emitted when the camera navigation \a speed is changed.
    void cameraNavigationSpeedChanged( double speed );

#endif
    /**
     *  Emitted when the cross section mode is enabled or disabled
     *  \since QGIS 4.0
     */
    void crossSectionEnabledChanged( bool enabled );

  private slots:
    void renderFrame();
    void captureDepthBuffer();
    void updateTemporalRange( const QgsDateTimeRange &timeRange );
    void onNavigationModeChanged( Qgis::NavigationMode mode );

  protected:
    //! Starts and stops the QRhi frame loop as the window is shown and hidden
    void exposeEvent( QExposeEvent *e ) override;

    /**
     * Resets the aspect ratio of the 3D window.
     */
    void resizeEvent( QResizeEvent * ) override;

    bool eventFilter( QObject *watched, QEvent *event ) override;

  private:
    QgsRhi3DEngine *mEngine = nullptr;
    //! Drives the frame loop: there is no aspect engine to tick the scene
    QTimer *mFrameTimer = nullptr;
    //! When the previous frame was rendered, used to work out the frame time
    QElapsedTimer mFrameElapsedTimer;

    //! Description of the 3D scene
    Qgs3DMapSettings *mMapSettings = nullptr;
    //! Root entity of the 3D scene
    Qgs3DMapScene *mScene = nullptr;

    //! Active map tool that receives events (if NULLPTR then mouse/keyboard events are used for camera manipulation)
    Qgs3DMapTool *mMapTool = nullptr;

    QString mCaptureFileName;
    QString mCaptureFileFormat;

    QgsTemporalController *mTemporalController = nullptr;

    QgsCrossSection mCrossSection;
};

#endif //QGS3DMAPCANVAS_H
