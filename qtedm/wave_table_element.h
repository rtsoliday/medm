#pragma once

#include <QColor>
#include <QMouseEvent>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QVector>

#include "text_properties.h"
#include "wave_table_properties.h"
#include "wave_table_model.h"

class WaveTableElement : public QTableView
{
  friend class DisplayWindow;

public:
  explicit WaveTableElement(QWidget *parent = nullptr);
  ~WaveTableElement() override;

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

  QString channel() const;
  void setChannel(const QString &channel);

  WaveTableLayout layoutMode() const;
  void setLayoutMode(WaveTableLayout layout);
  QString layoutString() const;
  void setLayoutString(const QString &layout);

  int columnCountSetting() const;
  void setColumnCountSetting(int count);

  int maxElements() const;
  void setMaxElements(int count);

  int indexBase() const;
  void setIndexBase(int indexBase);

  WaveTableValueFormat valueFormat() const;
  void setValueFormat(WaveTableValueFormat format);
  QString valueFormatString() const;
  void setValueFormatString(const QString &format);

  WaveTableCharMode charMode() const;
  void setCharMode(WaveTableCharMode mode);
  QString charModeString() const;
  void setCharModeString(const QString &mode);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setConnected(bool connected);
  void setSeverity(short severity);
  void setMetadata(short nativeFieldType, long nativeElementCount,
      const QString &units, int precision);
  void setValues(const QVector<QString> &values, long receivedElementCount);
  void clearRuntimeState();

  bool runtimeConnected() const;
  short runtimeSeverity() const;
  short nativeFieldType() const;
  long nativeElementCount() const;
  long receivedElementCount() const;
  int displayedElementCount() const;
  int tableRowCount() const;
  int tableColumnCount() const;
  QVector<QString> displayedValues() const;
  QString cellText(int row, int column) const;
  QString runtimeStatusText() const;

  static WaveTableLayout layoutFromString(const QString &layout);
  static QString layoutToString(WaveTableLayout layout);
  static WaveTableValueFormat valueFormatFromString(const QString &format);
  static QString valueFormatToString(WaveTableValueFormat format);
  static WaveTableCharMode charModeFromString(const QString &mode);
  static QString charModeToString(WaveTableCharMode mode);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  bool forwardMouseEventToParent(QMouseEvent *event) const;
  void updateSelectionVisual();
  void applyModelColors();
  void updateHeaderVisibility();
  void updateModelConfiguration();
  void updateRuntimeStatus();
  void applyFontSize();
  QColor defaultForegroundColor() const;
  QColor defaultBackgroundColor() const;

  WaveTableModel *model_ = nullptr;
  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kAlarm;
  bool showHeaders_ = true;
  int fontSize_ = 0;
  QString channel_;
  WaveTableLayout layout_ = WaveTableLayout::kGrid;
  int columnCount_ = 8;
  int maxElements_ = 0;
  int indexBase_ = 0;
  WaveTableValueFormat valueFormat_ = WaveTableValueFormat::kDefault;
  WaveTableCharMode charMode_ = WaveTableCharMode::kString;
  bool executeMode_ = false;
  WaveTableRuntimeState runtimeState_;
};
