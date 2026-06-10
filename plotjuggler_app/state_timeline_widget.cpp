/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "state_timeline_widget.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <QByteArray>
#include <QContextMenuEvent>
#include <QDataStream>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QDateTime>
#include <QFontMetrics>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>

// Tableau 10 color palette
const std::array<QColor, 10> StateTimelineWidget::PALETTE = {
  QColor(0x4e, 0x79, 0xa7),  // blue
  QColor(0xf2, 0x8e, 0x2b),  // orange
  QColor(0xe1, 0x57, 0x59),  // red
  QColor(0x76, 0xb7, 0xb2),  // teal
  QColor(0x59, 0xa1, 0x4f),  // green
  QColor(0xed, 0xc9, 0x48),  // yellow
  QColor(0xb0, 0x7a, 0xa1),  // purple
  QColor(0xff, 0x9d, 0xa7),  // pink
  QColor(0x9c, 0x75, 0x5f),  // brown
  QColor(0xba, 0xb0, 0xac),  // gray
};

StateTimelineWidget::StateTimelineWidget(PlotDataMapRef& datamap, QWidget* parent)
  : QWidget(parent), _datamap(datamap)
{
  setAcceptDrops(true);
  setMinimumSize(100, 60);
  setAttribute(Qt::WA_OpaquePaintEvent, false);
}

//-------------------------------------------------------------------
// Public interface
//-------------------------------------------------------------------

void StateTimelineWidget::addSeries(const QString& name)
{
  std::string sname = name.toStdString();
  // avoid duplicates
  for (const auto& s : _series)
  {
    if (s.name == sname)
    {
      return;
    }
  }

  bool is_string = (_datamap.strings.count(sname) > 0);
  bool is_numeric = (_datamap.numeric.count(sname) > 0);

  if (!is_string && !is_numeric)
  {
    return;
  }

  _series.push_back({ sname, is_string });
  fitToData();
  emit undoableChange();
  update();
}

void StateTimelineWidget::clearSeries()
{
  _series.clear();
  _color_map.clear();
  _next_color_idx = 0;
  emit undoableChange();
  update();
}

void StateTimelineWidget::replot()
{
  update();
}

void StateTimelineWidget::zoomOut()
{
  fitToData();
  update();
}

void StateTimelineWidget::setTrackerPosition(double t)
{
  _tracker_time = t;
  update();
}

void StateTimelineWidget::setXRange(double xmin, double xmax)
{
  _suppress_xrange_signal = true;
  _view_xmin = xmin;
  _view_xmax = xmax;
  _suppress_xrange_signal = false;
  update();
}

void StateTimelineWidget::setUseDateTimeScale(bool enable, bool use_utc)
{
  _use_date_time_scale = enable;
  _use_utc_time = use_utc;
  update();
}

void StateTimelineWidget::setLeftMargin(int px)
{
  if (_left_margin != px)
  {
    _left_margin = px;
    update();
  }
}

QDomElement StateTimelineWidget::xmlSaveState(QDomDocument& doc) const
{
  QDomElement elem = doc.createElement("StateTimeline");
  for (const auto& s : _series)
  {
    QDomElement series_elem = doc.createElement("series");
    series_elem.setAttribute("name", QString::fromStdString(s.name));
    series_elem.setAttribute("type", s.is_string ? "string" : "numeric");
    elem.appendChild(series_elem);
  }
  return elem;
}

bool StateTimelineWidget::xmlLoadState(QDomElement& element)
{
  clearSeries();
  for (auto e = element.firstChildElement("series"); !e.isNull();
       e = e.nextSiblingElement("series"))
  {
    addSeries(e.attribute("name"));
  }
  return true;
}

//-------------------------------------------------------------------
// Geometry helpers
//-------------------------------------------------------------------

QRect StateTimelineWidget::plotArea() const
{
  return QRect(_left_margin, TOP_MARGIN, width() - _left_margin - 4,
               height() - TOP_MARGIN - BOTTOM_MARGIN);
}

double StateTimelineWidget::timeToPixel(double t) const
{
  const QRect pa = plotArea();
  if (_view_xmax == _view_xmin)
  {
    return pa.left();
  }
  return pa.left() + (t - _view_xmin) / (_view_xmax - _view_xmin) * pa.width();
}

double StateTimelineWidget::pixelToTime(double px) const
{
  const QRect pa = plotArea();
  if (pa.width() == 0)
  {
    return _view_xmin;
  }
  return _view_xmin + (px - pa.left()) / pa.width() * (_view_xmax - _view_xmin);
}

