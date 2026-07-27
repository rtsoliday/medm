#include "led_monitor_element.h"

#include "update_coordinator.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QRadialGradient>

#include "medm_colors.h"
#include "pv_name_utils.h"

namespace {

constexpr short kInvalidSeverity = 3;
constexpr int kLedBezelWidth = 2;
constexpr qreal kRoundedSquareRadiusFactor = 0.22;

QColor clampLightness(const QColor &color, int amount)
{
  QColor adjusted = color;
  const int lightness = std::clamp(color.lightness() + amount, 0, 255);
  adjusted.setHsl(color.hslHue(), color.hslSaturation(), lightness,
      color.alpha());
  return adjusted;
}

QPainterPath ledPathForShape(LedShape shape, const QRectF &bounds)
{
  QPainterPath path;
  if (bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    return path;
  }

  switch (shape) {
  case LedShape::kSquare:
    path.addRect(bounds);
    break;
  case LedShape::kRoundedSquare: {
    const qreal radius = std::min(bounds.width(), bounds.height())
        * kRoundedSquareRadiusFactor;
    path.addRoundedRect(bounds, radius, radius);
    break;
  }
  case LedShape::kCircle:
  default:
    path.addEllipse(bounds);
    break;
  }

  return path;
}

} // namespace

LedMonitorElement::LedMonitorElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  foregroundColor_ = MedmColors::alarmColorForSeverity(0);
  onColor_ = MedmColors::alarmColorForSeverity(0);
  offColor_ = QColor(45, 45, 45);
  undefinedColor_ = defaultUndefined();
  syncStateColorsFromBinary();
}

void LedMonitorElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool LedMonitorElement::isSelected() const
{
  return selected_;
}

QColor LedMonitorElement::foregroundColor() const
{
  return foregroundColor_;
}

void LedMonitorElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor LedMonitorElement::backgroundColor() const
{
  return backgroundColor_;
}

void LedMonitorElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}

TextColorMode LedMonitorElement::colorMode() const
{
  return colorMode_;
}

void LedMonitorElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {
    return;
  }
  colorMode_ = mode;
  update();
}

LedShape LedMonitorElement::shape() const
{
  return shape_;
}

void LedMonitorElement::setShape(LedShape shape)
{
  if (shape_ == shape) {
    return;
  }
  shape_ = shape;
  update();
}

bool LedMonitorElement::bezel() const
{
  return bezel_;
}

void LedMonitorElement::setBezel(bool bezel)
{
  if (bezel_ == bezel) {
    return;
  }
  bezel_ = bezel;
  update();
}

QColor LedMonitorElement::onColor() const
{
  return onColor_;
}

void LedMonitorElement::setOnColor(const QColor &color)
{
  if (onColor_ == color) {
    return;
  }
  onColor_ = color;
  syncStateColorsFromBinary();
  update();
}

QColor LedMonitorElement::offColor() const
{
  return offColor_;
}

void LedMonitorElement::setOffColor(const QColor &color)
{
  if (offColor_ == color) {
    return;
  }
  offColor_ = color;
  syncStateColorsFromBinary();
  update();
}

QColor LedMonitorElement::undefinedColor() const
{
  return undefinedColor_;
}

void LedMonitorElement::setUndefinedColor(const QColor &color)
{
  if (undefinedColor_ == color) {
    return;
  }
  undefinedColor_ = color;
  update();
}

QColor LedMonitorElement::stateColor(int index) const
{
  if (index < 0 || index >= kLedStateCount) {
    return QColor();
  }
  return stateColors_[static_cast<std::size_t>(index)];
}

void LedMonitorElement::setStateColor(int index, const QColor &color)
{
  if (index < 0 || index >= kLedStateCount) {
    return;
  }
  QColor &target = stateColors_[static_cast<std::size_t>(index)];
  if (target == color) {
    return;
  }
  target = color;
  syncBinaryColorsFromStates();
  update();
}

int LedMonitorElement::stateCount() const
{
  return stateCount_;
}

void LedMonitorElement::setStateCount(int count)
{
  count = std::clamp(count, 1, kLedStateCount);
  if (stateCount_ == count) {
    return;
  }
  stateCount_ = count;
  update();
}

