#pragma once

#include <array>
#include <functional>
#include <vector>

#include <QColor>
#include <QDialog>
#include <QFont>
#include <QString>

class QLabel;
class QPushButton;

class ColorPaletteDialog : public QDialog
{
public:
  ColorPaletteDialog(const QPalette &basePalette, const QFont &labelFont,
      const QFont &valueFont, QWidget *parent = nullptr);

  void setColorSelectedCallback(std::function<void(const QColor &)> callback);
  void setCurrentColor(const QColor &color, const QString &description);

private:
  static constexpr int kColorRows = 5;

  static const std::array<QColor, 65> &paletteColors();
  void configureButtonColor(QPushButton *button, const QColor &color);
  void handleColorClicked(int index);
  void updateSelection();
  void updateMessageLabel();

  QFont labelFont_;
  QFont valueFont_;
  std::vector<QPushButton *> colorButtons_;
  QLabel *messageLabel_ = nullptr;
  QColor currentColor_;
  QString description_;
  std::function<void(const QColor &)> colorSelectedCallback_;
};

