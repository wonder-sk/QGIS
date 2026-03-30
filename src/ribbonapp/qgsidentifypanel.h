#ifndef QGSIDENTIFYPANEL_H
#define QGSIDENTIFYPANEL_H

#include "qgsmaptoolidentify.h"

#include <QWidget>

class QTreeWidget;

class QgsIdentifyPanel : public QWidget
{
    Q_OBJECT
  public:
    explicit QgsIdentifyPanel( QWidget *parent = nullptr );
  public slots:
    void showResults( const QList<QgsMapToolIdentify::IdentifyResult> &results );
    void clear();

  private:
    QTreeWidget *mTree = nullptr;
};

#endif // QGSIDENTIFYPANEL_H
