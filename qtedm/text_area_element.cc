#include "text_area_element.h"

#include <algorithm>
#include <cmath>

#include <QAction>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QFontDatabase>
#include <QFrame>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QMimeData>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextOption>
#include <QTimer>

#include <memory>

#include "cursor_utils.h"
#include "pv_name_utils.h"
#include "window_utils.h"

namespace {

QColor alarmColorForSeverity(short severity)
{
  switch (severity) {
  case 0:
    return QColor(0, 205, 0);
  case 1:
    return QColor(255, 255, 0);
  case 2:
    return QColor(255, 0, 0);
  case 3:
    return QColor(255, 255, 255);
  default:
    return QColor(204, 204, 204);
  }
}

QString singleLineText(QString text)
{
  text.replace(QStringLiteral("\r\n"), QStringLiteral(" "));
  text.replace(QLatin1Char('\r'), QLatin1Char(' '));
  text.replace(QLatin1Char('\n'), QLatin1Char(' '));
  return text;
}

class TextAreaEditor : public QTextEdit
{
public:
  explicit TextAreaEditor(QWidget *parent = nullptr)
    : QTextEdit(parent)
  {
  }

  void setSelectionVisible(bool selected)
  {
    if (selected_ == selected) {
      return;
    }
    selected_ = selected;
    viewport()->update();
    update();
  }

  void setDisconnectedVisible(bool disconnected)
  {
    if (disconnected_ == disconnected) {
      return;
    }
    disconnected_ = disconnected;
    viewport()->update();
    update();
  }

  void setDirtyVisible(bool dirty)
  {
    if (dirty_ == dirty) {
      return;
    }
    dirty_ = dirty;
    viewport()->update();
    update();
  }

  void setFlashErrorVisible(bool flashError)
  {
    if (flashError_ == flashError) {
      return;
    }
    flashError_ = flashError;
    viewport()->update();
    update();
  }

  void setContextMenuCallback(std::function<void(QMenu *)> callback)
  {
    contextMenuCallback_ = std::move(callback);
  }

  void setSingleLineMode(bool singleLine)
  {
    singleLine_ = singleLine;
  }

protected:
  void paintEvent(QPaintEvent *event) override
  {
    QTextEdit::paintEvent(event);

    QPainter painter(viewport());
    const QRect viewportRect = viewport()->rect();
    if (disconnected_) {
      painter.fillRect(viewportRect, QColor(255, 255, 255, 180));
      painter.fillRect(viewportRect,
          QBrush(QColor(120, 120, 120, 140), Qt::BDiagPattern));
    }

    if (dirty_) {
      painter.setRenderHint(QPainter::Antialiasing, true);
      const int marker = std::min(12, std::max(8, viewportRect.width() / 10));
      QPolygon triangle;
      triangle << QPoint(viewportRect.right() - marker, viewportRect.top())
               << QPoint(viewportRect.right(), viewportRect.top())
               << QPoint(viewportRect.right(), viewportRect.top() + marker);
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(216, 120, 0));
      painter.drawPolygon(triangle);
      painter.setRenderHint(QPainter::Antialiasing, false);
    }

