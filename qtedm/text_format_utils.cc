#include "text_format_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <QByteArray>

#include <cvtFast.h>

namespace TextFormatUtils {

namespace {

/* Match MEDM's localCvtDoubleToString behavior used by engineering notation. */
void localCvtDoubleToString(double value, char *textField,
    unsigned short precision)
{
  if (!textField) {
    return;
  }
  if (precision > kMaxPrecision) {
    precision = kMaxPrecision;
  }
  std::snprintf(textField, kMaxTextField, "%.*f",
      static_cast<int>(precision), value);
}

} // namespace

int clampPrecision(int precision)
{
  if (precision < 0) {
    return 0;
  }
  if (precision > kMaxPrecision) {
    return kMaxPrecision;
  }
  return precision;
}

QString formatNonFinite(double value)
{
  if (std::isnan(value)) {
    return QStringLiteral("NaN");
  }
  return std::signbit(value) ? QStringLiteral("-Inf")
                             : QStringLiteral("+Inf");
}

long saturatedLongFromDouble(double value, bool roundToNearest)
{
  if (std::isnan(value)) {
    return 0;
  }
  if (value <= static_cast<double>(std::numeric_limits<long>::min())) {
    return std::numeric_limits<long>::min();
  }
  if (value >= static_cast<double>(std::numeric_limits<long>::max())) {
    return std::numeric_limits<long>::max();
  }
  const double converted = roundToNearest ? std::round(value)
                                          : std::trunc(value);
  return static_cast<long>(converted);
}

/* Format value with engineering notation (powers of 10 in multiples of 3). */
void localCvtDoubleToExpNotationString(double value, char *textField,
    unsigned short precision)
{
  if (!textField) {
    return;
  }

  if (!std::isfinite(value)) {
    const QByteArray text = formatNonFinite(value).toLatin1();
    std::snprintf(textField, kMaxTextField, "%s", text.constData());
    return;
  }

  double absVal = std::fabs(value);
  bool isNegative = value < 0.0;
  double scaled = absVal;
  int exponent = 0;
  char buffer[kMaxTextField];

  if (absVal < 1.0) {
    if (absVal != 0.0) {
      while (scaled < 1.0) {
        scaled *= 1000.0;
        exponent -= 3;
      }
    }
  } else {
    while (scaled >= 1000.0) {
      scaled *= 0.001;
      exponent += 3;
    }
  }

  localCvtDoubleToString(scaled, buffer, precision);
  QByteArray formatted;
  formatted.reserve(32);
  if (isNegative) {
    formatted.append('-');
  }
  formatted.append(buffer);
  formatted.append('e');
  formatted.append(exponent < 0 ? '-' : '+');
  formatted.append(QByteArray::number(std::abs(exponent)).rightJustified(
      2, '0'));
  const int copyLength = static_cast<int>(std::min<qsizetype>(
      formatted.size(), kMaxTextField - 1));
  std::memcpy(textField, formatted.constData(),
      static_cast<size_t>(copyLength));
  textField[copyLength] = '\0';
}

/* Convert a scalar value into a colon-separated sexagesimal string.
 * The integer portion of `value` is treated as the base unit (hours, degrees, etc.)
 * and fractional portions are expanded into minutes and seconds. */
QString makeSexagesimal(double value, unsigned short precision)
{
  if (!std::isfinite(value)) {
    return formatNonFinite(value);
  }

  constexpr unsigned short kMaxPrecision = 8;
  if (precision > kMaxPrecision) {
    precision = kMaxPrecision;
  }

  static const double precTable[kMaxPrecision + 1] = {
      1.0, 1.0 / 6.0, 1.0 / 60.0, 1.0 / 360.0,
      1.0 / 3600.0, 1.0 / 36000.0, 1.0 / 360000.0,
      1.0 / 3600000.0, 1.0 / 36000000.0};

  double precisionFraction = precTable[precision];
  double adjusted = value + 0.5 * precisionFraction;

  bool negative = adjusted < 0.0;
  if (negative) {
    adjusted = -adjusted + precisionFraction;
  }

  double hours = std::floor(adjusted);
  adjusted = (adjusted - hours) * 60.0;
  int minutes = static_cast<int>(adjusted);
  adjusted = (adjusted - minutes) * 60.0;
  int seconds = static_cast<int>(adjusted);

  QString body;
  if (precision == 0) {
    body = QString::asprintf("%.0f", hours);
  } else if (precision == 1) {
    body = QString::asprintf("%.0f:%d", hours, minutes / 10);
  } else if (precision == 2) {
    body = QString::asprintf("%.0f:%02d", hours, minutes);
  } else if (precision == 3) {
    body = QString::asprintf("%.0f:%02d:%d", hours, minutes, seconds / 10);
  } else if (precision == 4) {
    body = QString::asprintf("%.0f:%02d:%02d", hours, minutes, seconds);
  } else {
    double fraction = std::floor((adjusted - seconds)
        / (precisionFraction * 3600.0));
    body = QString::asprintf("%.0f:%02d:%02d.%0*.0f", hours, minutes,
        seconds, precision - 4, fraction);
  }

  if (negative && !body.startsWith(QLatin1Char('-'))) {
    return QStringLiteral("-") + body;
  }
  return body;
}

QString formatHex(long value)
{
  char buffer[kMaxTextField];
  if (value == 0) {
    buffer[0] = '0';
    buffer[1] = 'x';
    buffer[2] = '0';
    buffer[3] = '\0';
    return QString::fromLatin1(buffer);
  }

  const bool negative = value < 0;
  unsigned long magnitude = static_cast<unsigned long>(value);
  if (negative) {
    magnitude = 0UL - magnitude;
  }
  char digits[sizeof(long) * 2 + 1];
  int index = 0;
  while (magnitude != 0 && index < static_cast<int>(sizeof(digits))) {
    unsigned long temp = magnitude / 16;
    unsigned long digit = magnitude - temp * 16;
    digits[index++] = static_cast<char>((digit < 10)
        ? ('0' + digit) : ('a' + digit - 10));
    magnitude = temp;
  }
  int pos = 0;
  if (negative) {
    buffer[pos++] = '-';
  }
  buffer[pos++] = '0';
  buffer[pos++] = 'x';
  for (int i = index - 1; i >= 0; --i) {
    buffer[pos++] = digits[i];
  }
  buffer[pos] = '\0';
  return QString::fromLatin1(buffer);
}

QString formatOctal(long value)
{
  char buffer[kMaxTextField];
  if (value == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return QString::fromLatin1(buffer);
  }

  const bool negative = value < 0;
  unsigned long magnitude = static_cast<unsigned long>(value);
  if (negative) {
    magnitude = 0UL - magnitude;
  }
  char digits[sizeof(long) * 3];
  int index = 0;
  while (magnitude != 0 && index < static_cast<int>(sizeof(digits))) {
    unsigned long temp = magnitude / 8;
    unsigned long digit = magnitude - temp * 8;
    digits[index++] = static_cast<char>('0' + digit);
    magnitude = temp;
  }
  int pos = 0;
  if (negative) {
    buffer[pos++] = '-';
  }
  for (int i = index - 1; i >= 0; --i) {
    buffer[pos++] = digits[i];
  }
  buffer[pos] = '\0';
  return QString::fromLatin1(buffer);
}

} // namespace TextFormatUtils
