/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef STATE_TIMELINE_WIDGET_H
#define STATE_TIMELINE_WIDGET_H

#include <array>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include <QDomDocument>
#include <QPoint>
#include <QWidget>
#include "PlotJuggler/plotdata.h"

using namespace PJ;

class StateTimelineWidget : public QWidget
{
  Q_OBJECT

public:
  explicit StateTimelineWidget(PlotDataMapRef& datamap, QWidget* parent = nullptr);

  void addSeries(const QString& name);
  void clearSeries();

  void replot();
  void zoomOut();
  void setTrackerPosition(double t);
  void setXRange(double xmin, double xmax);

  QDomElement xmlSaveState(QDomDocument& doc) const;
  bool xmlLoadState(QDomElement& element);

signals:
  void undoableChange();
  void xRangeChanged(double xmin, double xmax);

protected:
  void paintEvent(QPaintEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  PlotDataMapRef& _datamap;

  struct SeriesInfo
  {
    std::string name;
    bool is_string;  // true = StringSeries, false = numeric PlotData
  };

  std::vector<SeriesInfo> _series;

  double _view_xmin = 0.0;
  double _view_xmax = 10.0;

  // state value string -> assigned color
  std::map<std::string, QColor> _color_map;
  int _next_color_idx = 0;

  double _tracker_time = std::numeric_limits<double>::quiet_NaN();
  bool _suppress_xrange_signal = false;

  bool _panning = false;
  QPoint _pan_start;
  double _pan_xmin_start = 0.0;
  double _pan_xmax_start = 0.0;

  static constexpr int LEFT_MARGIN = 160;
  static constexpr int BOTTOM_MARGIN = 28;
  static constexpr int TOP_MARGIN = 6;
  static constexpr int ROW_GAP = 3;
  static constexpr int MIN_LABEL_WIDTH = 30;

  QRect plotArea() const;
  double timeToPixel(double t) const;
  double pixelToTime(double px) const;

  void fitToData();

  void drawStringSeries(QPainter& painter, const StringSeries& series, const QRect& row_rect);
  void drawNumericSeries(QPainter& painter, const PlotData& series, const QRect& row_rect);
  void drawTimeAxis(QPainter& painter, const QRect& plot_area);

  QColor colorForState(const std::string& state_val);

  static const std::array<QColor, 10> PALETTE;
};

#endif  // STATE_TIMELINE_WIDGET_H
