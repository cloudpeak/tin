#include "base/system/sys_info.h"
#include "tin/tin.h"
#include "tin/config.h"
#include <absl/log/log.h>

int TinMain(int argc, char** argv) {
  LOG(INFO) << "TinMain";
  return 0;
}

int main(int argc, char** argv) {
  tin::Config config = tin::DefaultConfig();
  config.SetMaxProcs(base::SysInfo::NumberOfProcessors());
  return tin::Run(TinMain, argc, argv, config);
}
