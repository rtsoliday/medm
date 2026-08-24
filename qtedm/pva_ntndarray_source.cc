#include "pva_ntndarray_source.h"

#include <QByteArray>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>

#include <pv/ntndarray.h>
#include <pv/pvData.h>

#include "pvaSDDS.h"

namespace {

constexpr double kConnectTimeoutSeconds = 1.0;
constexpr int kMaximumEventsPerPoll = 256;

class MonitorEventRelease
{
public:
  explicit MonitorEventRelease(
      const epics::pvaClient::PvaClientMonitorPtr &monitor)
    : monitor_(monitor)
  {
  }

  ~MonitorEventRelease()
  {
    if (monitor_) {
      monitor_->releaseEvent();
    }
  }

private:
  epics::pvaClient::PvaClientMonitorPtr monitor_;
};

template <typename T>
bool retainArray(const epics::pvData::PVScalarArrayPtr &array,
    NtNdArrayFrame *frame)
{
  if (!array || !frame) {
    return false;
  }
  using Vector = epics::pvData::shared_vector<const T>;
  auto *kept = new Vector;
  array->getAs(*kept);
  if (kept->empty()) {
    delete kept;
    return false;
  }
  if (kept->size() > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
    delete kept;
    return false;
  }
  frame->elementCount = kept->size();
  frame->byteCount = kept->size() * sizeof(T);
  frame->data = std::shared_ptr<const void>(kept->data(),
      [kept](const void *) { delete kept; });
  return true;
}

bool readDimensions(const epics::pvData::PVStructurePtr &root,
    NtNdArrayFrame *frame, QString *error)
{
  const auto dimensions =
      root->getSubField<epics::pvData::PVStructureArray>("dimension");
  if (!dimensions) {
    if (error) {
      *error = QStringLiteral("NTNDArray has no dimension field.");
    }
    return false;
  }
  const auto entries = dimensions->view();
  if (entries.size() < 2 || entries.size() > 3) {
    if (error) {
      *error = QStringLiteral(
          "Only raw 2D mono and 3D RGB NTNDArray dimensions are supported.");
    }
    return false;
  }
  frame->dimensions.clear();
  frame->dimensions.reserve(static_cast<int>(entries.size()));
  for (const auto &entry : entries) {
    if (!entry) {
      if (error) {
        *error = QStringLiteral("NTNDArray contains a null dimension.");
      }
      return false;
    }
    const auto size = entry->getSubField<epics::pvData::PVInt>("size");
    const auto offset = entry->getSubField<epics::pvData::PVInt>("offset");
    const auto fullSize =
        entry->getSubField<epics::pvData::PVInt>("fullSize");
    const auto binning =
        entry->getSubField<epics::pvData::PVInt>("binning");
    const auto reverse =
        entry->getSubField<epics::pvData::PVBoolean>("reverse");
    if (!size || size->get() <= 0) {
      if (error) {
        *error = QStringLiteral("NTNDArray has an invalid dimension size.");
      }
      return false;
    }
    NtNdArrayDimension dimension;
    dimension.size = size->get();
    dimension.offset = offset ? offset->get() : 0;
    dimension.fullSize = fullSize ? fullSize->get() : dimension.size;
    dimension.binning = binning ? std::max(1, binning->get()) : 1;
    dimension.reverse = reverse && reverse->get();
    frame->dimensions.append(dimension);
  }
  return true;
}

int readColorMode(const epics::pvData::PVStructurePtr &root)
{
  const auto attributes =
      root->getSubField<epics::pvData::PVStructureArray>("attribute");
  if (!attributes) {
    return -1;
  }
  const auto entries = attributes->view();
  for (const auto &entry : entries) {
    if (!entry) {
      continue;
    }
    const auto name = entry->getSubField<epics::pvData::PVString>("name");
    if (!name) {
      continue;
    }
    const QString attributeName =
        QString::fromStdString(name->get()).trimmed();
    if (attributeName.compare(
            QStringLiteral("ColorMode"), Qt::CaseInsensitive) != 0
        && attributeName.compare(
            QStringLiteral("NDColorMode"), Qt::CaseInsensitive) != 0) {
      continue;
    }
    const auto value =
        entry->getSubField<epics::pvData::PVUnion>("value");
    const auto scalar = value
        ? value->get<epics::pvData::PVScalar>()
        : epics::pvData::PVScalarPtr();
    if (!scalar) {
      continue;
    }
    try {
      return scalar->getAs<epics::pvData::int32>();
    } catch (...) {
      return -1;
    }
  }
  return -1;
}

bool extractFrameImpl(const epics::pvData::PVStructurePtr &root,
    NtNdArrayFrame *frame, QString *error)
{
  if (!root || !frame) {
    return false;
  }
  if (!epics::nt::NTNDArray::is_a(root)) {
    if (error) {
      *error = QStringLiteral("PVA value is not epics:nt/NTNDArray.");
    }
    return false;
  }

  NtNdArrayFrame candidate;
  const auto value = root->getSubField<epics::pvData::PVUnion>("value");
  const auto array = value
      ? value->get<epics::pvData::PVScalarArray>()
      : epics::pvData::PVScalarArrayPtr();
  if (!array) {
    if (error) {
      *error = QStringLiteral("NTNDArray value union has no scalar array.");
    }
    return false;
  }

  bool retained = false;
  switch (array->getScalarArray()->getElementType()) {
  case epics::pvData::pvByte:
    candidate.scalarType = NtNdArrayScalarType::kInt8;
    retained = retainArray<epics::pvData::int8>(array, &candidate);
    break;
  case epics::pvData::pvUByte:
    candidate.scalarType = NtNdArrayScalarType::kUInt8;
    retained = retainArray<epics::pvData::uint8>(array, &candidate);
    break;
  case epics::pvData::pvShort:
    candidate.scalarType = NtNdArrayScalarType::kInt16;
    retained = retainArray<epics::pvData::int16>(array, &candidate);
    break;
  case epics::pvData::pvUShort:
    candidate.scalarType = NtNdArrayScalarType::kUInt16;
    retained = retainArray<epics::pvData::uint16>(array, &candidate);
    break;
  case epics::pvData::pvInt:
    candidate.scalarType = NtNdArrayScalarType::kInt32;
    retained = retainArray<epics::pvData::int32>(array, &candidate);
    break;
  case epics::pvData::pvUInt:
    candidate.scalarType = NtNdArrayScalarType::kUInt32;
    retained = retainArray<epics::pvData::uint32>(array, &candidate);
    break;
  case epics::pvData::pvLong:
    candidate.scalarType = NtNdArrayScalarType::kInt64;
    retained = retainArray<epics::pvData::int64>(array, &candidate);
    break;
  case epics::pvData::pvULong:
    candidate.scalarType = NtNdArrayScalarType::kUInt64;
    retained = retainArray<epics::pvData::uint64>(array, &candidate);
    break;
  case epics::pvData::pvFloat:
    candidate.scalarType = NtNdArrayScalarType::kFloat32;
    retained = retainArray<float>(array, &candidate);
    break;
  case epics::pvData::pvDouble:
    candidate.scalarType = NtNdArrayScalarType::kFloat64;
    retained = retainArray<double>(array, &candidate);
    break;
  default:
    break;
  }
  if (!retained) {
    if (error) {
      *error = QStringLiteral(
          "NTNDArray scalar type is unsupported or the frame is empty.");
    }
    return false;
  }

  if (!readDimensions(root, &candidate, error)) {
    return false;
  }

  const int colorMode = readColorMode(root);
  switch (colorMode) {
  case 0:
    candidate.colorMode = NtNdArrayColorMode::kMono;
    break;
  case 2:
    candidate.colorMode = NtNdArrayColorMode::kRgb1;
    break;
  case 3:
    candidate.colorMode = NtNdArrayColorMode::kRgb2;
    break;
  case 4:
    candidate.colorMode = NtNdArrayColorMode::kRgb3;
    break;
  default:
    candidate.colorMode = candidate.dimensions.size() == 2
        ? NtNdArrayColorMode::kMono
        : NtNdArrayColorMode::kUnsupported;
    break;
  }

  const auto codec =
      root->getSubField<epics::pvData::PVString>("codec.name");
  candidate.codec = codec
      ? QString::fromStdString(codec->get()).trimmed() : QString();
  const auto compressed =
      root->getSubField<epics::pvData::PVLong>("compressedSize");
  const auto uncompressed =
      root->getSubField<epics::pvData::PVLong>("uncompressedSize");
  candidate.compressedSize = compressed ? compressed->get() : 0;
  candidate.uncompressedSize = uncompressed ? uncompressed->get() : 0;

  const auto uniqueId =
      root->getSubField<epics::pvData::PVInt>("uniqueId");
  candidate.uniqueId = uniqueId ? uniqueId->get() : 0;
  auto seconds =
      root->getSubField<epics::pvData::PVLong>(
          "dataTimeStamp.secondsPastEpoch");
  auto nanos =
      root->getSubField<epics::pvData::PVInt>(
          "dataTimeStamp.nanoseconds");
  if (!seconds) {
    seconds = root->getSubField<epics::pvData::PVLong>(
        "timeStamp.secondsPastEpoch");
    nanos = root->getSubField<epics::pvData::PVInt>(
        "timeStamp.nanoseconds");
  }
  candidate.secondsPastEpoch = seconds ? seconds->get() : 0;
  candidate.nanoseconds = nanos ? nanos->get() : 0;

  *frame = std::move(candidate);
  return true;
}

} // namespace

