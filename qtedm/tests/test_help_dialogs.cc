#include <QtTest/QtTest>

#include <QAction>
#include <QDialog>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>

#include "color_palette_dialog.h"
#include "display_state.h"
#include "help_dialog.h"
#include "object_palette_dialog.h"
#include "resource_palette_dialog.h"

namespace {
bool savePreview(QWidget *widget, const QString &name)
{
  const QString directory = qEnvironmentVariable("QTEDM_HELP_VISUAL_DIR");
  return directory.isEmpty()
      || widget->grab().save(QDir(directory).filePath(name));
}
}

class TestHelpDialogs : public QObject
{
  Q_OBJECT

private slots:
  void guideNavigationSearchAndReuse();
  void paletteMenusOpenTheirTopics();
  void modalOwnerKeepsHelpUsableAndOwnsItsLifetime();
  void aboutCreditsAndHelpHandoff();
  void aboutBannerInjectsBeam();
};

void TestHelpDialogs::guideNavigationSearchAndReuse()
{
  QWidget owner;
  owner.show();
  auto *help = showQtEdmHelp(&owner, QStringLiteral("Introduction"));
  QVERIFY(help->isVisible());
  QVERIFY(!help->isModal());
  auto *topics = help->findChild<QListWidget *>(QStringLiteral("helpTopics"));
  auto *browser = help->findChild<QTextBrowser *>(QStringLiteral("helpBrowser"));
  auto *find = help->findChild<QLineEdit *>(QStringLiteral("helpFind"));
  auto *matches = help->findChild<QLabel *>(QStringLiteral("helpMatches"));
  auto *next = help->findChild<QToolButton *>(QStringLiteral("helpNext"));
  auto *previous = help->findChild<QToolButton *>(QStringLiteral("helpPrevious"));
  QVERIFY(topics && browser && find && matches && next && previous);
  QVERIFY(topics->count() >= 40);
  QCOMPARE(topics->currentItem()->data(Qt::UserRole).toString(),
      QStringLiteral("Introduction"));
  QVERIFY(browser->toPlainText().contains(QStringLiteral("Channel Access")));
  const QString guideText = browser->toPlainText();
  QVERIFY(guideText.contains(QStringLiteral(
      "synthetic preview grid of at most 32 by 24 cells")));
  QVERIFY(guideText.contains(QStringLiteral(
      "Macro expansion is checked before parsing")));
  QVERIFY(guideText.contains(QStringLiteral(
      "A failed audit write or flush emits a warning")));
  QVERIFY(guideText.contains(QStringLiteral(
      "Failed pens remain named in the diagnostic")));
  QVERIFY(browser->document()->resource(QTextDocument::ImageResource,
      QUrl(QStringLiteral("qrc:/help/images/qtedm-widgets/slider.png")))
      .isValid());
  QApplication::processEvents();
  QVERIFY(savePreview(help, QStringLiteral("help-topics.png")));

  /* Internal manual links stay in the guide and update its topic selection. */
  browser->anchorClicked(QUrl(QStringLiteral("#WidgetSlider")));
  QCOMPARE(topics->currentItem()->data(Qt::UserRole).toString(),
      QStringLiteral("WidgetSlider"));
  QVERIFY(browser->verticalScrollBar()->value() > 0);
  QCOMPARE(showQtEdmHelp(&owner, QStringLiteral("KeyboardShortcuts")), help);
  QCOMPARE(topics->currentItem()->data(Qt::UserRole).toString(),
      QStringLiteral("KeyboardShortcuts"));

  /* Search spans all topics, ignores case, and wraps without closing Help. */
  find->setText(QStringLiteral("CHANNEL ACCESS"));
  const int count = browser->extraSelections().size();
  QVERIFY(count > 2);
  QVERIFY(next->isEnabled());
  const int initial = browser->textCursor().position();
  next->click();
  QVERIFY(browser->textCursor().position() != initial);
  previous->click();
  QCOMPARE(browser->textCursor().position(), initial);
  for (int i = 0; i < count; ++i) next->click();
  QCOMPARE(browser->textCursor().position(), initial);
  find->setText(QStringLiteral("channel access"));
  QCOMPARE(browser->extraSelections().size(), count);
  find->setFocus();
  QTest::keyClick(find, Qt::Key_Return);
  QVERIFY(help->isVisible());
  QVERIFY(browser->textCursor().position() != initial);
  QTest::keyClick(find, Qt::Key_Return, Qt::ShiftModifier);
  QCOMPARE(browser->textCursor().position(), initial);
  help->activateWindow();
  QTest::qWait(20);
  QTest::keyClick(browser, Qt::Key_F, Qt::ControlModifier);
  QTRY_VERIFY(find->hasFocus());
  QTest::keyClick(find, Qt::Key_F3);
  QVERIFY(browser->textCursor().position() != initial);
  QTest::keyClick(find, Qt::Key_F3, Qt::ShiftModifier);
  QCOMPARE(browser->textCursor().position(), initial);
  find->setText(QStringLiteral("no-such-help-phrase-123456"));
  QCOMPARE(matches->text(), QStringLiteral("No matches"));
  QVERIFY(browser->extraSelections().isEmpty());
  QVERIFY(!browser->textCursor().hasSelection());
  QVERIFY(!next->isEnabled());
  find->clear();
  QVERIFY(matches->text().isEmpty());

  topics->setCurrentRow(0);
  browser->verticalScrollBar()->setValue(
      browser->verticalScrollBar()->maximum());
  /* At the bottom, several short sections can share the viewport. The
   * sidebar follows the section at its top, rather than the final heading. */
  QVERIFY(topics->currentRow() > 0);
  browser->verticalScrollBar()->setValue(0);
  QCOMPARE(topics->currentRow(), 0);
  QTest::keyClick(find, Qt::Key_Escape);
  QVERIFY(!help->isVisible());
  QCOMPARE(showQtEdmHelp(&owner), help);
  QCOMPARE(owner.findChildren<QDialog *>(
      QStringLiteral("qtedmHelpDialog")).size(), 1);
  help->findChild<QPushButton *>(QStringLiteral("helpClose"))->click();
  QVERIFY(!help->isVisible());
}