void StateTimelineWidget::fitToData()
{
  double xmin = std::numeric_limits<double>::max();
  double xmax = std::numeric_limits<double>::lowest();

  for (const auto& info : _series)
  {
    if (info.is_string)
    {
      auto it = _datamap.strings.find(info.name);
      if (it != _datamap.strings.end() && it->second.size() > 0)
      {
        xmin = std::min(xmin, it->second.front().x);
        xmax = std::max(xmax, it->second.back().x);
      }
    }
    else
    {
      auto it = _datamap.numeric.find(info.name);
      if (it != _datamap.numeric.end() && it->second.size() > 0)
      {
        xmin = std::min(xmin, it->second.front().x);
        xmax = std::max(xmax, it->second.back().x);
      }
    }
  }

  if (xmin < xmax)
  {
    double margin = (xmax - xmin) * 0.02;
    _view_xmin = xmin - margin;
    _view_xmax = xmax + margin;
  }
  else if (xmin == xmax)
  {
    _view_xmin = xmin - 1.0;
    _view_xmax = xmax + 1.0;
  }
}

//-------------------------------------------------------------------
// Color assignment
//-------------------------------------------------------------------

QColor StateTimelineWidget::colorForState(const std::string& state_val)
{
  auto it = _color_map.find(state_val);
  if (it != _color_map.end())
  {
    return it->second;
  }

  QColor c = PALETTE[_next_color_idx % PALETTE.size()];
  _next_color_idx++;
  _color_map[state_val] = c;
  return c;
}

//-------------------------------------------------------------------
// Paint
//-------------------------------------------------------------------

void StateTimelineWidget::paintEvent(QPaintEvent*)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  QColor bg = palette().window().color();
  QColor fg = palette().text().color();
  painter.fillRect(rect(), bg);

  const QRect pa = plotArea();

  if (_series.empty())
  {
    painter.setPen(fg);
    painter.drawText(rect(), Qt::AlignCenter,
                     "Drop numeric or string series here\nto display a state timeline");
    return;
  }

  int n = static_cast<int>(_series.size());
  int total_gaps = (n > 1) ? ROW_GAP * (n - 1) : 0;
  int row_h = std::max(1, (pa.height() - total_gaps) / n);

  for (int row = 0; row < n; row++)
  {
    int y_top = pa.top() + row * (row_h + ROW_GAP);
    QRect row_rect(pa.left(), y_top, pa.width(), row_h);

    // Label on the left
    QRect label_rect(0, y_top, _left_margin - 6, row_h);
    painter.setPen(fg);
    QFont f = painter.font();
    f.setPointSizeF(std::max(7.0, f.pointSizeF() - 1));
    painter.setFont(f);
    QFontMetrics fm(f);
    QString label = fm.elidedText(QString::fromStdString(_series[row].name), Qt::ElideLeft,
                                  label_rect.width() - 4);
    painter.drawText(label_rect, Qt::AlignRight | Qt::AlignVCenter, label);

    // Row background
    painter.fillRect(row_rect, QColor(220, 220, 220));

    // Clipping to row area
    painter.setClipRect(row_rect);

    if (_series[row].is_string)
    {
      auto it = _datamap.strings.find(_series[row].name);
      if (it != _datamap.strings.end())
      {
        drawStringSeries(painter, it->second, row_rect);
      }
    }
    else
    {
      auto it = _datamap.numeric.find(_series[row].name);
      if (it != _datamap.numeric.end())
      {
        drawNumericSeries(painter, it->second, row_rect);
      }
    }

    painter.setClipping(false);

    // Row border
    painter.setPen(QColor(160, 160, 160));
    painter.drawRect(row_rect.adjusted(0, 0, -1, -1));
  }

  // Tracker vertical line
  if (!std::isnan(_tracker_time) && _tracker_time >= _view_xmin && _tracker_time <= _view_xmax)
  {
    int tx = static_cast<int>(timeToPixel(_tracker_time));
    painter.setPen(QPen(QColor(255, 100, 0, 200), 1, Qt::DashLine));
    painter.drawLine(tx, pa.top(), tx, pa.bottom());
  }

  drawTimeAxis(painter, pa);
}

void StateTimelineWidget::drawStringSeries(QPainter& painter, const StringSeries& series,
                                           const QRect& row_rect)
{
  if (series.size() == 0)
  {
    return;
  }

  size_t n = series.size();
  QFont f = painter.font();
  f.setPointSizeF(std::max(7.0, f.pointSizeF() - 1));
  painter.setFont(f);

  size_t i = 0;
  while (i < n)
  {
    std::string state_val = std::string(series.getString(series.at(i).y));
    double t_start = series.at(i).x;

    // Advance while the state stays the same — merge consecutive equal segments
    size_t j = i + 1;
    while (j < n && std::string(series.getString(series.at(j).y)) == state_val)
    {
      ++j;
    }

    double t_end = (j < n) ? series.at(j).x : _view_xmax;
    i = j;

    if (t_end < _view_xmin || t_start > _view_xmax)
    {
      continue;
    }

    int x0 = static_cast<int>(timeToPixel(t_start));
    int x1 = static_cast<int>(timeToPixel(t_end));
    x0 = std::max(x0, row_rect.left());
    x1 = std::min(x1, row_rect.right());
    if (x1 <= x0)
    {
      continue;
    }

    QRect seg(x0, row_rect.top(), x1 - x0, row_rect.height());
    QColor color = colorForState(state_val);
    painter.fillRect(seg, color);

    if (seg.width() >= MIN_LABEL_WIDTH)
    {
      painter.setPen(color.lightness() > 140 ? Qt::black : Qt::white);
      painter.drawText(seg.adjusted(2, 0, -2, 0),
                       Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                       QString::fromStdString(state_val));
    }
  }
}

