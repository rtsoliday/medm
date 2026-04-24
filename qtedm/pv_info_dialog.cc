#include "pv_info_dialog.h"

#include <algorithm>
#include <cmath>

#include <QAbstractTableModel>
#include <QAbstractItemView>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTableView>
#include <QTextCursor>
#include <QTextOption>
#include <QVBoxLayout>
#include <QFontDatabase>
#include <QPalette>
#include <QTextStream>

namespace {

QString formatArrayNumber(double value, int precision)
{
  if (std::isnan(value)) {
    return QStringLiteral("nan");
  }
  if (!std::isfinite(value)) {
    return value < 0.0 ? QStringLiteral("-inf") : QStringLiteral("inf");
  }
  if (precision >= 0) {
    return QString::number(value, 'f', std::clamp(precision, 0, 17));
  }
  return QString::number(value, 'g', 12);
}

QString printableByte(unsigned char value)
{
  switch (value) {
  case 0:
    return QStringLiteral("\\0");
  case '\n':
    return QStringLiteral("\\n");
  case '\r':
    return QStringLiteral("\\r");
  case '\t':
    return QStringLiteral("\\t");
  default:
    break;
  }
  if (value >= 32 && value <= 126) {
    return QString(1, QLatin1Char(static_cast<char>(value)));
  }
  return QStringLiteral(".");
}

QString decodedByteText(const QByteArray &bytes)
{
  QByteArray payload = bytes;
  const int nullIndex = payload.indexOf('\0');
  if (nullIndex >= 0) {
    payload.truncate(nullIndex);
  }
  return QString::fromLatin1(payload.constData(), payload.size());
}

QString csvCell(QString text)
{
  text.replace(QLatin1Char('"'), QStringLiteral("\"\""));
  return QStringLiteral("\"%1\"").arg(text);
}

QString defaultCsvName(QString channelName)
{
  channelName = channelName.trimmed();
  if (channelName.isEmpty()) {
    channelName = QStringLiteral("array");
  }
  channelName.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_-]")),
      QStringLiteral("_"));
  while (channelName.startsWith(QLatin1Char('_'))) {
    channelName.remove(0, 1);
  }
  if (channelName.isEmpty()) {
    channelName = QStringLiteral("array");
  }
  return channelName + QStringLiteral(".csv");
}

} // namespace

class PvArrayTableModel : public QAbstractTableModel
{
public:
  explicit PvArrayTableModel(QObject *parent = nullptr)
    : QAbstractTableModel(parent)
  {
  }

  void setArrayData(const PvInfoArrayData &data)
  {
    beginResetModel();
    data_ = data;
    endResetModel();
  }

  int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    return parent.isValid() ? 0 : data_.elementCount();
  }

  int columnCount(const QModelIndex &parent = QModelIndex()) const override
  {
    if (parent.isValid()) {
      return 0;
    }
    return data_.isCharArray ? 4 : 2;
  }

  QVariant data(const QModelIndex &index, int role) const override
  {
    if (!index.isValid()) {
      return QVariant();
    }
    if (role == Qt::DisplayRole) {
      return cellText(index.row(), index.column());
    }
    if (role == Qt::TextAlignmentRole) {
      if (index.column() == 0 || data_.isCharArray) {
        return int(Qt::AlignRight | Qt::AlignVCenter);
      }
      return int(Qt::AlignLeft | Qt::AlignVCenter);
    }
    return QVariant();
  }

  QVariant headerData(int section, Qt::Orientation orientation,
      int role) const override
  {
    if (role != Qt::DisplayRole) {
      return QVariant();
    }
    if (orientation == Qt::Vertical) {
      return section;
    }
    return headerText(section);
  }

  QString rowsAsText(int firstRow, int lastRow, bool csv,
      bool includeHeader) const
  {
    const int rows = rowCount();
    if (rows <= 0) {
      return QString();
    }
    firstRow = std::clamp(firstRow, 0, rows - 1);
    lastRow = std::clamp(lastRow, firstRow, rows - 1);

    QString result;
    QTextStream stream(&result);
    const int columns = columnCount();
    auto writeCell = [&stream, csv](const QString &text) {
      stream << (csv ? csvCell(text) : text);
    };

    if (includeHeader) {
      for (int col = 0; col < columns; ++col) {
        if (col > 0) {
          stream << (csv ? QStringLiteral(",") : QStringLiteral("\t"));
        }
        writeCell(headerText(col));
      }
      stream << '\n';
    }

    for (int row = firstRow; row <= lastRow; ++row) {
      for (int col = 0; col < columns; ++col) {
        if (col > 0) {
          stream << (csv ? QStringLiteral(",") : QStringLiteral("\t"));
        }
        writeCell(cellText(row, col));
      }
      stream << '\n';
    }
    return result;
  }

