#pragma once

#include <QColor>
#include <QEvent>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <functional>
#include <vector>

#include "display_properties.h"

class QMouseEvent;
class QKeyEvent;
class QTimer;

class WheelSwitchElement : public QWidget
{
public:
  explicit WheelSwitchElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  double precision() const;
  void setPrecision(double precision);

  QString format() const;
  void setFormat(const QString &format);

  const PvLimits &limits() const;
  void setLimits(const PvLimits &limits);

  QString channel() const;
  void setChannel(const QString &channel);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setRuntimeConnected(bool connected);
  void setRuntimeWriteAccess(bool writeAccess);
  void setRuntimeSeverity(short severity);
  void setRuntimeLimits(double low, double high);
  void setRuntimePrecision(int precision);
  void setRuntimeValue(double value);
  void clearRuntimeState();

  void setActivationCallback(const std::function<void(double)> &callback);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  enum class RepeatDirection
  {
    kNone,
    kUp,
    kDown,
  };

  struct Layout
  {
    struct Slot
    {
      QRectF charRect;
      QRectF upButton;
      QRectF downButton;
      QChar character;
      double step = 0.0;
      int exponent = 0;
      bool hasButtons = false;
    };

    QRectF outer;
    QRectF valueRect;
    double buttonHeight = 0.0;
    QString text;
    QFont font;
    std::vector<Slot> columns;
  };

  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QColor valueForeground() const;
  QColor buttonFillColor(bool isUp, bool pressed, bool enabled) const;
  Layout layoutForRect(const QRectF &bounds) const;
  void paintButton(QPainter &painter, const QRectF &rect, bool isUp,
    bool pressed, bool enabled, bool hovered) const;
  void paintValueDisplay(QPainter &painter, const Layout &layout) const;
  void paintSelectionOverlay(QPainter &painter) const;
  QString displayText() const;
  int formatDecimals() const;
  double displayedValue() const;
  double effectiveLowLimit() const;
  double effectiveHighLimit() const;
  int effectivePrecision() const;
  double sampleValue() const;
  double defaultSampleValue() const;
  double clampToLimits(double value) const;
  double valueStep(Qt::KeyboardModifiers mods) const;
  double applyModifiersToStep(double step, Qt::KeyboardModifiers mods) const;
  void startRepeating(RepeatDirection direction, double step, int slotIndex);
  void stopRepeating();
  void performStep(RepeatDirection direction, double step, bool forceSend);
  void activateValue(double value, bool forceSend);
  void updateCursor();
  bool isInteractive() const;
  double valueEpsilon() const;
  int slotIndexForStep(const Layout &layout, double step) const;
  int defaultSlotIndex(const Layout &layout) const;
  void updateHoverState(const QPointF &pos);
  void clearHoverState();
  bool forwardMouseEventToParent(QMouseEvent *event) const;

  void handleRepeatTimeout();

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  double precision_ = 1.0;
  QString format_;
  PvLimits limits_{};
  QString channel_;
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  bool runtimeWriteAccess_ = false;
  short runtimeSeverity_ = 0;
  double runtimeLow_ = 0.0;
  double runtimeHigh_ = 0.0;
  bool runtimeLimitsValid_ = false;
  int runtimePrecision_ = -1;
  double runtimeValue_ = 0.0;
  bool hasRuntimeValue_ = false;
  int pressedSlotIndex_ = -1;
  RepeatDirection pressedDirection_ = RepeatDirection::kNone;
  RepeatDirection repeatDirection_ = RepeatDirection::kNone;
  double repeatStep_ = 0.0;
  QTimer *repeatTimer_ = nullptr;
  std::function<void(double)> activationCallback_;
  double lastSentValue_ = 0.0;
  bool hasLastSentValue_ = false;
  int hoveredSlotIndex_ = -1;
  RepeatDirection hoveredDirection_ = RepeatDirection::kNone;
};
