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

#include "qgis_3d.h"
#include "qgsrhiengine.h"
#include "qgsscenerenderer.h"

#include <QImage>
#include <QObject>
#include <QSize>

#define SIP_NO_FILE

class Qgs3DMapScene;
class QColor;
class QSurface;
class QWindow;

/**
 * \ingroup qgis_3d
 * \brief The 3D engine: a graphics device, a render target and the frame drawn into it.
 *
 * It owns a QgsRhiEngine, which is the graphics device and the swapchain or offscreen
 * target, and a QgsSceneRenderer, which is the frame. renderFrame() asks the scene for
 * the commands each pass should draw and runs them.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsRhi3DEngine : public QObject
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

    //! Sets the colour the view is cleared to before anything is drawn
    void setClearColor( const QColor &color );

    //! Returns the size of the rendering area, in pixels
    QSize size() const;

    //! Sets the size of the rendering area, in pixels
    void setSize( QSize s );

    //! Returns the surface the engine renders into
    QSurface *surface() const;

    //! Returns the window the engine renders into, or NULLPTR if it renders offscreen
    QWindow *window() { return mWindow; }

    /**
     * Asks for an image of the next frame, returned through imageCaptured().
     *
     * Only an offscreen engine can do this: QRhi cannot read a swapchain's backbuffer
     * back, so capturing a windowed view means rendering the scene again offscreen.
     */
    void requestCaptureImage();

    /**
     * Asks for the depth buffer of the next frame, returned through depthBufferCaptured().
     *
     * \note The depth values are encoded into RGB channels and should be decoded with
     * Qgs3DUtils::decodeDepth()
     */
    void requestDepthBufferCapture();

  signals:
    //! Emitted after a call to requestCaptureImage() to return the captured image
    void imageCaptured( const QImage &image );

    //! Emitted after a call to requestDepthBufferCapture() to return the captured depth buffer
    void depthBufferCaptured( const QImage &image );

    //! Emitted after a call to setSize()
    void sizeChanged();

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
};

#endif // QGSRHI3DENGINE_H
