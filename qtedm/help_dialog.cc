#include "help_dialog.h"

#include "command_line_options.h"
#include "window_utils.h"

#include <epicsVersion.h>

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QScopedValueRollback>
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QtMath>

#include <cmath>

namespace {
const QUrl kManualUrl(
    QStringLiteral("https://ops.aps.anl.gov/manuals/QtEDM/"));
const QUrl kEmbeddedManual(QStringLiteral("qrc:/help/QtEDM.html"));

void resizeForScreen(QWidget *window, QSize desired)
{
  if (auto *screen = window->screen()) {
    desired = desired.boundedTo(
        screen->availableGeometry().size() - QSize(40, 60));
  }
  window->resize(desired);
}

struct HelpTopic
{
  QString anchor;
  QString title;
  int level;
  int position = -1;
};

class HelpDialog final : public QDialog
{
public:
  explicit HelpDialog(QWidget *parent) : QDialog(parent)
  {
    setObjectName(QStringLiteral("qtedmHelpDialog"));
    setWindowTitle(tr("QtEDM Help"));
    setModal(false);
    /* The main application uses fixed MEDM fonts and colors. Give the guide
     * the scalable desktop presentation used by the other Qt applications. */
    setFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));
    setPalette(QApplication::style()->standardPalette());
    setWindowFlags((windowFlags() | Qt::WindowMinMaxButtonsHint)
        & ~Qt::WindowContextHelpButtonHint);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 12);
    layout->setSpacing(10);
    auto *findRow = new QHBoxLayout;
    find_ = new QLineEdit(this);
    find_->setObjectName(QStringLiteral("helpFind"));
    find_->setPlaceholderText(tr("Find in help"));
    find_->setAccessibleName(tr("Find in help"));
    find_->setClearButtonEnabled(true);
    previous_ = new QToolButton(this);
    next_ = new QToolButton(this);
    previous_->setObjectName(QStringLiteral("helpPrevious"));
    next_->setObjectName(QStringLiteral("helpNext"));
    previous_->setArrowType(Qt::UpArrow);
    next_->setArrowType(Qt::DownArrow);
    previous_->setAutoRaise(true);
    next_->setAutoRaise(true);
    previous_->setToolTip(tr("Previous match (Shift+Enter / Shift+F3)"));
    next_->setToolTip(tr("Next match (Enter / F3)"));
    previous_->setAccessibleName(tr("Previous match"));
    next_->setAccessibleName(tr("Next match"));
    matchesLabel_ = new QLabel(this);
    matchesLabel_->setObjectName(QStringLiteral("helpMatches"));
    matchesLabel_->setMinimumWidth(
        matchesLabel_->fontMetrics().horizontalAdvance(tr("No matches")));
    findRow->addWidget(find_, 1);
    findRow->addWidget(previous_);
    findRow->addWidget(next_);
    findRow->addWidget(matchesLabel_);
    layout->addLayout(findRow);

    topicsList_ = new QListWidget(this);
    topicsList_->setObjectName(QStringLiteral("helpTopics"));
    topicsList_->setAccessibleName(tr("Help topics"));
    topicsList_->setWordWrap(true);
    topicsList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    browser_ = new QTextBrowser(this);
    browser_->setObjectName(QStringLiteral("helpBrowser"));
    browser_->setAccessibleName(tr("Help text"));
    browser_->setOpenLinks(false);
    browser_->document()->setDocumentMargin(14);
    browser_->document()->setBaseUrl(kEmbeddedManual);
    browser_->document()->setDefaultStyleSheet(QStringLiteral(
        "h2 { color: %1; margin-top: 22px; margin-bottom: 6px; } "
        "h3, h4 { margin-top: 14px; margin-bottom: 4px; } "
        "a { color: %1; } th { background-color: %2; } "
        "td, th { padding: 5px; }")
        .arg(palette().color(QPalette::Link).name(),
            palette().color(QPalette::AlternateBase).name()));
    auto *splitter = new QSplitter(this);
    splitter->setObjectName(QStringLiteral("helpSplitter"));
    splitter->addWidget(topicsList_);
    splitter->addWidget(browser_);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({270, 680});
    layout->addWidget(splitter, 1);

    QFile manual(QStringLiteral(":/help/QtEDM.html"));
    QString html;
    if (manual.open(QIODevice::ReadOnly)) {
      html = QString::fromUtf8(manual.readAll());
      /* Keep the authored content, anchors, and embedded images. The web
       * page's fixed fonts and white background do not suit a desktop guide. */
      const auto body = QRegularExpression(
          QStringLiteral("<body[^>]*>([\\s\\S]*)</body>"),
          QRegularExpression::CaseInsensitiveOption).match(html);
      if (body.hasMatch()) {
        html = body.captured(1);
      }
    } else {
      html = tr("<h2>Help Not Available</h2>"
          "<p>The bundled QtEDM reference manual could not be opened.</p>");
    }
    const QRegularExpression heading(QStringLiteral(
        "<h([234])[^>]*>\\s*<a\\s+name=\"([^\"]+)\"[^>]*>"
        "[\\s\\S]*?</a>([\\s\\S]*?)</h\\1>"));
    auto headings = heading.globalMatch(html);
    while (headings.hasNext()) {
      const auto match = headings.next();
      if (match.captured(2) == QLatin1String("Contents")) {
        continue;
      }
      topics_.append({match.captured(2),
          QTextDocumentFragment::fromHtml(match.captured(3)).toPlainText(),
          match.captured(1).toInt()});
    }
    browser_->setHtml(html);
    /* Locate named anchors once. Search and scrolling share this index. */
    for (auto &topic : topics_) {
      for (auto block = browser_->document()->begin(); block.isValid();
           block = block.next()) {
        for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
          if (fragment.fragment().charFormat().anchorNames()
              .contains(topic.anchor)) {
            topic.position = block.position();
          }
        }
        if (topic.position >= 0) {
          break;
        }
      }
      auto *item = new QListWidgetItem(
          (topic.level > 2 ? QStringLiteral("  ") : QString())
              + topic.title, topicsList_);
      item->setData(Qt::UserRole, topic.anchor);
      item->setToolTip(topic.title);
      if (topic.level == 2) {
        auto font = item->font();
        font.setBold(true);
        item->setFont(font);
      }
    }
    setStyleSheet(QStringLiteral(
        "QListWidget#helpTopics { border: 1px solid %1; "
        "border-radius: 6px; padding: 4px; } "
        "QListWidget#helpTopics::item { padding: 6px 8px; "
        "border-radius: 4px; } "
        "QListWidget#helpTopics::item:selected { background: %2; color: %3; } "
        "QTextBrowser#helpBrowser { border: 1px solid %1; "
        "border-radius: 6px; }")
        .arg(palette().color(QPalette::Mid).name(),
            palette().color(QPalette::Highlight).name(),
            palette().color(QPalette::HighlightedText).name()));

    auto *buttons = new QHBoxLayout;
    auto *online = new QPushButton(tr("Online manual"), this);
    online->setObjectName(QStringLiteral("helpOnlineManual"));
    online->setAutoDefault(false);
    auto *close = new QPushButton(tr("Close"), this);
    close->setObjectName(QStringLiteral("helpClose"));
    close->setAutoDefault(false);
    buttons->addWidget(online);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    connect(online, &QPushButton::clicked, this, [] {
      QDesktopServices::openUrl(kManualUrl);
    });
    connect(topicsList_, &QListWidget::currentRowChanged, this,
        [this](int row) {
          if (row >= 0) showTopic(topics_[row].anchor);
        });
    connect(browser_->verticalScrollBar(), &QScrollBar::valueChanged, this,
        [this] { followScroll(); });
    connect(browser_, &QTextBrowser::anchorClicked, this,
        [this](const QUrl &url) {
          if ((url.isRelative() && url.path().isEmpty())
              || url.adjusted(QUrl::RemoveFragment) == kEmbeddedManual) {
            showTopic(url.fragment());
          } else if (url.scheme() == QLatin1String("https")
              || url.scheme() == QLatin1String("http")
              || url.scheme() == QLatin1String("mailto")) {
            QDesktopServices::openUrl(url);
          }
        });
    connect(find_, &QLineEdit::textChanged, this,
        [this] { updateMatches(); });
    connect(find_, &QLineEdit::returnPressed, this, [this] {
      findNext(QApplication::keyboardModifiers() & Qt::ShiftModifier
          ? -1 : 1);
    });
    connect(previous_, &QToolButton::clicked, this,
        [this] { findNext(-1); });
    connect(next_, &QToolButton::clicked, this,
        [this] { findNext(1); });
    auto shortcut = [this](QList<QKeySequence> keys, auto callback) {
      auto *action = new QAction(this);
      action->setShortcuts(keys);
      addAction(action);
      connect(action, &QAction::triggered, this, callback);
    };
    shortcut(QKeySequence::keyBindings(QKeySequence::Find), [this] {
      find_->setFocus();
      find_->selectAll();
    });
    auto nextKeys = QKeySequence::keyBindings(QKeySequence::FindNext);
    const QKeySequence f3(Qt::Key_F3);
    if (!nextKeys.contains(f3)) nextKeys.append(f3);
    shortcut(nextKeys, [this] { findNext(1); });
    auto previousKeys = QKeySequence::keyBindings(QKeySequence::FindPrevious);
    const QKeySequence shiftF3(Qt::SHIFT | Qt::Key_F3);
    if (!previousKeys.contains(shiftF3)) previousKeys.append(shiftF3);
    shortcut(previousKeys, [this] { findNext(-1); });
    updateMatches();
    resizeForScreen(this, QSize(1000, 700));
  }

  void showTopic(const QString &anchor)
  {
    QScopedValueRollback<bool> guard(navigating_, true);
    for (int i = 0; i < topics_.size(); ++i) {
      if (topics_[i].anchor == anchor) {
        selectTopic(i);
        break;
      }
    }
    browser_->scrollToAnchor(anchor);
  }

