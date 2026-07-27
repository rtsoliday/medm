#pragma once

#include <QDialog>
#include <QVector>

#include "pv_snapshot.h"

class QCheckBox;
class QLabel;
class QTableWidget;

struct PvSnapshotComparison
{
  PvSnapshotEntry saved;
  PvSnapshotEntry current;
  PvSnapshotRestoreCheck check;
};

class PvSnapshotRestoreDialog : public QDialog
{
public:
  explicit PvSnapshotRestoreDialog(
      const QVector<PvSnapshotComparison> &comparisons,
      QWidget *parent = nullptr);

  QVector<int> selectedRows() const;

private:
  QTableWidget *table_ = nullptr;
  QVector<QCheckBox *> checks_;
};