    if (flashError_) {
      QPen errorPen(QColor(200, 30, 30));
      errorPen.setWidth(2);
      painter.setPen(errorPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(viewportRect.adjusted(1, 1, -2, -2));
    }

    if (selected_) {
      QPen selectionPen(Qt::black);
      selectionPen.setStyle(Qt::DashLine);
      selectionPen.setWidth(1);
      painter.setPen(selectionPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(viewportRect.adjusted(0, 0, -1, -1));
    }
  }

  void contextMenuEvent(QContextMenuEvent *event) override
  {
    std::unique_ptr<QMenu> menu(createStandardContextMenu());
    if (contextMenuCallback_) {
      contextMenuCallback_(menu.get());
    }
    if (menu) {
      menu->exec(event->globalPos());
    }
  }

  void insertFromMimeData(const QMimeData *source) override
  {
    if (singleLine_ && source && source->hasText()) {
      insertPlainText(singleLineText(source->text()));
      return;
    }
    QTextEdit::insertFromMimeData(source);
  }

private:
  bool selected_ = false;
  bool disconnected_ = false;
  bool dirty_ = false;
  bool flashError_ = false;
  bool singleLine_ = false;
  std::function<void(QMenu *)> contextMenuCallback_;
};

QFont baseTextAreaFont(const QString &family)
{
  QFont font = family.trimmed().isEmpty()
      ? QFontDatabase::systemFont(QFontDatabase::FixedFont)
      : QFont(family);
  font.setStyleHint(QFont::TypeWriter, QFont::PreferDefault);
  font.setFixedPitch(true);
  return font;
}

} // namespace

TextAreaElement::TextAreaElement(QWidget *parent)
  : QWidget(parent)
  , textEdit_(new TextAreaEditor(this))
  , flashTimer_(new QTimer(this))
{
  setAutoFillBackground(false);

  auto *editor = static_cast<TextAreaEditor *>(textEdit_);
  editor->setAcceptRichText(false);
  editor->setPlaceholderText(QStringLiteral("Text Area"));
  editor->setUndoRedoEnabled(true);
  editor->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  editor->setAutoFillBackground(true);
  editor->setContextMenuPolicy(Qt::DefaultContextMenu);
  editor->installEventFilter(this);
  editor->viewport()->installEventFilter(this);
  editor->setContextMenuCallback([this](QMenu *menu) {
    handleContextMenuRequested(menu);
  });

  flashTimer_->setSingleShot(true);
  connect(flashTimer_, &QTimer::timeout, this, [this]() {
    flashCommitErrorActive_ = false;
    updateOverlayState();
  });

  foregroundColor_ = defaultForegroundColor();
  backgroundColor_ = defaultBackgroundColor();

  connect(textEdit_, &QTextEdit::textChanged, this, [this]() {
    if (!executeMode_) {
      return;
    }
    dirty_ = currentCommitBytes() != comparableRuntimeBytes(lastReceivedBytes_);
    if (dirty_) {
      userTouchedSinceCommit_ = true;
    }
    updateOverlayState();
  });

  applyPaletteColors();
  updateSelectionVisual();
  updateWrapSettings();
  updateScrollBarSettings();
  updateTabSettings();
  updateTextEditState();
  setToolTip(QString());
}

void TextAreaElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  updateSelectionVisual();
  update();
}

bool TextAreaElement::isSelected() const
{
  return selected_;
}

QColor TextAreaElement::foregroundColor() const
{
  return foregroundColor_;
}

void TextAreaElement::setForegroundColor(const QColor &color)
{
  const QColor effective = color.isValid() ? color : defaultForegroundColor();
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  applyPaletteColors();
  update();
}

QColor TextAreaElement::backgroundColor() const
{
  return backgroundColor_;
}

void TextAreaElement::setBackgroundColor(const QColor &color)
{
  const QColor effective = color.isValid() ? color : defaultBackgroundColor();
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  applyPaletteColors();
  update();
}

TextColorMode TextAreaElement::colorMode() const
{
  return colorMode_;
}

void TextAreaElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
  applyPaletteColors();
}

TextMonitorFormat TextAreaElement::format() const
{
  return format_;
}

void TextAreaElement::setFormat(TextMonitorFormat format)
{
  format_ = format;
}

int TextAreaElement::precision() const
{
  if (limits_.precisionSource != PvLimitSource::kChannel) {
    return limits_.precisionDefault;
  }
  return -1;
}

void TextAreaElement::setPrecision(int precision)
{
  if (precision < 0) {
    if (limits_.precisionSource != PvLimitSource::kChannel) {
      limits_.precisionSource = PvLimitSource::kChannel;
    }
    return;
  }

  limits_.precisionDefault = std::clamp(precision, 0, 17);
  limits_.precisionSource = PvLimitSource::kDefault;
}

PvLimitSource TextAreaElement::precisionSource() const
{
  return limits_.precisionSource;
}

void TextAreaElement::setPrecisionSource(PvLimitSource source)
{
  limits_.precisionSource = source;
}

int TextAreaElement::precisionDefault() const
{
  return limits_.precisionDefault;
}

void TextAreaElement::setPrecisionDefault(int precision)
{
  limits_.precisionDefault = std::clamp(precision, 0, 17);
}

const PvLimits &TextAreaElement::limits() const
{
  return limits_;
}

void TextAreaElement::setLimits(const PvLimits &limits)
{
  limits_ = limits;
  limits_.precisionDefault = std::clamp(limits_.precisionDefault, 0, 17);
}

double TextAreaElement::displayLowLimit() const
{
  if (executeMode_ && limits_.lowSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeLow_;
  }
  return limits_.lowDefault;
}

double TextAreaElement::displayHighLimit() const
{
  if (executeMode_ && limits_.highSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeHigh_;
  }
  return limits_.highDefault;
}

