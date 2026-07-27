#include "tabbed_display_element.h"

#include <algorithm>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

QString effectivePageLabel(const TabbedDisplayPage &page, int index)
{
  const QString label = page.label.trimmed();
  return label.isEmpty()
      ? QStringLiteral("Page %1").arg(index + 1)
      : label;
}

QLabel *placeholderLabel(QWidget *host)
{
  return host ? host->findChild<QLabel *>(QStringLiteral("qtedmPagePlaceholder"))
              : nullptr;
}

}  // namespace

TabbedDisplayElement::TabbedDisplayElement(QWidget *parent)
  : QWidget(parent)
  , tabBar_(new QTabBar(this))
  , stack_(new QStackedWidget(this))
{
  setAutoFillBackground(true);
  setBackgroundRole(QPalette::Base);
  setFocusPolicy(Qt::StrongFocus);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(1, 1, 1, 1);
  layout->setSpacing(0);
  layout->addWidget(tabBar_);
  layout->addWidget(stack_, 1);

  tabBar_->setExpanding(false);
  tabBar_->setMovable(false);
  tabBar_->setDrawBase(true);
  QObject::connect(tabBar_, &QTabBar::currentChanged, this,
      [this](int index) { activatePage(index); });

  auto *nextShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown), this);
  nextShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(nextShortcut, &QShortcut::activated, this, [this]() {
    if (pages_.size() > 1) {
      setActivePageIndex((activePageIndex() + 1) % pages_.size());
    }
  });
  auto *previousShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp), this);
  previousShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(previousShortcut, &QShortcut::activated, this, [this]() {
    if (pages_.size() > 1) {
      setActivePageIndex(
          (activePageIndex() + pages_.size() - 1) % pages_.size());
    }
  });

  setPages({
      {QStringLiteral("page-1"), QStringLiteral("Page 1"), QString(), QString(),
          false},
      {QStringLiteral("page-2"), QStringLiteral("Page 2"), QString(), QString(),
          false},
  });
}

TabbedDisplayElement::~TabbedDisplayElement()
{
  for (int index = 0; index < runtimes_.size(); ++index) {
    unloadPage(index);
  }
}

QList<TabbedDisplayPage> TabbedDisplayElement::pages() const
{
  return pages_;
}

void TabbedDisplayElement::setPages(const QList<TabbedDisplayPage> &pages)
{
  const QString previousId = activePageId();
  pages_ = normalizedPages(pages);
  rebuildPages();
  if (!previousId.isEmpty()) {
    setActivePageId(previousId);
  }
}

bool TabbedDisplayElement::hiddenTabs() const
{
  return hiddenTabs_;
}

void TabbedDisplayElement::setHiddenTabs(bool hidden)
{
  hiddenTabs_ = hidden;
  tabBar_->setVisible(!hiddenTabs_);
}

QString TabbedDisplayElement::activePageId() const
{
  const int index = activePageIndex();
  return index >= 0 && index < pages_.size() ? pages_.at(index).id : QString();
}

int TabbedDisplayElement::activePageIndex() const
{
  return tabBar_ ? tabBar_->currentIndex() : -1;
}

bool TabbedDisplayElement::setActivePageId(const QString &id)
{
  for (int index = 0; index < pages_.size(); ++index) {
    if (pages_.at(index).id == id) {
      setActivePageIndex(index);
      return true;
    }
  }
  return false;
}

void TabbedDisplayElement::setActivePageIndex(int index)
{
  if (pages_.isEmpty()) {
    return;
  }
  const int bounded = std::clamp(index, 0,
      static_cast<int>(pages_.size()) - 1);
  if (tabBar_->currentIndex() == bounded) {
    activatePage(bounded);
  } else {
    tabBar_->setCurrentIndex(bounded);
  }
}

void TabbedDisplayElement::setPageLoader(PageLoader loader)
{
  loader_ = std::move(loader);
  if (executeMode_) {
    loadPage(activePageIndex());
  }
}

void TabbedDisplayElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  if (executeMode_) {
    loadPage(activePageIndex());
  } else {
    for (int index = 0; index < runtimes_.size(); ++index) {
      unloadPage(index);
    }
  }
  updatePlaceholders();
}

bool TabbedDisplayElement::isExecuteMode() const
{
  return executeMode_;
}

void TabbedDisplayElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool TabbedDisplayElement::isSelected() const
{
  return selected_;
}

void TabbedDisplayElement::setChangedCallback(std::function<void()> callback)
{
  changedCallback_ = std::move(callback);
}

int TabbedDisplayElement::loadedPageCount() const
{
  int count = 0;
  for (const PageRuntime &runtime : runtimes_) {
    if (!runtime.content.isNull()) {
      ++count;
    }
  }
  return count;
}

QWidget *TabbedDisplayElement::pageContent(int index) const
{
  return index >= 0 && index < runtimes_.size()
      ? runtimes_.at(index).content.data()
      : nullptr;
}

QString TabbedDisplayElement::pageDiagnostic(int index) const
{
  return index >= 0 && index < runtimes_.size()
      ? runtimes_.at(index).diagnostic
      : QString();
}

void TabbedDisplayElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);
  if (!selected_) {
    return;
  }
  QPainter painter(this);
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void TabbedDisplayElement::mousePressEvent(QMouseEvent *event)
{
  if (!executeMode_ && forwardMouseEventToParent(event)) {
    return;
  }
  QWidget::mousePressEvent(event);
}

void TabbedDisplayElement::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (!executeMode_ && event->button() == Qt::LeftButton) {
    showPageEditor();
    event->accept();
    return;
  }
  QWidget::mouseDoubleClickEvent(event);
}

void TabbedDisplayElement::rebuildPages()
{
  for (int index = 0; index < runtimes_.size(); ++index) {
    unloadPage(index);
  }
  while (stack_->count() > 0) {
    QWidget *page = stack_->widget(0);
    stack_->removeWidget(page);
    delete page;
  }
  while (tabBar_->count() > 0) {
    tabBar_->removeTab(0);
  }
  runtimes_.clear();

  for (int index = 0; index < pages_.size(); ++index) {
    const TabbedDisplayPage &page = pages_.at(index);
    tabBar_->addTab(effectivePageLabel(page, index));
    tabBar_->setTabData(index, page.id);

    auto *host = new QWidget(stack_);
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(4, 4, 4, 4);
    auto *placeholder = new QLabel(host);
    placeholder->setObjectName(QStringLiteral("qtedmPagePlaceholder"));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setWordWrap(true);
    layout->addWidget(placeholder, 1);
    stack_->addWidget(host);
    runtimes_.append({host, nullptr, QString()});
  }

  const int initial = pages_.isEmpty() ? -1 : 0;
  previousIndex_ = initial;
  if (initial >= 0) {
    tabBar_->setCurrentIndex(initial);
    stack_->setCurrentIndex(initial);
  }
  if (runtimes_.size() == pages_.size()) {
    updatePlaceholders();
  }
  if (executeMode_) {
    loadPage(initial);
  }
}

void TabbedDisplayElement::activatePage(int index)
{
  if (index < 0 || index >= pages_.size()) {
    return;
  }
  if (previousIndex_ >= 0 && previousIndex_ < pages_.size()
      && previousIndex_ != index && !pages_.at(previousIndex_).keepAlive) {
    unloadPage(previousIndex_);
  }
  previousIndex_ = index;
  stack_->setCurrentIndex(index);
  if (executeMode_) {
    loadPage(index);
  }
}

