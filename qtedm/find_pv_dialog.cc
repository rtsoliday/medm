#include "find_pv_dialog.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include "adl_writer.h"
#include "arc_element.h"
#include "bar_monitor_element.h"
#include "byte_monitor_element.h"
#include "cartesian_plot_element.h"
#include "choice_button_element.h"
#include "composite_element.h"
#include "display_state.h"
#include "display_window.h"
#include "image_element.h"
#include "line_element.h"
#include "menu_element.h"
#include "message_button_element.h"
#include "meter_element.h"
#include "oval_element.h"
#include "polygon_element.h"
#include "polyline_element.h"
#include "rectangle_element.h"
#include "related_display_element.h"
#include "scale_monitor_element.h"
#include "shell_command_element.h"
#include "slider_element.h"
#include "strip_chart_element.h"
#include "text_element.h"
#include "text_entry_element.h"
#include "text_monitor_element.h"
#include "wheel_switch_element.h"

namespace {

class SearchResultItem : public QListWidgetItem
{
public:
  SearchResultItem(const QString &text, int resultIndex, const QFont &font)
    : QListWidgetItem(text)
    , resultIndex_(resultIndex)
  {
    setFont(font);
  }

  int resultIndex() const { return resultIndex_; }

private:
  int resultIndex_ = -1;
};

} // namespace

FindPvDialog::FindPvDialog(const QPalette &basePalette, const QFont &labelFont,
    std::weak_ptr<DisplayState> state, QWidget *parent)
  : QDialog(parent)
  , state_(std::move(state))
  , labelFont_(labelFont)
{
  setObjectName(QStringLiteral("qtedmFindPvDialog"));
  setWindowTitle(QStringLiteral("Find PV"));
  setModal(false);
  setAutoFillBackground(true);
  setPalette(basePalette);
  setBackgroundRole(QPalette::Window);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setSizeGripEnabled(true);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(12, 12, 12, 12);
  mainLayout->setSpacing(10);

  /* Search input section */
  auto *searchGroup = new QGroupBox(QStringLiteral("Search"));
  searchGroup->setFont(labelFont_);
  auto *searchLayout = new QGridLayout(searchGroup);
  searchLayout->setContentsMargins(10, 14, 10, 10);
  searchLayout->setSpacing(8);

  auto *pvLabel = new QLabel(QStringLiteral("PV Name:"));
  pvLabel->setFont(labelFont_);
  searchLayout->addWidget(pvLabel, 0, 0);

  searchEdit_ = new QLineEdit;
  searchEdit_->setFont(labelFont_);
  searchEdit_->setPlaceholderText(QStringLiteral("Enter PV name or pattern..."));
  searchEdit_->setClearButtonEnabled(true);
  searchLayout->addWidget(searchEdit_, 0, 1);

  searchButton_ = new QPushButton(QStringLiteral("Search"));
  searchButton_->setFont(labelFont_);
  searchButton_->setDefault(true);
  searchLayout->addWidget(searchButton_, 0, 2);

  caseSensitiveCheck_ = new QCheckBox(QStringLiteral("Case sensitive"));
  caseSensitiveCheck_->setFont(labelFont_);
  searchLayout->addWidget(caseSensitiveCheck_, 1, 1);

  wildcardCheck_ = new QCheckBox(QStringLiteral("Use wildcards (* and ?)"));
  wildcardCheck_->setFont(labelFont_);
  wildcardCheck_->setChecked(true);
  searchLayout->addWidget(wildcardCheck_, 2, 1);

  allDisplaysCheck_ = new QCheckBox(QStringLiteral("Search all open displays"));
  allDisplaysCheck_->setFont(labelFont_);
  allDisplaysCheck_->setChecked(true);
  searchLayout->addWidget(allDisplaysCheck_, 3, 1);

  mainLayout->addWidget(searchGroup);

  /* Results section */
  auto *resultsGroup = new QGroupBox(QStringLiteral("Results"));
  resultsGroup->setFont(labelFont_);
  auto *resultsLayout = new QVBoxLayout(resultsGroup);
  resultsLayout->setContentsMargins(10, 14, 10, 10);
  resultsLayout->setSpacing(8);

  resultsList_ = new QListWidget;
  resultsList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  resultsList_->setFont(labelFont_);
  resultsList_->setAutoFillBackground(true);
  resultsList_->setPalette(basePalette);
  resultsList_->setMinimumHeight(200);
  resultsLayout->addWidget(resultsList_);

  statusLabel_ = new QLabel;
  statusLabel_->setFont(labelFont_);
  resultsLayout->addWidget(statusLabel_);

  mainLayout->addWidget(resultsGroup, 1);

  /* Button row */
  auto *buttonRow = new QHBoxLayout;
  buttonRow->setContentsMargins(0, 0, 0, 0);
  buttonRow->setSpacing(8);

  selectAllButton_ = new QPushButton(QStringLiteral("Select All Results"));
  selectAllButton_->setFont(labelFont_);
  selectAllButton_->setEnabled(false);
  buttonRow->addWidget(selectAllButton_);

  clearButton_ = new QPushButton(QStringLiteral("Clear"));
  clearButton_->setFont(labelFont_);
  buttonRow->addWidget(clearButton_);

  buttonRow->addStretch();

  closeButton_ = new QPushButton(QStringLiteral("Close"));
  closeButton_->setFont(labelFont_);
  buttonRow->addWidget(closeButton_);

  mainLayout->addLayout(buttonRow);

  /* Connections */
  connect(searchEdit_, &QLineEdit::textChanged, this,
      &FindPvDialog::handleSearchTextChanged);
  connect(searchEdit_, &QLineEdit::returnPressed, this,
      &FindPvDialog::handleSearchClicked);
  connect(searchButton_, &QPushButton::clicked, this,
      &FindPvDialog::handleSearchClicked);
  connect(resultsList_, &QListWidget::itemDoubleClicked, this,
      &FindPvDialog::handleResultDoubleClicked);
  connect(selectAllButton_, &QPushButton::clicked, this,
      &FindPvDialog::handleSelectAllClicked);
  connect(clearButton_, &QPushButton::clicked, this,
      &FindPvDialog::handleClearClicked);
  connect(closeButton_, &QPushButton::clicked, this, &QDialog::hide);

  resize(500, 450);
}