private:
  QVector<HelpTopic> topics_;
  QListWidget *topicsList_;
  QTextBrowser *browser_;
  QLineEdit *find_;
  QToolButton *previous_, *next_;
  QLabel *matchesLabel_;
  QVector<QTextCursor> matches_;
  int currentMatch_ = -1;
  bool navigating_ = false;

  void selectTopic(int row)
  {
    QSignalBlocker blocker(topicsList_);
    topicsList_->setCurrentRow(row);
    topicsList_->scrollToItem(topicsList_->currentItem());
  }

  int topicAt(int position) const
  {
    int current = 0;
    for (int i = 0; i < topics_.size(); ++i) {
      if (topics_[i].position >= 0 && topics_[i].position <= position)
        current = i;
    }
    return current;
  }

  void followScroll()
  {
    if (navigating_ || topics_.isEmpty()) return;
    int current = 0;
    auto *document = browser_->document();
    const int top = browser_->verticalScrollBar()->value() + 24;
    for (int i = 0; i < topics_.size(); ++i) {
      if (topics_[i].position >= 0
          && document->documentLayout()->blockBoundingRect(
              document->findBlock(topics_[i].position)).top() <= top)
        current = i;
    }
    selectTopic(current);
  }

  void updateMatches()
  {
    const int start = currentMatch_ >= 0
        ? matches_[currentMatch_].selectionStart()
        : browser_->cursorForPosition(QPoint(0, 0)).position();
    matches_.clear();
    currentMatch_ = -1;
    if (!find_->text().isEmpty()) {
      QTextCursor cursor(browser_->document());
      while (!(cursor = browser_->document()->find(find_->text(), cursor))
                 .isNull()) {
        if (currentMatch_ < 0 && cursor.selectionStart() >= start)
          currentMatch_ = matches_.size();
        matches_.append(cursor);
      }
      if (currentMatch_ < 0 && !matches_.isEmpty()) currentMatch_ = 0;
    }
    showMatches();
  }

  void findNext(int direction)
  {
    if (matches_.isEmpty()) return;
    currentMatch_ = (currentMatch_ + direction + matches_.size())
        % matches_.size();
    showMatches();
  }

  void showMatches()
  {
    QList<QTextEdit::ExtraSelection> selections;
    QColor highlight = browser_->palette().color(QPalette::Highlight);
    highlight.setAlpha(55);
    for (int i = 0; i < matches_.size(); ++i) {
      QTextEdit::ExtraSelection selection;
      selection.cursor = matches_[i];
      selection.format.setBackground(i == currentMatch_
          ? browser_->palette().color(QPalette::Highlight) : highlight);
      if (i == currentMatch_)
        selection.format.setForeground(
            browser_->palette().color(QPalette::HighlightedText));
      selections.append(selection);
    }
    browser_->setExtraSelections(selections);
    previous_->setEnabled(!matches_.isEmpty());
    next_->setEnabled(!matches_.isEmpty());
    matchesLabel_->setText(find_->text().isEmpty() ? QString()
        : matches_.isEmpty() ? tr("No matches")
        : tr("%1 of %2").arg(currentMatch_ + 1).arg(matches_.size()));
    if (currentMatch_ < 0) return;
    QScopedValueRollback<bool> guard(navigating_, true);
    auto cursor = matches_[currentMatch_];
    cursor.setPosition(cursor.selectionStart());
    browser_->setTextCursor(cursor);
    browser_->ensureCursorVisible();
    if (!topics_.isEmpty()) selectTopic(topicAt(cursor.position()));
  }
};

