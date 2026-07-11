#pragma once

#include "InspectionContracts.h"

#include <string>

namespace cigvision {

class IFrameSource {
public:
    virtual ~IFrameSource() = default;
    virtual bool start(std::string& errorMessage) = 0;
    virtual void stop() noexcept = 0;
    virtual bool tryRead(FramePacket& frame, std::string& errorMessage) = 0;
};

class IDetector {
public:
    virtual ~IDetector() = default;
    virtual DetectionBatch detect(const FramePacket& frame) = 0;
};

class IInspectionResultSink {
public:
    virtual ~IInspectionResultSink() = default;
    virtual bool store(const InspectionResult& result, std::string& errorMessage) = 0;
};

class IRejectOutput {
public:
    virtual ~IRejectOutput() = default;
    virtual RejectExecutionResult execute(const RejectCommand& command) = 0;
};

class IClock {
public:
    virtual ~IClock() = default;
    virtual TimestampMicros now() const noexcept = 0;
};

} // namespace cigvision
