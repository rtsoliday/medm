#pragma once

#include <array>

#include <QColor>
#include <QEvent>
#include <QHideEvent>
#include <QLabel>
#include <QMoveEvent>
#include <QPaintEvent>
#include <QRect>
#include <QResizeEvent>
#include <QShowEvent>
#include <QString>
#include <QTimer>

#include "display_properties.h"

class TextOverflowWidget;

class TextElement : public QLabel
{
public:
  explicit TextElement(QWidget *parent = nullptr);
  ~TextElement() override;

  void setSelected(bool selected);
  bool isSelected() const;

  QRect boundingRect() const;
  QRect visualBoundsRelativeToParent() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  void setText(const QString &value);

  Qt::Alignment textAlignment() const;
  void setTextAlignment(Qt::Alignment alignment);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  TextVisibilityMode visibilityMode() const;
  void setVisibilityMode(TextVisibilityMode mode);

  QString visibilityCalc() const;
  void setVisibilityCalc(const QString &calc);

  QString channel(int index) const;
  void setChannel(int index, const QString &value);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setRuntimeConnected(bool connected);
  void setRuntimeVisible(bool visible);
  void setRuntimeSeverity(short severity);

  void setVisible(bool visible) override;

protected:
  bool event(QEvent *event) override;
  void moveEvent(QMoveEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;

private:
  friend class TextOverflowWidget;

  QColor defaultForegroundColor() const;
  QColor effectiveForegroundColor() const;
  bool hasConfiguredChannel() const;
  bool shouldSuppressTextForDisconnect() const;
  void applyTextColor();
  void applyTextVisibility();
  void updateSelectionVisual();
  void updateFontForGeometry();
  void updateExecuteState();
  void ensureOverflowWidget();
  void updateOverflowParent();
  void updateOverflowGeometry();
  void updateOverflowVisibility();
  void updateOverflowStacking();
  void requestOverflowRepaint();

  bool selected_ = false;
  QColor foregroundColor_;
  Qt::Alignment alignment_ = Qt::AlignLeft | Qt::AlignVCenter;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode_ = TextVisibilityMode::kStatic;
  QString visibilityCalc_;
  std::array<QString, 5> channels_{};
  bool executeMode_ = false;
  bool designModeVisible_ = true;
  bool runtimeConnected_ = false;
  bool runtimeEverConnected_ = false;
  bool allowDisconnectIndication_ = false;
  bool runtimeVisible_ = true;
  short runtimeSeverity_ = 0;
  TextOverflowWidget *overflowWidget_ = nullptr;
  QTimer *disconnectIndicationTimer_ = nullptr;
};