void TestHelpDialogs::paletteMenusOpenTheirTopics()
{
  const auto state = std::make_shared<DisplayState>();
  ObjectPaletteDialog objects(QApplication::palette(), QFont(), QFont(), state);
  ResourcePaletteDialog resources(QApplication::palette(), QFont(), QFont());
  ColorPaletteDialog colors(QApplication::palette(), QFont(), QFont());
  struct PaletteCase {
    QWidget *owner;
    const char *action;
    const char *topic;
  };
  for (const auto &test : {
           PaletteCase{&objects, "objectPaletteHelpAction", "ObjectPalette"},
           PaletteCase{&resources, "resourcePaletteHelpAction", "ResourcePalette"},
           PaletteCase{&colors, "colorPaletteHelpAction", "ColorPalette"}}) {
    auto *action = test.owner->findChild<QAction *>(QLatin1String(test.action));
    QVERIFY(action);
    QVERIFY(action->shortcuts().contains(QKeySequence(Qt::Key_F1)));
    action->trigger();
    auto *help = test.owner->findChild<QDialog *>(
        QStringLiteral("qtedmHelpDialog"));
    QVERIFY(help && help->isVisible());
    QCOMPARE(help->findChild<QListWidget *>(QStringLiteral("helpTopics"))
        ->currentItem()->data(Qt::UserRole).toString(),
        QLatin1String(test.topic));
    action->trigger();
    QCOMPARE(test.owner->findChildren<QDialog *>(
        QStringLiteral("qtedmHelpDialog")).size(), 1);
    help->close();
  }
  objects.findChild<QAction *>(QStringLiteral("objectIndexHelpAction"))
      ->trigger();
  auto *help = objects.findChild<QDialog *>(QStringLiteral("qtedmHelpDialog"));
  QCOMPARE(help->findChild<QListWidget *>(QStringLiteral("helpTopics"))
      ->currentItem()->data(Qt::UserRole).toString(), QStringLiteral("Widgets"));
}

