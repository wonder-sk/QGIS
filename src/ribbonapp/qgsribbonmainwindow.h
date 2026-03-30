#ifndef QGSRIBBONMAINWINDOW_H
#define QGSRIBBONMAINWINDOW_H

#include "qgsmaptoolidentify.h"

#include <QMainWindow>

class SARibbonBar;
class QgsMapCanvas;
class QgsLayerTreeView;
class QgsLayerTreeMapCanvasBridge;
class QgsMapToolPan;
class QgsIdentifyPanel;

// Thin subclass to capture identify results via signal
class QgsRibbonIdentifyTool : public QgsMapToolIdentify
{
    Q_OBJECT
  public:
    explicit QgsRibbonIdentifyTool( QgsMapCanvas *canvas )
      : QgsMapToolIdentify( canvas )
    {}
    void canvasReleaseEvent( QgsMapMouseEvent *e ) override;
  signals:
    void resultsReady( const QList<QgsMapToolIdentify::IdentifyResult> &results );
};

class QgsRibbonMainWindow : public QMainWindow
{
    Q_OBJECT
  public:
    explicit QgsRibbonMainWindow( QWidget *parent = nullptr );
    ~QgsRibbonMainWindow() override;

  private slots:
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onActivatePan();
    void onZoomToProjectExtent();
    void onActivateIdentify();
    void onProjectRead();
    void onProjectCleared();

  private:
    void setupRibbon();
    void setupCentralWidget();
    void setupDockPanels();
    void setupMapTools();
    void connectProjectSignals();
    void rebuildLayerTreeModel();

    SARibbonBar *mRibbonBar = nullptr;
    QgsMapCanvas *mMapCanvas = nullptr;
    QgsLayerTreeView *mLayerTreeView = nullptr;
    QgsLayerTreeMapCanvasBridge *mBridge = nullptr;
    QgsIdentifyPanel *mIdentifyPanel = nullptr;
    QgsMapToolPan *mPanTool = nullptr;
    QgsRibbonIdentifyTool *mIdentifyTool = nullptr;
};

#endif // QGSRIBBONMAINWINDOW_H