private:
  QString headerText(int column) const
  {
    if (data_.isCharArray) {
      switch (column) {
      case 0:
        return QStringLiteral("Index");
      case 1:
        return QStringLiteral("Decimal");
      case 2:
        return QStringLiteral("Hex");
      case 3:
        return QStringLiteral("Character");
      default:
        return QString();
      }
    }
    return column == 0 ? QStringLiteral("Index") : QStringLiteral("Value");
  }

  QString cellText(int row, int column) const
  {
    if (column == 0) {
      return QString::number(row);
    }
    if (data_.isCharArray) {
      if (row < 0 || row >= data_.byteValues.size()) {
        return QString();
      }
      const unsigned char value =
          static_cast<unsigned char>(data_.byteValues.at(row));
      switch (column) {
      case 1:
        return QString::number(static_cast<int>(value));
      case 2:
        return QStringLiteral("0x%1").arg(static_cast<int>(value), 2, 16,
            QLatin1Char('0')).toUpper();
      case 3:
        return printableByte(value);
      default:
        return QString();
      }
    }
    if (data_.isStringArray) {
      return data_.stringValues.value(row);
    }
    return formatArrayNumber(data_.numericValues.value(row),
        data_.hasPrecision ? data_.precision : -1);
  }

  PvInfoArrayData data_;
};

