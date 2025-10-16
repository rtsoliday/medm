#include "display_list_dialog.h"

#include <QAbstractItemView>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

#include "display_state.h"
#include "display_window.h"

namespace {

class DisplayListItem : public QListWidgetItem
{
public:
  DisplayListItem(const QString &text, const QPointer<DisplayWindow> &display,
      const QFont &font)
    : QListWidgetItem(text)
    , display_(display)
  {
    setFont(font);
  }

  QPointer<DisplayWindow> display() const
  {
    return display_;
  }

private:
  QPointer<DisplayWindow> display_;
};

} // namespace

DisplayListDialog::DisplayListDialog(const QPalette &basePalette,
    const QFont &itemFont, std::weak_ptr<DisplayState> state, QWidget *parent)
  : QDialog(parent)
  , state_(std::move(state))
  , itemFont_(itemFont)
{
  setObjectName(QStringLiteral("qtedmDisplayListDialog"));
  setWindowTitle(QStringLiteral("Display List"));
  setModal(false);
  setAutoFillBackground(true);
  setPalette(basePalette);
  setBackgroundRole(QPalette::Window);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(10);

  listWidget_ = new QListWidget;
  listWidget_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  listWidget_->setFont(itemFont_);
  listWidget_->setAutoFillBackground(true);
  listWidget_->setPalette(basePalette);
  listWidget_->setBackgroundRole(QPalette::Base);
  layout->addWidget(listWidget_);

  auto *buttonRow = new QHBoxLayout;
  buttonRow->setContentsMargins(0, 0, 0, 0);
  buttonRow->setSpacing(8);
  buttonRow->addStretch();

  raiseButton_ = new QPushButton(QStringLiteral("Raise"));
  closeButton_ = new QPushButton(QStringLiteral("Close Display"));
  refreshButton_ = new QPushButton(QStringLiteral("Refresh"));
  closeDialogButton_ = new QPushButton(QStringLiteral("Close"));

  for (auto *button : {raiseButton_, closeButton_, refreshButton_, closeDialogButton_}) {
    button->setFont(itemFont_);
    button->setAutoFillBackground(true);
    button->setPalette(basePalette);
    buttonRow->addWidget(button);
  }

  layout->addLayout(buttonRow);

  connect(listWidget_, &QListWidget::itemSelectionChanged, this,
      [this]() {
        updateButtonStates();
      });
  connect(listWidget_, &QListWidget::itemDoubleClicked, this,
      [this](QListWidgetItem *) {
        handleRaiseRequested();
      });
  connect(raiseButton_, &QPushButton::clicked, this,
      [this]() {
        handleRaiseRequested();
      });
  connect(closeButton_, &QPushButton::clicked, this,
      [this]() {
        handleCloseRequested();
      });
  connect(refreshButton_, &QPushButton::clicked, this,
      [this]() {
        handleRefreshRequested();
      });
  connect(closeDialogButton_, &QPushButton::clicked, this,
      [this]() {
        hide();
      });

  refresh();
  updateButtonStates();
  adjustSize();
  const QSize hint = sizeHint();
  resize(hint.width() * 2, hint.height());
}

void DisplayListDialog::showAndRaise()
{
  refresh();
  show();
  raise();
  activateWindow();
}

void DisplayListDialog::handleStateChanged()
{
  if (isVisible()) {
    refresh();
  }
}

void DisplayListDialog::refresh()
{
  QList<QPointer<DisplayWindow>> previouslySelected;
  for (QListWidgetItem *item : listWidget_->selectedItems()) {
    if (auto *displayItem = dynamic_cast<DisplayListItem *>(item)) {
      previouslySelected.append(displayItem->display());
    }
  }

  listWidget_->clear();

  if (auto state = state_.lock()) {
    for (const auto &displayPtr : state->displays) {
      if (displayPtr.isNull()) {
        continue;
      }
      DisplayWindow *display = displayPtr.data();
      auto *item = new DisplayListItem(labelForDisplay(display), displayPtr,
          itemFont_);
      listWidget_->addItem(item);
      for (const auto &selected : previouslySelected) {
        if (selected == displayPtr) {
          item->setSelected(true);
          break;
        }
      }
    }
  }

  updateButtonStates();
}

void DisplayListDialog::updateButtonStates()
{
  const bool hasSelection = !selectedDisplays().isEmpty();
  raiseButton_->setEnabled(hasSelection);
  closeButton_->setEnabled(hasSelection);
}

QList<DisplayWindow *> DisplayListDialog::selectedDisplays() const
{
  QList<DisplayWindow *> result;
  for (QListWidgetItem *item : listWidget_->selectedItems()) {
    if (auto *displayItem = dynamic_cast<DisplayListItem *>(item)) {
      if (auto display = displayItem->display(); !display.isNull()) {
        result.append(display.data());
      }
    }
  }
  return result;
}

QString DisplayListDialog::labelForDisplay(DisplayWindow *display) const
{
  if (!display) {
    return QStringLiteral("(unavailable)");
  }

  QString title = display->windowTitle();
  if (title.isEmpty()) {
    title = QStringLiteral("(untitled)");
  }

  const QString path = display->filePath();
  if (path.isEmpty()) {
    return title;
  }

  const bool isDirty = title.endsWith(QLatin1Char('*'));
  if (isDirty) {
    return path + QLatin1Char('*');
  }
  return path;
}

void DisplayListDialog::handleRaiseRequested()
{
  const auto displays = selectedDisplays();
  if (displays.isEmpty()) {
    return;
  }

  for (DisplayWindow *display : displays) {
    if (!display) {
      continue;
    }
    display->show();
    display->raise();
    display->activateWindow();
  }

  updateButtonStates();
}

void DisplayListDialog::handleCloseRequested()
{
  const auto displays = selectedDisplays();
  if (displays.isEmpty()) {
    return;
  }

  for (DisplayWindow *display : displays) {
    if (!display) {
      continue;
    }
    display->close();
  }

  updateButtonStates();
}

void DisplayListDialog::handleRefreshRequested()
{
  refresh();
}
