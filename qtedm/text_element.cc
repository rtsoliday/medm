#include "text_element.h"

#include <algorithm>

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QHideEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QShowEvent>

#include "text_font_utils.h"

namespace {

constexpr int kTextMargin = 0;

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

int textPixelWidth(const QFontMetrics &metrics, const QString &text)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
  return metrics.horizontalAdvance(text);
#else
  return metrics.width(text);
#endif
}

} // namespace

class TextOverflowWidget : public QWidget
{
public:
  explicit TextOverflowWidget(TextElement *owner)
    : QWidget(owner ? owner->parentWidget() : nullptr)
    , owner_(owner)
  {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    hide();
  }

  void syncParent()
  {
    if (!owner_) {
      return;
    }
    QWidget *targetParent = owner_->parentWidget();
    if (parentWidget() != targetParent) {
      setParent(targetParent);
    }
  }

protected:
  void paintEvent(QPaintEvent *event) override
  {
    Q_UNUSED(event);
    if (!owner_ || !owner_->isVisible()) {
      return;
    }
    const QRect ownerRect = owner_->geometry();
    if (!ownerRect.isValid()) {
      return;
    }

    const QRect overlayRect = geometry();
    const int ownerLeft = ownerRect.x() - overlayRect.x();
    const int ownerTop = ownerRect.y() - overlayRect.y();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);

    const QString text = owner_->text();
    if (!text.isEmpty()) {
      const QFont font = owner_->font();
      painter.setFont(font);
      painter.setPen(owner_->effectiveForegroundColor());

      const QFontMetrics metrics(font);
      const int baseline = ownerTop + metrics.ascent();
      int textX = ownerLeft;
      const int textWidth = textPixelWidth(metrics, text);
      const Qt::Alignment alignment = owner_->textAlignment();
      switch (alignment & Qt::AlignHorizontal_Mask) {
      case Qt::AlignHCenter:
        textX = ownerLeft + (ownerRect.width() - textWidth) / 2;
        break;
      case Qt::AlignRight:
        textX = ownerLeft + ownerRect.width() - textWidth;
        break;
      default:
        break;
      }
      painter.drawText(textX, baseline, text);
    }

    if (owner_->isSelected()) {
      QPen pen(Qt::black);
      pen.setStyle(Qt::DashLine);
      pen.setWidth(1);
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);
      QRect border(ownerLeft, ownerTop, ownerRect.width(), ownerRect.height());
      border.adjust(0, 0, -1, -1);
      painter.drawRect(border);
    }
  }

private:
  TextElement *owner_ = nullptr;
};

TextElement::TextElement(QWidget *parent)
  : QLabel(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_NoSystemBackground);
  setWordWrap(false);
  setContentsMargins(kTextMargin, kTextMargin, kTextMargin, kTextMargin);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
  setForegroundColor(palette().color(QPalette::WindowText));
  setColorMode(TextColorMode::kStatic);
  setVisibilityMode(TextVisibilityMode::kStatic);
  updateSelectionVisual();
  designModeVisible_ = QLabel::isVisible();
  updateOverflowGeometry();
  updateOverflowVisibility();
  updateOverflowStacking();
}

TextElement::~TextElement()
{
  if (disconnectIndicationTimer_) {
    disconnectIndicationTimer_->stop();
    delete disconnectIndicationTimer_;
    disconnectIndicationTimer_ = nullptr;
  }
  if (overflowWidget_) {
    overflowWidget_->hide();
    overflowWidget_->setParent(nullptr);
    delete overflowWidget_;
    overflowWidget_ = nullptr;
  }
}

void TextElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  updateSelectionVisual();
  requestOverflowRepaint();
}

bool TextElement::isSelected() const
{
  return selected_;
}

QColor TextElement::foregroundColor() const
{
  return foregroundColor_;
}

void TextElement::setForegroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : defaultForegroundColor();
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  applyTextColor();
  requestOverflowRepaint();
}

void TextElement::setText(const QString &value)
{
  QLabel::setText(value);
  updateFontForGeometry();
}

QRect TextElement::boundingRect() const
{
  QRect bounds = QLabel::rect();
  bounds.adjust(-kTextMargin, -kTextMargin, kTextMargin, kTextMargin);
  return bounds;
}