void FindPvDialog::showAndRaise()
{
  show();
  raise();
  activateWindow();
  searchEdit_->setFocus();
  searchEdit_->selectAll();
}

void FindPvDialog::handleSearchTextChanged(const QString &text)
{
  searchButton_->setEnabled(!text.trimmed().isEmpty());
}

void FindPvDialog::handleSearchClicked()
{
  performSearch();
}

void FindPvDialog::handleResultDoubleClicked(QListWidgetItem *item)
{
  auto *resultItem = dynamic_cast<SearchResultItem *>(item);
  if (!resultItem) {
    return;
  }

  const int index = resultItem->resultIndex();
  if (index >= 0 && index < searchResults_.size()) {
    selectResult(searchResults_.at(index));
  }
}

void FindPvDialog::handleSelectAllClicked()
{
  selectAllResults();
}

void FindPvDialog::handleClearClicked()
{
  searchEdit_->clear();
  resultsList_->clear();
  searchResults_.clear();
  statusLabel_->clear();
  selectAllButton_->setEnabled(false);
}

void FindPvDialog::performSearch()
{
  const QString searchText = searchEdit_->text().trimmed();
  if (searchText.isEmpty()) {
    return;
  }

  searchResults_.clear();
  resultsList_->clear();

  auto state = state_.lock();
  if (!state) {
    statusLabel_->setText(QStringLiteral("No displays available."));
    selectAllButton_->setEnabled(false);
    return;
  }

  /* Build search pattern */
  QString pattern = searchText;
  if (wildcardCheck_->isChecked()) {
    /* Convert wildcards to regex */
    pattern = QRegularExpression::escape(pattern);
    pattern.replace(QStringLiteral("\\*"), QStringLiteral(".*"));
    pattern.replace(QStringLiteral("\\?"), QStringLiteral("."));
  } else {
    pattern = QRegularExpression::escape(pattern);
  }

  QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
  if (!caseSensitiveCheck_->isChecked()) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }

  QRegularExpression regex(QStringLiteral("^%1$").arg(pattern), options);
  if (!regex.isValid()) {
    statusLabel_->setText(QStringLiteral("Invalid search pattern."));
    selectAllButton_->setEnabled(false);
    return;
  }

  /* Determine which displays to search */
  QList<DisplayWindow *> displaysToSearch;
  if (allDisplaysCheck_->isChecked()) {
    for (const auto &displayPtr : state->displays) {
      if (!displayPtr.isNull()) {
        displaysToSearch.append(displayPtr.data());
      }
    }
  } else if (!state->activeDisplay.isNull()) {
    displaysToSearch.append(state->activeDisplay.data());
  }

  /* Search all widgets in each display */
  for (DisplayWindow *display : displaysToSearch) {
    if (!display) {
      continue;
    }

    /* Get all widgets from the display */
    QList<QWidget *> widgets = display->findPvWidgets();

    for (QWidget *widget : widgets) {
      if (!widget) {
        continue;
      }

      QStringList channels = channelsForWidget(widget);
      for (const QString &channel : channels) {
        QRegularExpressionMatch match = regex.match(channel);
        if (match.hasMatch()) {
          SearchResult result;
          result.display = display;
          result.widget = widget;
          result.pvName = channel;
          result.elementType = elementTypeLabel(widget);
          searchResults_.append(result);
        }
      }
    }
  }

  updateResultsList();
}