/* A decorative display panel, independent of the Demo and all live PVs.
 * Clicking it injects beam into a small storage ring, after the Advanced
 * Photon Source where QtEDM was developed. Each click adds a bunch. */
class AboutBanner final : public QWidget
{
public:
  explicit AboutBanner(QWidget *parent) : QWidget(parent), timer_(this)
  {
    setObjectName(QStringLiteral("aboutBanner"));
    setAccessibleName(tr("Illustrative QtEDM control display"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(132);
    timer_.setObjectName(QStringLiteral("aboutAnimationTimer"));
    timer_.setInterval(40);
    connect(&timer_, &QTimer::timeout, this, [this] {
      if (bunches_ && beamClock_.elapsed() >= beamEnd_) dumpBeam();
      update();
    });
  }

protected:
  void showEvent(QShowEvent *event) override
  {
    QWidget::showEvent(event);
    clock_.start();
    timer_.start();
  }

  void hideEvent(QHideEvent *event) override
  {
    timer_.stop();
    dumpBeam();
    QWidget::hideEvent(event);
  }

  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(event);
      return;
    }
    if (!bunches_) beamClock_.start();
    bunches_ = qMin(bunches_ + 1, kMaxBunches);
    beamEnd_ = beamClock_.elapsed() + kBeamLifetimeMs;
    setAccessibleDescription(bunches_ == 1 ? tr("Stored beam: 1 bunch")
        : tr("Stored beam: %1 bunches").arg(bunches_));
    event->accept();
    update();
  }

  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(width() / 460.0, height() / 132.0);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(palette().brush(QPalette::Base));
    painter.drawRoundedRect(QRectF(1, 1, 458, 130), 8, 8);
    const QRectF plot(18, 18, 270, 96);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DotLine));
    for (int i = 1; i < 5; ++i) {
      const qreal x = plot.left() + i * plot.width() / 5;
      painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    for (int i = 1; i < 4; ++i) {
      const qreal y = plot.top() + i * plot.height() / 4;
      painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }
    const qreal time = clock_.isValid() ? clock_.elapsed() / 1000.0 : 0;
    const QColor colors[] = {palette().color(QPalette::Link),
        QColor(34, 160, 107), QColor(214, 141, 36)};
    for (int trace = 0; trace < 3; ++trace) {
      QPainterPath path;
      for (int point = 0; point <= 100; ++point) {
        const qreal x = point / 100.0;
        const qreal value = 0.5
            + 0.32 * std::sin(x * 9 + time * 0.6 + trace * 2);
        const QPointF position(plot.left() + x * plot.width(),
            plot.bottom() - value * plot.height());
        if (!point) path.moveTo(position);
        else path.lineTo(position);
      }
      painter.setPen(QPen(colors[trace], 2));
      painter.setBrush(Qt::NoBrush);
      painter.drawPath(path);
      const QRectF bar(312, 23 + trace * 32, 125, 16);
      painter.setPen(Qt::NoPen);
      painter.setBrush(palette().brush(QPalette::AlternateBase));
      painter.drawRoundedRect(bar, 4, 4);
      painter.setBrush(colors[trace]);
      painter.drawRoundedRect(QRectF(bar.topLeft(),
          QSizeF(bar.width() * (0.55 + 0.3 * std::sin(time + trace)),
              bar.height())), 4, 4);
    }
    if (bunches_) paintStorageRing(painter);
  }