QRect TextElement::visualBoundsRelativeToParent() const
{
  if (!parentWidget()) {
    return QRect();
  }

  const QRect ownerRect = geometry();
  if (!ownerRect.isValid()) {
    return ownerRect;
  }

  const QFontMetrics metrics(font());
  const QString currentText = text();
  const int textWidth = textPixelWidth(metrics, currentText);
  const int textHeight = metrics.ascent() + metrics.descent();

  int textLeft = ownerRect.x();
  switch (alignment_ & Qt::AlignHorizontal_Mask) {
  case Qt::AlignHCenter:
    textLeft = ownerRect.x() + (ownerRect.width() - textWidth) / 2;
    break;
  case Qt::AlignRight:
    textLeft = ownerRect.x() + ownerRect.width() - textWidth;
    break;
  default:
    break;
  }

  const int ownerLeft = ownerRect.x();
  const int ownerRight = ownerRect.x() + ownerRect.width();
  const int ownerTop = ownerRect.y();
  const int ownerBottom = ownerRect.y() + ownerRect.height();
  const int textRight = textLeft + textWidth;
  const int textTop = ownerTop;
  const int textBottom = textTop + textHeight;

  const int overlayLeft = std::min(ownerLeft, textLeft);
  const int overlayRight = std::max(ownerRight, textRight);
  const int overlayTop = ownerTop;
  const int overlayBottom = std::max(ownerBottom, textBottom);

  const int overlayWidth = std::max(1, overlayRight - overlayLeft);
  const int overlayHeight = std::max(1, overlayBottom - overlayTop);

  return QRect(overlayLeft, overlayTop, overlayWidth, overlayHeight);
}

Qt::Alignment TextElement::textAlignment() const
{
  return alignment_;
}

void TextElement::setTextAlignment(Qt::Alignment alignment)
{
  Qt::Alignment effective = alignment;
  if (!(effective & Qt::AlignHorizontal_Mask)) {
    effective |= Qt::AlignLeft;
  }
  effective &= ~Qt::AlignVertical_Mask;
  effective |= Qt::AlignTop;
  if (alignment_ == effective) {
    return;
  }
  alignment_ = effective;
  QLabel::setAlignment(alignment_);
  updateOverflowGeometry();
}

TextColorMode TextElement::colorMode() const
{
  return colorMode_;
}

void TextElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextVisibilityMode TextElement::visibilityMode() const
{
  return visibilityMode_;
}

void TextElement::setVisibilityMode(TextVisibilityMode mode)
{
  visibilityMode_ = mode;
}

QString TextElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void TextElement::setVisibilityCalc(const QString &calc)
{
  if (visibilityCalc_ == calc) {
    return;
  }
  visibilityCalc_ = calc;
}

QString TextElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[index];
}

void TextElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  if (channels_[index] == value) {
    return;
  }
  channels_[index] = value;
}

void TextElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }

  if (execute) {
    designModeVisible_ = QLabel::isVisible();
    // Start timer to allow disconnect indication after brief connecting period
    allowDisconnectIndication_ = false;
    if (!disconnectIndicationTimer_) {
      disconnectIndicationTimer_ = new QTimer(this);
      disconnectIndicationTimer_->setSingleShot(true);
      connect(disconnectIndicationTimer_, &QTimer::timeout, this, [this]() {
        allowDisconnectIndication_ = true;
        applyTextColor();  // Re-evaluate background color
      });
    }
    disconnectIndicationTimer_->start(150);  // 150ms delay
  } else {
    if (disconnectIndicationTimer_) {
      disconnectIndicationTimer_->stop();
    }
    allowDisconnectIndication_ = false;
  }

  executeMode_ = execute;
  runtimeConnected_ = false;
  runtimeEverConnected_ = false;
  runtimeVisible_ = true;
  runtimeSeverity_ = 0;
  updateExecuteState();
  updateOverflowGeometry();
  updateOverflowVisibility();
}

bool TextElement::isExecuteMode() const
{
  return executeMode_;
}

void TextElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (connected) {
    runtimeEverConnected_ = true;
  }
  if (executeMode_) {
    applyTextColor();
    applyTextVisibility();
    requestOverflowRepaint();
  }
}

void TextElement::setRuntimeVisible(bool visible)
{
  if (runtimeVisible_ == visible) {
    return;
  }
  runtimeVisible_ = visible;
  if (executeMode_) {
    applyTextVisibility();
    requestOverflowRepaint();
  }
}

void TextElement::setRuntimeSeverity(short severity)
{
  if (severity < 0) {
    severity = 0;
  }
  severity = std::min<short>(severity, 3);
  if (runtimeSeverity_ == severity) {
    return;
  }
  runtimeSeverity_ = severity;
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    applyTextColor();
    requestOverflowRepaint();
  }
}

void TextElement::setVisible(bool visible)
{
  if (!executeMode_) {
    designModeVisible_ = visible;
  }
  QLabel::setVisible(visible);
  updateOverflowVisibility();
  requestOverflowRepaint();
}

void TextElement::resizeEvent(QResizeEvent *event)
{
  QLabel::resizeEvent(event);
  updateFontForGeometry();
}

void TextElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);
}

bool TextElement::event(QEvent *event)
{
  switch (event->type()) {
  case QEvent::ParentAboutToChange:
    if (overflowWidget_) {
      overflowWidget_->hide();
      overflowWidget_->setParent(nullptr);
    }
    break;
  case QEvent::ParentChange:
    updateOverflowParent();
    updateOverflowGeometry();
    break;
  case QEvent::ZOrderChange:
    updateOverflowStacking();
    break;
  default:
    break;
  }
  return QLabel::event(event);
}

void TextElement::moveEvent(QMoveEvent *event)
{
  QLabel::moveEvent(event);
  updateOverflowGeometry();
}

void TextElement::showEvent(QShowEvent *event)
{
  QLabel::showEvent(event);
  updateOverflowVisibility();
  requestOverflowRepaint();
}

void TextElement::hideEvent(QHideEvent *event)
{
  QLabel::hideEvent(event);
  updateOverflowVisibility();
}

QColor TextElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

void TextElement::applyTextColor()
{
  const QColor color = effectiveForegroundColor();
  QPalette pal = palette();
  pal.setColor(QPalette::WindowText, color);
  pal.setColor(QPalette::Text, color);
  pal.setColor(QPalette::ButtonText, color);
  
  // Set background to white if in execute mode, has a channel defined, 
  // is disconnected, and past the initial connecting period
  if (executeMode_ && !runtimeConnected_ && allowDisconnectIndication_) {
    bool hasChannel = false;
    for (const QString &ch : channels_) {
      if (!ch.trimmed().isEmpty()) {
        hasChannel = true;
        break;
      }
    }
    if (hasChannel) {
      setAttribute(Qt::WA_NoSystemBackground, false);
      setAutoFillBackground(true);
      pal.setColor(QPalette::Window, Qt::white);
      pal.setColor(QPalette::Base, Qt::white);
    } else {
      setAttribute(Qt::WA_NoSystemBackground, true);
      setAutoFillBackground(false);
    }
  } else {
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
  }
  
  setPalette(pal);
  requestOverflowRepaint();
}

void TextElement::applyTextVisibility()
{
  if (executeMode_) {
    // Check if any channel is defined
    bool hasChannel = false;
    for (const QString &ch : channels_) {
      if (!ch.trimmed().isEmpty()) {
        hasChannel = true;
        break;
      }
    }
    // Show if visible and either connected OR (not connected but has a channel defined)
    const bool visible = designModeVisible_ && runtimeVisible_ && 
                        (runtimeConnected_ || hasChannel);
    QLabel::setVisible(visible);
  } else {
    QLabel::setVisible(designModeVisible_);
  }
  updateOverflowVisibility();
}

void TextElement::updateSelectionVisual()
{
  // Keep the configured foreground color even when selected; the overflow
  // widget handles the dashed selection border.
  applyTextColor();
  requestOverflowRepaint();
}

