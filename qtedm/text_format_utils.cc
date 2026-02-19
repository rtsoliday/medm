#include "text_format_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

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

/* Format value with engineering notation (powers of 10 in multiples of 3). */
void localCvtDoubleToExpNotationString(double value, char *textField,
    unsigned short precision)
{
  if (!textField) {
    return;
  }

  double absVal = std::fabs(value);
  bool isNegative = value < 0.0;
  double scaled = absVal;
  int exponent = 0;
  char buffer[kMaxTextField];
  int index = 0;

  auto appendChar = [&](char ch) {
    if (index < kMaxTextField - 1) {
      textField[index++] = ch;
    }
  };

  auto appendString = [&](const char *source) {
    if (!source) {
      return;
    }
    while (*source != '\0' && index < kMaxTextField - 1) {
      textField[index++] = *source++;
    }
  };

  if (absVal < 1.0) {
    if (absVal != 0.0) {
      while (scaled < 1.0) {
        scaled *= 1000.0;
        exponent += 3;
      }
    }
    localCvtDoubleToString(scaled, buffer, precision);
    if (isNegative) {
      appendChar('-');
    }
    appendString(buffer);
    appendChar('e');
    appendChar((exponent == 0) ? '+' : '-');
    appendChar(static_cast<char>('0' + exponent / 10));
    appendChar(static_cast<char>('0' + exponent % 10));
    textField[index] = '\0';
    return;
  }

  while (scaled >= 1000.0) {
    scaled *= 0.001;
    exponent += 3;
  }

  localCvtDoubleToString(scaled, buffer, precision);
  if (isNegative) {
    appendChar('-');
  }
  appendString(buffer);
  appendChar('e');
  appendChar('+');
  appendChar(static_cast<char>('0' + exponent / 10));
  appendChar(static_cast<char>('0' + exponent % 10));
  textField[index] = '\0';
}

/* Convert a scalar value into a colon-separated sexagesimal string.
 * The integer portion of `value` is treated as the base unit (hours, degrees, etc.)
 * and fractional portions are expanded into minutes and seconds. */
QString makeSexagesimal(double value, unsigned short precision)
{
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

  bool negative = value < 0;
  unsigned long magnitude = negative ? static_cast<unsigned long>(-value)
      : static_cast<unsigned long>(value);
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

  bool negative = value < 0;
  unsigned long magnitude = negative ? static_cast<unsigned long>(-value)
      : static_cast<unsigned long>(value);
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
