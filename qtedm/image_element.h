#pragma once

#include <QColor>
#include <QPixmap>
#include <QWidget>
#include <QMovie>

#include "display_properties.h"
#include "graphic_shape_element.h"

class ImageElement : public GraphicShapeElement
{
public:
  explicit ImageElement(QWidget *parent = nullptr);

  ImageType imageType() const;
  void setImageType(ImageType type);

  QString imageName() const;
  void setImageName(const QString &name);
  QString baseDirectory() const;
  void setBaseDirectory(const QString &directory);

  QString calc() const;
  void setCalc(const QString &calc);

  void setRuntimeAnimate(bool animate);
  void setRuntimeFrameIndex(int index);
  void setRuntimeFrameValid(bool valid);

  int frameCount() const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void onRuntimeStateReset() override;
  void onRuntimeConnectedChanged() override;
  void onRuntimeSeverityChanged() override;
  short normalizeRuntimeSeverity(short severity) const override;

private:
  void reloadImage();
  void disposeMovie();
  void updateCurrentPixmap();
  QColor foregroundColor() const;
  QColor backgroundColor() const;

  ImageType imageType_ = ImageType::kNone;
  QString imageName_;
  QString calc_;
  QString baseDirectory_;
  QPixmap pixmap_;
  QMovie *movie_ = nullptr;
  int cachedFrameCount_ = 0;
  bool runtimeAnimate_ = false;
  bool runtimeFrameValid_ = true;
  int runtimeFrameIndex_ = 0;
};