bool TextAreaElement::hasExplicitLimitsBlock() const
{
  return hasExplicitLimitsBlock_;
}

void TextAreaElement::setHasExplicitLimitsBlock(bool hasBlock)
{
  hasExplicitLimitsBlock_ = hasBlock;
}

bool TextAreaElement::hasExplicitLimitsData() const
{
  return hasExplicitLimitsData_;
}

void TextAreaElement::setHasExplicitLimitsData(bool hasData)
{
  hasExplicitLimitsData_ = hasData;
}

bool TextAreaElement::hasExplicitLowLimitData() const
{
  return hasExplicitLowLimitData_;
}

void TextAreaElement::setHasExplicitLowLimitData(bool hasData)
{
  hasExplicitLowLimitData_ = hasData;
}

bool TextAreaElement::hasExplicitHighLimitData() const
{
  return hasExplicitHighLimitData_;
}

void TextAreaElement::setHasExplicitHighLimitData(bool hasData)
{
  hasExplicitHighLimitData_ = hasData;
}

bool TextAreaElement::hasExplicitPrecisionData() const
{
  return hasExplicitPrecisionData_;
}

void TextAreaElement::setHasExplicitPrecisionData(bool hasData)
{
  hasExplicitPrecisionData_ = hasData;
}

QString TextAreaElement::channel() const
{
  return channel_;
}

void TextAreaElement::setChannel(const QString &value)
{
  const QString normalized = PvNameUtils::normalizePvName(value);
  if (channel_ == normalized) {
    return;
  }
  channel_ = normalized;
  if (!executeMode_ && textEdit_) {
    applyDisplayTextToEditor(false);
  }
}

bool TextAreaElement::isReadOnly() const
{
  return readOnly_;
}

void TextAreaElement::setReadOnly(bool readOnly)
{
  if (readOnly_ == readOnly) {
    return;
  }
  readOnly_ = readOnly;
  updateTextEditState();
}

bool TextAreaElement::wordWrap() const
{
  return wordWrap_;
}

void TextAreaElement::setWordWrap(bool wordWrap)
{
  if (wordWrap_ == wordWrap) {
    return;
  }
  wordWrap_ = wordWrap;
  updateWrapSettings();
  updateFontForGeometry();
}

TextAreaWrapMode TextAreaElement::lineWrapMode() const
{
  return lineWrapMode_;
}

void TextAreaElement::setLineWrapMode(TextAreaWrapMode mode)
{
  if (lineWrapMode_ == mode) {
    return;
  }
  lineWrapMode_ = mode;
  updateWrapSettings();
  updateFontForGeometry();
}

int TextAreaElement::wrapColumnWidth() const
{
  return wrapColumnWidth_;
}

void TextAreaElement::setWrapColumnWidth(int width)
{
  const int clamped = std::max(8, width);
  if (wrapColumnWidth_ == clamped) {
    return;
  }
  wrapColumnWidth_ = clamped;
  updateWrapSettings();
  updateFontForGeometry();
}

bool TextAreaElement::showVerticalScrollBar() const
{
  return showVerticalScrollBar_;
}

void TextAreaElement::setShowVerticalScrollBar(bool visible)
{
  if (showVerticalScrollBar_ == visible) {
    return;
  }
  showVerticalScrollBar_ = visible;
  updateScrollBarSettings();
}

bool TextAreaElement::showHorizontalScrollBar() const
{
  return showHorizontalScrollBar_;
}

void TextAreaElement::setShowHorizontalScrollBar(bool visible)
{
  if (showHorizontalScrollBar_ == visible) {
    return;
  }
  showHorizontalScrollBar_ = visible;
  updateScrollBarSettings();
}

TextAreaCommitMode TextAreaElement::commitMode() const
{
  return commitMode_;
}

void TextAreaElement::setCommitMode(TextAreaCommitMode mode)
{
  commitMode_ = mode;
}

bool TextAreaElement::tabInsertsSpaces() const
{
  return tabInsertsSpaces_;
}

void TextAreaElement::setTabInsertsSpaces(bool enabled)
{
  if (tabInsertsSpaces_ == enabled) {
    return;
  }
  tabInsertsSpaces_ = enabled;
  updateTabSettings();
}

int TextAreaElement::tabWidth() const
{
  return tabWidth_;
}

void TextAreaElement::setTabWidth(int width)
{
  const int clamped = std::clamp(width, 1, 32);
  if (tabWidth_ == clamped) {
    return;
  }
  tabWidth_ = clamped;
  updateTabSettings();
  updateFontForGeometry();
}

