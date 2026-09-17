/***************************************************************************
  qgsrubberband3d.h
  --------------------------------------
  Date                 : June 2021
  Copyright            : (C) 2021 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSRUBBERBAND3D_H
#define QGSRUBBERBAND3D_H


/// @cond PRIVATE

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QGIS API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//

#include "qgis_3d.h"
#include "qgsgeometry.h"
#include "qobjectuniqueptr.h"

#include <QColor>

#define SIP_NO_FILE

class Qgs3DMapScene;
class QgsGeometry;
class QgsRhi3DEngine;
class QgsPoint;
class Qgs3DMapSettings;
class QgsMarkerSymbol;

/**
 * \ingroup qgis_3d
 * Rubber band implementation for use in 3D map views.
 *
 * Coordinates are expected to be passed in map CRS.
 *
 * \note Currently only supports multi point, linestring and polygon geometry.
 * \warning Nothing is drawn: the rubber band needs thick lines and billboarded markers,
 * neither of which has been ported to the QRhi engine yet. The geometry it is given is
 * still tracked, so the map tools that drive it keep working.
 * \since QGIS 3.20
 */
class _3D_EXPORT QgsRubberBand3D
{
  public:
    //! Icons
    enum MarkerType
    {

      /**
       * A box is used to highlight points (□)
       */
      Square,

      /**
       * A circle is used to highlight points (○)
       */
      Circle
    };

    QgsRubberBand3D( Qgs3DMapScene *scene, Qgis::GeometryType geometryType = Qgis::GeometryType::Line );
    ~QgsRubberBand3D();

    //! Returns the rubber band width in pixels
    float width() const;

    //! Sets the rubber band width in pixels
    void setWidth( float width );

    //! Returns the rubber band color
    QColor color() const;

    //! Sets the rubber band color.
    void setColor( QColor color );

    /**
     * Returns the rubber band outline color
     * \since QGIS 3.44
     */
    QColor outlineColor() const;

    /**
     * Sets the rubber band outline color
     * \since QGIS 3.44
     */
    void setOutlineColor( QColor color );

    /**
     * Sets the \a marker type to highlight point geometries and line vertices.
     */
    void setMarkerType( MarkerType marker );

    /**
     * Returns the current marker type to highlight point geometries and line vertices.
     */
    MarkerType markerType() const;

    /**
     * Sets the marker outline style
     * \note Default value is solid line
     * \since QGIS 3.44
     */
    void setMarkerOutlineStyle( Qt::PenStyle style );

    /**
     * Returns current marker outline style used by rubberband.
     * \since QGIS 3.44
     */
    Qt::PenStyle markerOutlineStyle() const;

    /**
     * Sets whether to show markers on vertices
     * \since QGIS 3.44
     */
    void setMarkersEnabled( bool enable );

    /**
     * Returns whether markers are being shown on vertices
     * \since QGIS 3.44
     */
    bool hasMarkersEnabled() const;

    /**
     * Sets whether to show edges
     * \since QGIS 3.44
     */
    void setEdgesEnabled( bool enable );

    /**
       * Returns whether edges are shown
       * \since QGIS 3.44
       */
    bool hasEdgesEnabled() const;

    /**
       * Sets whether to show polygon fill
       * \since QGIS 3.44
       */
    void setFillEnabled( bool enable );

    /**
       * Returns whether polygon fill is shown
       * \since QGIS 3.44
       */
    bool hasFillEnabled() const;

    void reset();

    void addPoint( const QgsPoint &pt );

    /**
     * Sets the geometry and \a GeometryType of rubberband
     *
     * \since QGIS 3.44
     */
    void setGeometry( const QgsGeometry &geometry );

    //! Removes the last point, usually the one tracked by the mouse
    void removeLastPoint();

    //! Removes the one-before-last point, usually the last digitized point. Useful when undoing digitizing.
    void removePenultimatePoint();

    //! Moves the last point, usually the one tracked by the mouse
    void moveLastPoint( const QgsPoint &pt );

    //! Sets whether the marker on the last vertex is displayed. We typically do not want it displayed while it is still tracked by the mouse.
    void setHideLastMarker( const bool hide ) { mHideLastMarker = hide; }

    bool isEmpty() const { return mGeometry.isEmpty(); }

  private:
    void updateGeometry();
    //! negative index counts from end
    void removePoint( int index );

    const float DEFAULT_POLYGON_OPACITY = 0.25;

    QgsGeometry mGeometry;
    bool mHideLastMarker = false;

    Qgs3DMapSettings *mMapSettings = nullptr; // not owned
    QgsRhi3DEngine *mEngine = nullptr;
    Qgis::GeometryType mGeometryType = Qgis::GeometryType::Line;

    //! point and vertex marker type
    MarkerType mMarkerType = Circle;
    float mWidth = 3.f;
    QColor mColor = Qt::red;
    QColor mOutlineColor;
    Qt::PenStyle mMarkerOutlineStyle = Qt::PenStyle::SolidLine;

    bool mMarkerEnabled = true;
    bool mEdgesEnabled = true;
    bool mPolygonFillEnabled = true;

    std::unique_ptr<QgsMarkerSymbol> mMarkerSymbol;

    // Disable copying as we have pointer members.
    QgsRubberBand3D( const QgsRubberBand3D & ) = delete;
    QgsRubberBand3D &operator=( const QgsRubberBand3D & ) = delete;
};

/// @endcond

#endif // QGSRUBBERBAND3D_H
