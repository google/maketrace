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

#include "configure.h"
#include "make_unique.h"
#include "reference.h"

namespace analysis {

Configure::Configure(const Options& opts)
    : opts_(opts) {
}

Configure::~Configure() {
}

bool Configure::Run(const Options& opts) {
  Configure c(opts);
  if (!c.OpenTrace()) {
    return false;
  }
  c.FindCreatedFiles();
  if (!c.WriteOutput()) {
    return false;
  }
  return true;
}

bool Configure::OpenTrace() {
  auto file = make_unique<utils::RecordFile<pb::Record>>(opts_.trace_filename);
  if (!file->Open(QIODevice::ReadOnly)) {
    LOG(ERROR) << "Failed to open " << opts_.trace_filename << " for reading";
    return false;
  }

  trace_.Read(std::move(file));
  return true;
}

void Configure::FindCreatedFiles() {
  QSet<QString> filenames;
  for (const FileEvent& event : trace_.events()) {
    const pb::Process* pb = &trace_.process(event.process_id);
    const pb::File* file = &pb->files(event.file_index);

    if (file->filename().startsWith("/")) {
      continue;
    }

    if (file->has_renamed_from()) {
      filenames.remove(file->renamed_from());
      filenames.insert(file->filename());
    } else {
      switch (file->access()) {
        case pb::File_Access_CREATED:
        case pb::File_Access_WRITTEN_BUT_UNCHANGED:
          filenames.insert(file->filename());
          break;
        case pb::File_Access_DELETED:
          filenames.remove(file->filename());
          break;
        default:
          break;
      }
    }
  }

  for (const QString& filename : filenames) {
    pb::Reference ref;
    CreateReference(trace_.metadata(), filename, &ref);

    if (ref.type() == pb::Reference_Type_RELATIVE_TO_BUILD_DIR &&
        (ref.name().startsWith("CMakeFiles/") ||
         ref.name().endsWith(".cmake") ||
         ref.name() == "CMakeCache.txt")) {
      continue;
    }

    if (filename.endsWith("Makefile") ||
        filename.endsWith("Makefile.in") ||
        filename == "libtool" ||
        filename == "stamp.h") {
      continue;
    }

    output_.add_generated_file()->CopyFrom(ref);
  }
}

bool Configure::WriteOutput() {
  utils::RecordFile<pb::Record> output(opts_.output_filename);
  if (!output.Open(QIODevice::WriteOnly)) {
    LOG(ERROR) << "Failed to open " << opts_.output_filename << " for writing";
    return false;
  }

  pb::Record metadata_record;
  metadata_record.mutable_metadata()->CopyFrom(trace_.metadata());
  output.WriteRecord(metadata_record);

  pb::Record configure_record;
  configure_record.mutable_configure_output()->CopyFrom(output_);
  output.WriteRecord(configure_record);

  LOG(INFO) << "Written " << output_.generated_file_size()
            << " filenames to " << opts_.output_filename;
  return true;
}

}  // namespace analysis
