#pragma once

#include <QByteArray>
#include <QDialog>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>

class QFont;
class QPalette;

class QPlainTextEdit;
class QPushButton;

struct PvInfoArrayData
{
  QString channelName;
  QString typeName;
  QString units;
  bool hasUnits = false;
  int precision = -1;
  bool hasPrecision = false;
  QVector<double> numericValues;
  QByteArray byteValues;
  QStringList stringValues;
  QString textValue;
  bool isCharArray = false;
  bool isStringArray = false;

  int elementCount() const
  {
    if (isCharArray) {
      return byteValues.size();
    }
    if (isStringArray) {
      return stringValues.size();
    }
    return numericValues.size();
  }
};

class PvArrayValueDialog;

class PvInfoDialog : public QDialog
{
public:
  PvInfoDialog(const QPalette &basePalette, const QFont &labelFont,
      const QFont &textFont, QWidget *parent = nullptr);

  void setContent(const QString &text,
      const QVector<PvInfoArrayData> &arrays = {});

private:
  void showArrayDialog();

  QPlainTextEdit *textEdit_ = nullptr;
  QPushButton *arrayButton_ = nullptr;
  QPushButton *closeButton_ = nullptr;
  QPushButton *helpButton_ = nullptr;
  QVector<PvInfoArrayData> arrays_;
  QPointer<PvArrayValueDialog> arrayDialog_;
};
