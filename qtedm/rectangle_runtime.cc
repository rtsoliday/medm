#include "rectangle_runtime.h"

/* RectangleRuntime implementation.
 *
 * All runtime logic is provided by the GraphicElementRuntimeBase template.
 * This file is intentionally minimal - the base class handles:
 *   - Channel Access initialization and cleanup
 *   - Channel subscriptions and callbacks
 *   - Value updates and DBR type conversions
 *   - Visibility evaluation (Static/IfNotZero/IfZero/Calc modes)
 *   - MEDM calc expression normalization and execution
 *
 * No additional implementation is needed for RectangleRuntime as it has
 * no element-specific behavior beyond what the base class provides.
 */
