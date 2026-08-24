#pragma once

#include <QByteArray>
#include <QString>

class CartesianPlotElement;

enum class PlotDataExportFormat {
  kCsv,
  kSdds,
};

QByteArray serializeCartesianPlotData(const CartesianPlotElement &plot,
    PlotDataExportFormat format, QString *errorMessage = nullptr);

bool writeCartesianPlotData(const CartesianPlotElement &plot,
    const QString &path, PlotDataExportFormat format,
    QString *errorMessage = nullptr);