QString TextAreaElement::fontFamily() const
{
  return fontFamily_;
}

void TextAreaElement::setFontFamily(const QString &family)
{
  const QString trimmed = family.trimmed();
  if (fontFamily_ == trimmed) {
    return;
  }
  fontFamily_ = trimmed;
  updateTabSettings();
  updateFontForGeometry();
}

void TextAreaElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  if (executeMode_) {
    runtimeConnected_ = false;
    runtimeWriteAccess_ = false;
    runtimeSeverity_ = 0;
    lastReceivedBytes_.clear();
    runtimeSingleLine_ = false;
    dirty_ = false;
    hasPendingRuntimeText_ = false;
    userTouchedSinceCommit_ = false;
    runtimeLimitsValid_ = false;
    runtimePrecision_ = -1;
    runtimeByteLimit_ = -1;
    runtimeNotice_.clear();
    lastCommitError_.clear();
    applyDisplayTextToEditor(false);
  } else {
    runtimeConnected_ = false;
    runtimeWriteAccess_ = false;
    runtimeSeverity_ = 0;
    lastReceivedBytes_.clear();
    runtimeSingleLine_ = false;
    dirty_ = false;
    hasPendingRuntimeText_ = false;
    userTouchedSinceCommit_ = false;
    runtimeLimitsValid_ = false;
    runtimePrecision_ = -1;
    runtimeByteLimit_ = -1;
    runtimeNotice_.clear();
    lastCommitError_.clear();
    applyDisplayTextToEditor(false);
  }
  updateWrapSettings();
  updateScrollBarSettings();
  updateTextEditState();
  applyPaletteColors();
  updateFontForGeometry();
  updateOverlayState();
  updateToolTip();
}

bool TextAreaElement::isExecuteMode() const
{
  return executeMode_;
}

void TextAreaElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (!runtimeConnected_) {
    runtimeWriteAccess_ = false;
    runtimeSeverity_ = 0;
  }
  updateTextEditState();
  if (executeMode_) {
    applyPaletteColors();
    updateOverlayState();
    update();
  }
}

void TextAreaElement::setRuntimeWriteAccess(bool writeAccess)
{
  if (runtimeWriteAccess_ == writeAccess) {
    return;
  }
  runtimeWriteAccess_ = writeAccess;
  updateTextEditState();
}

void TextAreaElement::setRuntimeSeverity(short severity)
{
  const short clamped = std::clamp<short>(severity, 0, 3);
  if (runtimeSeverity_ == clamped) {
    return;
  }
  runtimeSeverity_ = clamped;
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    applyPaletteColors();
    update();
  }
}

void TextAreaElement::setRuntimeText(const QString &text)
{
  setRuntimeTextBytes(text.toUtf8());
}

void TextAreaElement::setRuntimeTextBytes(const QByteArray &bytes)
{
  lastReceivedBytes_ = bytes;
  if (!executeMode_ || !textEdit_) {
    return;
  }
  if (dirty_) {
    hasPendingRuntimeText_ = true;
    updateOverlayState();
    return;
  }
  hasPendingRuntimeText_ = false;
  applyDisplayTextToEditor(userTouchedSinceCommit_);
}

void TextAreaElement::setRuntimeSingleLine(bool singleLine)
{
  if (runtimeSingleLine_ == singleLine) {
    return;
  }
  runtimeSingleLine_ = singleLine;
  if (auto *editor = static_cast<TextAreaEditor *>(textEdit_)) {
    editor->setSingleLineMode(runtimeSingleLine_);
  }
  updateWrapSettings();
  updateScrollBarSettings();
  if (executeMode_ && !dirty_) {
    applyDisplayTextToEditor(userTouchedSinceCommit_);
  }
  updateFontForGeometry();
}

void TextAreaElement::setRuntimeLimits(double low, double high)
{
  if (!std::isfinite(low) || !std::isfinite(high)) {
    return;
  }
  if (std::abs(high - low) < 1e-12) {
    high = low + 1.0;
  }
  runtimeLow_ = low;
  runtimeHigh_ = high;
  runtimeLimitsValid_ = true;
}

void TextAreaElement::setRuntimePrecision(int precision)
{
  const int clamped = std::clamp(precision, 0, 17);
  if (runtimePrecision_ == clamped) {
    return;
  }
  runtimePrecision_ = clamped;
}

