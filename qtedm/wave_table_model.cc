#include "wave_table_model.h"

#include <algorithm>

#include "medm_colors.h"

namespace {

QString severityText(short severity, bool connected)
{
  if (!connected) {
    return QStringLiteral("Disconnected");
  }
  switch (severity) {
  case 0:
    return QStringLiteral("OK");
  case 1:
    return QStringLiteral("MINOR");
  case 2:
    return QStringLiteral("MAJOR");
  case 3:
    return QStringLiteral("INVALID");
  default:
    return QStringLiteral("UNKNOWN");
  }
}

} // namespace

WaveTableModel::WaveTableModel(QObject *parent)
  : QAbstractTableModel(parent)
{
}

int WaveTableModel::rowCount(const QModelIndex &parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return displayRowCount();
}

int WaveTableModel::columnCount(const QModelIndex &parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return displayColumnCount();
}

QVariant WaveTableModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.column() < 0
      || index.row() >= displayRowCount()
      || index.column() >= displayColumnCount()) {
    return QVariant();
  }

  if (role == Qt::DisplayRole) {
    return cellText(index.row(), index.column());
  }

  if (role == Qt::TextAlignmentRole) {
    if (layout_ == WaveTableLayout::kColumn && index.column() == 0) {
      return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    }
    return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
  }

  if (role == Qt::ForegroundRole) {
    if (colorMode_ == TextColorMode::kAlarm) {
      return alarmColor(runtimeState_.connected ? runtimeState_.severity : 3);
    }
    return foregroundColor_;
  }

  if (role == Qt::BackgroundRole) {
    return backgroundColor_;
  }

  return QVariant();
}

QVariant WaveTableModel::headerData(int section, Qt::Orientation orientation,
    int role) const
{
  if (role != Qt::DisplayRole || section < 0) {
    return QAbstractTableModel::headerData(section, orientation, role);
  }

  if (layout_ == WaveTableLayout::kColumn) {
    if (orientation == Qt::Horizontal) {
      return section == 0 ? QStringLiteral("Index") : QStringLiteral("Value");
    }
    return QVariant();
  }

  if (layout_ == WaveTableLayout::kRow) {
    if (orientation == Qt::Horizontal && section < values_.size()) {
      return QString::number(indexBase_ + section);
    }
    return QVariant();
  }

  if (orientation == Qt::Horizontal) {
    return QString::number(indexBase_ + section);
  }

  const int startIndex = section * columnCount_;
  const int lastIndex = std::min(startIndex + columnCount_ - 1,
      std::max(0, values_.size() - 1));
  if (values_.isEmpty()) {
    return QString();
  }
  if (startIndex == lastIndex) {
    return QString::number(indexBase_ + startIndex);
  }
  return QStringLiteral("%1-%2")
      .arg(indexBase_ + startIndex)
      .arg(indexBase_ + lastIndex);
}

void WaveTableModel::setLayout(WaveTableLayout layout)
{
  if (layout_ == layout) {
    return;
  }
  beginResetModel();
  layout_ = layout;
  endResetModel();
}

void WaveTableModel::setColumnCount(int count)
{
  const int clamped = std::max(1, count);
  if (columnCount_ == clamped) {
    return;
  }
  beginResetModel();
  columnCount_ = clamped;
  endResetModel();
}

void WaveTableModel::setIndexBase(int indexBase)
{
  const int clamped = indexBase == 1 ? 1 : 0;
  if (indexBase_ == clamped) {
    return;
  }
  beginResetModel();
  indexBase_ = clamped;
  endResetModel();
}

void WaveTableModel::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
  if (displayRowCount() > 0 && displayColumnCount() > 0) {
    emit dataChanged(index(0, 0),
        index(displayRowCount() - 1, displayColumnCount() - 1),
        {Qt::ForegroundRole});
  }
}

void WaveTableModel::setForegroundColor(const QColor &color)
{
  foregroundColor_ = color;
  if (displayRowCount() > 0 && displayColumnCount() > 0) {
    emit dataChanged(index(0, 0),
        index(displayRowCount() - 1, displayColumnCount() - 1),
        {Qt::ForegroundRole});
  }
}

void WaveTableModel::setBackgroundColor(const QColor &color)
{
  backgroundColor_ = color;
  if (displayRowCount() > 0 && displayColumnCount() > 0) {
    emit dataChanged(index(0, 0),
        index(displayRowCount() - 1, displayColumnCount() - 1),
        {Qt::BackgroundRole});
  }
}

void WaveTableModel::setRuntimeState(const WaveTableRuntimeState &state)
{
  runtimeState_ = state;
  if (displayRowCount() > 0 && displayColumnCount() > 0) {
    emit dataChanged(index(0, 0),
        index(displayRowCount() - 1, displayColumnCount() - 1),
        {Qt::DisplayRole, Qt::ForegroundRole});
  }
}

void WaveTableModel::setValues(const QVector<QString> &values)
{
  beginResetModel();
  values_ = values;
  endResetModel();
}

void WaveTableModel::clearValues()
{
  setValues(QVector<QString>());
}

WaveTableLayout WaveTableModel::layout() const
{
  return layout_;
}

int WaveTableModel::configuredColumnCount() const
{
  return columnCount_;
}

int WaveTableModel::indexBase() const
{
  return indexBase_;
}

int WaveTableModel::displayedElementCount() const
{
  return values_.size();
}

QString WaveTableModel::cellText(int row, int column) const
{
  if (values_.isEmpty()) {
    return (row == 0 && column == 0) ? statusText() : QString();
  }

  if (layout_ == WaveTableLayout::kColumn) {
    if (row < 0 || row >= values_.size()) {
      return QString();
    }
    if (column == 0) {
      return QString::number(indexBase_ + row);
    }
    if (column == 1) {
      return values_.at(row);
    }
    return QString();
  }

  const int valueIndex = valueIndexForCell(row, column);
  if (valueIndex < 0 || valueIndex >= values_.size()) {
    return QString();
  }
  return values_.at(valueIndex);
}

QVector<QString> WaveTableModel::values() const
{
  return values_;
}

int WaveTableModel::displayRowCount() const
{
  if (layout_ == WaveTableLayout::kColumn) {
    return std::max(1, values_.size());
  }
  if (layout_ == WaveTableLayout::kRow) {
    return 1;
  }
  return std::max(1, (values_.size() + columnCount_ - 1) / columnCount_);
}

int WaveTableModel::displayColumnCount() const
{
  if (layout_ == WaveTableLayout::kColumn) {
    return 2;
  }
  if (layout_ == WaveTableLayout::kRow) {
    return std::max(1, values_.size());
  }
  return std::max(1, columnCount_);
}

int WaveTableModel::valueIndexForCell(int row, int column) const
{
  if (row < 0 || column < 0) {
    return -1;
  }
  switch (layout_) {
  case WaveTableLayout::kRow:
    return column;
  case WaveTableLayout::kColumn:
    return row;
  case WaveTableLayout::kGrid:
  default:
    return row * columnCount_ + column;
  }
}

QString WaveTableModel::statusText() const
{
  const QString state = severityText(runtimeState_.severity,
      runtimeState_.connected);
  return QStringLiteral("%1  native=%2 received=%3 displayed=%4")
      .arg(state)
      .arg(runtimeState_.nativeElementCount)
      .arg(runtimeState_.receivedElementCount)
      .arg(values_.size());
}

QColor WaveTableModel::alarmColor(short severity) const
{
  return MedmColors::alarmColorForSeverity(severity);
}
