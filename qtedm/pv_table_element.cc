#include "pv_table_element.h"

#include <algorithm>

#include <QCoreApplication>
#include <QHeaderView>
#include <QPalette>
#include <QResizeEvent>

#include "medm_colors.h"
#include "pv_name_utils.h"
#include "window_utils.h"

PvTableModel::PvTableModel(QObject *parent)
  : QAbstractTableModel(parent)
{
}

int PvTableModel::rowCount(const QModelIndex &parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return rows_.size();
}

int PvTableModel::columnCount(const QModelIndex &parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return columns_.size();
}

QVariant PvTableModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()
      || index.column() < 0 || index.column() >= columns_.size()) {
    return QVariant();
  }

  const PvTableRowConfig &row = rows_.at(index.row());
  const PvTableRuntimeRowState &state = runtimeStates_.at(index.row());
  const Column column = columns_.at(index.column());

  if (role == Qt::DisplayRole) {
    switch (column) {
    case Column::kLabel:
      return row.label.trimmed().isEmpty() ? row.channel.trimmed() : row.label;
    case Column::kPv:
      return row.channel;
    case Column::kValue:
      return state.connected ? state.valueText : QStringLiteral("---");
    case Column::kUnits:
      return state.connected ? state.units : QString();
    case Column::kSeverity:
      return severityText(state.severity, state.connected);
    }
  }

  if (role == Qt::TextAlignmentRole) {
    switch (column) {
    case Column::kValue:
      return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    default:
      return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }
  }

  if (role == Qt::ForegroundRole) {
    if (colorMode_ == TextColorMode::kAlarm) {
      return alarmColor(state.connected ? state.severity : 3);
    }
    return foregroundColor_;
  }

  if (role == Qt::BackgroundRole) {
    return backgroundColor_;
  }

  return QVariant();
}

QVariant PvTableModel::headerData(int section, Qt::Orientation orientation,
    int role) const
{
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole
      || section < 0 || section >= columns_.size()) {
    return QAbstractTableModel::headerData(section, orientation, role);
  }

  switch (columns_.at(section)) {
  case Column::kLabel:
    return QStringLiteral("Label");
  case Column::kPv:
    return QStringLiteral("PV");
  case Column::kValue:
    return QStringLiteral("Value");
  case Column::kUnits:
    return QStringLiteral("Units");
  case Column::kSeverity:
    return QStringLiteral("State");
  }
  return QVariant();
}

void PvTableModel::setRows(const QVector<PvTableRowConfig> &rows)
{
  beginResetModel();
  rows_ = rows;
  runtimeStates_.resize(rows_.size());
  endResetModel();
}

void PvTableModel::setRuntimeState(int row, const PvTableRuntimeRowState &state)
{
  if (row < 0 || row >= runtimeStates_.size()) {
    return;
  }
  runtimeStates_[row] = state;
  if (columns_.isEmpty()) {
    return;
  }
  emit dataChanged(index(row, 0), index(row, columns_.size() - 1),
      {Qt::DisplayRole, Qt::ForegroundRole, Qt::BackgroundRole,
          Qt::TextAlignmentRole});
}

void PvTableModel::setColumns(const QVector<Column> &columns)
{
  beginResetModel();
  columns_ = columns;
  endResetModel();
}

void PvTableModel::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
  if (!rows_.isEmpty() && !columns_.isEmpty()) {
    emit dataChanged(index(0, 0), index(rows_.size() - 1, columns_.size() - 1),
        {Qt::ForegroundRole});
  }
}

void PvTableModel::setForegroundColor(const QColor &color)
{
  foregroundColor_ = color;
  if (!rows_.isEmpty() && !columns_.isEmpty()) {
    emit dataChanged(index(0, 0), index(rows_.size() - 1, columns_.size() - 1),
        {Qt::ForegroundRole});
  }
}

void PvTableModel::setBackgroundColor(const QColor &color)
{
  backgroundColor_ = color;
  if (!rows_.isEmpty() && !columns_.isEmpty()) {
    emit dataChanged(index(0, 0), index(rows_.size() - 1, columns_.size() - 1),
        {Qt::BackgroundRole});
  }
}

QString PvTableModel::severityText(short severity, bool connected) const
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

QColor PvTableModel::alarmColor(short severity) const
{
  return MedmColors::alarmColorForSeverity(severity);
}

int PvTableModel::visualColumnIndex(Column column) const
{
  return columns_.indexOf(column);
}

PvTableElement::PvTableElement(QWidget *parent)
  : QTableView(parent)
  , model_(new PvTableModel(this))
{
  setModel(model_);
  setSelectionMode(QAbstractItemView::NoSelection);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setFocusPolicy(Qt::NoFocus);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setAlternatingRowColors(false);
  setShowGrid(true);
  setWordWrap(false);
  horizontalHeader()->setStretchLastSection(true);
  horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  verticalHeader()->setVisible(false);
  verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  foregroundColor_ = defaultForegroundColor();
  backgroundColor_ = defaultBackgroundColor();
  columns_ = parseColumns(QStringLiteral("label,pv,value,severity"));
  applyModelColors();
  updateHeaderVisibility();
  updateSelectionVisual();
}

