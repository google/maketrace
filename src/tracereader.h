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

#ifndef TRACEREADER_H
#define TRACEREADER_H

#include <memory>

#include "tracer.pb.h"
#include "utils/recordfile.h"

struct FileEvent {
  int ordering;
  int process_id;
  int file_index;

  bool operator <(const FileEvent& other) const {
    return ordering < other.ordering;
  }
};


class TraceReader {
 public:
  TraceReader();

  void IgnoreProcessFilenames(std::initializer_list<QString> filename);
  void IgnoreFileExtensions(std::initializer_list<QString> extension);

  void Read(std::unique_ptr<utils::RecordFile<pb::Record>> file);

  const pb::MetaData& metadata() const { return metadata_; }
  QList<FileEvent> events() const { return events_; }
  const pb::Process& process(int id) const { return processes_by_id_[id]; }

 private:
  QSet<QString> process_blacklist_;
  QSet<QString> file_extension_blacklist_;

  pb::MetaData metadata_;
  QList<FileEvent> events_;
  QList<pb::Process> processes_by_id_;
};

#endif // TRACEREADER_H