bool LedMonitorElement::isQtedmSymbol() const
{
  return qtedmSymbol_;
}

void LedMonitorElement::setQtedmSymbol(bool enabled)
{
  if (qtedmSymbol_ == enabled) {
    return;
  }
  qtedmSymbol_ = enabled;
  if (enabled && symbolStates_.isEmpty()) {
    symbolStates_ = {
        {0.0, 0.0, offColor_, QStringLiteral("Off"), QString()},
        {1.0, 1.0, onColor_, QStringLiteral("On"), QString()}};
  }
  update();
}

QVector<LedMonitorElement::SymbolState> LedMonitorElement::symbolStates() const
{
  return symbolStates_;
}

void LedMonitorElement::setSymbolStates(const QVector<SymbolState> &states)
{
  symbolStates_ = states.mid(0, kLedStateCount);
  update();
}

void LedMonitorElement::setSymbolState(int index, const SymbolState &state)
{
  if (index < 0 || index >= kLedStateCount) {
    return;
  }
  if (symbolStates_.size() <= index) {
    symbolStates_.resize(index + 1);
  }
  symbolStates_[index] = state;
  update();
}

QString LedMonitorElement::baseDirectory() const
{
  return baseDirectory_;
}

void LedMonitorElement::setBaseDirectory(const QString &directory)
{
  baseDirectory_ = directory;
  update();
}

QString LedMonitorElement::channel() const
{
  return channel_;
}

void LedMonitorElement::setChannel(const QString &channel)
{
  const QString normalized = PvNameUtils::normalizePvName(channel);
  if (channel_ == normalized) {
    return;
  }
  channel_ = normalized;
  setToolTip(QString());
  update();
}

QString LedMonitorElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(visibilityChannels_.size())) {
    return QString();
  }
  return visibilityChannels_[static_cast<std::size_t>(index)];
}

void LedMonitorElement::setChannel(int index, const QString &channel)
{
  if (index < 0 || index >= static_cast<int>(visibilityChannels_.size())) {
    return;
  }
  const QString normalized = PvNameUtils::normalizePvName(channel);
  QString &target = visibilityChannels_[static_cast<std::size_t>(index)];
  if (target == normalized) {
    return;
  }
  target = normalized;
  update();
}

TextVisibilityMode LedMonitorElement::visibilityMode() const
{
  return visibilityMode_;
}

void LedMonitorElement::setVisibilityMode(TextVisibilityMode mode)
{
  if (visibilityMode_ == mode) {
    return;
  }
  visibilityMode_ = mode;
  update();
}

QString LedMonitorElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void LedMonitorElement::setVisibilityCalc(const QString &calc)
{
  const QString trimmed = calc.trimmed();
  if (visibilityCalc_ == trimmed) {
    return;
  }
  visibilityCalc_ = trimmed;
  update();
}

void LedMonitorElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  clearRuntimeState();
  applyRuntimeVisibility();
}

bool LedMonitorElement::isExecuteMode() const
{
  return executeMode_;
}

void LedMonitorElement::setRuntimeConnected(bool connected)
{
  if (!executeMode_) {
    return;
  }
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (!runtimeConnected_) {
    runtimeSeverity_ = kInvalidSeverity;
    hasRuntimeValue_ = false;
    runtimeValue_ = 0.0;
  }
  UpdateCoordinator::instance().requestUpdate(this);
}

void LedMonitorElement::setRuntimeSeverity(short severity)
{
  if (!executeMode_) {
    return;
  }
  const short clamped = std::clamp<short>(severity, 0, kInvalidSeverity);
  if (runtimeSeverity_ == clamped) {
    return;
  }
  runtimeSeverity_ = clamped;
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    UpdateCoordinator::instance().requestUpdate(this);
  }
}

void LedMonitorElement::setRuntimeVisible(bool visible)
{
  if (!executeMode_) {
    return;
  }
  if (runtimeVisible_ == visible) {
    return;
  }
  runtimeVisible_ = visible;
  applyRuntimeVisibility();
}

void LedMonitorElement::setRuntimeValue(double value)
{
  if (!executeMode_) {
    return;
  }
  if (!std::isfinite(value)) {
    return;
  }
  if (hasRuntimeValue_ && qFuzzyCompare(runtimeValue_ + 1.0, value + 1.0)) {
    return;
  }
  runtimeValue_ = value;
  hasRuntimeValue_ = true;
  UpdateCoordinator::instance().requestUpdate(this);
}

