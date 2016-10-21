#include "tin/all.h"

// case 0
void HandleClient0(tin::net::TcpConn conn) {
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

// case 1, optimize for timeout, since SetReadDeadline is expensive.
void HandleClient1(tin::net::TcpConn conn) {
  // Set TCP Read Write buffer.
  conn->SetReadBuffer(64 * 1024);
  conn->SetWriteBuffer(64 * 1024);

  // user space buffer size.
  const int kIOBufferSize = 4 * 1024;
  scoped_ptr<char[]> buf(new char[kIOBufferSize]);

  // record read,  write timestamp.
  int64 last_set_recv_time = tin::MonoNow();
  int64 last_set_send_time = last_set_recv_time;

  // set read, write deadline.
  const int64 kRWDeadline = 20 * tin::kSecond;
  conn->SetDeadline(kRWDeadline);
  while (true) {
    int n = conn->Read(buf.get(), kIOBufferSize);
    int err = tin::GetErrorCode();
    if (n > 0) {
      int64 now = tin::MonoNow();
      int64 elapsed = now - last_set_recv_time;
      if (elapsed >= 5 * tin::kSecond) {
        conn->SetReadDeadline(kRWDeadline);
        last_set_recv_time = now;
        VLOG(1) << "reset read deadline";
      }
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
    } else {
      int64 now = tin::MonoNow();
      int64 elapsed = now - last_set_send_time;
      if (elapsed >= 5 * tin::kSecond) {
        conn->SetWriteDeadline(kRWDeadline);
        last_set_send_time = now;
        VLOG(1) << "reset write deadline";
      }
    }
  }
  conn->Close();
}

// case 2, optimize for timeout, since SetReadDeadline is expensive.
void HandleClient2(tin::net::TcpConn conn) {
  // Set TCP Read Write buffer.
  conn->SetReadBuffer(64 * 1024);
  conn->SetWriteBuffer(64 * 1024);

  // user space buffer size.
  const int kIOBufferSize = 4 * 1024;
  scoped_ptr<char[]> buf(new char[kIOBufferSize]);

  // record read, write timestamp.
  int64 last_recv_time = tin::MonoNow();
  int64 last_send_time = last_recv_time;

  // set read, write deadline.
  const int64 kRWDeadline = 20 * tin::kSecond;
  conn->SetDeadline(kRWDeadline);
  while (true) {
    int n = conn->Read(buf.get(), kIOBufferSize);
    int err = tin::GetErrorCode();
    if (n > 0) {
      // update last recv time.
      last_recv_time = tin::MonoNow();
    }
    if (err == TIN_ETIMEOUT_INTR) {
      int64 elspsed = tin::MonoNow() - last_recv_time;
      // deadline reached.
      if (elspsed >= kRWDeadline) {
        break;
      }
      conn->SetReadDeadline(kRWDeadline);
    } else if (err != 0) {
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

    // write data back.
    bool write_failed = false;
    int left = n;
    while (left > 0) {
      int written = conn->Write(buf.get(), n);
      if (written > 0) {
        left -= written;
        last_send_time = tin::MonoNow();
      }
      int err = tin::GetErrorCode();
      if (err == TIN_ETIMEOUT_INTR) {
        int64 elspsed = tin::MonoNow() - last_send_time;
        // deadline reached.
        if (elspsed >= kRWDeadline) {
          write_failed = true;
          break;
        }
        conn->SetWriteDeadline(kRWDeadline);
      } else if (err != 0) {
        VLOG(1) << "Write failed due to " << tin::GetErrorStr();
        write_failed = true;
        break;
      }
    }
    if (write_failed) {
      break;
    }
  }
  // explicit Close to TcpConn is not really required, it's done in TcpConn's
  // destructor.
  conn->Close();
}

void Dispatch(tin::net::TcpConn conn, const int64 id) {
  conn->SetNoDelay(true);
  const int kNumModes = 3;
  int64 which = id % kNumModes;
  switch (which) {
  case 0: {
    tin::Spawn(&HandleClient0, conn);
    break;
  }
  case 1: {
    tin::Spawn(&HandleClient1, conn);
    break;
  }
  case 2: {
    tin::Spawn(&HandleClient2, conn);
    break;
  }
  break;
  default:
    break;
  }
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
  int64 id = 0;
  while (true) {
    tin::net::TcpConn conn = listener->Accept();
    if (tin::GetErrorCode() == 0) {
      Dispatch(conn, id);
      id++;
    } else {
      LOG(INFO) << "Accept failed due to " << tin::GetErrorStr();
    }
  }
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

