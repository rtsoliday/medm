#include "wave_table_element.h"

#include <algorithm>

#include <QCoreApplication>
#include <QFontMetrics>
#include <QHeaderView>
#include <QPalette>
#include <QResizeEvent>

#include "legacy_fonts.h"
#include "pv_name_utils.h"
#include "window_utils.h"

namespace {

constexpr int kMinimumTableFontSize = 1;
constexpr int kMaximumTableFontSize = 200;
constexpr int kDefaultTableFontSize = 10;

int clampedTableFontSize(int pointSize)
{
  return std::clamp(pointSize, kMinimumTableFontSize, kMaximumTableFontSize);
}

int pointSizeForFont(const QFont &font)
{
  if (font.pointSize() > 0) {
    return font.pointSize();
  }
  if (font.pointSizeF() > 0.0) {
    return static_cast<int>(font.pointSizeF() + 0.5);
  }
  if (font.pixelSize() > 0) {
    return font.pixelSize();
  }
  return kDefaultTableFontSize;
}

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

WaveTableElement::WaveTableElement(QWidget *parent)
  : QTableView(parent)
  , model_(new WaveTableModel(this))
{
  setModel(model_);
  setSelectionMode(QAbstractItemView::NoSelection);
  setSelectionBehavior(QAbstractItemView::SelectItems);
  setFocusPolicy(Qt::NoFocus);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setAlternatingRowColors(false);
  setShowGrid(true);
  setWordWrap(false);
  horizontalHeader()->setStretchLastSection(false);
  horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  horizontalHeader()->setDefaultSectionSize(80);
  verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  verticalHeader()->setDefaultSectionSize(22);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  foregroundColor_ = defaultForegroundColor();
  backgroundColor_ = defaultBackgroundColor();
  applyModelColors();
  updateModelConfiguration();
  updateHeaderVisibility();
  updateSelectionVisual();
  updateRuntimeStatus();
}

WaveTableElement::~WaveTableElement() = default;

void WaveTableElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  updateSelectionVisual();
  viewport()->update();
}

bool WaveTableElement::isSelected() const
{
  return selected_;
}

QColor WaveTableElement::foregroundColor() const
{
  return foregroundColor_;
}

void WaveTableElement::setForegroundColor(const QColor &color)
{
  foregroundColor_ = color.isValid() ? color : defaultForegroundColor();
  applyModelColors();
}

QColor WaveTableElement::backgroundColor() const
{
  return backgroundColor_;
}

void WaveTableElement::setBackgroundColor(const QColor &color)
{
  backgroundColor_ = color.isValid() ? color : defaultBackgroundColor();
  applyModelColors();
}

TextColorMode WaveTableElement::colorMode() const
{
  return colorMode_;
}

void WaveTableElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
  applyModelColors();
}

bool WaveTableElement::showHeaders() const
{
  return showHeaders_;
}

void WaveTableElement::setShowHeaders(bool show)
{
  showHeaders_ = show;
  updateHeaderVisibility();
}

int WaveTableElement::fontSize() const
{
  if (fontSize_ > 0) {
    return fontSize_;
  }
  return clampedTableFontSize(pointSizeForFont(font()));
}

bool WaveTableElement::hasExplicitFontSize() const
{
  return fontSize_ > 0;
}

void WaveTableElement::setFontSize(int pointSize)
{
  const int clamped = clampedTableFontSize(pointSize);
  if (fontSize_ == clamped && pointSizeForFont(font()) == clamped) {
    return;
  }
  fontSize_ = clamped;
  applyFontSize();
}

QString WaveTableElement::channel() const
{
  return channel_;
}

void WaveTableElement::setChannel(const QString &channel)
{
  channel_ = PvNameUtils::normalizePvName(channel);
}

WaveTableLayout WaveTableElement::layoutMode() const
{
  return layout_;
}

void WaveTableElement::setLayoutMode(WaveTableLayout layout)
{
  layout_ = layout;
  updateModelConfiguration();
  updateHeaderVisibility();
}

QString WaveTableElement::layoutString() const
{
  return layoutToString(layout_);
}

void WaveTableElement::setLayoutString(const QString &layout)
{
  setLayoutMode(layoutFromString(layout));
}

int WaveTableElement::columnCountSetting() const
{
  return columnCount_;
}

void WaveTableElement::setColumnCountSetting(int count)
{
  columnCount_ = std::max(1, count);
  updateModelConfiguration();
}

