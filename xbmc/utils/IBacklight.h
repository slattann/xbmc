#include "ServiceBroker.h"

#include <memory>

class IBacklight
{
public:
  static void Register(std::shared_ptr<IBacklight> backlight) { CServiceBroker::RegisterBacklight(backlight); }

  virtual bool SetBrightness(int brightness) = 0;
  virtual int GetBrightness() = 0;
  virtual int GetMaxBrightness() = 0;
};