class PvArrayValueDialog : public QDialog
{
public:
  PvArrayValueDialog(const QPalette &basePalette, const QFont &labelFont,
      const QFont &textFont, QWidget *parent = nullptr)
    : QDialog(parent)
    , model_(new PvArrayTableModel(this))
  {
    setObjectName(QStringLiteral("qtedmPvArrayValueDialog"));
    setWindowTitle(QStringLiteral("PV Array Values"));
    setModal(false);
    setAutoFillBackground(true);
    setPalette(basePalette);
    setBackgroundRole(QPalette::Window);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setSizeGripEnabled(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *channelRow = new QHBoxLayout;
    channelLabel_ = new QLabel(QStringLiteral("Channel"));
    channelLabel_->setFont(labelFont);
    channelCombo_ = new QComboBox;
    channelCombo_->setFont(textFont);
    channelRow->addWidget(channelLabel_);
    channelRow->addWidget(channelCombo_, 1);
    layout->addLayout(channelRow);

    summaryLabel_ = new QLabel;
    summaryLabel_->setFont(labelFont);
    summaryLabel_->setWordWrap(true);
    layout->addWidget(summaryLabel_);

    QFont bodyFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (bodyFont.family().isEmpty()) {
      bodyFont = textFont;
    }

    textPreview_ = new QPlainTextEdit;
    textPreview_->setReadOnly(true);
    textPreview_->setWordWrapMode(QTextOption::NoWrap);
    textPreview_->setFont(bodyFont);
    textPreview_->setMaximumHeight(100);
    textPreview_->setVisible(false);
    layout->addWidget(textPreview_);

    table_ = new QTableView;
    table_->setModel(model_);
    table_->setAlternatingRowColors(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setSortingEnabled(false);
    table_->setWordWrap(false);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(table_, 1);

    auto *jumpRow = new QHBoxLayout;
    auto *jumpLabel = new QLabel(QStringLiteral("Jump to index"));
    jumpLabel->setFont(labelFont);
    jumpSpin_ = new QSpinBox;
    jumpSpin_->setFont(textFont);
    jumpSpin_->setMinimum(0);
    jumpSpin_->setMaximum(0);
    jumpButton_ = new QPushButton(QStringLiteral("Go"));
    jumpButton_->setFont(labelFont);
    jumpRow->addWidget(jumpLabel);
    jumpRow->addWidget(jumpSpin_);
    jumpRow->addWidget(jumpButton_);
    jumpRow->addStretch(1);
    layout->addLayout(jumpRow);

    auto *buttonBox = new QDialogButtonBox;
    copyVisibleButton_ = buttonBox->addButton(QStringLiteral("Copy Visible"),
        QDialogButtonBox::ActionRole);
    copyAllButton_ = buttonBox->addButton(QStringLiteral("Copy All"),
        QDialogButtonBox::ActionRole);
    saveCsvButton_ = buttonBox->addButton(QStringLiteral("Save CSV"),
        QDialogButtonBox::ActionRole);
    closeButton_ = buttonBox->addButton(QDialogButtonBox::Close);
    copyVisibleButton_->setFont(labelFont);
    copyAllButton_->setFont(labelFont);
    saveCsvButton_->setFont(labelFont);
    closeButton_->setFont(labelFont);
    layout->addWidget(buttonBox);

    connect(channelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) { updateCurrentArray(index); });
    connect(jumpButton_, &QPushButton::clicked, this, [this]() {
      const int row = jumpSpin_->value();
      if (row >= 0 && row < model_->rowCount()) {
        const QModelIndex target = model_->index(row, 0);
        table_->scrollTo(target, QAbstractItemView::PositionAtCenter);
        table_->selectRow(row);
      }
    });
    connect(copyVisibleButton_, &QPushButton::clicked, this,
        [this]() { copyVisibleRows(); });
    connect(copyAllButton_, &QPushButton::clicked, this,
        [this]() { copyAllRows(); });
    connect(saveCsvButton_, &QPushButton::clicked, this,
        [this]() { saveCsv(); });
    connect(closeButton_, &QPushButton::clicked, this, &QDialog::hide);

    resize(640, 520);
  }

  void setArrays(const QVector<PvInfoArrayData> &arrays)
  {
    arrays_ = arrays;
    channelCombo_->blockSignals(true);
    channelCombo_->clear();
    for (const PvInfoArrayData &array : arrays_) {
      const QString label = array.channelName.isEmpty()
          ? QStringLiteral("(unnamed array)")
          : array.channelName;
      channelCombo_->addItem(label);
    }
    channelCombo_->blockSignals(false);

    const bool multiple = arrays_.size() > 1;
    channelLabel_->setVisible(multiple);
    channelCombo_->setVisible(multiple);
    if (!arrays_.isEmpty()) {
      channelCombo_->setCurrentIndex(0);
      updateCurrentArray(0);
    } else {
      model_->setArrayData(PvInfoArrayData{});
      summaryLabel_->clear();
      textPreview_->hide();
      updateButtons();
    }
  }

private:
  void updateCurrentArray(int index)
  {
    if (index < 0 || index >= arrays_.size()) {
      return;
    }
    const PvInfoArrayData &array = arrays_.at(index);
    model_->setArrayData(array);

    const int count = array.elementCount();
    QStringList parts;
    parts << QStringLiteral("Type: %1")
        .arg(array.typeName.isEmpty() ? QStringLiteral("Unknown")
                                      : array.typeName);
    parts << QStringLiteral("Count: %1").arg(count);
    if (array.hasUnits) {
      parts << QStringLiteral("Units: %1").arg(array.units);
    }
    if (array.hasPrecision) {
      parts << QStringLiteral("Precision: %1").arg(array.precision);
    }
    summaryLabel_->setText(parts.join(QStringLiteral("  ")));

    if (array.isCharArray) {
      const QString text = array.textValue.isEmpty()
          ? decodedByteText(array.byteValues)
          : array.textValue;
      textPreview_->setPlainText(text);
      textPreview_->setVisible(true);
    } else {
      textPreview_->clear();
      textPreview_->setVisible(false);
    }

    jumpSpin_->setMaximum(std::max(0, count - 1));
    jumpSpin_->setEnabled(count > 0);
    jumpButton_->setEnabled(count > 0);
    table_->resizeColumnsToContents();
    updateButtons();
  }

  void updateButtons()
  {
    const bool hasRows = model_->rowCount() > 0;
    copyVisibleButton_->setEnabled(hasRows);
    copyAllButton_->setEnabled(hasRows);
    saveCsvButton_->setEnabled(hasRows);
  }

  void copyText(const QString &text)
  {
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
      clipboard->setText(text, QClipboard::Clipboard);
      clipboard->setText(text, QClipboard::Selection);
    }
  }

  void copyVisibleRows()
  {
    const int rows = model_->rowCount();
    if (rows <= 0) {
      return;
    }
    int first = table_->rowAt(0);
    int last = table_->rowAt(table_->viewport()->height() - 1);
    if (first < 0) {
      first = 0;
    }
    if (last < 0) {
      last = rows - 1;
    }
    copyText(model_->rowsAsText(first, last, false, true));
  }

  void copyAllRows()
  {
    const int rows = model_->rowCount();
    if (rows <= 0) {
      return;
    }
    copyText(model_->rowsAsText(0, rows - 1, false, true));
  }