void FindPvDialog::updateResultsList()
{
  resultsList_->clear();

  for (int i = 0; i < searchResults_.size(); ++i) {
    const SearchResult &result = searchResults_.at(i);
    QString displayName;
    if (auto display = result.display.data()) {
      displayName = display->windowTitle();
      if (displayName.isEmpty()) {
        displayName = QStringLiteral("(untitled)");
      }
    } else {
      displayName = QStringLiteral("(closed)");
    }

    QString text = QStringLiteral("%1 - %2 [%3]")
        .arg(result.pvName, result.elementType, displayName);

    auto *item = new SearchResultItem(text, i, labelFont_);
    resultsList_->addItem(item);
  }

  const int count = searchResults_.size();
  if (count == 0) {
    statusLabel_->setText(QStringLiteral("No matching PVs found."));
  } else if (count == 1) {
    statusLabel_->setText(QStringLiteral("Found 1 matching PV."));
  } else {
    statusLabel_->setText(QStringLiteral("Found %1 matching PVs.").arg(count));
  }

  selectAllButton_->setEnabled(count > 0);
}

void FindPvDialog::selectResult(const SearchResult &result)
{
  auto display = result.display.data();
  auto widget = result.widget.data();
  if (!display || !widget) {
    return;
  }

  /* Raise the display window */
  display->show();
  display->raise();
  display->activateWindow();

  /* Select the widget and scroll to it */
  display->selectAndScrollToWidget(widget);
}

void FindPvDialog::selectAllResults()
{
  if (searchResults_.isEmpty()) {
    return;
  }

  /* Group results by display */
  QHash<DisplayWindow *, QList<QWidget *>> widgetsByDisplay;
  for (const SearchResult &result : searchResults_) {
    auto display = result.display.data();
    auto widget = result.widget.data();
    if (display && widget) {
      if (!widgetsByDisplay[display].contains(widget)) {
        widgetsByDisplay[display].append(widget);
      }
    }
  }

  /* Select widgets in each display */
  for (auto it = widgetsByDisplay.begin(); it != widgetsByDisplay.end(); ++it) {
    DisplayWindow *display = it.key();
    const QList<QWidget *> &widgets = it.value();

    if (display && !widgets.isEmpty()) {
      display->show();
      display->raise();
      display->selectWidgets(widgets);
    }
  }
}

QString FindPvDialog::elementTypeLabel(QWidget *widget) const
{
  if (!widget) {
    return QStringLiteral("Unknown");
  }
  if (dynamic_cast<TextElement *>(widget)) {
    return QStringLiteral("Text");
  }
  if (dynamic_cast<TextMonitorElement *>(widget)) {
    return QStringLiteral("Text Monitor");
  }
  if (dynamic_cast<TextEntryElement *>(widget)) {
    return QStringLiteral("Text Entry");
  }
  if (dynamic_cast<SliderElement *>(widget)) {
    return QStringLiteral("Slider");
  }
  if (dynamic_cast<WheelSwitchElement *>(widget)) {
    return QStringLiteral("Wheel Switch");
  }
  if (dynamic_cast<ChoiceButtonElement *>(widget)) {
    return QStringLiteral("Choice Button");
  }
  if (dynamic_cast<MenuElement *>(widget)) {
    return QStringLiteral("Menu");
  }
  if (dynamic_cast<MessageButtonElement *>(widget)) {
    return QStringLiteral("Message Button");
  }
  if (dynamic_cast<ShellCommandElement *>(widget)) {
    return QStringLiteral("Shell Command");
  }
  if (dynamic_cast<RelatedDisplayElement *>(widget)) {
    return QStringLiteral("Related Display");
  }
  if (dynamic_cast<MeterElement *>(widget)) {
    return QStringLiteral("Meter");
  }
  if (dynamic_cast<BarMonitorElement *>(widget)) {
    return QStringLiteral("Bar Monitor");
  }
  if (dynamic_cast<ScaleMonitorElement *>(widget)) {
    return QStringLiteral("Scale Monitor");
  }
  if (dynamic_cast<ByteMonitorElement *>(widget)) {
    return QStringLiteral("Byte Monitor");
  }
  if (dynamic_cast<StripChartElement *>(widget)) {
    return QStringLiteral("Strip Chart");
  }
  if (dynamic_cast<CartesianPlotElement *>(widget)) {
    return QStringLiteral("Cartesian Plot");
  }
  if (dynamic_cast<RectangleElement *>(widget)) {
    return QStringLiteral("Rectangle");
  }
  if (dynamic_cast<ImageElement *>(widget)) {
    return QStringLiteral("Image");
  }
  if (dynamic_cast<OvalElement *>(widget)) {
    return QStringLiteral("Oval");
  }
  if (dynamic_cast<ArcElement *>(widget)) {
    return QStringLiteral("Arc");
  }
  if (dynamic_cast<LineElement *>(widget)) {
    return QStringLiteral("Line");
  }
  if (dynamic_cast<PolylineElement *>(widget)) {
    return QStringLiteral("Polyline");
  }
  if (dynamic_cast<PolygonElement *>(widget)) {
    return QStringLiteral("Polygon");
  }
  if (dynamic_cast<CompositeElement *>(widget)) {
    return QStringLiteral("Composite");
  }
  return QStringLiteral("Unknown");
}

