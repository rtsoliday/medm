#pragma once

#include <QDialog>
#include <QFont>
#include <QHash>
#include <QPalette>
#include <QString>

#include <vector>
#include <memory>

#include "display_state.h"

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
      const QFont &buttonFont, std::weak_ptr<DisplayState> state,
      QWidget *parent = nullptr);

  void showAndRaise();
  void refreshSelectionFromState();

private:
  struct ButtonDefinition {
    QString label;
    const unsigned char *bits;
    int width;
    int height;
    CreateTool tool;
  };

  QWidget *createCategory(const QString &title,
    const std::vector<ButtonDefinition> &buttons);
  QToolButton *createToolButton(const ButtonDefinition &definition);
  bool eventFilter(QObject *watched, QEvent *event) override;
  void handleButtonToggled(int id, bool checked);
  void updateStatusLabel(const QString &description);
  void applyCreateToolSelection(int id);
  void syncButtonsToState();
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
  QHash<int, CreateTool> buttonTools_;
  int nextButtonId_ = 0;
  std::weak_ptr<DisplayState> state_;
};
