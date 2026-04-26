#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <QString>
#include <QVariant>
#include <QVector>

#include "display_properties.h"

struct WaveTableRuntimeState
{
  bool connected = false;
  short severity = 3;
  short nativeFieldType = -1;
  long nativeElementCount = 0;
  long receivedElementCount = 0;
  QString units;
  int precision = -1;
};

class WaveTableModel : public QAbstractTableModel
{
public:
  explicit WaveTableModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
      int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
      int role = Qt::DisplayRole) const override;

  void setLayout(WaveTableLayout layout);
  void setColumnCount(int count);
  void setIndexBase(int indexBase);
  void setColorMode(TextColorMode mode);
  void setForegroundColor(const QColor &color);
  void setBackgroundColor(const QColor &color);
  void setRuntimeState(const WaveTableRuntimeState &state);
  void setValues(const QVector<QString> &values);
  void clearValues();

  WaveTableLayout layout() const;
  int configuredColumnCount() const;
  int indexBase() const;
  int displayedElementCount() const;
  QString cellText(int row, int column) const;
  QVector<QString> values() const;

private:
  int displayRowCount() const;
  int displayColumnCount() const;
  int valueIndexForCell(int row, int column) const;
  QString statusText() const;
  QColor alarmColor(short severity) const;

  WaveTableLayout layout_ = WaveTableLayout::kGrid;
  int columnCount_ = 8;
  int indexBase_ = 0;
  TextColorMode colorMode_ = TextColorMode::kAlarm;
  QColor foregroundColor_;
  QColor backgroundColor_;
  WaveTableRuntimeState runtimeState_;
  QVector<QString> values_;
};
