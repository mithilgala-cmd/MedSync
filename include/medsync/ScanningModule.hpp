#pragma once

#include "medsync/Types.hpp"

namespace medsync {

class Logger;

// Produces a synthetic scan record for a sample. There is no imaging model:
// the quality score is a stable hash of the sample id, used so that tests can
// assert reproducibility. Milestone 3 adds a worker thread and simulated
// duration; the result shape stays the same.
class ScanningModule {
public:
    explicit ScanningModule(Logger& logger);

    ScanResult scan(const Sample& sample);

private:
    Logger& logger_;
};

} // namespace medsync