void TextAreaElement::setRuntimeByteLimit(int maxBytes)
{
  runtimeByteLimit_ = maxBytes >= 0 ? maxBytes : -1;
}

void TextAreaElement::setRuntimeNotice(const QString &notice)
{
  if (runtimeNotice_ == notice) {
    return;
  }
  runtimeNotice_ = notice;
  updateToolTip();
}

void TextAreaElement::clearRuntimeState()
{
  runtimeConnected_ = false;
  runtimeWriteAccess_ = false;
  runtimeSeverity_ = 0;
  lastReceivedBytes_.clear();
  runtimeSingleLine_ = false;
  dirty_ = false;
  hasPendingRuntimeText_ = false;
  userTouchedSinceCommit_ = false;
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
  runtimeLimitsValid_ = false;
  runtimePrecision_ = -1;
  runtimeByteLimit_ = -1;
  runtimeNotice_.clear();
  lastCommitError_.clear();
  updateWrapSettings();
  updateScrollBarSettings();
  applyDisplayTextToEditor(false);
  updateTextEditState();
  applyPaletteColors();
  updateOverlayState();
  updateToolTip();
}

void TextAreaElement::setActivationCallback(
    const std::function<void(const QByteArray &)> &callback)
{
  activationCallback_ = callback;
  updateTextEditState();
}

void TextAreaElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  if (textEdit_) {
    textEdit_->setGeometry(rect());
  }
  updateFontForGeometry();
}

void TextAreaElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);
}

bool TextAreaElement::eventFilter(QObject *watched, QEvent *event)
{
  const bool editorObject = watched == textEdit_;
  const bool viewportObject = textEdit_ && watched == textEdit_->viewport();
  if (!(editorObject || viewportObject) || !event) {
    return QObject::eventFilter(watched, event);
  }

  auto forwardMouseEvent = [&](QMouseEvent *mouseEvent) {
    if (!mouseEvent) {
      return false;
    }
    QWidget *target = window();
    if (!target) {
      return false;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF globalPosF = mouseEvent->globalPosition();
    const QPoint globalPoint = globalPosF.toPoint();
    const QPointF localPos = target->mapFromGlobal(globalPoint);
    QMouseEvent forwarded(mouseEvent->type(), localPos, localPos, globalPosF,
        mouseEvent->button(), mouseEvent->buttons(), mouseEvent->modifiers());
#else
    const QPoint globalPoint = mouseEvent->globalPos();
    const QPointF localPos = target->mapFromGlobal(globalPoint);
    QMouseEvent forwarded(mouseEvent->type(), localPos, localPos,
        QPointF(globalPoint), mouseEvent->button(), mouseEvent->buttons(),
        mouseEvent->modifiers());
#endif
    QCoreApplication::sendEvent(target, &forwarded);
    return true;
  };

  switch (event->type()) {
  case QEvent::KeyPress: {
    if (!editorObject || !executeMode_) {
      break;
    }
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    const int key = keyEvent->key();
    const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
    const bool shift = modifiers.testFlag(Qt::ShiftModifier);
    const bool control = modifiers.testFlag(Qt::ControlModifier);

    if (key == Qt::Key_Escape && dirty_) {
      revertToLastReceivedText();
      return true;
    }

    const bool isReturnKey = key == Qt::Key_Return || key == Qt::Key_Enter;
    if (isReturnKey && isEditableRuntime()) {
      if (commitMode_ == TextAreaCommitMode::kCtrlEnter && control) {
        commitEditorText();
        return true;
      }
      if (commitMode_ == TextAreaCommitMode::kEnter && !shift && !control) {
        commitEditorText();
        return true;
      }
      if (runtimeSingleLine_) {
        return true;
      }
    }

    if (key == Qt::Key_Tab && tabInsertsSpaces_ && isEditableRuntime()) {
      textEdit_->insertPlainText(QString(tabWidth_, QLatin1Char(' ')));
      return true;
    }
    break;
  }
  case QEvent::FocusOut:
    if (editorObject && executeMode_
        && commitMode_ == TextAreaCommitMode::kOnFocusLost
        && dirty_ && isEditableRuntime()) {
      commitEditorText();
    }
    break;
  case QEvent::MouseButtonPress:
  case QEvent::MouseButtonRelease: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::MiddleButton) {
      if (forwardMouseEvent(mouseEvent)) {
        return true;
      }
    }
    if (mouseEvent->button() == Qt::LeftButton
        && isParentWindowInPvInfoMode(this)) {
      if (forwardMouseEvent(mouseEvent)) {
        return true;
      }
    }
    break;
  }
  case QEvent::MouseMove: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->buttons().testFlag(Qt::MiddleButton)) {
      if (forwardMouseEvent(mouseEvent)) {
        return true;
      }
    }
    break;
  }
  default:
    break;
  }

  return QObject::eventFilter(watched, event);
}

