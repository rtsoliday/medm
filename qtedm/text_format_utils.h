#pragma once

#include <QString>

namespace TextFormatUtils {

/* Maximum size for text field buffers */
constexpr int kMaxTextField = 512;

/* Maximum precision for numeric display */
constexpr int kMaxPrecision = 17;

/* Pi constant for angle conversions */
constexpr double kPi = 3.14159265358979323846;

/* Clamp precision value to valid range [0, kMaxPrecision].
 * Returns 0 if negative, kMaxPrecision if too large. */
int clampPrecision(int precision);

/* Format a non-finite value without passing it through integer conversions. */
QString formatNonFinite(double value);

/* cvtFloatToString (EPICS cvtFast.c), which Xc uses to print the limits
 * and the value of a float bar or indicator. */
QString xcFloatText(double value, int precision);

/* Convert a double to long without undefined behavior outside long's range. */
long saturatedLongFromDouble(double value, bool roundToNearest = false);

/* Convert a double to engineering-style exponential notation.
 * Exponents are emitted in multiples of three using e+NN / e-NN, growing
 * to three digits when required by the range of a double.
 * textField must be at least kMaxTextField bytes. */
void localCvtDoubleToExpNotationString(double value, char *textField,
    unsigned short precision);

/* Format a double as a generic sexagesimal string (units:minutes:seconds).
 * Callers can scale the input value beforehand to express hours, degrees, etc.
 * precision controls decimal places: 0-8 allowed.
 * Returns formatted string like "12:34:56.78". */
QString makeSexagesimal(double value, unsigned short precision);

/* Format a long as hexadecimal string with 0x prefix.
 * A negative value shows as its low 32 bits, as in MEDM.
 * Returns string like "0x1a2b", or "0xffffff01" for -255. */
QString formatHex(long value);

/* Returns the value to write for an integer read from hexadecimal or octal
 * input. For a channel of a signed integer type, a value from 0x80000000
 * to 0xffffffff is the negative number formatHex() shows that way, as
 * MEDM's (long)strtoul() and DBR_LONG put make it. */
double integerInputValue(qlonglong value, bool signedIntegerChannel);

/* Format a long as octal string.
 * Handles negative values with leading minus sign.
 * Returns string like "755" or "-123". */
QString formatOctal(long value);

} // namespace TextFormatUtils
