#include "window_utils.h"

#include <QCursor>
#include <QDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QString>

#include <algorithm>

namespace {

void centerDialog(QDialog *dialog)
{
  centerWindowOnScreen(dialog);
}

} // namespace

void positionWindowTopRight(QWidget *window, int rightMargin, int topMargin)
{
  if (!window) {
    return;
  }

  QScreen *screen = window->screen();
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }
  if (!screen) {
    return;
  }

  const QRect screenGeometry = screen->availableGeometry();
  QSize frameSize = window->frameGeometry().size();
  if (frameSize.isEmpty()) {
    frameSize = window->size();
  }

  const int xOffset = std::max(0,
      screenGeometry.width() - frameSize.width() - rightMargin);
  const int yOffset = std::max(0, topMargin);
  const int x = screenGeometry.x() + xOffset;
  const int y = screenGeometry.y() + yOffset;

  window->move(x, y);
}

void centerWindowOnScreen(QWidget *window)
{
  if (!window) {
    return;
  }

  QScreen *screen = window->screen();
  if (!screen) {
    if (QWidget *parent = window->parentWidget()) {
      screen = parent->screen();
    }
  }
  if (!screen) {
    screen = QGuiApplication::screenAt(QCursor::pos());
  }
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }
  if (!screen) {
    return;
  }

  const QRect screenGeometry = screen->availableGeometry();
  QSize targetSize = window->size();
  if (targetSize.isEmpty()) {
    targetSize = window->sizeHint();
  }

  const int x = screenGeometry.x() + std::max(0,
      (screenGeometry.width() - targetSize.width()) / 2);
  const int y = screenGeometry.y() + std::max(0,
      (screenGeometry.height() - targetSize.height()) / 2);

  window->move(x, y);
}

void showVersionDialog(QWidget *parent, const QFont &titleFont,
    const QFont &bodyFont, const QPalette &palette, bool autoClose)
{
  QDialog *dialog = parent ? parent->findChild<QDialog *>(
                                QStringLiteral("qtedmVersionDialog"))
                           : nullptr;

  if (!dialog) {
    dialog = new QDialog(parent,
        Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    dialog->setObjectName(QStringLiteral("qtedmVersionDialog"));
    dialog->setWindowTitle(QStringLiteral("Version"));
    dialog->setModal(false);
    dialog->setAutoFillBackground(true);
    dialog->setBackgroundRole(QPalette::Window);
    dialog->setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto *layout = new QHBoxLayout(dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(16);

    auto *nameFrame = new QFrame(dialog);
    nameFrame->setFrameShape(QFrame::Panel);
    nameFrame->setFrameShadow(QFrame::Raised);
    nameFrame->setLineWidth(2);
    nameFrame->setMidLineWidth(1);
    nameFrame->setAutoFillBackground(true);
    nameFrame->setBackgroundRole(QPalette::Button);
    nameFrame->setPalette(palette);

    auto *nameLayout = new QVBoxLayout(nameFrame);
    nameLayout->setContentsMargins(12, 8, 12, 8);
    nameLayout->setSpacing(0);

    auto *nameLabel = new QLabel(QStringLiteral("QtEDM"), nameFrame);
    QFont nameFont = titleFont;
    nameFont.setPixelSize(nameFont.pixelSize() + 4);
    nameLabel->setFont(nameFont);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLayout->addStretch(1);
    nameLayout->addWidget(nameLabel, 0, Qt::AlignCenter);
    nameLayout->addStretch(1);

    layout->addWidget(nameFrame, 0, Qt::AlignTop);

    auto *infoLayout = new QVBoxLayout;
    infoLayout->setSpacing(8);

    auto *descriptionLabel = new QLabel(
        QStringLiteral("Qt-Based Editor & Display Manager"), dialog);
    descriptionLabel->setFont(titleFont);
    descriptionLabel->setAlignment(Qt::AlignLeft);
    infoLayout->addWidget(descriptionLabel);

    auto *versionLabel = new QLabel(
        QStringLiteral("QtEDM Version 1.0.0  (EPICS 7.0.9.1-DEV)"), dialog);
    versionLabel->setFont(titleFont);
    versionLabel->setAlignment(Qt::AlignLeft);
    infoLayout->addWidget(versionLabel);

    auto *developedLabel = new QLabel(
        QStringLiteral(
            "Developed at Argonne National Laboratory\n"
            "by Robert Soliday"),
        dialog);
    developedLabel->setFont(bodyFont);
    developedLabel->setAlignment(Qt::AlignLeft);
    developedLabel->setWordWrap(false);
    infoLayout->addWidget(developedLabel);

    infoLayout->addStretch(1);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    auto *okButton = new QPushButton(QStringLiteral("OK"), dialog);
    okButton->setFont(titleFont);
    okButton->setAutoDefault(false);
    okButton->setDefault(false);
    buttonLayout->addWidget(okButton);
    infoLayout->addLayout(buttonLayout);

    layout->addLayout(infoLayout);

    QObject::connect(okButton, &QPushButton::clicked, dialog,
        &QDialog::accept);

    dialog->adjustSize();
    dialog->setFixedSize(dialog->sizeHint());
  }

  dialog->setPalette(palette);
  dialog->adjustSize();
  dialog->setFixedSize(dialog->sizeHint());
  centerDialog(dialog);

  if (autoClose) {
    QTimer::singleShot(5000, dialog, &QDialog::accept);
  }

  dialog->show();
  dialog->raise();
  dialog->activateWindow();
}
