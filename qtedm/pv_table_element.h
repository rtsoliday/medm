#pragma once

#include <array>

#include <QAbstractTableModel>
#include <QColor>
#include <QMouseEvent>
#include <QString>
#include <QTableView>
#include <QVector>

#include "display_properties.h"

struct PvTableRowConfig
{
  QString label;
  QString channel;
};

struct PvTableRuntimeRowState
{
  bool connected = false;
  short severity = 3;
  QString valueText;
  QString units;
};

class PvTableModel : public QAbstractTableModel
{
public:
  enum class Column
  {
    kLabel,
    kPv,
    kValue,
    kUnits,
    kSeverity,
  };

  explicit PvTableModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
      int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
      int role = Qt::DisplayRole) const override;

  void setRows(const QVector<PvTableRowConfig> &rows);
  void setRuntimeState(int row, const PvTableRuntimeRowState &state);
  void setColumns(const QVector<Column> &columns);
  void setColorMode(TextColorMode mode);
  void setForegroundColor(const QColor &color);
  void setBackgroundColor(const QColor &color);

private:
  QString severityText(short severity, bool connected) const;
  QColor alarmColor(short severity) const;
  int visualColumnIndex(Column column) const;

  QVector<PvTableRowConfig> rows_;
  QVector<PvTableRuntimeRowState> runtimeStates_;
  QVector<Column> columns_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  QColor foregroundColor_;
  QColor backgroundColor_;
};

class PvTableElement : public QTableView
{
  friend class DisplayWindow;

public:
  explicit PvTableElement(QWidget *parent = nullptr);
  ~PvTableElement() override;

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  bool showHeaders() const;
  void setShowHeaders(bool show);

  int fontSize() const;
  bool hasExplicitFontSize() const;
  void setFontSize(int pointSize);

  QVector<PvTableModel::Column> columns() const;
  QString columnsString() const;
  void setColumns(const QVector<PvTableModel::Column> &columns);
  void setColumnsString(const QString &columns);

  int rowCount() const;
  PvTableRowConfig row(int index) const;
  QVector<PvTableRowConfig> rows() const;
  void setRows(const QVector<PvTableRowConfig> &rows);
  void setRow(int index, const PvTableRowConfig &row);

  QString rowLabelText(int index) const;
  QString rowChannelText(int index) const;
  void setRowLabelText(int index, const QString &value);
  void setRowChannelText(int index, const QString &value);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setRowConnected(int row, bool connected);
  void setRowValue(int row, const QString &value);
  void setRowSeverity(int row, short severity);
  void setRowMetadata(int row, const QString &units);
  void clearRuntimeState();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  bool forwardMouseEventToParent(QMouseEvent *event) const;
  static constexpr int kMaxEditableRows = 8;

  void updateSelectionVisual();
  void applyModelColors();
  void updateHeaderVisibility();
  bool ensureRowIndex(int index);
  void updateRuntimeState(int row);
  void applyFontSize();
  QColor defaultForegroundColor() const;
  QColor defaultBackgroundColor() const;
  static QVector<PvTableModel::Column> parseColumns(const QString &columns);
  static QString columnToken(PvTableModel::Column column);
  static QStringList columnTokens(const QVector<PvTableModel::Column> &columns);

  PvTableModel *model_ = nullptr;
  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kAlarm;
  bool showHeaders_ = true;
  int fontSize_ = 0;
  bool executeMode_ = false;
  QVector<PvTableModel::Column> columns_;
  QVector<PvTableRowConfig> rows_;
  QVector<PvTableRuntimeRowState> runtimeStates_;
};