void TabbedDisplayElement::loadPage(int index)
{
  if (!executeMode_ || index < 0 || index >= pages_.size()
      || index >= runtimes_.size()) {
    return;
  }
  PageRuntime &runtime = runtimes_[index];
  if (!runtime.content.isNull()) {
    return;
  }

  runtime.diagnostic.clear();
  const TabbedDisplayPage &page = pages_.at(index);
  if (page.displayPath.trimmed().isEmpty()) {
    runtime.diagnostic = QStringLiteral("No child display is configured.");
  } else if (!loader_) {
    runtime.diagnostic = QStringLiteral("Child display loader is unavailable.");
  } else {
    QString error;
    QWidget *content = loader_(page, runtime.host, &error);
    if (content) {
      runtime.content = content;
      if (QLayout *layout = runtime.host->layout()) {
        if (QLabel *placeholder = placeholderLabel(runtime.host)) {
          placeholder->hide();
        }
        layout->addWidget(content);
      }
      content->show();
    } else {
      runtime.diagnostic = error.trimmed().isEmpty()
          ? QStringLiteral("Failed to load the child display.")
          : error.trimmed();
    }
  }
  updatePlaceholders();
}

void TabbedDisplayElement::unloadPage(int index)
{
  if (index < 0 || index >= runtimes_.size()) {
    return;
  }
  PageRuntime &runtime = runtimes_[index];
  if (!runtime.content.isNull()) {
    QWidget *content = runtime.content.data();
    runtime.content.clear();
    delete content;
  }
  if (runtimes_.size() == pages_.size()) {
    updatePlaceholders();
  }
}

void TabbedDisplayElement::updatePlaceholders()
{
  for (int index = 0; index < runtimes_.size(); ++index) {
    PageRuntime &runtime = runtimes_[index];
    QLabel *placeholder = placeholderLabel(runtime.host);
    if (!placeholder) {
      continue;
    }
    if (!runtime.content.isNull()) {
      placeholder->hide();
      continue;
    }
    const TabbedDisplayPage &page = pages_.at(index);
    if (!runtime.diagnostic.isEmpty()) {
      placeholder->setText(QStringLiteral("Unable to load “%1”\n%2")
          .arg(effectivePageLabel(page, index), runtime.diagnostic));
      QPalette palette = placeholder->palette();
      palette.setColor(QPalette::WindowText, QColor(180, 0, 0));
      placeholder->setPalette(palette);
    } else if (executeMode_) {
      placeholder->setText(QStringLiteral("Loading %1…")
          .arg(effectivePageLabel(page, index)));
    } else {
      placeholder->setText(QStringLiteral("%1\n%2\nMacros: %3\nkeepAlive: %4")
          .arg(effectivePageLabel(page, index),
              page.displayPath.trimmed().isEmpty()
                  ? QStringLiteral("(no child display)")
                  : page.displayPath,
              page.macros.trimmed().isEmpty()
                  ? QStringLiteral("(inherited)")
                  : page.macros,
              page.keepAlive ? QStringLiteral("true")
                             : QStringLiteral("false")));
    }
    placeholder->show();
  }
}