QStringList FindPvDialog::channelsForWidget(QWidget *widget) const
{
  QStringList channels;
  if (!widget) {
    return channels;
  }

  auto appendChannel = [&](const QString &channel) {
    const QString trimmed = channel.trimmed();
    if (trimmed.isEmpty()) {
      return;
    }
    if (!channels.contains(trimmed)) {
      channels.append(trimmed);
    }
  };

  auto appendChannelArray = [&](const auto &array) {
    for (const QString &value : array) {
      appendChannel(value);
    }
  };

  if (auto *element = dynamic_cast<TextElement *>(widget)) {
    appendChannelArray(AdlWriter::collectChannels(element));
  } else if (auto *element = dynamic_cast<TextMonitorElement *>(widget)) {
    appendChannelArray(AdlWriter::collectChannels(element));
  } else if (auto *element = dynamic_cast<TextEntryElement *>(widget)) {
    appendChannel(element->channel());
  } else if (auto *element = dynamic_cast<SliderElement *>(widget)) {
    appendChannel(element->channel());
  } else if (auto *element = dynamic_cast<WheelSwitchElement *>(widget)) {
    appendChannel(element->channel());
  } else if (auto *element = dynamic_cast<ChoiceButtonElement *>(widget)) {
    appendChannel(element->channel());
  } else if (auto *element = dynamic_cast<MenuElement *>(widget)) {
    appendChannel(element->channel());
  } else if (auto *element = dynamic_cast<MessageButtonElement *>(widget)) {
    appendChannel(element->channel());
  } else if (auto *element = dynamic_cast<MeterElement *>(widget)) {
    appendChannel(element->channel());
  } else if (auto *element = dynamic_cast<BarMonitorElement *>(widget)) {
    appendChannel(element->channel());
  } else if (auto *element = dynamic_cast<ScaleMonitorElement *>(widget)) {
    appendChannel(element->channel());
  } else if (auto *element = dynamic_cast<ByteMonitorElement *>(widget)) {
    appendChannel(element->channel());
  } else if (auto *element = dynamic_cast<RectangleElement *>(widget)) {
    appendChannelArray(AdlWriter::collectChannels(element));
  } else if (auto *element = dynamic_cast<ImageElement *>(widget)) {
    appendChannelArray(AdlWriter::collectChannels(element));
  } else if (auto *element = dynamic_cast<OvalElement *>(widget)) {
    appendChannelArray(AdlWriter::collectChannels(element));
  } else if (auto *element = dynamic_cast<ArcElement *>(widget)) {
    appendChannelArray(AdlWriter::collectChannels(element));
  } else if (auto *element = dynamic_cast<LineElement *>(widget)) {
    appendChannelArray(AdlWriter::collectChannels(element));
  } else if (auto *element = dynamic_cast<PolylineElement *>(widget)) {
    appendChannelArray(AdlWriter::collectChannels(element));
  } else if (auto *element = dynamic_cast<PolygonElement *>(widget)) {
    appendChannelArray(AdlWriter::collectChannels(element));
  } else if (auto *element = dynamic_cast<CompositeElement *>(widget)) {
    appendChannelArray(element->channels());
  } else if (auto *element = dynamic_cast<StripChartElement *>(widget)) {
    const int penCount = element->penCount();
    for (int i = 0; i < penCount; ++i) {
      appendChannel(element->channel(i));
    }
  } else if (auto *element = dynamic_cast<CartesianPlotElement *>(widget)) {
    appendChannel(element->triggerChannel());
    appendChannel(element->eraseChannel());
    appendChannel(element->countChannel());
    const int traceCount = element->traceCount();
    for (int i = 0; i < traceCount; ++i) {
      appendChannel(element->traceXChannel(i));
      appendChannel(element->traceYChannel(i));
    }
  }

  return channels;
}

#include "moc_find_pv_dialog.cpp"
