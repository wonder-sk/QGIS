#include "qgsapplication.h"
#include "qgsribbonmainwindow.h"

#include <QApplication>
#include <QStyleHints>
#include <QSurfaceFormat>

int main( int argc, char *argv[] )
{
  QApplication::setAttribute( Qt::AA_DontUseNativeMenuBar ); // macOS: show ribbon in window

  QSurfaceFormat format;
  format.setDepthBufferSize( 24 );
  format.setStencilBufferSize( 8 );
  QSurfaceFormat::setDefaultFormat( format );

  // Restrict provider registry to actual data providers only.
  // The plugin directory also contains GUI plugins (e.g. libplugin_geometrychecker) that
  // link against a different build-suffix variant of qgis_core/qgis_gui. Loading them would
  // drag in a second copy of the QGIS libraries whose static initializers then conflict with
  // the already-initialized settings tree, causing a QgsSettingsException on startup.
  if ( qEnvironmentVariableIsEmpty( "QGIS_PROVIDER_FILE" ) )
    qputenv( "QGIS_PROVIDER_FILE", "libprovider_(?!.*rd\\.)" );

  QgsApplication app( argc, argv, true );
  app.styleHints()->setColorScheme( Qt::ColorScheme::Light );
  QgsApplication::init();
  QgsApplication::initQgis();

  QgsRibbonMainWindow window;
  window.show();

  const int retval = app.exec();
  QgsApplication::exitQgis();
  return retval;
}
