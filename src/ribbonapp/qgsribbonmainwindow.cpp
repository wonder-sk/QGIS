#include "qgsribbonmainwindow.h"

#include "qgsidentifypanel.h"

#include <QString>

// SARibbon headers
#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include "SARibbonPannel.h"

#include "qgsapplication.h"
#include "qgsmapcanvas.h"
#include "qgslayertreeview.h"
#include "qgslayertreemodel.h"
#include "qgslayertreemapcanvasbridge.h"
#include "qgsmapmouseevent.h"
#include "qgsmaptoolpan.h"
#include "qgsproject.h"
#include "qgsprojectviewsettings.h"

#include <QDockWidget>
#include <QFileDialog>
#include <QFrame>
#include <QMessageBox>
#include <QToolBar>
#include <QToolButton>

using namespace Qt::StringLiterals;

// --- QgsRibbonIdentifyTool ---

void QgsRibbonIdentifyTool::canvasReleaseEvent( QgsMapMouseEvent *e )
{
  emit resultsReady( identify( e->x(), e->y(), TopDownAll ) );
}

// --- QgsRibbonMainWindow ---

QgsRibbonMainWindow::QgsRibbonMainWindow( QWidget *parent )
  : QMainWindow( parent )
{
  setWindowTitle( u"QGIS Pro"_s );
  resize( 1280, 800 );
  setupCentralWidget();
  setupRibbon();
  setupDockPanels();
  setupMapTools();
  connectProjectSignals();
  mMapCanvas->setMapTool( mPanTool );
}

QgsRibbonMainWindow::~QgsRibbonMainWindow() = default;

void QgsRibbonMainWindow::setupCentralWidget()
{
  mMapCanvas = new QgsMapCanvas( this );
  mMapCanvas->setProject( QgsProject::instance() );
  mMapCanvas->enableAntiAliasing( true );
  setCentralWidget( mMapCanvas );
}

