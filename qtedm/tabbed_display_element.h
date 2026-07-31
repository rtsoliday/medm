#pragma once

#include <functional>

#include <QList>
#include <QPointer>
#include <QString>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;
class QStackedWidget;
class QTabBar;

struct TabbedDisplayPage
{
  QString id;
  QString label;
  QString displayPath;
  QString macros;
  bool keepAlive = false;
};

class TabbedDisplayElement : public QWidget
{
public:
  using PageLoader = std::function<QWidget *(
      const TabbedDisplayPage &, QWidget *, QString *)>;

  explicit TabbedDisplayElement(QWidget *parent = nullptr);
  ~TabbedDisplayElement() override;

  QList<TabbedDisplayPage> pages() const;
  void setPages(const QList<TabbedDisplayPage> &pages);

  bool hiddenTabs() const;
  void setHiddenTabs(bool hidden);

  QString activePageId() const;
  int activePageIndex() const;
  bool setActivePageId(const QString &id);
  void setActivePageIndex(int index);

  void setPageLoader(PageLoader loader);
  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setSelected(bool selected);
  bool isSelected() const;

  void setChangedCallback(std::function<void()> callback);

  // Opens the same page editor used by an edit-mode double-click.  Exposing
  // this lets the Resource Palette provide the conventional properties entry
  // point used by other QtEDM objects.
  void showPageEditor();

  int loadedPageCount() const;
  QWidget *pageContent(int index) const;
  QString pageDiagnostic(int index) const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
  struct PageRuntime
  {
    QWidget *host = nullptr;
    QPointer<QWidget> content;
    QString diagnostic;
  };

  void rebuildPages();
  void activatePage(int index);
  void loadPage(int index);
  void unloadPage(int index);
  void updatePlaceholders();
  void updateEditInteraction();
  bool forwardMouseEventToParent(QMouseEvent *event) const;
  static QList<TabbedDisplayPage> normalizedPages(
      const QList<TabbedDisplayPage> &pages);

  QTabBar *tabBar_ = nullptr;
  QStackedWidget *stack_ = nullptr;
  QList<TabbedDisplayPage> pages_;
  QList<PageRuntime> runtimes_;
  PageLoader loader_;
  std::function<void()> changedCallback_;
  bool hiddenTabs_ = false;
  bool executeMode_ = false;
  bool selected_ = false;
  int previousIndex_ = -1;
};
