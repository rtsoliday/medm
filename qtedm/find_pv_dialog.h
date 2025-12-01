#pragma once

#include <QDialog>
#include <QFont>
#include <QPalette>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <memory>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

struct DisplayState;
class DisplayWindow;

class FindPvDialog : public QDialog
{
  Q_OBJECT

public:
  FindPvDialog(const QPalette &basePalette, const QFont &labelFont,
      std::weak_ptr<DisplayState> state, QWidget *parent = nullptr);

  void showAndRaise();

private slots:
  void handleSearchTextChanged(const QString &text);
  void handleSearchClicked();
  void handleResultDoubleClicked(QListWidgetItem *item);
  void handleSelectAllClicked();
  void handleClearClicked();

private:
  struct SearchResult {
    QPointer<DisplayWindow> display;
    QPointer<QWidget> widget;
    QString pvName;
    QString elementType;
  };

  void performSearch();
  void updateResultsList();
  void selectResult(const SearchResult &result);
  void selectAllResults();
  QString elementTypeLabel(QWidget *widget) const;
  QStringList channelsForWidget(QWidget *widget) const;

  std::weak_ptr<DisplayState> state_;
  QLineEdit *searchEdit_ = nullptr;
  QCheckBox *caseSensitiveCheck_ = nullptr;
  QCheckBox *wildcardCheck_ = nullptr;
  QCheckBox *allDisplaysCheck_ = nullptr;
  QPushButton *searchButton_ = nullptr;
  QListWidget *resultsList_ = nullptr;
  QLabel *statusLabel_ = nullptr;
  QPushButton *selectAllButton_ = nullptr;
  QPushButton *clearButton_ = nullptr;
  QPushButton *closeButton_ = nullptr;
  QFont labelFont_;
  QVector<SearchResult> searchResults_;
};
