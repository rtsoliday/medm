#include "pv_snapshot_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

PvSnapshotRestoreDialog::PvSnapshotRestoreDialog(
    const QVector<PvSnapshotComparison> &comparisons, QWidget *parent)
  : QDialog(parent)
{
  setWindowTitle(QStringLiteral("Compare / Restore PV Snapshot"));
  setModal(true);
  resize(1050, 520);
  auto *layout = new QVBoxLayout(this);
  auto *note = new QLabel(QStringLiteral(
      "No values are selected by default. Select only the PVs that should "
      "be restored; connection, type, access, limits, and observe-only "
      "policy are checked again immediately before every write."), this);
  note->setWordWrap(true);
  layout->addWidget(note);

  table_ = new QTableWidget(comparisons.size(), 6, this);
  table_->setHorizontalHeaderLabels({
      QStringLiteral("Restore"), QStringLiteral("PV"),
      QStringLiteral("Saved"), QStringLiteral("Current"),
      QStringLiteral("Type"), QStringLiteral("Status")});
  table_->verticalHeader()->setVisible(false);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
  checks_.reserve(comparisons.size());
  for (int row = 0; row < comparisons.size(); ++row) {
    const PvSnapshotComparison &comparison = comparisons.at(row);
    auto *check = new QCheckBox(table_);
    check->setChecked(false);
    check->setEnabled(comparison.check.allowed);
    table_->setCellWidget(row, 0, check);
    checks_.append(check);
    table_->setItem(row, 1,
        new QTableWidgetItem(comparison.saved.pvName));
    table_->setItem(row, 2,
        new QTableWidgetItem(comparison.saved.displayValue()));
    table_->setItem(row, 3,
        new QTableWidgetItem(comparison.current.displayValue()));
    table_->setItem(row, 4,
        new QTableWidgetItem(comparison.saved.exactType));
    table_->setItem(row, 5,
        new QTableWidgetItem(comparison.check.reason));
  }
  layout->addWidget(table_);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setText(
      QStringLiteral("Continue"));
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
}

QVector<int> PvSnapshotRestoreDialog::selectedRows() const
{
  QVector<int> rows;
  for (int index = 0; index < checks_.size(); ++index) {
    if (checks_.at(index) && checks_.at(index)->isChecked()
        && checks_.at(index)->isEnabled()) {
      rows.append(index);
    }
  }
  return rows;
}
