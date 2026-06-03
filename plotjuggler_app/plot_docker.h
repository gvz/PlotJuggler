/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PLOT_DOCKER_H
#define PLOT_DOCKER_H

#include <QDomElement>
#include <QStackedWidget>
#include <QXmlStreamReader>
#include "PlotJuggler/plotdata.h"
#include "plotwidget.h"
#include "plot_docker_toolbar.h"
#include "state_timeline_widget.h"

class DockWidget : public ads::CDockWidget
{
  Q_OBJECT

public:
  enum VisualizationType
  {
    LINE_CHART = 0,
    STATE_TIMELINE = 1
  };

  DockWidget(PlotDataMapRef& datamap, QWidget* parent = nullptr);

  ~DockWidget() override;

  PlotWidget* plotWidget();

  StateTimelineWidget* stateTimeline();

  DockToolbar* toolBar();

  QString name() const;

  VisualizationType vizType() const
  {
    return _viz_type;
  }

  void setVizType(int type);

public slots:
  DockWidget* splitHorizontal();

  DockWidget* splitVertical();

private:
  QStackedWidget* _stacked_widget = nullptr;
  PlotWidget* _plot_widget = nullptr;
  StateTimelineWidget* _state_timeline = nullptr;

  DockToolbar* _toolbar;

  PlotDataMapRef& _datamap;

  VisualizationType _viz_type = LINE_CHART;

signals:
  void undoableChange();
  void vizTypeChanged(VisualizationType type);
};

class PlotDocker : public ads::CDockManager
{
  Q_OBJECT

public:
  PlotDocker(QString name, PlotDataMapRef& datamap, QWidget* parent = nullptr);

  ~PlotDocker();

  QString name() const;

  void setName(QString name);

  QDomElement xmlSaveState(QDomDocument& doc) const;

  bool xmlLoadState(QDomElement& tab_element);

  int plotCount() const;

  PlotWidget* plotAt(int index);

  void setHorizontalLink(bool enabled);

  void zoomOut();

  void replot();

public slots:

  void on_stylesheetChanged(QString theme);

  void savePlotsToFile();

private:
  void restoreSplitter(QDomElement elem, DockWidget* widget);

  QRect plotRelativeFootprint(int index, QSize plot_size) const;

  QString _name;

  PlotDataMapRef& _datamap;

signals:

  void plotWidgetAdded(PlotWidget*);

  void undoableChange();
};

#endif  // PLOT_DOCKER_H