void TextAreaElement::applyPaletteColors()
{
  if (!textEdit_) {
    return;
  }

  QPalette pal = textEdit_->palette();
  const QColor fg = effectiveForegroundColor();
  const QColor bg = effectiveBackgroundColor();
  pal.setColor(QPalette::Text, fg);
  pal.setColor(QPalette::WindowText, fg);
  pal.setColor(QPalette::ButtonText, fg);
  pal.setColor(QPalette::Base, bg);
  pal.setColor(QPalette::Window, bg);
  textEdit_->setPalette(pal);

  const QString fgName = fg.name(QColor::HexRgb);
  const QString bgName = bg.name(QColor::HexRgb);
  const QString topColor = bg.darker(145).name(QColor::HexRgb);
  const QString bottomColor = bg.lighter(135).name(QColor::HexRgb);
  const QString styleSheet = QStringLiteral(
      "QTextEdit { background-color: %1; color: %2; "
      "border-width: 2px; border-style: solid; "
      "border-top-color: %3; border-left-color: %3; "
      "border-bottom-color: %4; border-right-color: %4; }")
      .arg(bgName, fgName, topColor, bottomColor);
  textEdit_->setStyleSheet(styleSheet);
  textEdit_->viewport()->update();
}

void TextAreaElement::updateSelectionVisual()
{
  updateOverlayState();
}

void TextAreaElement::updateFontForGeometry()
{
  if (!textEdit_) {
    return;
  }

  const QSize available = textEdit_->viewport()->size();
  if (available.width() <= 0 || available.height() <= 0) {
    return;
  }

  const QString sample = executeMode_
      ? decodedBytes(lastReceivedBytes_)
      : designText();
  QStringList lines = sample.split(QLatin1Char('\n'));
  if (lines.isEmpty()) {
    lines.append(QStringLiteral("Text Area"));
  }

  int longest = 0;
  for (const QString &line : lines) {
    longest = std::max(longest, line.size());
  }
  if (longest <= 0) {
    longest = std::max(8, wrapColumnWidth_);
  }

  const bool wrapToWidth = wordWrap_
      && lineWrapMode_ != TextAreaWrapMode::kNoWrap
      && !runtimeSingleLine_;
  const int visibleLines = runtimeSingleLine_ ? 1 : 3;
  QFont chosen = baseTextAreaFont(fontFamily_);
  bool found = false;

  for (int pointSize = 24; pointSize >= 6; --pointSize) {
    QFont candidate = baseTextAreaFont(fontFamily_);
    candidate.setPointSize(pointSize);
    const QFontMetrics metrics(candidate);
    const int requiredHeight = metrics.lineSpacing() * visibleLines + 8;
    if (requiredHeight > available.height()) {
      continue;
    }
    if (!wrapToWidth) {
      const int maxWidth = metrics.horizontalAdvance(
          QString(std::max(1, longest), QLatin1Char('M'))) + 12;
      if (maxWidth > available.width()) {
        continue;
      }
    }
    chosen = candidate;
    found = true;
    break;
  }

  if (!found) {
    chosen = baseTextAreaFont(fontFamily_);
    chosen.setPointSize(6);
  }

  if (textEdit_->font() != chosen) {
    textEdit_->setFont(chosen);
    updateTabSettings();
  }
}

void TextAreaElement::updateTextEditState()
{
  if (!textEdit_) {
    return;
  }

  const bool editable = isEditableRuntime();
  const bool readable = executeMode_ && runtimeConnected_;

  textEdit_->setReadOnly(!editable);
  textEdit_->setFocusPolicy(editable ? Qt::StrongFocus : Qt::NoFocus);
  textEdit_->setTextInteractionFlags(editable
      ? Qt::TextEditorInteraction
      : (readable
            ? Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard
            : Qt::NoTextInteraction));

  if (executeMode_ && runtimeConnected_ && !readOnly_ && !runtimeWriteAccess_) {
    textEdit_->setCursor(CursorUtils::forbiddenCursor());
    setCursor(CursorUtils::forbiddenCursor());
  } else if (editable) {
    textEdit_->viewport()->setCursor(Qt::IBeamCursor);
    unsetCursor();
  } else {
    textEdit_->unsetCursor();
    unsetCursor();
  }

  updateOverlayState();
}

