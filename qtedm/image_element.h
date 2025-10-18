#pragma once

#include <array>

#include <QColor>
#include <QPixmap>
#include <QWidget>
#include <QMovie>

#include "display_properties.h"

class ImageElement : public QWidget
{
public:
  explicit ImageElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  ImageType imageType() const;
  void setImageType(ImageType type);

  QString imageName() const;
  void setImageName(const QString &name);
  QString baseDirectory() const;
  void setBaseDirectory(const QString &directory);

  QString calc() const;
  void setCalc(const QString &calc);

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
  void setRuntimeAnimate(bool animate);
  void setRuntimeFrameIndex(int index);
  void setRuntimeFrameValid(bool valid);

  int frameCount() const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void setVisible(bool visible) override;

private:
  void reloadImage();
  void disposeMovie();
  void updateCurrentPixmap();
  void applyRuntimeVisibility();
  QColor foregroundColor() const;
  QColor backgroundColor() const;

  bool selected_ = false;
  ImageType imageType_ = ImageType::kNone;
  QString imageName_;
  QString calc_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode_ = TextVisibilityMode::kStatic;
  QString visibilityCalc_;
  std::array<QString, 5> channels_{};
  QString baseDirectory_;
  QPixmap pixmap_;
  QMovie *movie_ = nullptr;
  int cachedFrameCount_ = 0;
  bool executeMode_ = false;
  bool designModeVisible_ = true;
  bool runtimeConnected_ = false;
  bool runtimeVisible_ = true;
  short runtimeSeverity_ = 0;
  bool runtimeAnimate_ = false;
  bool runtimeFrameValid_ = true;
  int runtimeFrameIndex_ = 0;
};
