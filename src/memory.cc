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

#include "memory.h"

#include <sys/ptrace.h>

#include "utils/logging.h"

QString Memory::ReadNullTerminatedUtf8(void* addr) const {
  return QString::fromUtf8(ReadNullTerminated(addr));
}

void Memory::WriteNullTerminated(const QByteArray& data, void* addr) const {
  QByteArray copy(data);
  copy.append('\0');
  Write(copy, addr);
}

void Memory::WriteNullTerminatedUtf8(const QString& data, void* addr) const {
  WriteNullTerminated(data.toUtf8(), addr);
}

QStringList Memory::ReadNullTerminatedUtf8Array(void* addr) const {
  QStringList ret;
  if (addr == nullptr) {
    return ret;
  }

  char* p = static_cast<char*>(addr);
  forever {
    // Read a pointer to a string.
    QByteArray pointer = Read(p, sizeof(char*));
    if (pointer.length() != sizeof(char*)) {
      break;
    }
    p += sizeof(char*);

    void* ptr = *reinterpret_cast<void* const*>(pointer.constData());
    if (ptr == nullptr) {
      break;
    }

    // Read the string itself.
    ret.append(ReadNullTerminatedUtf8(ptr));
  }
  return ret;
}

QByteArray LocalMemory::Read(void* addr, size_t length) const {
  if (addr == nullptr) {
    return QByteArray();
  }
  return QByteArray(reinterpret_cast<char*>(addr), length);
}

QByteArray LocalMemory::ReadNullTerminated(void* addr) const {
  QByteArray ret;

  char* p = static_cast<char*>(addr);
  forever {
    const QByteArray ch = Read(p, 1);
    if (ch.isEmpty() || ch[0] == '\0') {
      break;
    }
    ret.append(ch);
  }
  return ret;
}

void LocalMemory::Write(const QByteArray& data, void* addr) const {
  if (addr == nullptr) {
    return;
  }
  memcpy(addr, data.constData(), data.length());
}

QByteArray TraceeMemory::Read(void* addr, size_t length) const {
  QByteArray ret;
  if (addr == nullptr) {
    return ret;
  }

  ret.reserve(length);

  char* p = static_cast<char*>(addr);
  char* const end = p + length;
  while (p != end) {
    const long data = ptrace(PTRACE_PEEKDATA, pid_, p, nullptr);
    for (uint i = 0; i < sizeof(data) && p != end; ++i, ++p) {
      ret.append(*(reinterpret_cast<const char*>(&data) + i));
    }
  }
  return ret;
}

QByteArray TraceeMemory::ReadNullTerminated(void* addr) const {
  QByteArray ret;
  if (addr == nullptr) {
    return ret;
  }

  char* p = static_cast<char*>(addr);
  forever {
    const long data = ptrace(PTRACE_PEEKDATA, pid_, p, nullptr);

    for (uint i = 0; i < sizeof(data); ++i) {
      const char c = *(reinterpret_cast<const char*>(&data) + i);
      if (c == 0) {
        return ret;
      } else {
        ret.append(c);
      }
    }

    p += sizeof(data);
  }
}

void TraceeMemory::Write(const QByteArray& data, void* addr) const {
  if (addr == nullptr) {
    return;
  }

  const int bytes_before = reinterpret_cast<long>(addr) % sizeof(long);
  const int bytes_after =
      (sizeof(long) - (data.length() + bytes_before)) % sizeof(long);

  QByteArray padded_data = data;
  if (bytes_after != 0) {
    padded_data.append(
        Read(reinterpret_cast<char*>(addr) + padded_data.length(),
             bytes_after));
  }

  if (bytes_before != 0) {
    addr = reinterpret_cast<char*>(addr) - bytes_before;
    padded_data.insert(0, Read(addr, bytes_before));
  }

  long* client_p = static_cast<long*>(addr);
  const long* p = reinterpret_cast<const long*>(padded_data.constData());
  const long* const end = reinterpret_cast<const long* const>(
      padded_data.constData() + padded_data.length());

  while (p != end) {
    if (ptrace(PTRACE_POKEDATA, pid_, client_p, *p) != 0) {
      LOG(WARNING) << pid_ << " failed to write bytes " << data << " to "
                   << client_p;
    }
    client_p ++;
    p ++;
  }
}