QColor TextElement::effectiveForegroundColor() const
{
  QColor baseColor = foregroundColor_.isValid() ? foregroundColor_ : defaultForegroundColor();
  if (!executeMode_) {
    return baseColor;
  }

  if (!runtimeConnected_) {
    return QColor(255, 255, 255);
  }

  switch (colorMode_) {
  case TextColorMode::kAlarm:
    return alarmColorForSeverity(runtimeSeverity_);
  case TextColorMode::kDiscrete:
  case TextColorMode::kStatic:
  default:
    return baseColor;
  }
}

void TextElement::updateExecuteState()
{
  applyTextColor();
  applyTextVisibility();
  updateOverflowGeometry();
  requestOverflowRepaint();
}

void TextElement::updateFontForGeometry()
{
  // Use full widget size to match MEDM behavior
  // MEDM uses dlText->object.height directly in medmText.c
  const QSize available(width(), height());
  if (!available.isEmpty()) {
    // Static Text widgets use full height like Text Monitor, not Text Entry constraint
    const QFont newFont = medmTextMonitorFont(text(), available);
    if (!newFont.family().isEmpty() && font() != newFont) {
      QLabel::setFont(newFont);
    }
  }
  updateOverflowGeometry();
}

void TextElement::ensureOverflowWidget()
{
  if (!overflowWidget_) {
    overflowWidget_ = new TextOverflowWidget(this);
  }
  if (overflowWidget_) {
    overflowWidget_->syncParent();
  }
}

void TextElement::updateOverflowParent()
{
  if (!overflowWidget_) {
    return;
  }
  overflowWidget_->syncParent();
}

void TextElement::updateOverflowGeometry()
{
  if (!parentWidget()) {
    if (overflowWidget_) {
      overflowWidget_->hide();
    }
    return;
  }

  ensureOverflowWidget();
  if (!overflowWidget_ || overflowWidget_->parentWidget() != parentWidget()) {
    return;
  }

  const QRect ownerRect = geometry();
  const QFontMetrics metrics(font());
  const QString currentText = text();
  const int textWidth = textPixelWidth(metrics, currentText);
  const int textHeight = metrics.ascent() + metrics.descent();

  int textLeft = ownerRect.x();
  switch (alignment_ & Qt::AlignHorizontal_Mask) {
  case Qt::AlignHCenter:
    textLeft = ownerRect.x() + (ownerRect.width() - textWidth) / 2;
    break;
  case Qt::AlignRight:
    textLeft = ownerRect.x() + ownerRect.width() - textWidth;
    break;
  default:
    break;
  }

  const int ownerLeft = ownerRect.x();
  const int ownerRight = ownerRect.x() + ownerRect.width();
  const int ownerTop = ownerRect.y();
  const int ownerBottom = ownerRect.y() + ownerRect.height();
  const int textRight = textLeft + textWidth;
  const int textTop = ownerTop;
  const int textBottom = textTop + textHeight;

  const int overlayLeft = std::min(ownerLeft, textLeft);
  const int overlayRight = std::max(ownerRight, textRight);
  const int overlayTop = ownerTop;
  const int overlayBottom = std::max(ownerBottom, textBottom);

  const int overlayWidth = std::max(1, overlayRight - overlayLeft);
  const int overlayHeight = std::max(1, overlayBottom - overlayTop);

  overflowWidget_->setGeometry(overlayLeft, overlayTop, overlayWidth, overlayHeight);
  updateOverflowStacking();
  updateOverflowVisibility();
  requestOverflowRepaint();
}

void TextElement::updateOverflowVisibility()
{
  ensureOverflowWidget();
  if (!overflowWidget_) {
    return;
  }
  QWidget *parent = overflowWidget_->parentWidget();
  if (!parent) {
    overflowWidget_->hide();
    return;
  }
  const bool visible = isVisible();
  if (overflowWidget_->isVisible() != visible) {
    if (visible) {
      overflowWidget_->show();
    } else {
      overflowWidget_->hide();
    }
  }
}

void TextElement::updateOverflowStacking()
{
  if (!overflowWidget_) {
    return;
  }
  if (overflowWidget_->parentWidget() == parentWidget()) {
    overflowWidget_->raise();
  }
}

void TextElement::requestOverflowRepaint()
{
  if (overflowWidget_ && overflowWidget_->isVisible()) {
    overflowWidget_->update();
  }
}
