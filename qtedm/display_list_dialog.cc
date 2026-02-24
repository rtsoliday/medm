#include "display_list_dialog.h"

#include <QAbstractItemView>
#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>
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
        updateClipboardFromSelection();
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
  const auto displays = selectedDisplays();
  bool hasValidSelection = false;
  for (const auto &display : displays) {
    if (!display.isNull()) {
      hasValidSelection = true;
      break;
    }
  }
  raiseButton_->setEnabled(hasValidSelection);
  closeButton_->setEnabled(hasValidSelection);
}

void DisplayListDialog::updateClipboardFromSelection()
{
  QStringList selectedPaths;
  for (const auto &display : selectedDisplays()) {
    if (display.isNull()) {
      continue;
    }
    const QString path = display->filePath().trimmed();
    if (!path.isEmpty()) {
      selectedPaths.append(path);
    }
  }

  if (selectedPaths.isEmpty()) {
    return;
  }

  selectedPaths.removeDuplicates();
  const QString clipboardText = selectedPaths.join(QStringLiteral("\n"));
  if (QClipboard *clipboard = QGuiApplication::clipboard()) {
    clipboard->setText(clipboardText, QClipboard::Clipboard);
    clipboard->setText(clipboardText, QClipboard::Selection);
  }
}

QList<QPointer<DisplayWindow>> DisplayListDialog::selectedDisplays() const
{
  QList<QPointer<DisplayWindow>> result;
  for (QListWidgetItem *item : listWidget_->selectedItems()) {
    if (auto *displayItem = dynamic_cast<DisplayListItem *>(item)) {
      if (auto display = displayItem->display(); !display.isNull()) {
        result.append(display);
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
  QString result = isDirty ? (path + QLatin1Char('*')) : path;

  /* Append macro values like medm does */
  const QHash<QString, QString> &macros = display->macroDefinitions();
  if (!macros.isEmpty()) {
    QStringList macroList;
    for (auto it = macros.constBegin(); it != macros.constEnd(); ++it) {
      macroList.append(it.key() + QLatin1Char('=') + it.value());
    }
    /* Sort to ensure consistent display order */
    macroList.sort();
    for (const QString &macro : macroList) {
      result += QLatin1Char(' ') + macro;
    }
  }

  return result;
}

void DisplayListDialog::handleRaiseRequested()
{
  const auto displays = selectedDisplays();
  if (displays.isEmpty()) {
    return;
  }

  for (const auto &display : displays) {
    if (display.isNull()) {
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

  for (const auto &display : displays) {
    if (display.isNull()) {
      continue;
    }
    display->close();
  }

  /* Refresh the list after closing displays. The destroyed signal will also
   * trigger a refresh via handleStateChanged(), but that happens after
   * deferred deletion processes. Do an immediate refresh to update the UI
   * promptly. */
  refresh();
}

void DisplayListDialog::handleRefreshRequested()
{
  refresh();
}
