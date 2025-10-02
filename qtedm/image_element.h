#pragma once

#include <array>

#include <QColor>
#include <QPixmap>
#include <QWidget>

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

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void loadPixmap();
  QColor foregroundColor() const;
  QColor backgroundColor() const;

  bool selected_ = false;
  ImageType imageType_ = ImageType::kNone;
  QString imageName_;
  QString calc_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode_ = TextVisibilityMode::kStatic;
  QString visibilityCalc_;
  std::array<QString, 4> channels_{};
  QPixmap pixmap_;
};