static std::string numericStateLabel(double val)
{
  char buf[32];
  if (std::fabs(val - std::round(val)) < 1e-9)
  {
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(val)));
  }
  else
  {
    std::snprintf(buf, sizeof(buf), "%.4g", val);
  }
  return buf;
}

void StateTimelineWidget::drawNumericSeries(QPainter& painter, const PlotData& series,
                                            const QRect& row_rect)
{
  if (series.size() == 0)
  {
    return;
  }

  size_t n = series.size();
  QFont f = painter.font();
  f.setPointSizeF(std::max(7.0, f.pointSizeF() - 1));
  painter.setFont(f);

  size_t i = 0;
  while (i < n)
  {
    std::string state_val = numericStateLabel(series.at(i).y);
    double t_start = series.at(i).x;

    // Advance while the value stays the same — merge consecutive equal segments
    size_t j = i + 1;
    while (j < n && numericStateLabel(series.at(j).y) == state_val)
    {
      ++j;
    }

    double t_end = (j < n) ? series.at(j).x : _view_xmax;
    i = j;

    if (t_end < _view_xmin || t_start > _view_xmax)
    {
      continue;
    }

    int x0 = static_cast<int>(timeToPixel(t_start));
    int x1 = static_cast<int>(timeToPixel(t_end));
    x0 = std::max(x0, row_rect.left());
    x1 = std::min(x1, row_rect.right());
    if (x1 <= x0)
    {
      continue;
    }

    QRect seg(x0, row_rect.top(), x1 - x0, row_rect.height());
    QColor color = colorForState(state_val);
    painter.fillRect(seg, color);

    if (seg.width() >= MIN_LABEL_WIDTH)
    {
      painter.setPen(color.lightness() > 140 ? Qt::black : Qt::white);
      painter.drawText(seg.adjusted(2, 0, -2, 0),
                       Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                       QString::fromStdString(state_val));
    }
  }
}

void StateTimelineWidget::drawTimeAxis(QPainter& painter, const QRect& pa)
{
  QColor fg = palette().text().color();
  int axis_y = pa.bottom() + 1;
  painter.setPen(fg);
  painter.drawLine(pa.left(), axis_y, pa.right(), axis_y);

  double range = _view_xmax - _view_xmin;
  if (range <= 0)
  {
    return;
  }

  // Pick a "nice" tick interval
  double raw = range / 7.0;
  double mag = std::pow(10.0, std::floor(std::log10(raw)));
  double norm = raw / mag;
  double interval;
  if (norm < 1.5)
  {
    interval = mag;
  }
  else if (norm < 3.5)
  {
    interval = 2 * mag;
  }
  else if (norm < 7.5)
  {
    interval = 5 * mag;
  }
  else
  {
    interval = 10 * mag;
  }

  QFont f = painter.font();
  f.setPointSizeF(std::max(7.0, f.pointSizeF() - 1));
  painter.setFont(f);

  // Index-based iteration avoids floating-point accumulation from repeated +=
  long long first_idx = static_cast<long long>(std::ceil(_view_xmin / interval));
  for (long long idx = first_idx;; ++idx)
  {
    double t = idx * interval;  // no cumulative error
    if (t > _view_xmax + 1e-9 * interval)
    {
      break;
    }
    int x = static_cast<int>(timeToPixel(t));
    if (x < pa.left() || x > pa.right())
    {
      continue;
    }
    painter.drawLine(x, axis_y, x, axis_y + 4);
    QString label;
    if (_use_date_time_scale)
    {
      QDateTime dt = _use_utc_time ? QDateTime::fromMSecsSinceEpoch((qint64)(t * 1000), Qt::UTC) :
                                     QDateTime::fromMSecsSinceEpoch((qint64)(t * 1000));
      if (dt.date().year() == 1970 && dt.date().month() == 1 && dt.date().day() == 1)
      {
        label = dt.toString("hh:mm:ss.z");
      }
      else
      {
        label = dt.toString("hh:mm:ss.z\nyyyy MMM dd");
      }
    }
    else
    {
      // Match Qwt's QwtScaleDraw::label() which uses QLocale().toString(v)
      label = QLocale().toString(t, 'g', 6);
    }
    painter.drawText(x - 40, axis_y + 5, 80, BOTTOM_MARGIN - 5, Qt::AlignHCenter | Qt::AlignTop,
                     label);
  }
}

