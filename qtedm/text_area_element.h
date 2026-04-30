#pragma once

#include <functional>

#include <QByteArray>
#include <QColor>
#include <QString>
#include <QWidget>

#include "monitor_properties.h"
#include "text_properties.h"

class QEvent;
class QPaintEvent;
class QResizeEvent;
class QTextEdit;
class QTimer;

class TextAreaElement : public QWidget
{
public:
  explicit TextAreaElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  TextMonitorFormat format() const;
  void setFormat(TextMonitorFormat format);

  int precision() const;
  void setPrecision(int precision);

  PvLimitSource precisionSource() const;
  void setPrecisionSource(PvLimitSource source);
  int precisionDefault() const;
  void setPrecisionDefault(int precision);

  const PvLimits &limits() const;
  void setLimits(const PvLimits &limits);
  double displayLowLimit() const;
  double displayHighLimit() const;
  bool hasExplicitLimitsBlock() const;
  void setHasExplicitLimitsBlock(bool hasBlock);
  bool hasExplicitLimitsData() const;
  void setHasExplicitLimitsData(bool hasData);
  bool hasExplicitLowLimitData() const;
  void setHasExplicitLowLimitData(bool hasData);
  bool hasExplicitHighLimitData() const;
  void setHasExplicitHighLimitData(bool hasData);
  bool hasExplicitPrecisionData() const;
  void setHasExplicitPrecisionData(bool hasData);

  QString channel() const;
  void setChannel(const QString &value);

  bool isReadOnly() const;
  void setReadOnly(bool readOnly);

  bool wordWrap() const;
  void setWordWrap(bool wordWrap);

  TextAreaWrapMode lineWrapMode() const;
  void setLineWrapMode(TextAreaWrapMode mode);

  int wrapColumnWidth() const;
  void setWrapColumnWidth(int width);

  bool showVerticalScrollBar() const;
  void setShowVerticalScrollBar(bool visible);

  bool showHorizontalScrollBar() const;
  void setShowHorizontalScrollBar(bool visible);

  TextAreaCommitMode commitMode() const;
  void setCommitMode(TextAreaCommitMode mode);

  bool tabInsertsSpaces() const;
  void setTabInsertsSpaces(bool enabled);

  int tabWidth() const;
  void setTabWidth(int width);

  QString fontFamily() const;
  void setFontFamily(const QString &family);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setRuntimeConnected(bool connected);
  void setRuntimeWriteAccess(bool writeAccess);
  void setRuntimeSeverity(short severity);
  void setRuntimeText(const QString &text);
  void setRuntimeTextBytes(const QByteArray &bytes);
  void setRuntimeSingleLine(bool singleLine);
  void setRuntimeLimits(double low, double high);
  void setRuntimePrecision(int precision);
  void setRuntimeByteLimit(int maxBytes);
  void setRuntimeNotice(const QString &notice);
  void acceptRuntimeCommit(const QByteArray &bytes);
  void rejectRuntimeCommit(const QString &message);
  void clearRuntimeState();

  void setActivationCallback(
      const std::function<void(const QByteArray &)> &callback);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void applyPaletteColors();
  void updateSelectionVisual();
  void updateFontForGeometry();
  void updateTextEditState();
  void updateWrapSettings();
  void updateScrollBarSettings();
  void updateTabSettings();
  void updateOverlayState();
  void updateToolTip();
  void applyDisplayTextToEditor(bool preserveCursor);
  void commitEditorText();
  void revertToLastReceivedText();
  void handleContextMenuRequested(class QMenu *menu);
  QByteArray comparableRuntimeBytes(const QByteArray &bytes) const;
  QByteArray currentCommitBytes() const;
  QString decodedBytes(const QByteArray &bytes) const;
  QString editorTextForBytes(const QByteArray &bytes) const;
  QString designText() const;
  QColor defaultForegroundColor() const;
  QColor defaultBackgroundColor() const;
  QColor effectiveForegroundColor() const;
  QColor effectiveBackgroundColor() const;
  bool isEditableRuntime() const;
  void flashCommitError(const QString &message);

  bool selected_ = false;
  QTextEdit *textEdit_ = nullptr;
  QTimer *flashTimer_ = nullptr;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextMonitorFormat format_ = TextMonitorFormat::kDecimal;
  PvLimits limits_{};
  bool hasExplicitLimitsBlock_ = false;
  bool hasExplicitLimitsData_ = false;
  bool hasExplicitLowLimitData_ = false;
  bool hasExplicitHighLimitData_ = false;
  bool hasExplicitPrecisionData_ = false;
  QString channel_;
  bool readOnly_ = false;
  bool wordWrap_ = true;
  TextAreaWrapMode lineWrapMode_ = TextAreaWrapMode::kWidgetWidth;
  int wrapColumnWidth_ = 80;
  bool showVerticalScrollBar_ = true;
  bool showHorizontalScrollBar_ = false;
  TextAreaCommitMode commitMode_ = TextAreaCommitMode::kCtrlEnter;
  bool tabInsertsSpaces_ = true;
  int tabWidth_ = 8;
  QString fontFamily_;
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  bool runtimeWriteAccess_ = false;
  short runtimeSeverity_ = 0;
  QByteArray lastReceivedBytes_;
  bool runtimeSingleLine_ = false;
  bool dirty_ = false;
  bool hasPendingRuntimeText_ = false;
  bool userTouchedSinceCommit_ = false;
  bool flashCommitErrorActive_ = false;
  QString lastCommitError_;
  double runtimeLow_ = 0.0;
  double runtimeHigh_ = 0.0;
  bool runtimeLimitsValid_ = false;
  int runtimePrecision_ = -1;
  int runtimeByteLimit_ = -1;
  QString runtimeNotice_;
  std::function<void(const QByteArray &)> activationCallback_;
};
