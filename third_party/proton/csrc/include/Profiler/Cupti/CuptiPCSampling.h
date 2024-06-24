#ifndef PROTON_PROFILER_CUPTI_PC_SAMPLING_H_
#define PROTON_PROFILER_CUPTI_PC_SAMPLING_H_

#include "CuptiProfiler.h"
#include "Driver/GPU/CudaApi.h"
#include "Driver/GPU/CuptiApi.h"
#include "Utility/Map.h"
#include "Utility/Singleton.h"
#include <atomic>
#include <mutex>

namespace proton {

struct CubinData {
  size_t cubinCrc;
  const char *cubin;
  size_t cubinSize;

  struct LineInfoKey {
    uint32_t functionIndex;
    uint64_t pcOffset;

    bool operator<(const LineInfoKey &other) const {
      return functionIndex < other.functionIndex ||
             (functionIndex == other.functionIndex &&
              pcOffset < other.pcOffset);
    }
  };

  struct LineInfoValue {
    uint32_t lineNumber;
    std::string functionName;
    // dirName + fileName
    std::string fileName;
  };

  std::map<LineInfoKey, LineInfoValue> lineInfo;
};

struct ConfigureData {
  ConfigureData() = default;

  ~ConfigureData() {
    for (size_t i = 0; i < numStallReasons; i++)
      std::free(stallReasonNames[i]);
    if (stallReasonNames)
      std::free(stallReasonNames);
    if (stallReasonIndices)
      std::free(stallReasonIndices);
  }

  void initialize(CUcontext context);

  CUpti_PCSamplingConfigurationInfo configureStallReasons();
  CUpti_PCSamplingConfigurationInfo configureSamplingPeriod();
  CUpti_PCSamplingConfigurationInfo configureSamplingBuffer();
  CUpti_PCSamplingConfigurationInfo configureScratchBuffer();
  CUpti_PCSamplingConfigurationInfo configureHardwareBufferSize();
  CUpti_PCSamplingConfigurationInfo configureStartStopControl();
  CUpti_PCSamplingConfigurationInfo configureCollectionMode();

  // The amount of data reserved on the GPU
  static constexpr uint32_t HardwareBufferSize = 512 * 1024 * 1024;
  // The amount of data copied from the hardware buffer each time
  static constexpr uint32_t ScratchBufferSize = 16 * 1024 * 1024;
  // The number of PCs copied from the scratch buffer each time
  static constexpr uint32_t ScratchBufferPCCount = 4096;
  // The sampling period in cycles = 2^frequency
  static constexpr uint32_t DefaultFrequency = 10;

  bool initialized{false};
  CUcontext context{};
  uint32_t numStallReasons{};
  uint32_t numValidStallReasons{};
  char **stallReasonNames{};
  uint32_t *stallReasonIndices{};
  std::map<size_t, size_t> stallReasonIndexToMetricIndex{};
  std::set<size_t> notIssuedStallReasonIndices{};
  CUpti_PCSamplingData pcSamplingData{};
};

class CuptiPCSampling : public Singleton<CuptiPCSampling> {

public:
  CuptiPCSampling() = default;
  virtual ~CuptiPCSampling() = default;

  void initialize(CUcontext context);

  void start(CUcontext context);

  void stop(CUcontext context, uint64_t externId, bool isAPI);

  void finalize(CUcontext context);

  void loadModule(CUpti_ResourceData *resourceData);

  void unloadModule(CUpti_ResourceData *resourceData);

private:
  ConfigureData *getConfigureData(CUcontext context);

  CubinData *getCubinData(uint64_t cubinCrc);

  void processPCSamplingData(ConfigureData *configureData, uint64_t externId,
                             bool isAPI);

  ThreadSafeMap<size_t, ConfigureData> contextIdToConfigureData;
  ThreadSafeMap<size_t, CubinData> cubinCrcToCubinData;

  std::atomic<bool> pcSamplingStarted{false};
};

} // namespace proton

#endif // PROTON_PROFILER_CUPTI_PC_SAMPLING_H_