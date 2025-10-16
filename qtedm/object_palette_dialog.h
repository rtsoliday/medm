#pragma once

#include <QDialog>
#include <QFont>
#include <QHash>
#include <QPalette>
#include <QString>

#include <vector>

class QAbstractButton;
class QButtonGroup;
class QLabel;
class QEvent;
class QToolButton;
class QWidget;

class ObjectPaletteDialog : public QDialog
{
public:
  ObjectPaletteDialog(const QPalette &basePalette, const QFont &labelFont,
      const QFont &buttonFont, QWidget *parent = nullptr);

  void showAndRaise();

private:
  struct ButtonDefinition {
    QString label;
    const unsigned char *bits;
    int width;
    int height;
  };

  QWidget *createCategory(const QString &title,
    const std::vector<ButtonDefinition> &buttons);
  QToolButton *createToolButton(const ButtonDefinition &definition);
  bool eventFilter(QObject *watched, QEvent *event) override;
  void handleButtonToggled(int id, bool checked);
  void updateStatusLabel(const QString &description);
  static std::vector<ButtonDefinition> graphicsButtons();
  static std::vector<ButtonDefinition> monitorButtons();
  static std::vector<ButtonDefinition> controlButtons();
  static std::vector<ButtonDefinition> miscButtons();

  QPalette basePalette_;
  QFont labelFont_;
  QFont buttonFont_;
  QButtonGroup *buttonGroup_ = nullptr;
  QLabel *statusLabel_ = nullptr;
  QAbstractButton *selectButton_ = nullptr;
  QHash<int, QString> buttonDescriptions_;
  int nextButtonId_ = 0;
};
