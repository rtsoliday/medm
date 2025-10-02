#pragma once

#include <array>

#include <QColor>

namespace MedmColors {

const std::array<QColor, 65> &palette();
int indexForColor(const QColor &color);

} // namespace MedmColors

