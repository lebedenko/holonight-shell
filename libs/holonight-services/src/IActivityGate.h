#pragma once

class IActivityGate {
 public:
  virtual ~IActivityGate() = default;

  IActivityGate(const IActivityGate&) = delete;
  IActivityGate& operator=(const IActivityGate&) = delete;
  IActivityGate(IActivityGate&&) = delete;
  IActivityGate& operator=(IActivityGate&&) = delete;

  virtual void pauseActivity() = 0;
  virtual void resumeActivity() = 0;

 protected:
  IActivityGate() = default;
};