void TabbedDisplayElement::showPageEditor()
{
  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("Tabbed Display Properties"));
  dialog.resize(780, 360);
  auto *outer = new QVBoxLayout(&dialog);

  auto *hiddenCheck = new QCheckBox(QStringLiteral("Stacked mode (hide tabs)"),
      &dialog);
  hiddenCheck->setChecked(hiddenTabs_);
  outer->addWidget(hiddenCheck);

  auto *table = new QTableWidget(pages_.size(), 5, &dialog);
  table->setHorizontalHeaderLabels({QStringLiteral("Stable ID"),
      QStringLiteral("Label"), QStringLiteral("Display path"),
      QStringLiteral("Macros"), QStringLiteral("keepAlive")});
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  for (int row = 0; row < pages_.size(); ++row) {
    const TabbedDisplayPage &page = pages_.at(row);
    table->setItem(row, 0, new QTableWidgetItem(page.id));
    table->setItem(row, 1, new QTableWidgetItem(page.label));
    table->setItem(row, 2, new QTableWidgetItem(page.displayPath));
    table->setItem(row, 3, new QTableWidgetItem(page.macros));
    auto *keepItem = new QTableWidgetItem;
    keepItem->setFlags(keepItem->flags() | Qt::ItemIsUserCheckable);
    keepItem->setCheckState(page.keepAlive ? Qt::Checked : Qt::Unchecked);
    table->setItem(row, 4, keepItem);
  }
  outer->addWidget(table, 1);

  auto *rowButtons = new QHBoxLayout;
  auto *addButton = new QPushButton(QStringLiteral("Add Page"), &dialog);
  auto *removeButton = new QPushButton(QStringLiteral("Remove Page"), &dialog);
  rowButtons->addWidget(addButton);
  rowButtons->addWidget(removeButton);
  rowButtons->addStretch(1);
  outer->addLayout(rowButtons);
  QObject::connect(addButton, &QPushButton::clicked, &dialog, [table]() {
    const int row = table->rowCount();
    table->insertRow(row);
    table->setItem(row, 0,
        new QTableWidgetItem(QStringLiteral("page-%1").arg(row + 1)));
    table->setItem(row, 1,
        new QTableWidgetItem(QStringLiteral("Page %1").arg(row + 1)));
    table->setItem(row, 2, new QTableWidgetItem);
    table->setItem(row, 3, new QTableWidgetItem);
    auto *keepItem = new QTableWidgetItem;
    keepItem->setFlags(keepItem->flags() | Qt::ItemIsUserCheckable);
    keepItem->setCheckState(Qt::Unchecked);
    table->setItem(row, 4, keepItem);
  });
  QObject::connect(removeButton, &QPushButton::clicked, &dialog, [table]() {
    if (table->rowCount() > 1) {
      const int row = std::max(0, table->currentRow());
      table->removeRow(row);
    }
  });

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  outer->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog,
      &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog,
      &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }
  QList<TabbedDisplayPage> updated;
  for (int row = 0; row < table->rowCount(); ++row) {
    auto textAt = [table, row](int column) {
      QTableWidgetItem *item = table->item(row, column);
      return item ? item->text().trimmed() : QString();
    };
    QTableWidgetItem *keepItem = table->item(row, 4);
    updated.append({textAt(0), textAt(1), textAt(2), textAt(3),
        keepItem && keepItem->checkState() == Qt::Checked});
  }
  setHiddenTabs(hiddenCheck->isChecked());
  setPages(updated);
  if (changedCallback_) {
    changedCallback_();
  }
}

bool TabbedDisplayElement::forwardMouseEventToParent(QMouseEvent *event) const
{
  QWidget *target = parentWidget();
  if (!target || !event) {
    return false;
  }
  const QPoint mapped = mapTo(target, event->pos());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  QMouseEvent forwarded(event->type(), QPointF(mapped), event->globalPosition(),
      event->button(), event->buttons(), event->modifiers());
#else
  QMouseEvent forwarded(event->type(), QPointF(mapped), event->globalPos(),
      event->button(), event->buttons(), event->modifiers());
#endif
  QApplication::sendEvent(target, &forwarded);
  return forwarded.isAccepted();
}

QList<TabbedDisplayPage> TabbedDisplayElement::normalizedPages(
    const QList<TabbedDisplayPage> &pages)
{
  QList<TabbedDisplayPage> result = pages;
  if (result.isEmpty()) {
    result.append({QStringLiteral("page-1"), QStringLiteral("Page 1"),
        QString(), QString(), false});
  }
  QSet<QString> used;
  for (int index = 0; index < result.size(); ++index) {
    TabbedDisplayPage &page = result[index];
    QString base = page.id.trimmed();
    if (base.isEmpty()) {
      base = QStringLiteral("page-%1").arg(index + 1);
    }
    QString candidate = base;
    int suffix = 2;
    while (used.contains(candidate)) {
      candidate = QStringLiteral("%1-%2").arg(base).arg(suffix++);
    }
    page.id = candidate;
    if (page.label.trimmed().isEmpty()) {
      page.label = QStringLiteral("Page %1").arg(index + 1);
    }
    used.insert(candidate);
  }
  return result;
}