PvTableElement::~PvTableElement() = default;

void PvTableElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  updateSelectionVisual();
  viewport()->update();
}

bool PvTableElement::isSelected() const
{
  return selected_;
}

QColor PvTableElement::foregroundColor() const
{
  return foregroundColor_;
}

void PvTableElement::setForegroundColor(const QColor &color)
{
  foregroundColor_ = color.isValid() ? color : defaultForegroundColor();
  applyModelColors();
}

QColor PvTableElement::backgroundColor() const
{
  return backgroundColor_;
}

void PvTableElement::setBackgroundColor(const QColor &color)
{
  backgroundColor_ = color.isValid() ? color : defaultBackgroundColor();
  applyModelColors();
}

TextColorMode PvTableElement::colorMode() const
{
  return colorMode_;
}

void PvTableElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
  applyModelColors();
}

bool PvTableElement::showHeaders() const
{
  return showHeaders_;
}

void PvTableElement::setShowHeaders(bool show)
{
  showHeaders_ = show;
  updateHeaderVisibility();
}

QVector<PvTableModel::Column> PvTableElement::columns() const
{
  return columns_;
}

QString PvTableElement::columnsString() const
{
  return columnTokens(columns_).join(QStringLiteral(","));
}

void PvTableElement::setColumns(const QVector<PvTableModel::Column> &columns)
{
  columns_ = columns.isEmpty() ? parseColumns(QString()) : columns;
  model_->setColumns(columns_);
}

void PvTableElement::setColumnsString(const QString &columns)
{
  setColumns(parseColumns(columns));
}

int PvTableElement::rowCount() const
{
  return rows_.size();
}

PvTableRowConfig PvTableElement::row(int index) const
{
  if (index < 0 || index >= rows_.size()) {
    return PvTableRowConfig();
  }
  return rows_.at(index);
}

QVector<PvTableRowConfig> PvTableElement::rows() const
{
  return rows_;
}

void PvTableElement::setRows(const QVector<PvTableRowConfig> &rows)
{
  rows_ = rows;
  runtimeStates_.resize(rows_.size());
  model_->setRows(rows_);
}

void PvTableElement::setRow(int index, const PvTableRowConfig &row)
{
  if (!ensureRowIndex(index)) {
    return;
  }
  rows_[index] = row;
  model_->setRows(rows_);
  updateRuntimeState(index);
}

QString PvTableElement::rowLabelText(int index) const
{
  return row(index).label;
}

QString PvTableElement::rowChannelText(int index) const
{
  return row(index).channel;
}

void PvTableElement::setRowLabelText(int index, const QString &value)
{
  if (!ensureRowIndex(index)) {
    return;
  }
  rows_[index].label = value;
  model_->setRows(rows_);
  updateRuntimeState(index);
}

void PvTableElement::setRowChannelText(int index, const QString &value)
{
  if (!ensureRowIndex(index)) {
    return;
  }
  rows_[index].channel = PvNameUtils::normalizePvName(value);
  if (rows_[index].label.trimmed().isEmpty()) {
    rows_[index].label = value.trimmed();
  }
  runtimeStates_[index] = PvTableRuntimeRowState();
  model_->setRows(rows_);
  updateRuntimeState(index);
}

void PvTableElement::setExecuteMode(bool execute)
{
  executeMode_ = execute;
  setAttribute(Qt::WA_TransparentForMouseEvents, !execute);
  updateSelectionVisual();
}

bool PvTableElement::isExecuteMode() const
{
  return executeMode_;
}

void PvTableElement::setRowConnected(int row, bool connected)
{
  if (!ensureRowIndex(row)) {
    return;
  }
  runtimeStates_[row].connected = connected;
  if (!connected) {
    runtimeStates_[row].severity = 3;
    runtimeStates_[row].valueText.clear();
    runtimeStates_[row].units.clear();
  }
  updateRuntimeState(row);
}

void PvTableElement::setRowValue(int row, const QString &value)
{
  if (!ensureRowIndex(row)) {
    return;
  }
  runtimeStates_[row].valueText = value;
  updateRuntimeState(row);
}

void PvTableElement::setRowSeverity(int row, short severity)
{
  if (!ensureRowIndex(row)) {
    return;
  }
  runtimeStates_[row].severity = severity;
  updateRuntimeState(row);
}

void PvTableElement::setRowMetadata(int row, const QString &units)
{
  if (!ensureRowIndex(row)) {
    return;
  }
  runtimeStates_[row].units = units;
  updateRuntimeState(row);
}

void PvTableElement::clearRuntimeState()
{
  for (int i = 0; i < runtimeStates_.size(); ++i) {
    runtimeStates_[i] = PvTableRuntimeRowState();
    updateRuntimeState(i);
  }
}

void PvTableElement::mousePressEvent(QMouseEvent *event)
{
  if (executeMode_ && (event->button() == Qt::MiddleButton
          || event->button() == Qt::RightButton)) {
    if (forwardMouseEventToParent(event)) {
      return;
    }
  }

  if (executeMode_ && event->button() == Qt::LeftButton
      && isParentWindowInPvInfoMode(this)) {
    if (forwardMouseEventToParent(event)) {
      return;
    }
  }

  QTableView::mousePressEvent(event);
}