private:
  static constexpr int kMaxBunches = 8;
  static constexpr qint64 kBeamLifetimeMs = 8000;

  QTimer timer_;
  QElapsedTimer clock_;
  QElapsedTimer beamClock_;
  qint64 beamEnd_ = 0;
  int bunches_ = 0;

  void dumpBeam()
  {
    bunches_ = 0;
    setAccessibleDescription(QString());
  }

  void paintStorageRing(QPainter &painter)
  {
    const qint64 elapsed = beamClock_.elapsed();
    const qreal t = elapsed / 1000.0;
    /* Fade in on injection and out as the beam decays. */
    painter.setOpacity(qBound<qreal>(0, qMin(elapsed / 400.0,
        (beamEnd_ - elapsed) / 600.0), 1));
    QPainterPath card;
    card.addRoundedRect(QRectF(2, 2, 456, 128), 7, 7);
    painter.setClipPath(card);
    painter.fillPath(card, palette().brush(QPalette::Base));

    const QPointF center(230, 66);
    const qreal rx = 196;
    const qreal ry = 46;
    auto onRing = [&](qreal angle) {
      return center + QPointF(rx * std::cos(angle), ry * std::sin(angle));
    };
    const QColor photon(214, 141, 36);
    const QColor electron = palette().color(QPalette::Link);

    /* Vacuum chamber, with a magnet at each of the 40 sectors. */
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(palette().color(QPalette::Mid), 6));
    painter.drawEllipse(center, rx, ry);
    painter.setPen(QPen(palette().color(QPalette::AlternateBase), 3));
    painter.drawEllipse(center, rx, ry);
    painter.setPen(QPen(palette().color(QPalette::Mid), 2));
    for (int sector = 0; sector < 40; ++sector) {
      const qreal angle = sector * 2 * M_PI / 40;
      QPointF normal(std::cos(angle) / rx, std::sin(angle) / ry);
      normal /= std::hypot(normal.x(), normal.y());
      const QPointF at = onRing(angle);
      painter.drawLine(at - normal * 6, at + normal * 6);
    }

    for (int bunch = 0; bunch < bunches_; ++bunch) {
      const qreal phase = t * 2.4 + bunch * 2 * M_PI / bunches_;
      const QPointF at = onRing(phase);
      /* Synchrotron light leaves each bunch along the tangent. */
      QPointF tangent(-rx * std::sin(phase), ry * std::cos(phase));
      tangent /= std::hypot(tangent.x(), tangent.y());
      for (int ray = -1; ray <= 1; ++ray) {
        const QPointF direction = tangent
            + QPointF(-tangent.y(), tangent.x()) * (0.06 * ray);
        QLinearGradient light(at, at + direction * 70);
        QColor bright = photon;
        bright.setAlpha(ray ? 90 : 200);
        QColor faded = photon;
        faded.setAlpha(0);
        light.setColorAt(0, bright);
        light.setColorAt(1, faded);
        painter.setPen(QPen(QBrush(light), ray ? 1.2 : 2, Qt::SolidLine,
            Qt::RoundCap));
        painter.drawLine(at, at + direction * 70);
      }
      painter.setPen(Qt::NoPen);
      for (int trail = 11; trail >= 0; --trail) {
        QColor color = electron;
        color.setAlphaF(1 - trail / 12.0);
        painter.setBrush(color);
        painter.drawEllipse(onRing(phase - trail * 0.045),
            3.6 - trail * 0.2, 3.6 - trail * 0.2);
      }
    }

    /* The stored current fills toward the APS-U design current. */
    const qreal current = 200.0 * qMin<qreal>(1, t / 1.5);
    QFont label = font();
    label.setPointSizeF(7.5);
    label.setBold(true);
    label.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    painter.setFont(label);
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(QRectF(130, 44, 200, 14), Qt::AlignCenter,
        current < 200 ? tr("INJECTING") : tr("BEAM STORED"));
    QFont value = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    value.setPointSizeF(15);
    value.setBold(true);
    painter.setFont(value);
    painter.setPen(electron);
    painter.drawText(QRectF(130, 58, 200, 28), Qt::AlignCenter,
        QStringLiteral("%1 mA").arg(current, 5, 'f', 1));
    painter.setClipping(false);
    painter.setOpacity(1);
  }
};
} // namespace

