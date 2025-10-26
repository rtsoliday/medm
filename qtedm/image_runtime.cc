#include "image_runtime.h"

#include <algorithm>
#include <cmath>

#include <QDebug>

#include "image_element.h"
#include "runtime_utils.h"

extern "C" {
long calcPerform(double *parg, double *presult, char *post);
long postfix(char *pinfix, char *ppostfix, short *perror);
}

namespace {
constexpr int kCalcInputCount = 12;
}

ImageRuntime::ImageRuntime(ImageElement *element)
  : GraphicElementRuntimeBase<ImageElement>(element)
{
}

void ImageRuntime::onStart()
{
  /* Initialize image-specific calc expression for frame selection */
  const QString imageCalcExpr = element()->calc().trimmed();
  hasImageCalcExpression_ = !imageCalcExpr.isEmpty();
  animate_ = imageCalcExpr.isEmpty() && element()->frameCount() > 1;

  if (hasImageCalcExpression_) {
    /* Normalize expression: convert == to = and != to # for MEDM calc engine */
    QString normalized = RuntimeUtils::normalizeCalcExpression(imageCalcExpr);
    QByteArray infix = normalized.toLatin1();
    imageCalcPostfix_.resize(512);
    imageCalcPostfix_.fill('\0');
    short error = 0;
    long status = postfix(infix.data(), imageCalcPostfix_.data(), &error);
    if (status == 0) {
      imageCalcValid_ = true;
    } else {
      imageCalcValid_ = false;
      qWarning() << "Invalid image calc expression:" << imageCalcExpr
                 << "(error" << error << ')';
    }
  }
}

void ImageRuntime::onStop()
{
  /* Clean up image-specific state */
  imageCalcPostfix_.clear();
  imageCalcValid_ = false;
  hasImageCalcExpression_ = false;
  animate_ = false;

  if (element()) {
    element()->setRuntimeAnimate(false);
    element()->setRuntimeFrameValid(element()->frameCount() > 0);
    element()->setRuntimeFrameIndex(0);
  }
}

void ImageRuntime::onStateEvaluated()
{
  /* Add image-specific evaluation after base class evaluates visibility/color */
  if (!element()) {
    return;
  }

  /* Check if all channels are connected */
  bool anyChannels = false;
  bool allConnected = true;
  for (const auto &channel : channels()) {
    if (channel.name.isEmpty()) {
      continue;
    }
    anyChannels = true;
    if (!channel.connected) {
      allConnected = false;
      break;
    }
  }

  if (!anyChannels) {
    if (animate_ && element()->frameCount() > 1) {
      element()->setRuntimeAnimate(true);
      element()->setRuntimeFrameValid(element()->frameCount() > 0);
    } else {
      element()->setRuntimeAnimate(false);
      evaluateFrameSelection();
    }
    return;
  }

  if (!allConnected) {
    element()->setRuntimeAnimate(false);
    element()->setRuntimeFrameValid(false);
    return;
  }

  if (animate_ && element()->frameCount() > 1) {
    element()->setRuntimeAnimate(true);
    element()->setRuntimeFrameValid(true);
    return;
  }

  element()->setRuntimeAnimate(false);
  evaluateFrameSelection();
}

void ImageRuntime::evaluateFrameSelection()
{
  if (!element()) {
    return;
  }

  const int count = element()->frameCount();
  if (count <= 0) {
    element()->setRuntimeFrameValid(false);
    return;
  }

  if (!hasImageCalcExpression_) {
    element()->setRuntimeFrameIndex(0);
    element()->setRuntimeFrameValid(true);
    return;
  }

  if (!imageCalcValid_) {
    element()->setRuntimeFrameValid(false);
    return;
  }

  double result = 0.0;
  if (!evaluateImageCalc(result)) {
    element()->setRuntimeFrameValid(false);
    return;
  }

  double clamped = std::max(0.0, std::min(result,
      static_cast<double>(count - 1)));
  int frameIndex = static_cast<int>(std::floor(clamped + 0.5));
  frameIndex = std::max(0, std::min(frameIndex, count - 1));

  element()->setRuntimeFrameIndex(frameIndex);
  element()->setRuntimeFrameValid(true);
}

bool ImageRuntime::evaluateImageCalc(double &result) const
{
  if (!imageCalcValid_ || imageCalcPostfix_.isEmpty()) {
    return false;
  }

  const auto &chans = channels();
  double args[kCalcInputCount] = {0.0};
  args[0] = chans[0].value;
  args[1] = chans[1].value;
  args[2] = chans[2].value;
  args[3] = chans[3].value;
  args[4] = 0.0;
  args[5] = 0.0;

  const auto &primary = chans[0];
  args[6] = static_cast<double>(std::max<long>(primary.elementCount, 1));
  args[7] = primary.hopr;
  args[8] = static_cast<double>(primary.status);
  args[9] = static_cast<double>(primary.severity);
  args[10] = static_cast<double>(primary.precision >= 0 ? primary.precision : 0);
  args[11] = primary.lopr;

  long status = calcPerform(args, &result,
      const_cast<char *>(imageCalcPostfix_.constData()));
  return status == 0;
}