void PvTableElement::resizeEvent(QResizeEvent *event)
{
  QTableView::resizeEvent(event);
  horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void PvTableElement::updateSelectionVisual()
{
  if (executeMode_) {
    setFrameShape(QFrame::StyledPanel);
    setLineWidth(1);
    setStyleSheet(QString());
    return;
  }
  if (selected_) {
    setStyleSheet(QStringLiteral("QTableView { border: 2px dashed #ffffff; }"));
  } else {
    setStyleSheet(QStringLiteral("QTableView { border: 1px solid #808080; }"));
  }
}

void PvTableElement::applyModelColors()
{
  QPalette pal = palette();
  pal.setColor(QPalette::Base, backgroundColor_);
  pal.setColor(QPalette::Window, backgroundColor_);
  pal.setColor(QPalette::Text, foregroundColor_);
  pal.setColor(QPalette::WindowText, foregroundColor_);
  setPalette(pal);
  model_->setForegroundColor(foregroundColor_);
  model_->setBackgroundColor(backgroundColor_);
  model_->setColorMode(colorMode_);
  viewport()->update();
}

void PvTableElement::updateHeaderVisibility()
{
  horizontalHeader()->setVisible(showHeaders_);
}

bool PvTableElement::ensureRowIndex(int index)
{
  if (index < 0) {
    return false;
  }
  if (rows_.size() <= index) {
    rows_.resize(index + 1);
    runtimeStates_.resize(index + 1);
    model_->setRows(rows_);
  }
  return true;
}

void PvTableElement::updateRuntimeState(int row)
{
  if (row < 0 || row >= runtimeStates_.size()) {
    return;
  }
  model_->setRuntimeState(row, runtimeStates_.at(row));
}

QColor PvTableElement::defaultForegroundColor() const
{
  return palette().color(QPalette::WindowText);
}

QColor PvTableElement::defaultBackgroundColor() const
{
  return palette().color(QPalette::Window);
}

bool PvTableElement::forwardMouseEventToParent(QMouseEvent *event) const
{
  if (!event) {
    return false;
  }
  QWidget *target = window();
  if (!target) {
    return false;
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF globalPosF = event->globalPosition();
  const QPoint globalPoint = globalPosF.toPoint();
  const QPointF localPos = target->mapFromGlobal(globalPoint);
  QMouseEvent forwarded(event->type(), localPos, localPos, globalPosF,
      event->button(), event->buttons(), event->modifiers());
#else
  const QPoint globalPoint = event->globalPos();
  const QPointF localPos = target->mapFromGlobal(globalPoint);
  QMouseEvent forwarded(event->type(), localPos, localPos,
      QPointF(globalPoint), event->button(), event->buttons(),
      event->modifiers());
#endif
  QCoreApplication::sendEvent(target, &forwarded);
  return true;
}

QVector<PvTableModel::Column> PvTableElement::parseColumns(const QString &columns)
{
  QVector<PvTableModel::Column> parsed;
  const QStringList tokens = columns.trimmed().isEmpty()
      ? QStringList{QStringLiteral("label"), QStringLiteral("pv"),
          QStringLiteral("value"), QStringLiteral("severity")}
      : columns.split(',', Qt::SkipEmptyParts);
  for (QString token : tokens) {
    token = token.trimmed().toLower();
    if (token == QStringLiteral("label")) {
      parsed.append(PvTableModel::Column::kLabel);
    } else if (token == QStringLiteral("pv")) {
      parsed.append(PvTableModel::Column::kPv);
    } else if (token == QStringLiteral("value")) {
      parsed.append(PvTableModel::Column::kValue);
    } else if (token == QStringLiteral("units")) {
      parsed.append(PvTableModel::Column::kUnits);
    } else if (token == QStringLiteral("severity")) {
      parsed.append(PvTableModel::Column::kSeverity);
    }
  }
  if (parsed.isEmpty()) {
    parsed.append(PvTableModel::Column::kLabel);
    parsed.append(PvTableModel::Column::kPv);
    parsed.append(PvTableModel::Column::kValue);
    parsed.append(PvTableModel::Column::kSeverity);
  }
  return parsed;
}

QString PvTableElement::columnToken(PvTableModel::Column column)
{
  switch (column) {
  case PvTableModel::Column::kLabel:
    return QStringLiteral("label");
  case PvTableModel::Column::kPv:
    return QStringLiteral("pv");
  case PvTableModel::Column::kValue:
    return QStringLiteral("value");
  case PvTableModel::Column::kUnits:
    return QStringLiteral("units");
  case PvTableModel::Column::kSeverity:
    return QStringLiteral("severity");
  }
  return QStringLiteral("value");
}

QStringList PvTableElement::columnTokens(
    const QVector<PvTableModel::Column> &columns)
{
  QStringList tokens;
  for (PvTableModel::Column column : columns) {
    tokens.append(columnToken(column));
  }
  return tokens;
}