int WaveTableElement::maxElements() const
{
  return maxElements_;
}

void WaveTableElement::setMaxElements(int count)
{
  maxElements_ = std::max(0, count);
}

int WaveTableElement::indexBase() const
{
  return indexBase_;
}

void WaveTableElement::setIndexBase(int indexBase)
{
  indexBase_ = indexBase == 1 ? 1 : 0;
  updateModelConfiguration();
}

WaveTableValueFormat WaveTableElement::valueFormat() const
{
  return valueFormat_;
}

void WaveTableElement::setValueFormat(WaveTableValueFormat format)
{
  valueFormat_ = format;
}

QString WaveTableElement::valueFormatString() const
{
  return valueFormatToString(valueFormat_);
}

void WaveTableElement::setValueFormatString(const QString &format)
{
  setValueFormat(valueFormatFromString(format));
}

WaveTableCharMode WaveTableElement::charMode() const
{
  return charMode_;
}

void WaveTableElement::setCharMode(WaveTableCharMode mode)
{
  charMode_ = mode;
}

QString WaveTableElement::charModeString() const
{
  return charModeToString(charMode_);
}

void WaveTableElement::setCharModeString(const QString &mode)
{
  setCharMode(charModeFromString(mode));
}

void WaveTableElement::setExecuteMode(bool execute)
{
  executeMode_ = execute;
  setAttribute(Qt::WA_TransparentForMouseEvents, !execute);
  updateSelectionVisual();
}

bool WaveTableElement::isExecuteMode() const
{
  return executeMode_;
}

void WaveTableElement::setConnected(bool connected)
{
  runtimeState_.connected = connected;
  if (!connected) {
    runtimeState_.severity = 3;
  }
  model_->setRuntimeState(runtimeState_);
  updateRuntimeStatus();
}

void WaveTableElement::setSeverity(short severity)
{
  runtimeState_.severity = severity;
  model_->setRuntimeState(runtimeState_);
  updateRuntimeStatus();
}

void WaveTableElement::setMetadata(short nativeFieldType,
    long nativeElementCount, const QString &units, int precision)
{
  runtimeState_.nativeFieldType = nativeFieldType;
  runtimeState_.nativeElementCount = std::max<long>(0, nativeElementCount);
  runtimeState_.units = units;
  runtimeState_.precision = precision;
  model_->setRuntimeState(runtimeState_);
  updateRuntimeStatus();
}

void WaveTableElement::setValues(const QVector<QString> &values,
    long receivedElementCount)
{
  runtimeState_.receivedElementCount = std::max<long>(0, receivedElementCount);
  model_->setRuntimeState(runtimeState_);
  model_->setValues(values);
  updateRuntimeStatus();
}

void WaveTableElement::clearRuntimeState()
{
  runtimeState_ = WaveTableRuntimeState();
  model_->setRuntimeState(runtimeState_);
  model_->clearValues();
  updateRuntimeStatus();
}

bool WaveTableElement::runtimeConnected() const
{
  return runtimeState_.connected;
}

short WaveTableElement::runtimeSeverity() const
{
  return runtimeState_.severity;
}

short WaveTableElement::nativeFieldType() const
{
  return runtimeState_.nativeFieldType;
}

long WaveTableElement::nativeElementCount() const
{
  return runtimeState_.nativeElementCount;
}

long WaveTableElement::receivedElementCount() const
{
  return runtimeState_.receivedElementCount;
}

int WaveTableElement::displayedElementCount() const
{
  return model_->displayedElementCount();
}

int WaveTableElement::tableRowCount() const
{
  return model_->rowCount();
}

int WaveTableElement::tableColumnCount() const
{
  return model_->columnCount();
}

QVector<QString> WaveTableElement::displayedValues() const
{
  return model_->values();
}

QString WaveTableElement::cellText(int row, int column) const
{
  return model_->cellText(row, column);
}

QString WaveTableElement::runtimeStatusText() const
{
  return QStringLiteral("%1 native=%2 received=%3 displayed=%4")
      .arg(severityText(runtimeState_.severity, runtimeState_.connected))
      .arg(runtimeState_.nativeElementCount)
      .arg(runtimeState_.receivedElementCount)
      .arg(displayedElementCount());
}

WaveTableLayout WaveTableElement::layoutFromString(const QString &layout)
{
  const QString token = layout.trimmed().toLower();
  if (token == QStringLiteral("row")) {
    return WaveTableLayout::kRow;
  }
  if (token == QStringLiteral("column")) {
    return WaveTableLayout::kColumn;
  }
  return WaveTableLayout::kGrid;
}