//-------------------------------------------------------------------
// Drag-and-drop
//-------------------------------------------------------------------

void StateTimelineWidget::dragEnterEvent(QDragEnterEvent* event)
{
  const QMimeData* mime = event->mimeData();
  if (!mime->hasFormat("curveslist/add_curve"))
  {
    event->ignore();
    return;
  }

  QByteArray encoded = mime->data("curveslist/add_curve");
  QDataStream stream(&encoded, QIODevice::ReadOnly);
  while (!stream.atEnd())
  {
    QString name;
    stream >> name;
    std::string sname = name.toStdString();
    bool ok = _datamap.numeric.count(sname) > 0 || _datamap.strings.count(sname) > 0;
    if (!ok)
    {
      event->ignore();
      return;
    }
  }
  event->acceptProposedAction();
}

void StateTimelineWidget::dragLeaveEvent(QDragLeaveEvent*)
{
}

void StateTimelineWidget::dropEvent(QDropEvent* event)
{
  const QMimeData* mime = event->mimeData();
  if (!mime->hasFormat("curveslist/add_curve"))
  {
    return;
  }

  QByteArray encoded = mime->data("curveslist/add_curve");
  QDataStream stream(&encoded, QIODevice::ReadOnly);
  while (!stream.atEnd())
  {
    QString name;
    stream >> name;
    addSeries(name);
  }
  event->acceptProposedAction();
}

//-------------------------------------------------------------------
// Mouse / zoom / pan
//-------------------------------------------------------------------

void StateTimelineWidget::wheelEvent(QWheelEvent* event)
{
  double factor = (event->angleDelta().y() > 0) ? 0.8 : 1.25;
  double center = pixelToTime(event->pos().x());
  double range = (_view_xmax - _view_xmin) * factor;
  double ratio =
      (_view_xmax - _view_xmin > 0) ? (center - _view_xmin) / (_view_xmax - _view_xmin) : 0.5;
  _view_xmin = center - ratio * range;
  _view_xmax = center + (1.0 - ratio) * range;
  update();
  emit xRangeChanged(_view_xmin, _view_xmax);
}

void StateTimelineWidget::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)
  {
    _panning = true;
    _pan_start = event->pos();
    _pan_xmin_start = _view_xmin;
    _pan_xmax_start = _view_xmax;
    setCursor(Qt::ClosedHandCursor);
  }
}

void StateTimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
  if (!_panning)
  {
    return;
  }
  const QRect pa = plotArea();
  if (pa.width() == 0)
  {
    return;
  }
  double dt =
      (event->pos().x() - _pan_start.x()) * (_pan_xmax_start - _pan_xmin_start) / pa.width();
  _view_xmin = _pan_xmin_start - dt;
  _view_xmax = _pan_xmax_start - dt;
  update();
}

void StateTimelineWidget::mouseReleaseEvent(QMouseEvent*)
{
  if (_panning)
  {
    _panning = false;
    setCursor(Qt::ArrowCursor);
    emit xRangeChanged(_view_xmin, _view_xmax);
  }
}

void StateTimelineWidget::mouseDoubleClickEvent(QMouseEvent*)
{
  fitToData();
  update();
}

void StateTimelineWidget::resizeEvent(QResizeEvent*)
{
  update();
}

void StateTimelineWidget::contextMenuEvent(QContextMenuEvent* event)
{
  QMenu menu;
  QAction* switch_action = menu.addAction("Switch to Line Chart");
  menu.addSeparator();
  QAction* fit_action = menu.addAction("Zoom to fit");
  menu.addSeparator();

  // Per-series remove actions
  std::vector<QAction*> remove_actions;
  for (const auto& s : _series)
  {
    QAction* a = menu.addAction("Remove: " + QString::fromStdString(s.name));
    remove_actions.push_back(a);
  }
  menu.addSeparator();
  QAction* clear_action = menu.addAction("Clear all series");

  QAction* selected = menu.exec(event->globalPos());
  if (!selected)
  {
    return;
  }

  if (selected == switch_action)
  {
    emit switchToLineChart();
  }
  else if (selected == fit_action)
  {
    fitToData();
    update();
  }
  else if (selected == clear_action)
  {
    clearSeries();
  }
  else
  {
    for (size_t i = 0; i < remove_actions.size(); i++)
    {
      if (selected == remove_actions[i])
      {
        _series.erase(_series.begin() + static_cast<int>(i));
        emit undoableChange();
        update();
        break;
      }
    }
  }
}
