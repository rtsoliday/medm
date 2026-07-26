#pragma once

#include <QByteArray>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QThread>

#include <utility>

#include <cadef.h>

namespace RuntimeUtils {

/* Channel callbacks are already marshalled to the GUI thread by the channel
 * managers. Execute immediately there, while retaining a safe queued fallback
 * for callers on another thread. */
template <typename Object, typename Func>
void invokeOnObject(QPointer<Object> target, Func &&func)
{
  if (!target) {
    return;
  }
  auto task = [target, func = std::forward<Func>(func)]() mutable {
    if (target) {
      func(target.data());
    }
  };
  if (QThread::currentThread() == target->thread()) {
    task();
  } else {
    QMetaObject::invokeMethod(target.data(), std::move(task),
        Qt::QueuedConnection);
  }
}

template <typename Object, typename Guard, typename Func>
void invokeOnObject(QPointer<Object> target, QPointer<Guard> guard, Func &&func)
{
  if (!target || !guard) {
    return;
  }
  auto task =
      [target, guard, func = std::forward<Func>(func)]() mutable {
        if (target && guard) {
          func(target.data());
        }
      };
  if (QThread::currentThread() == target->thread()) {
    task();
  } else {
    QMetaObject::invokeMethod(target.data(), std::move(task),
        Qt::QueuedConnection);
  }
}

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

/* Convert a PV/channel label into an SDDS-safe column name.
 * Invalid identifier characters become underscores, and leading underscores
 * are stripped because SDDS rejects names that start with '_'. */
QString sanitizeSddsColumnName(const QString &name,
    const QString &fallback = QString());

} // namespace RuntimeUtils
