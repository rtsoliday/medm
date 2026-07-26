#include <QtTest/QtTest>

#include "legacy_fonts.h"
#include "pv_table_element.h"

// PvTableElement shares its translation unit with PvTableModel. These stubs
// satisfy element-only helpers that this model test never invokes.
bool isParentWindowInPvInfoMode(QWidget *)
{
  return false;
}

namespace LegacyFonts {
QFont fontForLegacySize(const QFont &basis, int size)
{
  QFont font = basis;
  font.setPointSize(size);
  return font;
}
} // namespace LegacyFonts

class TestPvTableModel : public QObject
{
  Q_OBJECT

private slots:
  void valueChangeUsesOneCellAndOneRole();
  void severityChangeUsesOnlySeverityDisplayAndRowForeground();
  void contiguousValueRowsUseOneRange();
};

static void initializeModel(PvTableModel &model)
{
  model.setColumns({PvTableModel::Column::kLabel,
      PvTableModel::Column::kPv, PvTableModel::Column::kValue,
      PvTableModel::Column::kUnits, PvTableModel::Column::kSeverity});
  model.setRows({{QStringLiteral("A"), QStringLiteral("pv:a")},
      {QStringLiteral("B"), QStringLiteral("pv:b")}});
}

void TestPvTableModel::valueChangeUsesOneCellAndOneRole()
{
  PvTableModel model;
  initializeModel(model);
  PvTableRuntimeRowState state;
  state.connected = true;
  state.severity = 0;
  state.valueText = QStringLiteral("1.0");
  state.units = QStringLiteral("A");
  model.setRuntimeState(0, state);

  QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
  state.valueText = QStringLiteral("2.0");
  model.setRuntimeState(0, state);

  QCOMPARE(changes.size(), 1);
  const QList<QVariant> args = changes.takeFirst();
  QCOMPARE(qvariant_cast<QModelIndex>(args.at(0)).row(), 0);
  QCOMPARE(qvariant_cast<QModelIndex>(args.at(0)).column(), 2);
  QCOMPARE(qvariant_cast<QModelIndex>(args.at(1)).column(), 2);
  QCOMPARE(args.at(2).value<QList<int>>(), QList<int>{Qt::DisplayRole});
}

void TestPvTableModel::severityChangeUsesOnlySeverityDisplayAndRowForeground()
{
  PvTableModel model;
  initializeModel(model);
  PvTableRuntimeRowState state;
  state.connected = true;
  state.severity = 0;
  model.setRuntimeState(0, state);

  QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
  state.severity = 2;
  model.setRuntimeState(0, state);

  QCOMPARE(changes.size(), 2);
  const QList<QVariant> foreground = changes.at(0);
  QCOMPARE(qvariant_cast<QModelIndex>(foreground.at(0)).column(), 0);
  QCOMPARE(qvariant_cast<QModelIndex>(foreground.at(1)).column(), 4);
  QCOMPARE(foreground.at(2).value<QList<int>>(),
      QList<int>{Qt::ForegroundRole});
  const QList<QVariant> display = changes.at(1);
  QCOMPARE(qvariant_cast<QModelIndex>(display.at(0)).column(), 4);
  QCOMPARE(qvariant_cast<QModelIndex>(display.at(1)).column(), 4);
  QCOMPARE(display.at(2).value<QList<int>>(), QList<int>{Qt::DisplayRole});
}

void TestPvTableModel::contiguousValueRowsUseOneRange()
{
  PvTableModel model;
  initializeModel(model);
  QVector<PvTableRuntimeRowState> states(2);
  states[0].valueText = QStringLiteral("10");
  states[1].valueText = QStringLiteral("20");

  QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
  model.setRuntimeValueStates(0, 1, states);

  QCOMPARE(changes.size(), 1);
  const QList<QVariant> args = changes.takeFirst();
  const QModelIndex first = qvariant_cast<QModelIndex>(args.at(0));
  const QModelIndex last = qvariant_cast<QModelIndex>(args.at(1));
  QCOMPARE(first.row(), 0);
  QCOMPARE(last.row(), 1);
  QCOMPARE(first.column(), 2);
  QCOMPARE(last.column(), 2);
  QCOMPARE(args.at(2).value<QList<int>>(), QList<int>{Qt::DisplayRole});
}

QTEST_MAIN(TestPvTableModel)

#include "test_pv_table_model.moc"
