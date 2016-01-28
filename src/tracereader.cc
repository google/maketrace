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

#include "tracereader.h"

#include "utils/path.h"

using utils::path::Extension;
using utils::path::Filename;

TraceReader::TraceReader() {
}

void TraceReader::IgnoreFileExtensions(
    std::initializer_list<QString> extensions) {
  for (const QString& extension : extensions) {
    file_extension_blacklist_.insert(extension);
  }
}

void TraceReader::IgnoreProcessFilenames(
    std::initializer_list<QString> filenames) {
  for (const QString& filename : filenames) {
    process_blacklist_.insert(filename);
  }
}

void TraceReader::Read(std::unique_ptr<utils::RecordFile<pb::Record>> file) {
  while (!file->AtEnd()) {
    pb::Record record;
    CHECK(file->ReadRecord(&record));

    if (record.has_metadata()) {
      metadata_ = record.metadata();
    } else if (record.has_process()) {
      const pb::Process* pb = &record.process();
      if (pb->argv_size() == 0) {
        continue;
      }

      if (process_blacklist_.contains(Filename(pb->filename()))) {
        continue;
      }

      while (processes_by_id_.size() <= pb->id()) {
        processes_by_id_.append(pb::Process());
      }
      processes_by_id_[pb->id()] = *pb;

      for (int i = 0; i < pb->files_size(); ++i) {
        const pb::File* file = &pb->files(i);

        if (file_extension_blacklist_.contains(Extension(file->filename()))) {
          continue;
        }

        events_.append(FileEvent{file->close_ordering(), pb->id(), i});
      }
    }
  }

  qSort(events_);
}
