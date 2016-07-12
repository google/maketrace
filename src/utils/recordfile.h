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

#ifndef RECORDFILE_H
#define RECORDFILE_H

#include <QDataStream>
#include <QDir>
#include <QFile>

#include "utils/logging.h"

namespace utils {

template <typename T>
class RecordWriter {
 public:
  virtual ~RecordWriter() {}

  virtual void WriteRecord(const T& message) = 0;

  template <typename Container>
  void WriteAll(const Container& list);
};


template <typename T>
class RecordReader {
 public:
  virtual ~RecordReader() {}

  virtual bool AtEnd() const = 0;
  virtual bool ReadRecord(T* message) = 0;

  template <typename Container>
  bool ReadAll(Container* list);
};


template <typename T>
class RecordFile : public RecordReader<T>, public RecordWriter<T> {
 public:
  explicit RecordFile(QIODevice* device);
  explicit RecordFile(const QString& filename);

  QString filename() const;

  bool Open(QFile::OpenMode mode);

  // Reading.
  bool AtEnd() const override;
  bool ReadRecord(T* message) override;

  // Writing.
  void WriteRecord(const T& message) override;

  // Convenience functions.
  static QList<T> ReadAllFrom(const QString& filename);
  static void WriteAllTo(const QList<T>&, const QString& filename);

 private:
  QFile file_;
  QDataStream stream_;
};


template <typename T>
class MemoryRecordWriter : public RecordWriter<T> {
 public:
  explicit MemoryRecordWriter(QList<T>* records) : records_(records) {}

  void WriteRecord(const T& record) override { records_->append(record); }

 private:
  QList<T>* records_;
};


template <typename T>
RecordFile<T>::RecordFile(QIODevice* device)
    : stream_(device) {
}

template <typename T>
RecordFile<T>::RecordFile(const QString& filename)
    : file_(filename),
      stream_(&file_) {
}

template <typename T>
QString RecordFile<T>::filename() const {
  if (file_.fileName().isEmpty()) {
    return "<stream>";
  }
  return file_.fileName();
}

template <typename T>
bool RecordFile<T>::Open(QIODevice::OpenMode mode) {
  if (file_.fileName().isNull()) {
    LOG(FATAL) << "Tried to open a RecordFile created with a QIODevice";
  }

  if (file_.isOpen()) {
    file_.close();
  }

  return file_.open(mode);
}

template <typename T>
template <typename Container>
bool RecordReader<T>::ReadAll(Container* list) {
  list->clear();
  while (!AtEnd()) {
    T record;
    if (!ReadRecord(&record)) {
      return false;
    }
    list->push_back(record);
  }
  return true;
}

template <typename T>
bool RecordFile<T>::AtEnd() const {
  return stream_.atEnd();
}

template <typename T>
bool RecordFile<T>::ReadRecord(T* message) {
  QByteArray bytes;
  stream_ >> bytes;

  return message->ParseFromArray(bytes.constData(), bytes.size());
}

template <typename T>
template <typename Container>
void RecordWriter<T>::WriteAll(const Container& list) {
  for (const T& record : list) {
    WriteRecord(record);
  }
}

template <typename T>
void RecordFile<T>::WriteRecord(const T& message) {
  QByteArray bytes;
  bytes.resize(message.ByteSize());
  message.SerializeToArray(bytes.data(), bytes.size());
  stream_ << bytes;
}

template <typename T>
QList<T> RecordFile<T>::ReadAllFrom(const QString& filename) {
  QList<T> ret;

  RecordFile<T> file(filename);
  if (file.Open(QFile::ReadOnly)) {
    if (!file.ReadAll(&ret)) {
      LOG(ERROR) << "Failed to read records from: " << filename;
    }
  }
  return ret;
}

template <typename T>
void RecordFile<T>::WriteAllTo(const QList<T>& records, const QString& filename) {
  RecordFile<T> file(filename);
  if (!file.Open(QFile::WriteOnly)) {
    LOG(ERROR) << "Failed to open " << filename << " for writing";
    return;
  }

  file.WriteAll(records);
}

}  // namespace utils

#endif // RECORDFILE_H