void LedMonitorElement::setRuntimeLimits(double low, double high)
{
  Q_UNUSED(low);
  Q_UNUSED(high);
}

void LedMonitorElement::setRuntimePrecision(int precision)
{
  Q_UNUSED(precision);
}

void LedMonitorElement::clearRuntimeState()
{
  runtimeConnected_ = false;
  hasRuntimeValue_ = false;
  runtimeValue_ = 0.0;
  runtimeSeverity_ = kInvalidSeverity;
  if (executeMode_) {
    UpdateCoordinator::instance().requestUpdate(this);
  } else {
    update();
  }
}

void LedMonitorElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.fillRect(rect(), effectiveBackground());

  QRectF bounds = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
  if (bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  const FillState fillState = effectiveFillState();
  if (qtedmSymbol_) {
    paintSymbol(painter, bounds, fillState);
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }
  QColor fillColor = fillState.color.isValid() ? fillState.color : defaultUndefined();
  const QPainterPath outerPath = ledPathForShape(shape_, bounds);
  if (outerPath.isEmpty()) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  painter.save();
  painter.setPen(Qt::NoPen);

  QPainterPath innerPath = outerPath;
  QColor ringColor = clampLightness(fillColor, -80);
  if (bezel_) {
    const QRectF innerBounds = bounds.adjusted(kLedBezelWidth, kLedBezelWidth,
        -kLedBezelWidth, -kLedBezelWidth);
    if (innerBounds.width() > 0.0 && innerBounds.height() > 0.0) {
      innerPath = ledPathForShape(shape_, innerBounds);
    }
    painter.fillPath(outerPath, ringColor);
  }

  if (bezel_) {
    QRectF gradientBounds = innerPath.boundingRect();
    QPointF highlightCenter = gradientBounds.topLeft()
        + QPointF(gradientBounds.width() * 0.35, gradientBounds.height() * 0.35);
    qreal radius = std::max(gradientBounds.width(), gradientBounds.height());
    QRadialGradient gradient(highlightCenter, radius, highlightCenter);
    gradient.setColorAt(0.0, clampLightness(fillColor, 70));
    gradient.setColorAt(0.55, fillColor);
    gradient.setColorAt(1.0, clampLightness(fillColor, -60));
    painter.fillPath(innerPath, gradient);
  } else {
    painter.fillPath(innerPath, fillColor);
  }

  if (fillState.hatched) {
    painter.save();
    painter.setClipPath(innerPath);
    painter.fillPath(innerPath, fillColor);
    QColor hatchColor = clampLightness(fillColor, -120);
    hatchColor.setAlpha(200);
    painter.fillPath(innerPath, QBrush(hatchColor, Qt::BDiagPattern));
    painter.restore();
  }

  painter.restore();

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

void LedMonitorElement::applyRuntimeVisibility()
{
  QWidget::setVisible(!executeMode_ || runtimeVisible_);
}

void LedMonitorElement::paintSelectionOverlay(QPainter &painter) const
{
  painter.save();
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
  painter.restore();
}

LedMonitorElement::FillState LedMonitorElement::effectiveFillState() const
{
  if (executeMode_ && !runtimeConnected_) {
    return {undefinedColor_.isValid() ? undefinedColor_ : defaultUndefined(),
        true};
  }

  if (!executeMode_) {
    if (colorMode_ == TextColorMode::kStatic) {
      return {foregroundColor_.isValid() ? foregroundColor_ : defaultForeground(),
          false};
    }
    return {offColor_.isValid() ? offColor_ : QColor(45, 45, 45), false};
  }

  switch (colorMode_) {
  case TextColorMode::kStatic:
    return {foregroundColor_.isValid() ? foregroundColor_ : defaultForeground(),
        false};
  case TextColorMode::kAlarm:
    return {MedmColors::alarmColorForSeverity(runtimeSeverity_), false};
  case TextColorMode::kDiscrete:
  default:
    if (hasRuntimeValue_) {
      const double minimum =
          static_cast<double>(std::numeric_limits<int>::min());
      const double maximum =
          static_cast<double>(std::numeric_limits<int>::max());
      const int index = static_cast<int>(
          std::clamp(runtimeValue_, minimum, maximum));
      return {currentColorForState(index), false};
    }
    return {undefinedColor_.isValid() ? undefinedColor_ : defaultUndefined(),
        false};
  }
}

int LedMonitorElement::currentSymbolStateIndex() const
{
  if (executeMode_ && (!runtimeConnected_ || !hasRuntimeValue_)) {
    return -1;
  }
  if (!executeMode_) {
    return symbolStates_.isEmpty() ? -1 : 0;
  }
  for (int i = 0; i < symbolStates_.size(); ++i) {
    const SymbolState &state = symbolStates_.at(i);
    const double low = std::min(state.minimum, state.maximum);
    const double high = std::max(state.minimum, state.maximum);
    if (runtimeValue_ >= low && runtimeValue_ <= high) {
      return i;
    }
  }
  return -1;
}

void LedMonitorElement::paintSymbol(QPainter &painter, const QRectF &bounds,
    const FillState &fillState)
{
  const int index = currentSymbolStateIndex();
  const SymbolState *state = index >= 0 && index < symbolStates_.size()
      ? &symbolStates_.at(index) : nullptr;
  QColor color = state && state->color.isValid()
      ? state->color : fillState.color;
  if (!color.isValid()) {
    color = defaultUndefined();
  }
  painter.fillRect(bounds, color);

  if (state && !state->imagePath.trimmed().isEmpty()) {
    QString path = state->imagePath;
    if (QFileInfo(path).isRelative() && !baseDirectory_.isEmpty()) {
      path = QDir(baseDirectory_).filePath(path);
    }
    const QPixmap pixmap(path);
    if (!pixmap.isNull()) {
      painter.drawPixmap(bounds.toRect(), pixmap, pixmap.rect());
    }
  }

  if (fillState.hatched || !state) {
    QColor hatch = clampLightness(color, -100);
    hatch.setAlpha(190);
    painter.fillRect(bounds, QBrush(hatch, Qt::BDiagPattern));
  }

  const QString label = state ? state->label
      : (executeMode_ ? QStringLiteral("Undefined")
                      : QStringLiteral("Symbol"));
  if (!label.isEmpty()) {
    QColor textColor = color.lightness() < 128 ? Qt::white : Qt::black;
    painter.setPen(textColor);
    painter.drawText(bounds.adjusted(3, 2, -3, -2),
        Qt::AlignCenter | Qt::TextWordWrap, label);
  }

  if (executeMode_ && runtimeConnected_ && runtimeSeverity_ > 0
      && runtimeSeverity_ < kInvalidSeverity) {
    QPen alarmPen(MedmColors::alarmColorForSeverity(runtimeSeverity_));
    alarmPen.setWidth(3);
    painter.setPen(alarmPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(bounds.adjusted(1.5, 1.5, -1.5, -1.5));
  }
}

QColor LedMonitorElement::effectiveBackground() const
{
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return defaultBackground();
}

QColor LedMonitorElement::defaultForeground() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

QColor LedMonitorElement::defaultBackground() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::Window);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::Window);
  }
  return QColor(Qt::white);
}

QColor LedMonitorElement::defaultUndefined() const
{
  return QColor(204, 204, 204);
}

QColor LedMonitorElement::currentColorForState(int index) const
{
  if (index >= 0 && index < stateCount_ && index < kLedStateCount) {
    const QColor color = stateColors_[static_cast<std::size_t>(index)];
    if (color.isValid()) {
      return color;
    }
  }
  return undefinedColor_.isValid() ? undefinedColor_ : defaultUndefined();
}

void LedMonitorElement::syncBinaryColorsFromStates()
{
  if (stateColors_[0].isValid()) {
    offColor_ = stateColors_[0];
  }
  if (stateColors_[1].isValid()) {
    onColor_ = stateColors_[1];
  }
}

void LedMonitorElement::syncStateColorsFromBinary()
{
  stateColors_[0] = offColor_;
  stateColors_[1] = onColor_;
  for (int i = 2; i < kLedStateCount; ++i) {
    if (!stateColors_[static_cast<std::size_t>(i)].isValid()) {
      stateColors_[static_cast<std::size_t>(i)] = defaultUndefined();
    }
  }
}