void TextAreaElement::updateWrapSettings()
{
  if (!textEdit_) {
    return;
  }

  QTextOption option = textEdit_->document()->defaultTextOption();
  if (runtimeSingleLine_
      || !wordWrap_ || lineWrapMode_ == TextAreaWrapMode::kNoWrap) {
    textEdit_->setLineWrapMode(QTextEdit::NoWrap);
    option.setWrapMode(QTextOption::NoWrap);
  } else if (lineWrapMode_ == TextAreaWrapMode::kFixedColumnWidth) {
    textEdit_->setLineWrapMode(QTextEdit::FixedColumnWidth);
    textEdit_->setLineWrapColumnOrWidth(wrapColumnWidth_);
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  } else {
    textEdit_->setLineWrapMode(QTextEdit::WidgetWidth);
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  }
  textEdit_->document()->setDefaultTextOption(option);
}

void TextAreaElement::updateScrollBarSettings()
{
  if (!textEdit_) {
    return;
  }
  Qt::ScrollBarPolicy verticalPolicy = showVerticalScrollBar_
      ? Qt::ScrollBarAsNeeded
      : Qt::ScrollBarAlwaysOff;
  if (runtimeSingleLine_) {
    verticalPolicy = Qt::ScrollBarAlwaysOff;
  }
  textEdit_->setVerticalScrollBarPolicy(verticalPolicy);
  textEdit_->setHorizontalScrollBarPolicy(showHorizontalScrollBar_
      ? Qt::ScrollBarAsNeeded
      : Qt::ScrollBarAlwaysOff);
}

void TextAreaElement::updateTabSettings()
{
  if (!textEdit_) {
    return;
  }
  QFont font = textEdit_->font();
  if (font.family().isEmpty()) {
    font = baseTextAreaFont(fontFamily_);
  }
  const QFontMetrics metrics(font);
  const qreal tabDistance = std::max(1, tabWidth_) *
      std::max(1, metrics.horizontalAdvance(QLatin1Char(' ')));
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
  textEdit_->setTabStopDistance(tabDistance);
#else
  textEdit_->setTabStopWidth(static_cast<int>(std::lround(tabDistance)));
#endif
}

void TextAreaElement::updateOverlayState()
{
  if (!textEdit_) {
    return;
  }
  auto *editor = static_cast<TextAreaEditor *>(textEdit_);
  editor->setSelectionVisible(selected_);
  editor->setDisconnectedVisible(executeMode_ && !runtimeConnected_
      && !channel_.trimmed().isEmpty());
  editor->setDirtyVisible(dirty_);
  editor->setFlashErrorVisible(flashCommitErrorActive_);
}

void TextAreaElement::updateToolTip()
{
  QString toolTip = runtimeNotice_;
  if (!lastCommitError_.isEmpty()) {
    if (!toolTip.isEmpty()) {
      toolTip.append(QLatin1Char('\n'));
    }
    toolTip.append(lastCommitError_);
  }
  setToolTip(toolTip);
  if (textEdit_) {
    textEdit_->setToolTip(toolTip);
    textEdit_->viewport()->setToolTip(toolTip);
  }
}

void TextAreaElement::applyDisplayTextToEditor(bool preserveCursor)
{
  if (!textEdit_) {
    return;
  }

  const QString targetText = executeMode_ ? editorTextForBytes(lastReceivedBytes_)
                                          : designText();
  const QSignalBlocker blocker(textEdit_);
  QTextCursor previousCursor = textEdit_->textCursor();
  textEdit_->setPlainText(targetText);

  if (preserveCursor) {
    QTextCursor cursor = textEdit_->textCursor();
    const int maxPosition = textEdit_->document()->characterCount() - 1;
    const int position = std::clamp(previousCursor.position(), 0, maxPosition);
    const int anchor = std::clamp(previousCursor.anchor(), 0, maxPosition);
    cursor.setPosition(anchor);
    cursor.setPosition(position, QTextCursor::KeepAnchor);
    textEdit_->setTextCursor(cursor);
  } else {
    QTextCursor cursor = textEdit_->textCursor();
    cursor.movePosition(QTextCursor::Start);
    textEdit_->setTextCursor(cursor);
  }
  updateFontForGeometry();
}

