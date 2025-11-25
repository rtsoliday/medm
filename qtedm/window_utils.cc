#include "window_utils.h"

#include <QCursor>
#include <QDialog>
#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QTextBrowser>
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

    auto *nameLabel = new QLabel(nameFrame);
    QPixmap iconPixmap(QStringLiteral(":/icons/QtEDM.png"));
    if (!iconPixmap.isNull()) {
      nameLabel->setPixmap(iconPixmap);
      nameLabel->setScaledContents(false);
    } else {
      /* Fallback to text if icon not found */
      QFont nameFont = titleFont;
      nameFont.setPixelSize(nameFont.pixelSize() + 4);
      nameLabel->setFont(nameFont);
      nameLabel->setText(QStringLiteral("QtEDM"));
    }
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
            "by Robert Soliday. Based off of MEDM by\n"
            "Mark Anderson, Fred Vong & Ken Evans\n"),
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

void showHelpBrowser(QWidget *parent, const QString &title,
    const QString &htmlFilePath, const QFont &font, const QPalette &palette)
{
  /* Check if the dialog already exists */
  QString dialogName = QStringLiteral("qtedmHelpBrowser_") + title;
  dialogName.replace(QLatin1Char(' '), QLatin1Char('_'));
  
  QDialog *dialog = parent ? parent->findChild<QDialog *>(dialogName)
                           : nullptr;

  if (!dialog) {
    dialog = new QDialog(parent, Qt::Window);
    dialog->setObjectName(dialogName);
    dialog->setWindowTitle(title);
    dialog->setModal(false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAutoFillBackground(true);
    dialog->setBackgroundRole(QPalette::Window);
    dialog->setPalette(palette);

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *browser = new QTextBrowser(dialog);
    browser->setFont(font);
    browser->setOpenExternalLinks(true);
    browser->setReadOnly(true);
    browser->setMinimumSize(700, 500);

    /* Try to load the HTML file */
    QFile file(htmlFilePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QString html = QString::fromUtf8(file.readAll());
      file.close();
      browser->setHtml(html);
    } else {
      browser->setHtml(QStringLiteral(
          "<html><body><h1>Help Not Available</h1>"
          "<p>Could not open help file:</p>"
          "<p><code>%1</code></p></body></html>").arg(htmlFilePath));
    }

    layout->addWidget(browser);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    auto *closeButton = new QPushButton(QStringLiteral("Close"), dialog);
    closeButton->setFont(font);
    closeButton->setAutoDefault(false);
    closeButton->setDefault(false);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    QObject::connect(closeButton, &QPushButton::clicked, dialog,
        &QDialog::close);

    dialog->resize(750, 600);
  }

  centerDialog(dialog);
  dialog->show();
  dialog->raise();
  dialog->activateWindow();
}
