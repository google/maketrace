// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MEMORY_H
#define MEMORY_H

#include <QByteArray>
#include <QString>
#include <QStringList>

// Abstraction for reading and writing bytes and strings to and from memory.
class Memory {
 public:
  virtual ~Memory() {}

  virtual QByteArray Read(void* addr, size_t length) const = 0;
  virtual QByteArray ReadNullTerminated(void* addr) const = 0;
  QString ReadNullTerminatedUtf8(void* addr) const;
  QStringList ReadNullTerminatedUtf8Array(void* addr) const;

  virtual void Write(const QByteArray& data, void* addr) const = 0;
  void WriteNullTerminated(const QByteArray& data, void* addr) const;
  void WriteNullTerminatedUtf8(const QString& data, void* addr) const;
};


// Operates on this process' memory.
class LocalMemory : public Memory {
 public:
  QByteArray Read(void* addr, size_t length) const override;
  QByteArray ReadNullTerminated(void* addr) const override;
  void Write(const QByteArray& data, void* addr) const override;
};


// Operates on a ptraced process that is in a ptrace-stopped state.
class TraceeMemory : public Memory {
 public:
  explicit TraceeMemory(pid_t pid) : pid_(pid) {}

  QByteArray Read(void* addr, size_t length) const override;
  QByteArray ReadNullTerminated(void* addr) const override;
  void Write(const QByteArray& data, void* addr) const override;

 private:
  pid_t pid_;
};

#endif // MEMORY_H
