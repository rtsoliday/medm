#include <QtTest/QtTest>

#include "wave_table_model.h"

class TestWaveTableModel : public QObject
{
  Q_OBJECT

private slots:
  void mapsGridCellsAndHeaders();
  void mapsColumnCellsWithOneBasedIndex();
  void showsStatusWhenEmpty();
};

void TestWaveTableModel::mapsGridCellsAndHeaders()
{
  WaveTableModel model;
  WaveTableRuntimeState state;
  state.connected = true;
  state.severity = 0;
  state.nativeElementCount = 6;
  state.receivedElementCount = 6;
  model.setRuntimeState(state);
  model.setLayout(WaveTableLayout::kGrid);
  model.setColumnCount(3);
  model.setIndexBase(0);
  model.setValues({QStringLiteral("0"), QStringLiteral("1"),
      QStringLiteral("2"), QStringLiteral("3"), QStringLiteral("4"),
      QStringLiteral("5")});

  QCOMPARE(model.rowCount(), 2);
  QCOMPARE(model.columnCount(), 3);
  QCOMPARE(model.cellText(0, 0), QStringLiteral("0"));
  QCOMPARE(model.cellText(1, 2), QStringLiteral("5"));
  QCOMPARE(model.headerData(2, Qt::Horizontal).toString(),
      QStringLiteral("2"));
  QCOMPARE(model.headerData(1, Qt::Vertical).toString(),
      QStringLiteral("3-5"));
}

void TestWaveTableModel::mapsColumnCellsWithOneBasedIndex()
{
  WaveTableModel model;
  model.setLayout(WaveTableLayout::kColumn);
  model.setIndexBase(1);
  model.setValues({QStringLiteral("A"), QStringLiteral("B"),
      QStringLiteral("C")});

  QCOMPARE(model.rowCount(), 3);
  QCOMPARE(model.columnCount(), 2);
  QCOMPARE(model.headerData(0, Qt::Horizontal).toString(),
      QStringLiteral("Index"));
  QCOMPARE(model.headerData(1, Qt::Horizontal).toString(),
      QStringLiteral("Value"));
  QCOMPARE(model.cellText(0, 0), QStringLiteral("1"));
  QCOMPARE(model.cellText(2, 1), QStringLiteral("C"));
}

void TestWaveTableModel::showsStatusWhenEmpty()
{
  WaveTableModel model;
  WaveTableRuntimeState state;
  state.connected = false;
  state.nativeElementCount = 64;
  state.receivedElementCount = 0;
  model.setRuntimeState(state);
  model.setValues({});

  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.columnCount(), 8);
  QVERIFY(model.cellText(0, 0).startsWith(QStringLiteral("Disconnected")));
}

QTEST_APPLESS_MAIN(TestWaveTableModel)

#include "test_wave_table_model.moc"
