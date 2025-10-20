#include "pv_info_dialog.h"

#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTextOption>
#include <QVBoxLayout>
#include <QFontDatabase>
#include <QPalette>

PvInfoDialog::PvInfoDialog(const QPalette &basePalette,
    const QFont &labelFont, const QFont &textFont, QWidget *parent)
  : QDialog(parent)
{
  setObjectName(QStringLiteral("qtedmPvInfoDialog"));
  setWindowTitle(QStringLiteral("PV Info"));
  setModal(false);
  setAutoFillBackground(true);
  setPalette(basePalette);
  setBackgroundRole(QPalette::Window);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setSizeGripEnabled(true);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(10);

  QFont bodyFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  if (bodyFont.family().isEmpty()) {
    bodyFont = textFont;
  }

  textEdit_ = new QPlainTextEdit;
  textEdit_->setReadOnly(true);
  textEdit_->setWordWrapMode(QTextOption::NoWrap);
  textEdit_->setFont(bodyFont);
  textEdit_->setAutoFillBackground(true);
  textEdit_->setPalette(basePalette);
  layout->addWidget(textEdit_);

  auto *buttonBox = new QDialogButtonBox;
  closeButton_ = buttonBox->addButton(QDialogButtonBox::Close);
  helpButton_ = buttonBox->addButton(QStringLiteral("Help"),
      QDialogButtonBox::HelpRole);
  closeButton_->setFont(labelFont);
  helpButton_->setFont(labelFont);
  layout->addWidget(buttonBox);

  connect(closeButton_, &QPushButton::clicked, this, &QDialog::hide);
  connect(helpButton_, &QPushButton::clicked, this, [this]() {
    QMessageBox::information(this, windowTitle(),
        QStringLiteral("Displays detailed information about the process "
                       "variables associated with the object under the cursor."));
  });

  resize(540, 420);
}

void PvInfoDialog::setContent(const QString &text)
{
  if (!textEdit_) {
    return;
  }
  textEdit_->setPlainText(text);
  textEdit_->moveCursor(QTextCursor::Start);
}
