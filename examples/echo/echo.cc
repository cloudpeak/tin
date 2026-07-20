#include "tin/tin.h"
#include "tin/config.h"
#include "tin/status.h"
#include "tin/result.h"
#include "tin/time.h"
#include "tin/runtime.h"
#include "tin/net/tcp.h"

#include <absl/log/log.h>
#include <absl/log/check.h>
#include <absl/log/globals.h>
#include <thread>
#include <memory>
#include <cstdint>

// case 0
void HandleClient0(tin::net::TcpConn conn) {
  // Set TCP Read Write buffer.
  conn.SetReadBuffer(64 * 1024);
  conn.SetWriteBuffer(64 * 1024);

  // user space buffer size.
  const int kIoBufferSize = 4 * 1024;
  std::unique_ptr<char[]> buf(new char[kIoBufferSize]);

  // set read, write deadline.
  const int64_t kRWDeadline = 20 * tin::kSecond;
  conn.SetDeadline(kRWDeadline);
  while (true) {
    auto read_result = conn.Read(buf.get(), kIoBufferSize);
    size_t n = read_result.value_or(0);
    if (n > 0) {
      conn.SetReadDeadline(kRWDeadline);
    }
    if (!read_result.ok()) {
      VLOG(1) << "Read failed due to: " << read_result.error().ToString();
      // FIN received, graceful close, we can still send.
      if (read_result.error().IsEOF()) {
        if (n > 0) {
          conn.Write(buf.get(), static_cast<int>(n));
        }
        conn.CloseWrite();
        // delay a while to avoid RST.
        tin::NanoSleep(500 * tin::kMillisecond);
      }
      break;
    }
    DCHECK_GT(n, 0u);
    auto write_result = conn.Write(buf.get(), static_cast<int>(n));
    if (!write_result.ok()) {
      VLOG(1) << "Write failed due to " << write_result.error().ToString();
      break;
    }
    conn.SetWriteDeadline(kRWDeadline);
  }
  conn.Close();
}

// case 1, optimize for timeout, since SetReadDeadline is expensive.
void HandleClient1(tin::net::TcpConn conn) {
  // Set TCP Read Write buffer.
  conn.SetReadBuffer(64 * 1024);
  conn.SetWriteBuffer(64 * 1024);

  // user space buffer size.
  const int kIoBufferSize = 4 * 1024;
  std::unique_ptr<char[]> buf(new char[kIoBufferSize]);

  // record read,  write timestamp.
  int64_t last_set_recv_time = tin::MonoNow();
  int64_t last_set_send_time = last_set_recv_time;

  // set read, write deadline.
  const int64_t kRWDeadline = 20 * tin::kSecond;
  conn.SetDeadline(kRWDeadline);
  while (true) {
    auto read_result = conn.Read(buf.get(), kIoBufferSize);
    size_t n = read_result.value_or(0);
    if (n > 0) {
      int64_t now = tin::MonoNow();
      int64_t elapsed = now - last_set_recv_time;
      if (elapsed >= 5 * tin::kSecond) {
        conn.SetReadDeadline(kRWDeadline);
        last_set_recv_time = now;
        VLOG(1) << "reset read deadline";
      }
    }
    if (!read_result.ok()) {
      VLOG(1) << "Read failed due to: " << read_result.error().ToString();
      // FIN received, graceful close, we can still send.
      if (read_result.error().IsEOF()) {
        if (n > 0) {
          conn.Write(buf.get(), static_cast<int>(n));
        }
        conn.CloseWrite();
        // delay a while to avoid RST.
        tin::NanoSleep(500 * tin::kMillisecond);
      }
      break;
    }
    DCHECK_GT(n, 0u);
    auto write_result = conn.Write(buf.get(), static_cast<int>(n));
    if (!write_result.ok()) {
      VLOG(1) << "Write failed due to " << write_result.error().ToString();
      break;
    } else {
      int64_t now = tin::MonoNow();
      int64_t elapsed = now - last_set_send_time;
      if (elapsed >= 5 * tin::kSecond) {
        conn.SetWriteDeadline(kRWDeadline);
        last_set_send_time = now;
        VLOG(1) << "reset write deadline";
      }
    }
  }
  conn.Close();
}

