// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "tin/error/error.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/util.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/threadpoll.h"
#include "tin/io/ioutil.h"

namespace tin {

using namespace runtime;  // NOLINT

// OpenFile
class OpenFileWork : public GletWork {
 public:
  OpenFileWork(const path_t& name,  // NOLINT
               int& flags,  // NOLINT
               bool*& created,  // NOLINT
               file_error_t*& error)  // NOLINT
    : file_(kInvalidFile)
    , name_(name)
    , flags_(flags)
    , created_(created)
    , error_(error) {
  }

  virtual ~OpenFileWork() { }

  virtual void Run() {
    file_ = base::CreatePlatformFile(name_, flags_, created_, error_);
    Finalize();
  }

  file_t File() {
    return file_;
  }

 private:
  file_t file_;
  const path_t& name_;
  int& flags_;
  bool*& created_;
  file_error_t*& error_;
};

file_t OpenFile(const path_t& name,
                int flags,
                bool* created,
                file_error_t* error) {
  scoped_ptr<OpenFileWork> work(new OpenFileWork(name, flags, created, error));
  SubmitGletWork(work.get());
  return work->File();
}

// CloseFile
class CloseFileWork : public GletWork {
 public:
  CloseFileWork(file_t& file)  // NOLINT
    : file_(file) {
  }

  virtual ~CloseFileWork() { }

  virtual void Run() {
    succeed_ = base::ClosePlatformFile(file_);
    Finalize();
  }

  bool Succeed() const {
    return succeed_;
  }

 private:
  bool succeed_;
  file_t& file_;
};

bool CloseFile(file_t file) {
  scoped_ptr<CloseFileWork> work(new CloseFileWork(file));
  SubmitGletWork(work.get());
  return work->Succeed();
}

// TruncateFile
class TruncateFileWork : public GletWork {
 public:
  TruncateFileWork(file_t& file, int64& length)  // NOLINT
    : file_(file)
    , length_(length) {
  }

  virtual ~TruncateFileWork() { }

  virtual void Run() {
    succeed_ = base::TruncatePlatformFile(file_, length_);
    Finalize();
  }

  bool Succeed() const {
    return succeed_;
  }

 private:
  bool succeed_;
  file_t& file_;
  int64& length_;
};

bool TruncateFile(file_t file, int64_t length) {
  scoped_ptr<TruncateFileWork> work(new TruncateFileWork(file, length));
  SubmitGletWork(work.get());
  return work->Succeed();
}

// ReadFile
class ReadFileWork : public GletWork {
 public:
  ReadFileWork(file_t& file, char*& data, int& size)  // NOLINT
    : bytes_read_(-1)
    , file_(file)
    , data_(data)
    , size_(size) {
  }

  virtual ~ReadFileWork() { }

  virtual void Run() {
    int64_t offset =
      base::SeekPlatformFile(file_, base::PLATFORM_FILE_FROM_CURRENT, 0);
    if (offset >= 0)
      bytes_read_ = base::ReadPlatformFile(file_, offset, data_, size_);
    Finalize();
  }

  int BytesRead() const {
    return bytes_read_;
  }

 private:
  int bytes_read_;
  file_t& file_;
  char*& data_;
  int& size_;
};

int ReadFile(file_t file, char* data, int size) {
  scoped_ptr<ReadFileWork> work(new ReadFileWork(file, data, size));
  SubmitGletWork(work.get());
  return work->BytesRead();
}

// WriteFile
class WriteFileWork : public GletWork {
 public:
  WriteFileWork(file_t& file, const char*& data, int& size)  // NOLINT
    : bytes_written_(-1)
    , file_(file)
    , data_(data)
    , size_(size) {
  }

  virtual ~WriteFileWork() { }

  virtual void Run() {
    int64_t offset =
      base::SeekPlatformFile(file_, base::PLATFORM_FILE_FROM_CURRENT, 0);
    if (offset >= 0)
      bytes_written_ = base::WritePlatformFile(file_, offset, data_, size_);
    Finalize();
  }

  int BytesWritten() const {
    return bytes_written_;
  }

 private:
  int bytes_written_;
  file_t& file_;
  const char*& data_;
  int& size_;
};

int WriteFile(file_t file, const char* data, int size) {
  scoped_ptr<WriteFileWork> work(new WriteFileWork(file, data, size));
  SubmitGletWork(work.get());
  return work->BytesWritten();
}

// DeleteFile
class DeleteFileWork : public GletWork {
 public:
  DeleteFileWork(path_t& path, bool& recursive)  // NOLINT
    : path_(path)
    , recursive_(recursive) {
  }

  virtual ~DeleteFileWork() { }

  virtual void Run() {
    succeed_ = base::DeleteFile(path_, recursive_);
    Finalize();
  }

  bool Succeed() const {
    return succeed_;
  }

 private:
  bool succeed_;
  path_t& path_;
  bool& recursive_;
};

bool DeleteFile(path_t& path, bool recursive) {  // NOLINT
  scoped_ptr<DeleteFileWork> work(new DeleteFileWork(path, recursive));
  SubmitGletWork(work.get());
  return work->Succeed();
}

// handy functions.

file_t OpenFileForRead(const path_t& name, file_error_t* error) {
  int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ;
  return tin::OpenFile(name, flags, NULL, error);
}

}  // namespace tin