void TextAreaElement::commitEditorText()
{
  if (!isEditableRuntime() || !dirty_) {
    return;
  }

  const QByteArray bytes = currentCommitBytes();
  if (runtimeByteLimit_ >= 0 && bytes.size() > runtimeByteLimit_) {
    flashCommitError(QStringLiteral(
        "Text Area write rejected: %1 bytes exceeds limit of %2 bytes.")
        .arg(bytes.size()).arg(runtimeByteLimit_));
    return;
  }

  lastCommitError_.clear();
  if (activationCallback_) {
    activationCallback_(bytes);
  }
}

void TextAreaElement::revertToLastReceivedText()
{
  dirty_ = false;
  hasPendingRuntimeText_ = false;
  userTouchedSinceCommit_ = false;
  lastCommitError_.clear();
  applyDisplayTextToEditor(false);
  updateOverlayState();
  updateToolTip();
}

void TextAreaElement::handleContextMenuRequested(QMenu *menu)
{
  if (!menu) {
    return;
  }
  if (!executeMode_ || commitMode_ != TextAreaCommitMode::kExplicit) {
    return;
  }

  menu->addSeparator();
  QAction *commitAction = menu->addAction(QStringLiteral("Commit"));
  commitAction->setEnabled(dirty_ && isEditableRuntime());
  connect(commitAction, &QAction::triggered, this,
      [this]() { commitEditorText(); });

  QAction *revertAction = menu->addAction(QStringLiteral("Revert"));
  revertAction->setEnabled(dirty_ || hasPendingRuntimeText_);
  connect(revertAction, &QAction::triggered, this,
      [this]() { revertToLastReceivedText(); });
}

QByteArray TextAreaElement::currentCommitBytes() const
{
  if (!textEdit_) {
    return QByteArray();
  }
  QString text = textEdit_->toPlainText();
  if (runtimeSingleLine_) {
    text = singleLineText(text);
  }
  return text.toUtf8();
}

QByteArray TextAreaElement::comparableRuntimeBytes(const QByteArray &bytes) const
{
  return editorTextForBytes(bytes).toUtf8();
}

QString TextAreaElement::decodedBytes(const QByteArray &bytes) const
{
  if (bytes.isEmpty()) {
    return QString();
  }
  QByteArray payload = bytes;
  const int nullIndex = payload.indexOf('\0');
  if (nullIndex >= 0) {
    payload.truncate(nullIndex);
  }
  return QString::fromUtf8(payload.constData(), payload.size());
}

QString TextAreaElement::editorTextForBytes(const QByteArray &bytes) const
{
  QString text = decodedBytes(bytes);
  if (runtimeSingleLine_) {
    text = singleLineText(text);
  }
  return text;
}

QString TextAreaElement::designText() const
{
  return channel_.trimmed().isEmpty() ? QString() : channel_;
}

QColor TextAreaElement::defaultForegroundColor() const
{
  return palette().color(QPalette::WindowText);
}

QColor TextAreaElement::defaultBackgroundColor() const
{
  return palette().color(QPalette::Base);
}

QColor TextAreaElement::effectiveForegroundColor() const
{
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    return alarmColorForSeverity(runtimeSeverity_);
  }
  if (foregroundColor_.isValid()) {
    return foregroundColor_;
  }
  return defaultForegroundColor();
}

QColor TextAreaElement::effectiveBackgroundColor() const
{
  if (executeMode_ && !runtimeConnected_) {
    return QColor(Qt::white);
  }
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return defaultBackgroundColor();
}

bool TextAreaElement::isEditableRuntime() const
{
  return executeMode_ && runtimeConnected_ && runtimeWriteAccess_
      && !readOnly_ && activationCallback_;
}

void TextAreaElement::acceptRuntimeCommit(const QByteArray &bytes)
{
  lastReceivedBytes_ = bytes;
  dirty_ = false;
  hasPendingRuntimeText_ = false;
  userTouchedSinceCommit_ = false;
  flashCommitErrorActive_ = false;
  lastCommitError_.clear();

  if (textEdit_ && textEdit_->toPlainText() != editorTextForBytes(bytes)) {
    applyDisplayTextToEditor(true);
  }

  updateOverlayState();
  updateToolTip();
}

void TextAreaElement::rejectRuntimeCommit(const QString &message)
{
  flashCommitError(message);
}

void TextAreaElement::flashCommitError(const QString &message)
{
  flashCommitErrorActive_ = true;
  lastCommitError_ = message;
  updateOverlayState();
  updateToolTip();
  flashTimer_->start(1200);
}