// case 2, optimize for timeout, since SetReadDeadline is expensive.
void HandleClient2(tin::net::TcpConn conn) {
  // Set TCP Read Write buffer.
  conn.SetReadBuffer(64 * 1024);
  conn.SetWriteBuffer(64 * 1024);

  // user space buffer size.
  const int kIoBufferSize = 4 * 1024;
  std::unique_ptr<char[]> buf(new char[kIoBufferSize]);

  // record read, write timestamp.
  int64_t last_recv_time = tin::MonoNow();
  int64_t last_send_time = last_recv_time;

  // set read, write deadline.
  const int64_t kRWDeadline = 20 * tin::kSecond;
  conn.SetDeadline(kRWDeadline);
  while (true) {
    auto read_result = conn.Read(buf.get(), kIoBufferSize);
    size_t n = read_result.value_or(0);
    if (n > 0) {
      // update last recv time.
      last_recv_time = tin::MonoNow();
    }
    if (read_result.error().IsTimeout()) {
      int64_t elapsed = tin::MonoNow() - last_recv_time;
      // deadline reached.
      if (elapsed >= kRWDeadline) {
        break;
      }
      conn.SetReadDeadline(kRWDeadline);
    } else if (!read_result.ok()) {
      VLOG(1) << "Read failed due to: " << read_result.error().ToString();
      // FIN received, graceful close, we can still send.
      if (read_result.error().IsEOF()) {
        if (n > 0) {
          conn.Write(buf.get(), static_cast<int>(n));
        }
        conn.CloseWrite();
        // delay a while to avoid RST.
        tin::NanoSleep(500 * tin::kMillisecond);
      }
      break;
    }

    // write data back.
    bool write_failed = false;
    int left = static_cast<int>(n);
    while (left > 0) {
      auto write_result = conn.Write(buf.get(), static_cast<int>(n));
      size_t written = write_result.value_or(0);
      if (written > 0) {
        left -= static_cast<int>(written);
        last_send_time = tin::MonoNow();
      }
      if (write_result.error().IsTimeout()) {
        int64_t elapsed = tin::MonoNow() - last_send_time;
        // deadline reached.
        if (elapsed >= kRWDeadline) {
          write_failed = true;
          break;
        }
        conn.SetWriteDeadline(kRWDeadline);
      } else if (!write_result.ok()) {
        VLOG(1) << "Write failed due to " << write_result.error().ToString();
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
  conn.Close();
}

void Dispatch(tin::net::TcpConn conn, const int64_t id) {
  conn.SetNoDelay(true);
  const int kNumModes = 3;
  int64_t which = id % kNumModes;
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
  constexpr uint16_t kPort = 2222;
  const bool use_ipv6 = false;
  const char* listen_address = use_ipv6 ? "0:0:0:0:0:0:0:0" : "0.0.0.0";

  auto listen_result = tin::net::ListenTcp(listen_address, kPort);
  if (!listen_result.ok()) {
    LOG(FATAL) << "Listen failed: " << listen_result.error().ToString();
    return 1;
  }

  tin::net::TcpListener listener = std::move(listen_result.value());
  LOG(INFO) << "Echo server listening on port: " << kPort;

  int64_t connection_id = 0;
  while (true) {
    auto accept_result = listener.Accept();
    if (!accept_result.ok()) {
      LOG(ERROR) << "Accept failed: " << accept_result.error().ToString();
      continue;
    }

    Dispatch(std::move(accept_result.value()), connection_id);
    ++connection_id;
  }

  return 0;
}
 
int main(int argc, char** argv) {
  absl::SetMinLogLevel(absl::LogSeverityAtLeast::kInfo);

  tin::Config config = tin::DefaultConfig();
  config.SetMaxProcs(static_cast<int>(std::thread::hardware_concurrency()));

  return tin::Run(TinMain, argc, argv, config);
}