QDialog *showQtEdmHelp(QWidget *parent, const QString &topic)
{
  auto *dialog = parent ? static_cast<HelpDialog *>(
      parent->findChild<QDialog *>(QStringLiteral("qtedmHelpDialog"),
          Qt::FindDirectChildrenOnly)) : nullptr;
  if (!dialog) {
    dialog = new HelpDialog(parent);
    centerWindowOnScreen(dialog);
  }
  dialog->showNormal();
  if (!topic.isEmpty()) dialog->showTopic(topic);
  activateWindowWhenExposed(dialog);
  return dialog;
}

QDialog *showQtEdmAbout(QWidget *parent)
{
  auto *dialog = parent ? parent->findChild<QDialog *>(
      QStringLiteral("qtedmAboutDialog"), Qt::FindDirectChildrenOnly) : nullptr;
  if (!dialog) {
    dialog = new QDialog(parent);
    dialog->setObjectName(QStringLiteral("qtedmAboutDialog"));
    dialog->setWindowTitle(QObject::tr("About QtEDM"));
    dialog->setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog->setFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));
    dialog->setPalette(QApplication::style()->standardPalette());
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(18, 18, 18, 14);
    layout->setSpacing(6);
    layout->addWidget(new AboutBanner(dialog));
    auto *title = new QLabel(QStringLiteral("QtEDM"), dialog);
    title->setObjectName(QStringLiteral("aboutTitle"));
    auto titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() > 0
        ? titleFont.pointSizeF() * 1.7 : 18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);
    const auto version = QRegularExpression(QStringLiteral("Version (\\S+)"))
        .match(QString::fromLatin1(kQtEdmVersionString)).captured(1);
    auto *subtitle = new QLabel(QObject::tr("Version %1 · EPICS Display Manager")
        .arg(version), dialog);
    subtitle->setObjectName(QStringLiteral("aboutSubtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);
    auto *credits = new QTextBrowser(dialog);
    credits->setObjectName(QStringLiteral("aboutCredits"));
    credits->setAccessibleName(QObject::tr("Author credits"));
    credits->setFrameShape(QFrame::NoFrame);
    auto creditsPalette = credits->palette();
    creditsPalette.setColor(QPalette::Base,
        dialog->palette().color(QPalette::Window));
    credits->setPalette(creditsPalette);
    credits->document()->setDocumentMargin(0);
    credits->setOpenExternalLinks(true);
    credits->setHtml(QObject::tr(
        "<p>Qt-based Editor and Display Manager for EPICS.<br>"
        "Developed at Argonne National Laboratory</p>"
        "<p><b>Qt port development and maintenance</b><br>"
        "Robert Soliday — <a href=\"mailto:soliday@anl.gov\">"
        "soliday@anl.gov</a></p>"
        "<p><b>Original MEDM authors</b><br>"
        "Mark Anderson — original design and implementation<br>"
        "Frederick Vong — enhancements and ADL format<br>"
        "Kenneth Evans, Jr. — maintenance and reference manual</p>"
        "<p><b>Additional contributors</b><br>"
        "The EPICS community</p>"));
    layout->addWidget(credits, 1);
    QString epics = QString::fromLatin1(EPICS_VERSION_STRING);
    if (epics.startsWith(QLatin1String("EPICS "))) epics.remove(0, 6);
    auto *footer = new QLabel(QObject::tr("Qt %1 · EPICS %2")
        .arg(QString::fromLatin1(qVersion()), epics), dialog);
    footer->setObjectName(QStringLiteral("aboutFooter"));
    footer->setForegroundRole(QPalette::PlaceholderText);
    footer->setWordWrap(true);
    footer->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(footer);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, dialog);
    auto *help = buttons->addButton(QObject::tr("Open help"),
        QDialogButtonBox::HelpRole);
    help->setObjectName(QStringLiteral("aboutOpenHelp"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog,
        &QDialog::accept);
    QObject::connect(help, &QPushButton::clicked, dialog, [dialog, parent] {
      dialog->accept();
      showQtEdmHelp(parent, QStringLiteral("Introduction"));
    });
    layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Ok)->setDefault(true);
    buttons->button(QDialogButtonBox::Ok)->setFocus();
    /* The browser keeps its own document wrapped to the (not yet shown)
     * viewport, so measure the credits on a copy at their final width. */
    const int chromeHeight = dialog->sizeHint().height()
        - credits->sizeHint().height();
    QScopedPointer<QTextDocument> measured(credits->document()->clone());
    measured->setTextWidth(464);
    resizeForScreen(dialog, QSize(500, chromeHeight
        + qCeil(measured->size().height()) + 12));
    centerWindowOnScreen(dialog);
  }
  dialog->showNormal();
  activateWindowWhenExposed(dialog);
  return dialog;
}