void QgsRibbonMainWindow::setupRibbon()
{
  mRibbonBar = new SARibbonBar( this );
  setMenuBar( mRibbonBar );
  mRibbonBar->setTitleVisible( false );
  mRibbonBar->setApplicationButton( nullptr );

  // Large button: icon on top, label below
  auto makeLargeBtn = [this]( const QString &label, const QString &icon ) -> QToolButton * {
    auto *btn = new QToolButton( this );
    btn->setText( label );
    btn->setIcon( QgsApplication::getThemeIcon( icon ) );
    btn->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
    btn->setIconSize( QSize( 32, 32 ) );
    btn->setAutoRaise( true );
    btn->setStyleSheet( u"QToolButton { padding: 2px 10px; border: none; }"_s );
    return btn;
  };

  // Small button: icon only with tooltip
  auto makeSmallBtn = [this]( const QString &tooltip, const QString &icon ) -> QToolButton * {
    auto *btn = new QToolButton( this );
    btn->setToolTip( tooltip );
    btn->setIcon( QgsApplication::getThemeIcon( icon ) );
    btn->setToolButtonStyle( Qt::ToolButtonIconOnly );
    btn->setAutoRaise( true );
    btn->setStyleSheet( u"QToolButton { padding: 2px 5px; border: none; }"_s );
    return btn;
  };

  // ===== TAB 1: PROJECT =====
  auto *projectTab = mRibbonBar->addCategoryPage( tr( "Project" ) );

  // Group: Project
  {
    auto *panel = projectTab->addPannel( tr( "Project" ) );
    auto *btnNew = makeLargeBtn( tr( "New" ), u"/mActionFileNew.svg"_s );
    auto *btnOpen = makeLargeBtn( tr( "Open" ), u"/mActionFileOpen.svg"_s );
    auto *btnSave = makeLargeBtn( tr( "Save" ), u"/mActionFileSave.svg"_s );
    connect( btnNew, &QToolButton::clicked, this, &QgsRibbonMainWindow::onNewProject );
    connect( btnOpen, &QToolButton::clicked, this, &QgsRibbonMainWindow::onOpenProject );
    connect( btnSave, &QToolButton::clicked, this, &QgsRibbonMainWindow::onSaveProject );
    panel->addLargeWidget( btnNew );
    panel->addLargeWidget( btnOpen );
    panel->addLargeWidget( btnSave );
  }

  // Group: Print & Export
  {
    auto *panel = projectTab->addPannel( tr( "Print & Export" ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Print\nLayout" ), u"/mActionNewLayout.svg"_s ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Layout\nManager" ), u"/mActionLayoutManager.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Save as Image" ), u"/mActionSaveMapAsImage.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Save as PDF" ), u"/mActionSaveAsPDF.svg"_s ) );
  }

  // Group: Settings
  {
    auto *panel = projectTab->addPannel( tr( "Settings" ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Project\nProperties" ), u"/mActionProjectProperties.svg"_s ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Style\nManager" ), u"/mActionStyleManager.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Options" ), u"/mActionOptions.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Custom CRS" ), u"/mActionCustomProjection.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Keyboard Shortcuts" ), u"/mActionKeyboardShortcuts.svg"_s ) );
  }

  // ===== TAB 2: MAP =====
  auto *mapTab = mRibbonBar->addCategoryPage( tr( "Map" ) );

  // Group: Navigate
  {
    auto *panel = mapTab->addPannel( tr( "Navigate" ) );
    auto *btnPan = makeLargeBtn( tr( "Pan" ), u"/mActionPan.svg"_s );
    auto *btnZoomIn = makeLargeBtn( tr( "Zoom In" ), u"/mActionZoomIn.svg"_s );
    auto *btnZoomOut = makeLargeBtn( tr( "Zoom Out" ), u"/mActionZoomOut.svg"_s );
    connect( btnPan, &QToolButton::clicked, this, &QgsRibbonMainWindow::onActivatePan );
    panel->addLargeWidget( btnPan );
    panel->addLargeWidget( btnZoomIn );
    panel->addLargeWidget( btnZoomOut );
  }

  // Group: Zoom To
  {
    auto *panel = mapTab->addPannel( tr( "Zoom To" ) );
    auto *btnFull = makeSmallBtn( tr( "Full Extent" ), u"/mActionZoomFullExtent.svg"_s );
    connect( btnFull, &QToolButton::clicked, this, &QgsRibbonMainWindow::onZoomToProjectExtent );
    panel->addSmallWidget( btnFull );
    panel->addSmallWidget( makeSmallBtn( tr( "Layer Extent" ), u"/mActionZoomToLayer.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Selection" ), u"/mActionZoomToSelected.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Native Resolution" ), u"/mActionZoomActual.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Previous Extent" ), u"/mActionZoomLast.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Next Extent" ), u"/mActionZoomNext.svg"_s ) );
  }

  // Group: Inspect
  {
    auto *panel = mapTab->addPannel( tr( "Inspect" ) );
    auto *btnIdentify = makeLargeBtn( tr( "Identify" ), u"/mActionIdentify.svg"_s );
    connect( btnIdentify, &QToolButton::clicked, this, &QgsRibbonMainWindow::onActivateIdentify );
    panel->addLargeWidget( btnIdentify );
    panel->addLargeWidget( makeLargeBtn( tr( "Open\nTable" ), u"/mActionOpenTable.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Map Tips" ), u"/mActionMapTips.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Statistics" ), u"/mActionSum.svg"_s ) );
  }

  // Group: Measure
  {
    auto *panel = mapTab->addPannel( tr( "Measure" ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Measure\nLine" ), u"/mActionMeasure.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Measure Area" ), u"/mActionMeasureArea.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Measure Bearing" ), u"/mActionMeasureBearing.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Measure Angle" ), u"/mActionMeasureAngle.svg"_s ) );
  }

  // Group: Bookmarks
  {
    auto *panel = mapTab->addPannel( tr( "Bookmarks" ) );
    panel->addSmallWidget( makeSmallBtn( tr( "New Bookmark" ), u"/mActionNewBookmark.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Show Bookmarks" ), u"/mActionShowBookmarks.svg"_s ) );
  }

  // Group: Views
  {
    auto *panel = mapTab->addPannel( tr( "Views" ) );
    panel->addSmallWidget( makeSmallBtn( tr( "New Map Canvas" ), u"/mActionNewMap.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "New 3D Map" ), u"/mActionNew3DMap.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Temporal Controller" ), u"/mTemporalNavigationAnimated.svg"_s ) );
  }

  // ===== TAB 3: LAYERS =====
  auto *layersTab = mRibbonBar->addCategoryPage( tr( "Layers" ) );

  // Group: Add Data
  {
    auto *panel = layersTab->addPannel( tr( "Add Data" ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Data Source\nManager" ), u"/mActionDataSourceManager.svg"_s ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Add Vector\nLayer" ), u"/mActionAddOgrLayer.svg"_s ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Add Raster\nLayer" ), u"/mActionAddRasterLayer.svg"_s ) );
  }

  // Group: Add Web & DB
  {
    auto *panel = layersTab->addPannel( tr( "Add Web & DB" ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Add WMS/WMTS" ), u"/mActionAddWmsLayer.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Add WFS" ), u"/mActionAddWfsLayer.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Add WCS" ), u"/mActionAddWcsLayer.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Add PostGIS" ), u"/mActionAddPostgisLayer.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Add Mesh Layer" ), u"/mActionAddMeshLayer.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Add Point Cloud" ), u"/mActionAddPointCloudLayer.svg"_s ) );
  }

  // Group: New Layer
  {
    auto *panel = layersTab->addPannel( tr( "New Layer" ) );
    panel->addLargeWidget( makeLargeBtn( tr( "New\nGeoPackage" ), u"/mActionNewGeoPackageLayer.svg"_s ) );
    panel->addLargeWidget( makeLargeBtn( tr( "New\nShapefile" ), u"/mActionNewVectorLayer.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "New Scratch Layer" ), u"/mActionCreateMemory.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "New SpatiaLite Layer" ), u"/mActionNewSpatiaLiteLayer.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "New Mesh Layer" ), u"/mActionNewMeshLayer.svg"_s ) );
  }

  // Group: Manage
  {
    auto *panel = layersTab->addPannel( tr( "Manage" ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Layer Properties" ), u"/mIconProperties.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Set Layer CRS" ), u"/mActionSetProjection.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Filter Layer" ), u"/mActionFilter2.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Duplicate Layer" ), u"/mActionDuplicateLayer.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Remove Layer" ), u"/mActionRemoveLayer.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Save Layer As" ), u"/mActionFileSaveAs.svg"_s ) );
  }

  // ===== TAB 4: SELECT & ANALYZE =====
  auto *selectTab = mRibbonBar->addCategoryPage( tr( "Select & Analyze" ) );

  // Group: Select
  {
    auto *panel = selectTab->addPannel( tr( "Select" ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Select\nFeatures" ), u"/mActionSelectRectangle.svg"_s ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Select by\nExpression" ), u"/mIconExpressionSelect.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Select by Form" ), u"/mIconFormSelect.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Select All" ), u"/mActionSelectAll.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Invert Selection" ), u"/mActionInvertSelection.svg"_s ) );
  }

  // Group: Deselect
  {
    auto *panel = selectTab->addPannel( tr( "Deselect" ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Deselect All" ), u"/mActionDeselectAll.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Deselect Active Layer" ), u"/mActionDeselectActiveLayer.svg"_s ) );
  }

  // Group: Attributes
  {
    auto *panel = selectTab->addPannel( tr( "Attributes" ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Open\nTable" ), u"/mActionOpenTable.svg"_s ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Field\nCalculator" ), u"/mActionCalculateField.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Multi-Edit" ), u"/mActionMultiEdit.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Statistics" ), u"/mActionSum.svg"_s ) );
  }

  // Group: Labels
  {
    auto *panel = selectTab->addPannel( tr( "Labels" ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Layer\nLabeling" ), u"/mActionLabeling.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Pin Labels" ), u"/mActionPinLabels.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Show/Hide Labels" ), u"/mActionShowHideLabels.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Move Label" ), u"/mActionMoveLabel.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Rotate Label" ), u"/mActionRotateLabel.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Change Label Properties" ), u"/mActionChangeLabelProperties.svg"_s ) );
  }

  // ===== TAB 5: EDIT =====
  auto *editTab = mRibbonBar->addCategoryPage( tr( "Edit" ) );

  // Group: Session
  {
    auto *panel = editTab->addPannel( tr( "Session" ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Toggle\nEditing" ), u"/mActionToggleEditing.svg"_s ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Save\nEdits" ), u"/mActionSaveAllEdits.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Undo" ), u"/mActionUndo.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Redo" ), u"/mActionRedo.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "All Edits..." ), u"/mActionAllEdits.svg"_s ) );
  }

  // Group: Add Feature
  {
    auto *panel = editTab->addPannel( tr( "Add Feature" ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Add\nFeature" ), u"/mActionCapturePolygon.svg"_s ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Digitize\nMode" ), u"/mActionDigitizeWithSegment.svg"_s ) );
  }

  // Group: Vertex & Move
  {
    auto *panel = editTab->addPannel( tr( "Vertex & Move" ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Vertex\nTool" ), u"/mActionVertexTool.svg"_s ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Vertex Tool\n(Active)" ), u"/mActionVertexToolActiveLayer.svg"_s ) );
    panel->addLargeWidget( makeLargeBtn( tr( "Move\nFeature" ), u"/mActionMoveFeature.svg"_s ) );
  }

  // Group: Features
  {
    auto *panel = editTab->addPannel( tr( "Features" ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Delete Feature" ), u"/mActionDeleteSelectedFeatures.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Cut Features" ), u"/mActionEditCut.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Copy Features" ), u"/mActionEditCopy.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Paste Features" ), u"/mActionEditPaste.svg"_s ) );
  }

  // Group: Reshape
  {
    auto *panel = editTab->addPannel( tr( "Reshape" ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Reshape" ), u"/mActionReshape.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Split Features" ), u"/mActionSplitFeatures.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Split Parts" ), u"/mActionSplitParts.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Merge Features" ), u"/mActionMergeFeatures.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Merge Attributes" ), u"/mActionMergeFeatureAttributes.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Simplify" ), u"/mActionSimplify.svg"_s ) );
  }

  // Group: Topology
  {
    auto *panel = editTab->addPannel( tr( "Topology" ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Add Ring" ), u"/mActionAddRing.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Delete Ring" ), u"/mActionDeleteRing.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Fill Ring" ), u"/mActionFillRing.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Add Part" ), u"/mActionAddPart.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Delete Part" ), u"/mActionDeletePart.svg"_s ) );
  }

  // Group: Line Tools
  {
    auto *panel = editTab->addPannel( tr( "Line Tools" ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Rotate Feature" ), u"/mActionRotateFeature.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Scale Feature" ), u"/mActionScaleFeature.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Offset Curve" ), u"/mActionOffsetCurve.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Reverse Line" ), u"/mActionReverseLine.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Trim/Extend" ), u"/mActionTrimExtendFeature.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Chamfer/Fillet" ), u"/mActionChamferFillet.svg"_s ) );
  }

  // Group: Snapping
  {
    auto *panel = editTab->addPannel( tr( "Snapping" ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Enable Snapping" ), u"/mIconSnapping.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Snapping Options" ), u"/mIconSnappingAdvanced.svg"_s ) );
    panel->addSmallWidget( makeSmallBtn( tr( "Enable Tracing" ), u"/mActionTracing.svg"_s ) );
  }

  // 1-pixel separator line between ribbon and main content
  auto *sep = new QToolBar( this );
  sep->setMovable( false );
  sep->setFloatable( false );
  sep->setFixedHeight( 1 );
  sep->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
  sep->setStyleSheet( u"QToolBar { background-color: palette(mid); border: none; margin: 0; padding: 0; spacing: 0; }"_s );
  addToolBar( Qt::TopToolBarArea, sep );
}

void QgsRibbonMainWindow::setupDockPanels()
{
  // Left: layer tree
  auto *layerDock = new QDockWidget( tr( "Layers" ), this );
  layerDock->setObjectName( u"LayersDock"_s );
  mLayerTreeView = new QgsLayerTreeView( layerDock );
  rebuildLayerTreeModel();
  layerDock->setWidget( mLayerTreeView );
  addDockWidget( Qt::LeftDockWidgetArea, layerDock );

  // Right: identify results
  auto *identifyDock = new QDockWidget( tr( "Identify Results" ), this );
  identifyDock->setObjectName( u"IdentifyDock"_s );
  mIdentifyPanel = new QgsIdentifyPanel( identifyDock );
  identifyDock->setWidget( mIdentifyPanel );
  addDockWidget( Qt::RightDockWidgetArea, identifyDock );
}

void QgsRibbonMainWindow::rebuildLayerTreeModel()
{
  auto *model = new QgsLayerTreeModel( QgsProject::instance()->layerTreeRoot(), mLayerTreeView );
  model->setFlag( QgsLayerTreeModel::AllowNodeChangeVisibility );
  model->setFlag( QgsLayerTreeModel::ShowLegend );
  mLayerTreeView->setModel( model );

  delete mBridge;
  mBridge = new QgsLayerTreeMapCanvasBridge( QgsProject::instance()->layerTreeRoot(), mMapCanvas, this );
}

void QgsRibbonMainWindow::setupMapTools()
{
  mPanTool = new QgsMapToolPan( mMapCanvas );
  mIdentifyTool = new QgsRibbonIdentifyTool( mMapCanvas );
  connect( mIdentifyTool, &QgsRibbonIdentifyTool::resultsReady, mIdentifyPanel, &QgsIdentifyPanel::showResults );
}

void QgsRibbonMainWindow::connectProjectSignals()
{
  connect( QgsProject::instance(), &QgsProject::readProject, this, &QgsRibbonMainWindow::onProjectRead );
  connect( QgsProject::instance(), &QgsProject::cleared, this, &QgsRibbonMainWindow::onProjectCleared );
}

void QgsRibbonMainWindow::onNewProject()
{
  if ( QgsProject::instance()->isDirty() )
  {
    const int ret = QMessageBox::question( this, tr( "New Project" ), tr( "Save current project?" ), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel );
    if ( ret == QMessageBox::Cancel )
      return;
    if ( ret == QMessageBox::Yes )
      onSaveProject();
  }
  QgsProject::instance()->clear();
}

void QgsRibbonMainWindow::onOpenProject()
{
  const QString fileName = QFileDialog::getOpenFileName( this, tr( "Open QGIS Project" ), {}, tr( "QGIS Projects (*.qgs *.qgz);;All files (*)" ) );
  if ( fileName.isEmpty() )
    return;
  if ( !QgsProject::instance()->read( fileName ) )
    QMessageBox::critical( this, tr( "Open Project" ), tr( "Could not open project: %1" ).arg( fileName ) );
}

void QgsRibbonMainWindow::onSaveProject()
{
  QString fileName = QgsProject::instance()->fileName();
  if ( fileName.isEmpty() )
  {
    fileName = QFileDialog::getSaveFileName( this, tr( "Save QGIS Project" ), {}, tr( "QGIS Projects (*.qgs *.qgz)" ) );
    if ( fileName.isEmpty() )
      return;
  }
  if ( !QgsProject::instance()->write( fileName ) )
    QMessageBox::critical( this, tr( "Save Project" ), tr( "Could not save project: %1" ).arg( fileName ) );
}

void QgsRibbonMainWindow::onActivatePan()
{
  mMapCanvas->setMapTool( mPanTool );
}
void QgsRibbonMainWindow::onActivateIdentify()
{
  mMapCanvas->setMapTool( mIdentifyTool );
}

void QgsRibbonMainWindow::onZoomToProjectExtent()
{
  const QgsRectangle extent = mMapCanvas->fullExtent();
  if ( !extent.isEmpty() )
    mMapCanvas->zoomToFullExtent();
  else
    mMapCanvas->zoomToProjectExtent();
}

void QgsRibbonMainWindow::onProjectRead()
{
  rebuildLayerTreeModel();
  const QgsReferencedRectangle ext = QgsProject::instance()->viewSettings()->defaultViewExtent();
  if ( !ext.isEmpty() )
    mMapCanvas->setReferencedExtent( ext );
  else
    mMapCanvas->zoomToFullExtent();
  setWindowTitle( u"QGIS Pro \u2014 %1"_s.arg( QgsProject::instance()->title() ) );
}

void QgsRibbonMainWindow::onProjectCleared()
{
  rebuildLayerTreeModel();
  mIdentifyPanel->clear();
  setWindowTitle( u"QGIS Pro"_s );
}

#include "moc_qgsribbonmainwindow.cpp"