void TestHelpDialogs::modalOwnerKeepsHelpUsableAndOwnsItsLifetime()
{
  QPointer<QDialog> help;
  {
    QDialog sliderEntry;
    sliderEntry.setModal(true);
    sliderEntry.show();
    help = showQtEdmHelp(&sliderEntry, QStringLiteral("WidgetSlider"));
    QCOMPARE(help->parentWidget(), &sliderEntry);
    QVERIFY(!help->isModal());
    QVERIFY(sliderEntry.isModal());
    auto *find = help->findChild<QLineEdit *>(QStringLiteral("helpFind"));
    QTest::keyClicks(find, QStringLiteral("increment"));
    QVERIFY(!help->findChild<QTextBrowser *>(QStringLiteral("helpBrowser"))
        ->extraSelections().isEmpty());
  }
  QVERIFY(help.isNull());
}

void TestHelpDialogs::aboutCreditsAndHelpHandoff()
{
  QWidget owner;
  auto *about = showQtEdmAbout(&owner);
  auto *credits = about->findChild<QTextBrowser *>(QStringLiteral("aboutCredits"));
  auto *footer = about->findChild<QLabel *>(QStringLiteral("aboutFooter"));
  auto *timer = about->findChild<QTimer *>(QStringLiteral("aboutAnimationTimer"));
  QVERIFY(credits && footer && timer);
  for (const char *author : {"Robert Soliday", "Mark Anderson",
           "Frederick Vong", "Kenneth Evans", "EPICS community"}) {
    QVERIFY(credits->toPlainText().contains(QLatin1String(author)));
  }
  QVERIFY(footer->text().contains(QString::fromLatin1(qVersion())));
  QVERIFY(footer->text().contains(QStringLiteral("EPICS")));
  QVERIFY(timer->isActive());
  QApplication::processEvents();
  QVERIFY(savePreview(about, QStringLiteral("about.png")));
  /* Unless a small screen limits its height, About shows every credit
   * without scrolling or a large empty gap below them. */
  const auto last = credits->document()->find(QStringLiteral("EPICS community"));
  QVERIFY(!last.isNull());
  if (about->height()
      < about->screen()->availableGeometry().height() - 60) {
    QCOMPARE(credits->verticalScrollBar()->maximum(), 0);
    QVERIFY(credits->viewport()->height()
        - credits->cursorRect(last).bottom() < 60);
  }
  credits->setTextCursor(last);
  credits->ensureCursorVisible();
  QVERIFY(credits->cursorRect(last).bottom() <= credits->viewport()->height());
  about->close();
  QVERIFY(!timer->isActive());
  QCOMPARE(showQtEdmAbout(&owner), about);
  QVERIFY(timer->isActive());
  about->findChild<QPushButton *>(QStringLiteral("aboutOpenHelp"))->click();
  QVERIFY(!about->isVisible());
  QVERIFY(!timer->isActive());
  auto *help = owner.findChild<QDialog *>(QStringLiteral("qtedmHelpDialog"));
  QVERIFY(help && help->isVisible());
  QVERIFY(!QApplication::activeModalWidget());
}

void TestHelpDialogs::aboutBannerInjectsBeam()
{
  QWidget owner;
  auto *about = showQtEdmAbout(&owner);
  auto *banner = about->findChild<QWidget *>(QStringLiteral("aboutBanner"));
  QVERIFY(banner);
  QVERIFY(banner->accessibleDescription().isEmpty());

  /* Each click injects another bunch, up to a full ring of eight. */
  QTest::mouseClick(banner, Qt::LeftButton);
  QCOMPARE(banner->accessibleDescription(),
      QStringLiteral("Stored beam: 1 bunch"));
  QTest::mouseClick(banner, Qt::RightButton);
  QCOMPARE(banner->accessibleDescription(),
      QStringLiteral("Stored beam: 1 bunch"));
  for (int i = 0; i < 10; ++i) QTest::mouseClick(banner, Qt::LeftButton);
  QCOMPARE(banner->accessibleDescription(),
      QStringLiteral("Stored beam: 8 bunches"));
  QTest::qWait(1600);
  QVERIFY(savePreview(about, QStringLiteral("about-beam.png")));

  /* Closing About dumps the beam so the next opening starts normally. */
  about->close();
  QVERIFY(banner->accessibleDescription().isEmpty());
  QCOMPARE(showQtEdmAbout(&owner), about);
  QVERIFY(banner->accessibleDescription().isEmpty());
  about->close();
}

QTEST_MAIN(TestHelpDialogs)
#include "test_help_dialogs.moc"