bool pvaNtNdArrayExtractFrame(
    const epics::pvData::PVStructurePtr &root,
    NtNdArrayFrame *frame, QString *error)
{
  return extractFrameImpl(root, frame, error);
}

struct PvaNtNdArraySource
{
  QString rawName;
  QString pvName;
  PVA_OVERALL *pva = nullptr;
  bool connected = false;
};

PvaNtNdArraySource *pvaNtNdArrayCreateSource(
    const QString &rawName, const QString &pvName, QString *error)
{
  if (pvName.trimmed().isEmpty()) {
    if (error) {
      *error = QStringLiteral("NTNDArray PV name is empty.");
    }
    return nullptr;
  }

  std::unique_ptr<PvaNtNdArraySource> source(new PvaNtNdArraySource);
  source->rawName = rawName.trimmed();
  source->pvName = pvName.trimmed();
  source->pva = new PVA_OVERALL();
  allocPVA(source->pva, 1);
  source->pva->includeAlarmSeverity = false;

  epics::pvData::shared_vector<std::string> names(1);
  names[0] = source->pvName.toStdString();
  source->pva->pvaChannelNames = freeze(names);
  epics::pvData::shared_vector<std::string> providers(1);
  providers[0] = "pva";
  source->pva->pvaProvider = freeze(providers);

  ConnectPVA(source->pva, kConnectTimeoutSeconds);
  if (source->pva->isConnected.size() > 0) {
    source->connected = source->pva->isConnected[0];
  }
  if (MonitorPVAValues(source->pva) != 0 && source->connected) {
    if (error) {
      *error = QStringLiteral("Could not start NTNDArray PVA monitor.");
    }
    freePVA(source->pva);
    delete source->pva;
    source->pva = nullptr;
    return nullptr;
  }
  return source.release();
}