  void saveCsv()
  {
    const int rows = model_->rowCount();
    if (rows <= 0 || arrays_.isEmpty()) {
      return;
    }
    const int current = std::clamp(channelCombo_->currentIndex(), 0,
        arrays_.size() - 1);
    QString fileName = QFileDialog::getSaveFileName(this,
        QStringLiteral("Save Array CSV"),
        QDir::currentPath() + QDir::separator()
            + defaultCsvName(arrays_.at(current).channelName),
        QStringLiteral("CSV File (*.csv)"));
    if (fileName.isEmpty()) {
      return;
    }
    if (QFileInfo(fileName).suffix().isEmpty()) {
      fileName += QStringLiteral(".csv");
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QMessageBox::warning(this, QStringLiteral("Save Failed"),
          QStringLiteral("Failed to open file for writing:\n%1")
              .arg(fileName));
      return;
    }
    QTextStream stream(&file);
    stream << model_->rowsAsText(0, rows - 1, true, true);
  }

  QVector<PvInfoArrayData> arrays_;
  PvArrayTableModel *model_ = nullptr;
  QLabel *channelLabel_ = nullptr;
  QComboBox *channelCombo_ = nullptr;
  QLabel *summaryLabel_ = nullptr;
  QPlainTextEdit *textPreview_ = nullptr;
  QTableView *table_ = nullptr;
  QSpinBox *jumpSpin_ = nullptr;
  QPushButton *jumpButton_ = nullptr;
  QPushButton *copyVisibleButton_ = nullptr;
  QPushButton *copyAllButton_ = nullptr;
  QPushButton *saveCsvButton_ = nullptr;
  QPushButton *closeButton_ = nullptr;
};

PvInfoDialog::PvInfoDialog(const QPalette &basePalette,
    const QFont &labelFont, const QFont &textFont, QWidget *parent)
  : QDialog(parent)
{
  setObjectName(QStringLiteral("qtedmPvInfoDialog"));
  setWindowTitle(QStringLiteral("PV Info"));
  setModal(false);
  setAutoFillBackground(true);
  setPalette(basePalette);
  setBackgroundRole(QPalette::Window);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setSizeGripEnabled(true);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(10);

  QFont bodyFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  if (bodyFont.family().isEmpty()) {
    bodyFont = textFont;
  }

  textEdit_ = new QPlainTextEdit;
  textEdit_->setReadOnly(true);
  textEdit_->setWordWrapMode(QTextOption::NoWrap);
  textEdit_->setFont(bodyFont);
  textEdit_->setAutoFillBackground(true);
  textEdit_->setPalette(basePalette);
  layout->addWidget(textEdit_);

  auto *buttonBox = new QDialogButtonBox;
  arrayButton_ = buttonBox->addButton(QStringLiteral("View Array..."),
      QDialogButtonBox::ActionRole);
  closeButton_ = buttonBox->addButton(QDialogButtonBox::Close);
  helpButton_ = buttonBox->addButton(QStringLiteral("Help"),
      QDialogButtonBox::HelpRole);
  arrayButton_->setFont(labelFont);
  arrayButton_->setEnabled(false);
  closeButton_->setFont(labelFont);
  helpButton_->setFont(labelFont);
  layout->addWidget(buttonBox);

  connect(arrayButton_, &QPushButton::clicked, this,
      [this]() { showArrayDialog(); });
  connect(closeButton_, &QPushButton::clicked, this, &QDialog::hide);
  connect(helpButton_, &QPushButton::clicked, this, [this]() {
    QMessageBox::information(this, windowTitle(),
        QStringLiteral("Displays detailed information about the IOC-backed "
                       "channels or local soft PVs associated with the object "
                       "under the cursor."));
  });

  resize(350, 420);
}

void PvInfoDialog::setContent(const QString &text,
    const QVector<PvInfoArrayData> &arrays)
{
  if (!textEdit_) {
    return;
  }
  arrays_ = arrays;
  textEdit_->setPlainText(text);
  textEdit_->moveCursor(QTextCursor::Start);
  if (arrayButton_) {
    arrayButton_->setEnabled(!arrays_.isEmpty());
    arrayButton_->setText(arrays_.size() == 1
        ? QStringLiteral("View Array...")
        : QStringLiteral("View Arrays..."));
  }
  if (arrayDialog_) {
    arrayDialog_->setArrays(arrays_);
  }
}

void PvInfoDialog::showArrayDialog()
{
  if (arrays_.isEmpty()) {
    return;
  }
  if (!arrayDialog_) {
    arrayDialog_ = new PvArrayValueDialog(palette(), font(), textEdit_->font(),
        this);
  }
  arrayDialog_->setArrays(arrays_);
  arrayDialog_->show();
  arrayDialog_->raise();
  arrayDialog_->activateWindow();
}
