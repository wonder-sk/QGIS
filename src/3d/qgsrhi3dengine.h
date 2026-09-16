/***************************************************************************
  qgsrhi3dengine.h
  --------------------------------------
  Date                 : September 2026
  Copyright            : (C) 2026 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSRHI3DENGINE_H
#define QGSRHI3DENGINE_H

#include <memory>

#include "qgsabstract3dengine.h"
#include "qgsrhiengine.h"
#include "qgsscenerenderer.h"

#include <QImage>
#include <QSize>

#define SIP_NO_FILE

class Qgs3DMapScene;
class QOffscreenSurface;
class QWindow;

namespace Qt3DCore
{
  class QEntity;
}

/**
 * \ingroup qgis_3d
 * \brief 3D engine that draws a scene with QRhi instead of Qt3D.
 *
 * This is the third QgsAbstract3DEngine, next to QgsWindow3DEngine and
 * QgsOffscreen3DEngine, and the one meant to outlive them. It owns a QgsRhiEngine, which
 * is the graphics device and the swapchain, and a QgsSceneRenderer, which is the frame.
 * renderFrame() asks the scene for the commands each pass should draw and runs them.
 *
 * It still answers the Qt3D half of QgsAbstract3DEngine - render settings, a camera, a
 * frame graph, a root entity - because Qgs3DMapScene is still partly written against
 * those. Nothing renders them: they are there so that the parts of the scene that have
 * not been ported yet still have something to talk to, and they go when Qt3D does.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsRhi3DEngine : public QgsAbstract3DEngine
{
    Q_OBJECT

  public:
    /**
     * Constructs an engine drawing into \a window.
     *
     * The window must already have been passed to prepareWindow(), before it was shown.
     * The graphics device is not created until initialize() is called, which has to
     * happen once the window is exposed.
     */
    explicit QgsRhi3DEngine( QWindow *window, QObject *parent = nullptr );

    /**
     * Constructs an engine rendering offscreen, into a texture of \a size pixels.
     *
     * This is the plain path for the QRhi engine rather than the awkward one: it needs no
     * window, no surface and no particular platform plugin, so image export, layout 3D
     * maps and the rendering tests all work headlessly. renderFrame() reads the result
     * back, which renderedImage() returns.
     */
    explicit QgsRhi3DEngine( QSize size, QObject *parent = nullptr );

    ~QgsRhi3DEngine() override;

    /**
     * Returns whether the QRhi engine should be used for 3D views.
     *
     * Set QGIS_3D_RHI to a true value to switch a 3D view over to it. This is how the new
     * engine is tried out while Qt3D still draws by default; the variable goes away once
     * the port is finished and the QRhi engine is the only one.
     */
    static bool isEnabled();

    /**
     * Configures \a window so that a swapchain can be created for it.
     *
     * Must be called before the window is shown, since it sets the surface type.
     */
    static void prepareWindow( QWindow *window );

    /**
     * Creates the graphics device and the swapchain for the window.
     *
     * Must be called once the window is exposed, and returns TRUE on success. Calling it
     * again once it has succeeded does nothing.
     */
    bool initialize();

    //! Returns whether the graphics device has been created
    bool isValid() const { return mRhiEngine && mRhiEngine->isValid(); }

    //! Returns the graphics device and swapchain, or NULLPTR before initialize() succeeded
    QgsRhiEngine *rhiEngine() { return mRhiEngine.get(); }

    //! Returns the frame the engine renders
    QgsSceneRenderer *sceneRenderer() { return mSceneRenderer.get(); }

    //! Returns the image of the last frame an offscreen engine rendered, or a null image
    QImage renderedImage() const;

    //! Sets the scene that produces the commands each frame draws. Does not take ownership.
    void setScene( Qgs3DMapScene *scene ) { mScene = scene; }

    /**
     * Renders one frame of the scene set by setScene().
     *
     * Does nothing if the device does not exist yet or the window is not exposed.
     * Returns TRUE if a frame was rendered.
     */
    bool renderFrame();

    void setClearColor( const QColor &color ) override;
    void setFrustumCullingEnabled( bool enabled ) override;
    void setRootEntity( Qt3DCore::QEntity *root ) override;
    Qt3DRender::QRenderSettings *renderSettings() override;
    Qt3DRender::QCamera *camera() override;
    QSize size() const override;
    void setSize( QSize s ) override;
    QSurface *surface() const override;
    QWindow *window() override { return mWindow; }

    void requestCaptureImage() override;
    void requestDepthBufferCapture() override;

  private:
    //! Copies the scene's camera, lights and effect settings onto the frame
    void updateFrameSettings();

    QWindow *mWindow = nullptr;
    //! TRUE when the engine renders into a texture rather than into a window
    bool mOffscreen = false;
    Qgs3DMapScene *mScene = nullptr;
    QSize mSize { 640, 480 };

    std::unique_ptr<QgsRhiEngine> mRhiEngine;
    std::unique_ptr<QgsSceneRenderer> mSceneRenderer;

    //! TRUE while a depth buffer has been asked for and not yet delivered
    bool mDepthCaptureRequested = false;
    //! TRUE while an image of the finished frame has been asked for and not yet delivered
    bool mImageCaptureRequested = false;

    // The Qt3D half of QgsAbstract3DEngine. Nothing renders any of this; it exists so
    // that the not yet ported parts of Qgs3DMapScene have something to talk to.
    std::unique_ptr<QOffscreenSurface> mQt3DSurface;
    Qt3DCore::QEntity *mQt3DRoot = nullptr;
    Qt3DCore::QEntity *mQt3DSceneRoot = nullptr;
    Qt3DRender::QCamera *mQt3DCamera = nullptr;
    Qt3DRender::QRenderSettings *mQt3DRenderSettings = nullptr;
};

#endif // QGSRHI3DENGINE_H
