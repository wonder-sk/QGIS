/***************************************************************************
  qgs3dmapcanvas.cpp
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

#include "qgs3dmapcanvas.h"

#include "qgs3dmapscene.h"
#include "qgs3dmapsettings.h"
#include "qgs3dmaptool.h"
#include "qgs3dutils.h"
#include "qgscamera3d.h"
#include "qgscameracontroller.h"
#include "qgsraycastcontext.h"
#include "qgsrhi3dengine.h"
#include "qgstemporalcontroller.h"

#include <QMouseEvent>
#include <QString>
#include <QTimer>

#include "moc_qgs3dmapcanvas.cpp"

using namespace Qt::StringLiterals;

Qgs3DMapCanvas::Qgs3DMapCanvas()
{
  // the surface type depends on the graphics backend, and has to be set before showing
  QgsRhi3DEngine::prepareWindow( this );
  mEngine = new QgsRhi3DEngine( this, this );

  // there is no aspect engine to tick the scene, so the canvas drives the frame loop
  mFrameTimer = new QTimer( this );
  mFrameTimer->setInterval( 16 );
  connect( mFrameTimer, &QTimer::timeout, this, &Qgs3DMapCanvas::renderFrame );

  connect( mEngine, &QgsRhi3DEngine::imageCaptured, this, [this]( const QImage &image ) {
    if ( image.save( mCaptureFileName, mCaptureFileFormat.toLocal8Bit().data() ) )
    {
      emit savedAsImage( mCaptureFileName );
    }
  } );

  setCursor( Qt::OpenHandCursor );
  installEventFilter( this );
}

Qgs3DMapCanvas::~Qgs3DMapCanvas()
{
  if ( mMapTool )
    delete mMapTool;
  // make sure the scene is deleted while map settings object is still alive
  mScene->deleteLater();
  mScene = nullptr;
  mMapSettings->deleteLater();
  mMapSettings = nullptr;
}

void Qgs3DMapCanvas::exposeEvent( QExposeEvent *e )
{
  QWindow::exposeEvent( e );

  if ( isExposed() )
  {
    mFrameElapsedTimer.start();
    mFrameTimer->start();
    renderFrame();
  }
  else
  {
    mFrameTimer->stop();
  }
}

void Qgs3DMapCanvas::renderFrame()
{
  if ( !mScene || !isExposed() )
    return;

  const float dt = mFrameElapsedTimer.isValid() ? static_cast<float>( mFrameElapsedTimer.restart() ) / 1000.0f : 0.0f;
  mScene->onFrameTriggered( dt );
  mEngine->renderFrame();
}

void Qgs3DMapCanvas::resizeEvent( QResizeEvent * )
{
  mEngine->setSize( size() );
}

void Qgs3DMapCanvas::setMapSettings( Qgs3DMapSettings *mapSettings )
{
  // TODO: eventually we want to get rid of this
  Q_ASSERT( !mMapSettings );
  Q_ASSERT( !mScene );

  Qgs3DMapScene *newScene = new Qgs3DMapScene( *mapSettings, mEngine );

  mEngine->setSize( size() );

  if ( mScene )
  {
    mScene->deleteLater();
  }
  mScene = newScene;
  connect( mScene, &Qgs3DMapScene::fpsCountChanged, this, &Qgs3DMapCanvas::fpsCountChanged );
  connect( mScene, &Qgs3DMapScene::fpsCounterEnabledChanged, this, &Qgs3DMapCanvas::fpsCounterEnabledChanged );
  connect( mScene, &Qgs3DMapScene::viewed2DExtentFrom3DChanged, this, &Qgs3DMapCanvas::viewed2DExtentFrom3DChanged );

  mEngine->setScene( mScene );

  delete mMapSettings;
  mMapSettings = mapSettings;

  resetView();

  connect( cameraController(), &QgsCameraController::setCursorPosition, this, [this]( QPoint point ) { QCursor::setPos( mapToGlobal( point ) ); } );
  connect( cameraController(), &QgsCameraController::cameraMovementSpeedChanged, mMapSettings, &Qgs3DMapSettings::setCameraMovementSpeed );
  connect( cameraController(), &QgsCameraController::cameraMovementSpeedChanged, this, &Qgs3DMapCanvas::cameraNavigationSpeedChanged );
  connect( cameraController(), &QgsCameraController::navigationModeChanged, this, &Qgs3DMapCanvas::onNavigationModeChanged );
  connect( cameraController(), &QgsCameraController::requestDepthBufferCapture, this, &Qgs3DMapCanvas::captureDepthBuffer );

  connect( mEngine, &QgsRhi3DEngine::depthBufferCaptured, cameraController(), &QgsCameraController::depthBufferCaptured );

  emit mapSettingsChanged();
}

QgsCameraController *Qgs3DMapCanvas::cameraController()
{
  return mScene ? mScene->cameraController() : nullptr;
}

QgsRayCastResult Qgs3DMapCanvas::castRay( const QPoint &screenPoint, QgsRayCastContext context )
{
  const QgsCamera3D *cam = mScene->cameraController()->camera();
  const QgsRay3D ray = Qgs3DUtils::rayFromScreenPoint( screenPoint, size(), cam );
  if ( context.maximumDistance() < 0 )
    context.setMaximumDistance( cam->farPlane() );
  const QgsRayCastResult res = Qgs3DUtils::castRay( mScene, ray, context );
  return res;
}

void Qgs3DMapCanvas::setCrossSection( const QgsCrossSection &crossSection )
{
  mCrossSection = crossSection;

  if ( !mScene )
    return;

  if ( !mCrossSection.isValid() )
  {
    mScene->disableClipping();
    emit crossSectionEnabledChanged( false );
    return;
  }

  const QgsPoint startPoint = mCrossSection.startPoint();
  const QgsPoint endPoint = mCrossSection.endPoint();
  const double width = mCrossSection.halfWidth();

  const QgsVector3D startVec { startPoint.x(), startPoint.y(), 0 };
  const QgsVector3D endVec { endPoint.x(), endPoint.y(), 0 };
  const QList<QVector4D> clippingPlanes = Qgs3DUtils::lineSegmentToClippingPlanes( startVec, endVec, width, mMapSettings->origin() );

  mScene->enableClipping( clippingPlanes );
  emit crossSectionEnabledChanged( true );
}


bool Qgs3DMapCanvas::crossSectionEnabled() const
{
  return mScene ? !mScene->clipPlaneEquations().isEmpty() : false;
}

void Qgs3DMapCanvas::resetView()
{
  if ( !mScene )
    return;

  mScene->viewZoomFull();
}

void Qgs3DMapCanvas::setViewFromTop( const QgsPointXY &center, float distance, float rotation )
{
  if ( !mScene )
    return;

  const float worldX = center.x() - mMapSettings->origin().x();
  const float worldY = center.y() - mMapSettings->origin().y();
  mScene->cameraController()->setViewFromTop( worldX, worldY, distance, rotation );
}

void Qgs3DMapCanvas::saveAsImage( const QString &fileName, const QString &fileFormat )
{
  if ( !mScene || fileName.isEmpty() )
    return;

  mCaptureFileName = fileName;
  mCaptureFileFormat = fileFormat;

  // the next frame the canvas renders hands the image back
  mEngine->requestCaptureImage();
}

void Qgs3DMapCanvas::captureDepthBuffer()
{
  if ( !mScene )
    return;

  mEngine->requestDepthBufferCapture();
}

void Qgs3DMapCanvas::setMapTool( Qgs3DMapTool *tool )
{
  if ( !mScene )
    return;

  if ( tool == mMapTool )
    return;

  // For Camera Control tool
  if ( mMapTool && !tool )
  {
    mScene->cameraController()->setInputHandlersEnabled( true );
    setCursor( Qt::OpenHandCursor );
  }

  if ( mMapTool )
    mMapTool->deactivate();

  mMapTool = tool;

  if ( mMapTool )
  {
    mMapTool->activate();
    setCursor( mMapTool->cursor() );
  }
}

bool Qgs3DMapCanvas::eventFilter( QObject *watched, QEvent *event )
{
  if ( watched != this )
    return false;

  // ShortcutOverride is sent if the pressed key is "claimed" by a shortcut,
  // but we are given a chance to handle it anyway. We need to basically treat
  // it as if it were a KeyPress.
  if ( event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease || event->type() == QEvent::ShortcutOverride )
  {
    // if the camera controller will handle a key event, don't allow it to propagate
    // outside of the 3d window or it may be grabbed by a parent window level shortcut
    // and accordingly never be received by the camera controller
    if ( cameraController() && cameraController()->keyboardEventFilter( static_cast<QKeyEvent *>( event ) ) )
    {
      event->accept();
      return true;
    }
  }

  if ( !mMapTool )
  {
    // With no map tool active, mouse input drives the camera: the events come straight
    // off this window and go to the camera controller.
    if ( QgsCameraController *controller = cameraController() )
    {
      switch ( event->type() )
      {
        case QEvent::MouseButtonPress:
          controller->handleMousePressEvent( static_cast<QMouseEvent *>( event ) );
          break;
        case QEvent::MouseButtonRelease:
          controller->handleMouseReleaseEvent( static_cast<QMouseEvent *>( event ) );
          break;
        case QEvent::MouseMove:
          controller->handleMouseMoveEvent( static_cast<QMouseEvent *>( event ) );
          break;
        case QEvent::Wheel:
          controller->handleWheelEvent( static_cast<QWheelEvent *>( event ) );
          break;
        default:
          break;
      }
    }
    return false;
  }

  switch ( event->type() )
  {
    case QEvent::MouseButtonPress:
      mMapTool->mousePressEvent( static_cast<QMouseEvent *>( event ) );
      break;
    case QEvent::MouseButtonRelease:
      mMapTool->mouseReleaseEvent( static_cast<QMouseEvent *>( event ) );
      break;
    case QEvent::MouseMove:
      mMapTool->mouseMoveEvent( static_cast<QMouseEvent *>( event ) );
      break;
    case QEvent::KeyPress:
      mMapTool->keyPressEvent( static_cast<QKeyEvent *>( event ) );
      break;
    case QEvent::KeyRelease:
      mMapTool->keyReleaseEvent( static_cast<QKeyEvent *>( event ) );
      break;
    case QEvent::Wheel:
      mMapTool->mouseWheelEvent( static_cast<QWheelEvent *>( event ) );
      break;
    default:
      break;
  }
  return false;
}

void Qgs3DMapCanvas::setTemporalController( QgsTemporalController *temporalController )
{
  if ( mTemporalController )
    disconnect( mTemporalController, &QgsTemporalController::updateTemporalRange, this, &Qgs3DMapCanvas::updateTemporalRange );

  mTemporalController = temporalController;
  connect( mTemporalController, &QgsTemporalController::updateTemporalRange, this, &Qgs3DMapCanvas::updateTemporalRange );
}

void Qgs3DMapCanvas::updateTemporalRange( const QgsDateTimeRange &temporalrange )
{
  if ( !mScene )
    return;

  mMapSettings->setTemporalRange( temporalrange );
  mScene->updateTemporal();
}

void Qgs3DMapCanvas::onNavigationModeChanged( Qgis::NavigationMode mode )
{
  mMapSettings->setCameraNavigationMode( mode );
}

void Qgs3DMapCanvas::setViewFrom2DExtent( const QgsRectangle &extent )
{
  if ( !mScene )
    return;

  mScene->setViewFrom2DExtent( extent );
}

QVector<QgsPointXY> Qgs3DMapCanvas::viewFrustum2DExtent()
{
  return mScene ? mScene->viewFrustum2DExtent() : QVector<QgsPointXY>();
}
