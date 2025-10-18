#include "medm_colors.h"

namespace MedmColors {

namespace {

constexpr std::array<QColor, 5> kAlarmColors = {
    QColor(0, 205, 0),   // Green3
    QColor(255, 255, 0), // Yellow
    QColor(255, 0, 0),   // Red
    QColor(255, 255, 255), // White
    QColor(204, 204, 204)}; // Gray80 fallback

} // namespace

const std::array<QColor, 65> &palette()
{
  static const std::array<QColor, 65> colors = {QColor(255, 255, 255),
      QColor(236, 236, 236), QColor(218, 218, 218), QColor(200, 200, 200),
      QColor(187, 187, 187), QColor(174, 174, 174), QColor(158, 158, 158),
      QColor(145, 145, 145), QColor(133, 133, 133), QColor(120, 120, 120),
      QColor(105, 105, 105), QColor(90, 90, 90), QColor(70, 70, 70),
      QColor(45, 45, 45), QColor(0, 0, 0), QColor(0, 216, 0),
      QColor(30, 187, 0), QColor(51, 153, 0), QColor(45, 127, 0),
      QColor(33, 108, 0), QColor(253, 0, 0), QColor(222, 19, 9),
      QColor(190, 25, 11), QColor(160, 18, 7), QColor(130, 4, 0),
      QColor(88, 147, 255), QColor(89, 126, 225), QColor(75, 110, 199),
      QColor(58, 94, 171), QColor(39, 84, 141), QColor(251, 243, 74),
      QColor(249, 218, 60), QColor(238, 182, 43), QColor(225, 144, 21),
      QColor(205, 97, 0), QColor(255, 176, 255), QColor(214, 127, 226),
      QColor(174, 78, 188), QColor(139, 26, 150), QColor(97, 10, 117),
      QColor(164, 170, 255), QColor(135, 147, 226), QColor(106, 115, 193),
      QColor(77, 82, 164), QColor(52, 51, 134), QColor(199, 187, 109),
      QColor(183, 157, 92), QColor(164, 126, 60), QColor(125, 86, 39),
      QColor(88, 52, 15), QColor(153, 255, 255), QColor(115, 223, 255),
      QColor(78, 165, 249), QColor(42, 99, 228), QColor(10, 0, 184),
      QColor(235, 241, 181), QColor(212, 219, 157), QColor(187, 193, 135),
      QColor(166, 164, 98), QColor(139, 130, 57), QColor(115, 255, 107),
      QColor(82, 218, 59), QColor(60, 180, 32), QColor(40, 147, 21),
      QColor(26, 115, 9)};
  return colors;
}

int indexForColor(const QColor &color)
{
  if (!color.isValid()) {
    return -1;
  }
  const QColor normalized = color.toRgb();
  const auto &colors = palette();
  for (int i = 0; i < static_cast<int>(colors.size()); ++i) {
    const QColor paletteColor = colors[i].toRgb();
    if (paletteColor.rgba() == normalized.rgba()) {
      return i;
    }
  }
  return -1;
}

QColor alarmColorForSeverity(short severity)
{
  int index = static_cast<int>(severity);
  if (index < 0 || index >= static_cast<int>(kAlarmColors.size() - 1)) {
    index = static_cast<int>(kAlarmColors.size() - 1);
  }
  return kAlarmColors[static_cast<std::size_t>(index)];
}

} // namespace MedmColors

