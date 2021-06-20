
# Platforms
  +  Windows XP or later
  +  OS X 10.8 or later
  +  Linux 2.6.23 or later

# How to build
  + git clone --recursive https://github.com/cloudpeak/tin.git
  + cd tin
  + mkdir build
  + cd build
  + Visual Studio 2019 Win64
    + cmake -G "Visual Studio 16 2019" -A x64  ../ -DCMAKE_BUILD_TYPE=RELEASE
  + Visual Studio 2015 Win64
    + cmake -G "Visual Studio 14 2015 Win64" ../ -DCMAKE_BUILD_TYPE=RELEASE
  + Visual Studio 2015 Win32
    + cmake -G "Visual Studio 14 2015" ../ -DCMAKE_BUILD_TYPE=RELEASE
  + Visual Studio 2008 Win32
    + cmake -G "Visual Studio 9 2008" ../ -DCMAKE_BUILD_TYPE=RELEASE
  + GCC or Clang
    + cmake ../ -DCMAKE_BUILD_TYPE=RELEASE && make

## Example(echo server)
```c++
#include "tin/all.h"

void HandleClient(tin::net::TcpConn conn) {
  // Set TCP Read Write buffer.
  conn->SetReadBuffer(64 * 1024);
  conn->SetWriteBuffer(64 * 1024);

  // user space buffer size.
  const int kIOBufferSize = 4 * 1024;
  scoped_ptr<char[]> buf(new char[kIOBufferSize]);

  // set read, write deadline.
  const int64 kRWDeadline = 20 * tin::kSecond;
  conn->SetDeadline(kRWDeadline);
  while (true) {
    int n = conn->Read(buf.get(), kIOBufferSize);
    int err = tin::GetErrorCode();
    if (n > 0) {
      conn->SetReadDeadline(kRWDeadline);
    }
    if (err != 0) {
      VLOG(1) << "Read failed due to: " << tin::GetErrorStr();
      // FIN received, graceful close, we can still send.
      if (err == TIN_EOF) {
        if (n > 0) {
          conn->Write(buf.get(), n);
        }
        conn->CloseWrite();
        // delay a while to avoid RST.
        tin::NanoSleep(500 * tin::kMillisecond);
      }
      break;
    }
    DCHECK_GT(n, 0);
    conn->Write(buf.get(), n);
    if (tin::GetErrorCode() != 0) {
      VLOG(1) << "Write failed due to " << tin::GetErrorStr();
      break;
    }
    conn->SetWriteDeadline(kRWDeadline);
  }
  conn->Close();
}

int TinMain(int argc, char** argv) {
  const uint16 kPort = 2222;
  bool use_ipv6 = false;
  tin::net::TCPListener listener =
    tin::net::ListenTcp(use_ipv6 ? "0:0:0:0:0:0:0:0" : "0.0.0.0", kPort);
  if (tin::GetErrorCode() != 0) {
    LOG(FATAL) << "Listen failed due to " << tin::GetErrorStr();
  }
  LOG(INFO) << "echo server is listening on port: " << kPort;
  while (true) {
    tin::net::TcpConn conn = listener->Accept();
    if (tin::GetErrorCode() == 0) {
      tin::Spawn(&HandleClient, conn);
    } else {
      LOG(INFO) << "Accept failed due to " << tin::GetErrorStr();
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  tin::Initialize();

  // set logging level.
  logging::SetMinLogLevel(logging::LOG_INFO);

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


```
