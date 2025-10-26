#pragma once

#include <QByteArray>
#include <QString>

#include <cadef.h>

namespace RuntimeUtils {

/* Common constants used across runtime classes */
constexpr short kInvalidSeverity = 3;
constexpr double kVisibilityEpsilon = 1e-12;
constexpr int kCalcInputCount = 12;

/* Ensure a QByteArray is null-terminated for passing to C functions.
 * Appends '\0' if not already present. */
void appendNullTerminator(QByteArray &bytes);

/* Normalize calc expression to MEDM calc engine syntax.
 * MEDM calc uses single '=' for equality (not '==') and '#' for inequality (not '!=').
 * This function converts modern C-style operators to MEDM syntax. */
QString normalizeCalcExpression(const QString &expr);

/* Check if a Channel Access field type is numeric.
 * Returns true for DBR_CHAR, DBR_SHORT, DBR_LONG, DBR_FLOAT, DBR_DOUBLE. */
bool isNumericFieldType(chtype fieldType);

} // namespace RuntimeUtils
