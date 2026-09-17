/***************************************************************************
  qgsrhi3dengine.cpp
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

#include "qgsrhi3dengine.h"

#include "qgs3dmapscene.h"
#include "qgs3dmapsettings.h"
#include "qgscamera3d.h"
#include "qgscameracontroller.h"
#include "qgsdirectionallightsettings.h"
#include "qgslogger.h"
#include "qgspointlightsettings.h"
#include "qgsshadowsettings.h"
#include "qgssunlightsettings.h"

#include <QOffscreenSurface>
#include <QString>
#include <QWindow>

#include "moc_qgsrhi3dengine.cpp"

using namespace Qt::StringLiterals;

QgsRhi3DEngine::QgsRhi3DEngine( QWindow *window, QObject *parent )
  : QObject( parent )
  , mWindow( window )
{
  mRhiEngine = std::make_unique<QgsRhiEngine>();
  mSceneRenderer = std::make_unique<QgsSceneRenderer>();
}

QgsRhi3DEngine::QgsRhi3DEngine( QSize size, QObject *parent )
  : QgsRhi3DEngine( static_cast<QWindow *>( nullptr ), parent )
{
  mOffscreen = true;
  mSize = size;
}

QgsRhi3DEngine::~QgsRhi3DEngine()
{
  // the passes hold GPU resources, so they have to go while the device is still alive
  if ( mRhiEngine && mRhiEngine->isValid() )
    mSceneRenderer->releaseGpuResources();

  mSceneRenderer.reset();
  mRhiEngine.reset();
}

void QgsRhi3DEngine::prepareWindow( QWindow *window )
{
  QgsRhiEngine::prepareWindow( window, QgsRhiEngine::defaultBackend() );
}

bool QgsRhi3DEngine::initialize()
{
  if ( isValid() )
    return true;

  if ( mOffscreen )
  {
    if ( !mRhiEngine->createOffscreen( mSize, QgsRhiEngine::defaultBackend() ) )
    {
      QgsDebugError( u"Could not create the QRhi graphics device for offscreen 3D rendering"_s );
      return false;
    }
    mRhiEngine->setReadbackEnabled( true );
  }
  else
  {
    if ( !mWindow || !mWindow->isExposed() )
      return false;

    if ( !mRhiEngine->createForWindow( mWindow, QgsRhiEngine::defaultBackend() ) )
    {
      QgsDebugError( u"Could not create the QRhi graphics device for the 3D view"_s );
      return false;
    }

    mSize = mWindow->size() * mWindow->devicePixelRatio();
  }

  QgsDebugMsgLevel( u"3D view is using the %1 backend"_s.arg( mRhiEngine->backendName() ), 1 );

  if ( !mSceneRenderer->prepare( mRhiEngine.get(), mSize ) )
  {
    QgsDebugError( u"Could not prepare the 3D scene renderer"_s );
    return false;
  }

  emit sizeChanged();
  return true;
}

void QgsRhi3DEngine::setClearColor( const QColor &color )
{
  mSceneRenderer->setClearColor( color );
}

QSize QgsRhi3DEngine::size() const
{
  return mWindow ? mWindow->size() : mSize;
}

QImage QgsRhi3DEngine::renderedImage() const
{
  return mRhiEngine ? mRhiEngine->readbackImage() : QImage();
}

void QgsRhi3DEngine::setSize( QSize s )
{
  if ( s.isEmpty() )
    return;

  const QSize pixelSize = s * ( mWindow ? mWindow->devicePixelRatio() : 1.0 );
  if ( pixelSize == mSize && isValid() )
  {
    emit sizeChanged();
    return;
  }

  mSize = pixelSize;

  if ( isValid() )
  {
    mRhiEngine->setOutputSize( pixelSize );
    mSceneRenderer->prepare( mRhiEngine.get(), pixelSize );
  }

  emit sizeChanged();
}

QSurface *QgsRhi3DEngine::surface() const
{
  return mWindow;
}

void QgsRhi3DEngine::requestCaptureImage()
{
  if ( !mOffscreen )
  {
    // reading a swapchain's backbuffer back is not something QRhi offers; capturing a
    // windowed view means rendering the same scene once more through an offscreen
    // engine, which is what Qgs3DUtils::captureSceneImage() already does
    QgsDebugError( u"Capturing an image of a windowed QRhi 3D view is not supported"_s );
    return;
  }

  mImageCaptureRequested = true;
}

void QgsRhi3DEngine::requestDepthBufferCapture()
{
  mDepthCaptureRequested = true;
}

void QgsRhi3DEngine::updateFrameSettings()
{
  const Qgs3DMapSettings *map = mScene->mapSettings();
  const QgsCamera3D *camera = mScene->cameraController()->camera();

  mSceneRenderer->setCamera( camera->viewMatrix(), camera->projectionMatrix(), camera->position(), camera->fieldOfView(), camera->nearPlane(), camera->farPlane() );

  QgsLightUniforms lights;
  const QList<QgsLightSource *> sources = map->lightSources();
  QVector3D sunDirection;
  int sunIndex = -1;
  for ( const QgsLightSource *source : sources )
  {
    switch ( source->type() )
    {
      case Qgis::LightSourceType::Point:
      {
        const QgsPointLightSettings *light = static_cast<const QgsPointLightSettings *>( source );
        lights.addPointLight(
          ( light->position() - map->origin() ).toVector3D(),
          light->color(),
          static_cast<float>( light->intensity() ),
          static_cast<float>( light->constantAttenuation() ),
          static_cast<float>( light->linearAttenuation() ),
          static_cast<float>( light->quadraticAttenuation() )
        );
        break;
      }
      case Qgis::LightSourceType::Directional:
      {
        const QgsDirectionalLightSettings *light = static_cast<const QgsDirectionalLightSettings *>( source );
        const QVector3D direction = light->direction().toVector3D();
        if ( sunIndex < 0 )
        {
          sunIndex = lights.lightCount;
          sunDirection = direction;
        }
        lights.addDirectionalLight( direction, light->color(), static_cast<float>( light->intensity() ) );
        break;
      }
      case Qgis::LightSourceType::Sun:
      {
        const QgsSunLightSettings *light = static_cast<const QgsSunLightSettings *>( source );
        const QVector3D direction = light->direction( *map ).toVector3D();
        if ( sunIndex < 0 )
        {
          sunIndex = lights.lightCount;
          sunDirection = direction;
        }
        lights.addDirectionalLight( direction, light->color(), static_cast<float>( light->intensity() ) );
        break;
      }
    }
  }
  mSceneRenderer->setLights( lights );

  mSceneRenderer->setClearColor( map->backgroundColor() );

  // shadows need a directional light to cast from, so they are off without one
  const QgsShadowSettings &shadows = map->shadowSettings();
  mSceneRenderer->setShadowsEnabled(
    shadows.renderShadows() && sunIndex >= 0,
    sunDirection,
    std::max( 0, sunIndex ),
    QgsShadowSettings::qualityToMapResolution( shadows.shadowQuality() ),
    static_cast<float>( shadows.maximumShadowRenderingDistance() ),
    static_cast<float>( shadows.shadowBias() )
  );

  const QgsAmbientOcclusionSettings &occlusion = map->ambientOcclusionSettings();
  mSceneRenderer->setAmbientOcclusionEnabled( occlusion.isEnabled(), static_cast<float>( occlusion.intensity() ), static_cast<float>( occlusion.radius() ), static_cast<float>( occlusion.threshold() ) );

  const QgsBloomSettings &bloom = map->bloomSettings();
  mSceneRenderer->setBloomEnabled( bloom.isEnabled(), static_cast<float>( bloom.intensity() ), static_cast<float>( bloom.radius() ) / 1000.0f );

  mSceneRenderer->setEyeDomeLightingEnabled( map->eyeDomeLightingEnabled(), static_cast<float>( map->eyeDomeLightingStrength() ), static_cast<float>( map->eyeDomeLightingDistance() ) );

  const QgsColorGradingSettings &grading = map->colorGradingSettings();
  mSceneRenderer->setColorGrading( static_cast<float>( grading.exposureAdjustment() ), grading.toneMapping() );

  // the depth buffer is only read back when somebody asked for it, since reading it stalls
  mSceneRenderer->setDepthReadbackEnabled( mDepthCaptureRequested );
}

bool QgsRhi3DEngine::renderFrame()
{
  if ( !mScene || !initialize() )
    return false;

  updateFrameSettings();

  mSceneRenderer->clearCommands();
  mScene->collectRenderCommands( *mSceneRenderer );

  if ( !mRhiEngine->beginFrame() )
    return false;

  const bool rendered = mSceneRenderer->renderFrame( mRhiEngine.get() );
  mRhiEngine->endFrame();

  // An offscreen frame has finished by the time endFrame() returns, so its readbacks are
  // ready; a swapchain frame has only been submitted, so anything read back in it has to
  // be waited for. Camera navigation depends on the depth buffer arriving, and it would
  // arrive empty without this.
  if ( !mOffscreen && mDepthCaptureRequested )
    mRhiEngine->rhi()->finish();

  mSceneRenderer->finishFrame( mRhiEngine.get() );

  if ( mDepthCaptureRequested )
  {
    mDepthCaptureRequested = false;
    const QImage depth = mSceneRenderer->depthImage();
    if ( !depth.isNull() )
      emit depthBufferCaptured( depth );
  }

  if ( mImageCaptureRequested )
  {
    mImageCaptureRequested = false;
    const QImage image = mRhiEngine->readbackImage();
    if ( !image.isNull() )
      emit imageCaptured( image );
  }

  return rendered;
}