void pvaNtNdArrayDestroySource(PvaNtNdArraySource *source)
{
  if (!source) {
    return;
  }
  if (source->pva) {
    freePVA(source->pva);
    delete source->pva;
    source->pva = nullptr;
  }
  delete source;
}

PvaNtNdArrayPollResult pvaNtNdArrayPoll(PvaNtNdArraySource *source)
{
  PvaNtNdArrayPollResult result;
  if (!source || !source->pva) {
    result.error = QStringLiteral("NTNDArray source is not initialized.");
    return result;
  }

  bool reconnect = false;
  for (const auto &multi : source->pva->pvaClientMultiChannelPtr) {
    if (multi && multi->connectionChange()) {
      reconnect = true;
    }
  }
  if (reconnect && MonitorPVAValues(source->pva) != 0) {
    result.error =
        QStringLiteral("Could not restart the NTNDArray PVA monitor.");
  }
  if (source->pva->isConnected.size() > 0) {
    result.connected = source->pva->isConnected[0];
  }
  result.connectionChanged = result.connected != source->connected;
  source->connected = result.connected;

  if (!result.connected || source->pva->pvaClientMonitorPtr.empty()
      || !source->pva->pvaClientMonitorPtr[0]) {
    return result;
  }

  const auto monitor = source->pva->pvaClientMonitorPtr[0];
  for (int event = 0; event < kMaximumEventsPerPoll; ++event) {
    bool polled = false;
    try {
      polled = monitor->poll();
    } catch (const std::exception &exception) {
      result.error = QStringLiteral("PVA monitor poll failed: %1")
          .arg(QString::fromLocal8Bit(exception.what()));
      return result;
    }
    if (!polled) {
      break;
    }
    MonitorEventRelease release(monitor);
    ++result.receivedFrames;
    try {
      const auto root = monitor->getData()->getPVStructure();
      NtNdArrayFrame candidate;
      QString error;
      if (pvaNtNdArrayExtractFrame(root, &candidate, &error)) {
        if (result.hasFrame) {
          ++result.droppedFrames;
        }
        result.frame = std::move(candidate);
        result.hasFrame = true;
      } else {
        result.error = error;
      }
    } catch (const std::exception &exception) {
      result.error = QStringLiteral("NTNDArray extraction failed: %1")
          .arg(QString::fromLocal8Bit(exception.what()));
    }
  }
  return result;
}
