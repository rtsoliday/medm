#pragma once

#include <QDialog>
#include <QFont>
#include <QList>
#include <QPalette>
#include <QPointer>
#include <QString>

#include <memory>

class QListWidget;
class QListWidgetItem;
class QPushButton;

struct DisplayState;
class DisplayWindow;

class DisplayListDialog : public QDialog
{
public:
  DisplayListDialog(const QPalette &basePalette, const QFont &itemFont,
      std::weak_ptr<DisplayState> state, QWidget *parent = nullptr);

  void showAndRaise();
  void handleStateChanged();

private:
  void refresh();
  void updateButtonStates();
  QList<QPointer<DisplayWindow>> selectedDisplays() const;
  QString labelForDisplay(DisplayWindow *display) const;
  void handleRaiseRequested();
  void handleCloseRequested();
  void handleRefreshRequested();

  std::weak_ptr<DisplayState> state_;
  QListWidget *listWidget_ = nullptr;
  QPushButton *raiseButton_ = nullptr;
  QPushButton *closeButton_ = nullptr;
  QPushButton *refreshButton_ = nullptr;
  QPushButton *closeDialogButton_ = nullptr;
  QFont itemFont_;
};
