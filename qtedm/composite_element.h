#pragma once

#include <array>

#include <QColor>
#include <QList>
#include <QPointer>
#include <QWidget>

#include "display_properties.h"

class CompositeElement : public QWidget
{
public:
  explicit CompositeElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QString compositeName() const;
  void setCompositeName(const QString &name);

  QString compositeFile() const;
  void setCompositeFile(const QString &filePath);

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  TextVisibilityMode visibilityMode() const;
  void setVisibilityMode(TextVisibilityMode mode);

  QString visibilityCalc() const;
  void setVisibilityCalc(const QString &calc);

  QString channel(int index) const;
  void setChannel(int index, const QString &value);

  std::array<QString, 5> channels() const;

  void adoptChild(QWidget *child);
  QList<QWidget *> childWidgets() const;

  void setExecuteMode(bool execute);
  
  void setChannelConnected(bool connected);
  bool isChannelConnected() const;
  
  void expandToFitChildren();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  QColor defaultForegroundColor() const;
  QColor defaultBackgroundColor() const;
  bool forwardMouseEventToParent(QMouseEvent *event) const;
  void updateMouseTransparency();
  bool hasAnyChannel() const;
  bool hasInteractiveChildren() const;

  bool selected_ = false;
  QString compositeName_;
  QString compositeFile_;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode_ = TextVisibilityMode::kStatic;
  QString visibilityCalc_;
  std::array<QString, 5> channels_{};
  QList<QPointer<QWidget>> childWidgets_;
  bool executeMode_ = false;
  bool channelConnected_ = false;
};
