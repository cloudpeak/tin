#include "tin/all.h"

int TinMain(int argc, char** argv) {
  LOG(INFO) << "TinMain";
  return 0;
}

int main(int argc, char** argv) {
  tin::Initialize();

  // set logging level.
  logging::SetMinLogLevel(-1);

  // set max p count.
  tin::Config config = tin::DefaultConfig();
  config.SetMaxProcs(base::SysInfo::NumberOfProcessors());

  // start the world.
  tin::PowerOn(TinMain, argc, argv, &config);

  // wait for power off
  tin::WaitForPowerOff();

  // cleanup.
  tin::Deinitialize();

  return 0;
}