QString WaveTableElement::layoutToString(WaveTableLayout layout)
{
  switch (layout) {
  case WaveTableLayout::kRow:
    return QStringLiteral("row");
  case WaveTableLayout::kColumn:
    return QStringLiteral("column");
  case WaveTableLayout::kGrid:
  default:
    return QStringLiteral("grid");
  }
}

WaveTableValueFormat WaveTableElement::valueFormatFromString(
    const QString &format)
{
  const QString token = format.trimmed().toLower();
  if (token == QStringLiteral("fixed")) {
    return WaveTableValueFormat::kFixed;
  }
  if (token == QStringLiteral("scientific")) {
    return WaveTableValueFormat::kScientific;
  }
  if (token == QStringLiteral("hex")) {
    return WaveTableValueFormat::kHex;
  }
  if (token == QStringLiteral("engineering")) {
    return WaveTableValueFormat::kEngineering;
  }
  return WaveTableValueFormat::kDefault;
}

QString WaveTableElement::valueFormatToString(WaveTableValueFormat format)
{
  switch (format) {
  case WaveTableValueFormat::kFixed:
    return QStringLiteral("fixed");
  case WaveTableValueFormat::kScientific:
    return QStringLiteral("scientific");
  case WaveTableValueFormat::kHex:
    return QStringLiteral("hex");
  case WaveTableValueFormat::kEngineering:
    return QStringLiteral("engineering");
  case WaveTableValueFormat::kDefault:
  default:
    return QStringLiteral("default");
  }
}

WaveTableCharMode WaveTableElement::charModeFromString(const QString &mode)
{
  const QString token = mode.trimmed().toLower();
  if (token == QStringLiteral("bytes")) {
    return WaveTableCharMode::kBytes;
  }
  if (token == QStringLiteral("ascii")) {
    return WaveTableCharMode::kAscii;
  }
  if (token == QStringLiteral("numeric")) {
    return WaveTableCharMode::kNumeric;
  }
  return WaveTableCharMode::kString;
}

QString WaveTableElement::charModeToString(WaveTableCharMode mode)
{
  switch (mode) {
  case WaveTableCharMode::kBytes:
    return QStringLiteral("bytes");
  case WaveTableCharMode::kAscii:
    return QStringLiteral("ascii");
  case WaveTableCharMode::kNumeric:
    return QStringLiteral("numeric");
  case WaveTableCharMode::kString:
  default:
    return QStringLiteral("string");
  }
}

void WaveTableElement::mousePressEvent(QMouseEvent *event)
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

void WaveTableElement::resizeEvent(QResizeEvent *event)
{
  QTableView::resizeEvent(event);
}

bool WaveTableElement::forwardMouseEventToParent(QMouseEvent *event) const
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

void WaveTableElement::updateSelectionVisual()
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

void WaveTableElement::applyModelColors()
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

void WaveTableElement::updateHeaderVisibility()
{
  horizontalHeader()->setVisible(showHeaders_);
  verticalHeader()->setVisible(showHeaders_ && layout_ == WaveTableLayout::kGrid);
}

void WaveTableElement::updateModelConfiguration()
{
  model_->setLayout(layout_);
  model_->setColumnCount(columnCount_);
  model_->setIndexBase(indexBase_);
}

void WaveTableElement::updateRuntimeStatus()
{
  const QString text = runtimeStatusText();
  setToolTip(QString());
  viewport()->setToolTip(QString());
  setStatusTip(text);
  setAccessibleDescription(text);
}

void WaveTableElement::applyFontSize()
{
  if (fontSize_ <= 0) {
    return;
  }
  const QFont tableFont = LegacyFonts::fontForLegacySize(font(), fontSize_);
  QTableView::setFont(tableFont);
  horizontalHeader()->setFont(tableFont);
  verticalHeader()->setFont(tableFont);

  const int rowHeight = std::max(1, QFontMetrics(tableFont).height() + 6);
  verticalHeader()->setDefaultSectionSize(rowHeight);
  horizontalHeader()->setMinimumHeight(rowHeight);
  resizeRowsToContents();
  viewport()->update();
}

QColor WaveTableElement::defaultForegroundColor() const
{
  return palette().color(QPalette::WindowText);
}

QColor WaveTableElement::defaultBackgroundColor() const
{
  return palette().color(QPalette::Window);
}
