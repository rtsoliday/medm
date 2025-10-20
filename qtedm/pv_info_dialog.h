#pragma once

#include <QDialog>

class QFont;
class QPalette;

class QPlainTextEdit;
class QPushButton;

class PvInfoDialog : public QDialog
{
public:
  PvInfoDialog(const QPalette &basePalette, const QFont &labelFont,
      const QFont &textFont, QWidget *parent = nullptr);

  void setContent(const QString &text);

private:
  QPlainTextEdit *textEdit_ = nullptr;
  QPushButton *closeButton_ = nullptr;
  QPushButton *helpButton_ = nullptr;
};
