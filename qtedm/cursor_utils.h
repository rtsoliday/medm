#pragma once

#include <QCursor>

namespace CursorUtils {

void setUseBigCursor(bool enabled);

QCursor arrowCursor();
QCursor crossCursor();
QCursor forbiddenCursor();

}  // namespace CursorUtils
