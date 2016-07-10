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

#include "analysis/install.h"

#include "make_unique.h"
#include "reference.h"

namespace analysis {

Install::Install(const Options& opts)
    : opts_(opts) {
}

bool Install::Run(const Options& opts) {
  Install i(opts);
  if (!i.OpenTrace()) {
    return false;
  }
  i.FindInstalledFiles();
  if (!i.WriteOutput()) {
    return false;
  }
  return true;
}

bool Install::OpenTrace() {
  auto file = make_unique<utils::RecordFile<pb::Record>>(opts_.trace_filename);
  if (!file->Open(QIODevice::ReadOnly)) {
    LOG(ERROR) << "Failed to open " << opts_.trace_filename << " for reading";
    return false;
  }

  trace_.Read(std::move(file));
  return true;
}

void Install::FindInstalledFiles() {
  QMap<QByteArray, pb::Reference> project_files;
  QMap<pb::Reference, QByteArray> installed_files;
  for (const FileEvent& event : trace_.events()) {
    const pb::Process* proc = &trace_.process(event.process_id);
    const pb::File* file = &proc->files(event.file_index);

    pb::Reference ref;
    CreateReference(trace_.metadata(), file->filename(), &ref);

    if (file->has_sha1_before() &&
        file->access() == pb::File_Access_READ &&
        (ref.type() == pb::Reference_Type_RELATIVE_TO_BUILD_DIR ||
         ref.type() == pb::Reference_Type_RELATIVE_TO_PROJECT_ROOT)) {
      project_files[file->sha1_before()] = ref;
    }

    // Don't check for access type - files that exist already with the same
    // contents will count as a read-only access.
    if (file->has_sha1_after() &&
        ref.type() == pb::Reference_Type_ABSOLUTE) {
      installed_files[ref] = file->sha1_after();
    }
  }

  for (auto it = installed_files.constBegin();
       it != installed_files.constEnd(); ++it) {
    const pb::Reference& installed_ref = it.key();
    const QByteArray hash = it.value();
    if (!project_files.contains(hash)) {
      continue;
    }

    const pb::Reference& source_ref = project_files[hash];

    pb::InstalledFile file;
    file.mutable_original()->CopyFrom(source_ref);
    file.mutable_target()->CopyFrom(installed_ref);

    if (installed_ref.name().endsWith(".h") ||
        installed_ref.name().endsWith(".hpp")) {
      file.set_type(pb::InstalledFile_Type_HEADER);
      files_.append(file);
    } else if (installed_ref.name().endsWith(".a") ||
               installed_ref.name().endsWith(".so")) {
      file.set_type(pb::InstalledFile_Type_LIBRARY);
      files_.append(file);
    } else if (installed_ref.name().contains("/bin/")) {
      file.set_type(pb::InstalledFile_Type_BINARY);
      files_.append(file);
    } else {
      LOG(WARNING) << "Installed file not recognised: "
                   << installed_ref.name()
                   << " (from " << source_ref.name() << ")";
    }
  }
}

bool Install::WriteOutput() {
  utils::RecordFile<pb::Record> output(opts_.output_filename);
  if (!output.Open(QIODevice::WriteOnly)) {
    LOG(ERROR) << "Failed to open " << opts_.output_filename << " for writing";
    return false;
  }

  pb::Record metadata_record;
  metadata_record.mutable_metadata()->CopyFrom(trace_.metadata());
  output.WriteRecord(metadata_record);

  for (const pb::InstalledFile& file : files_) {
    pb::Record record;
    record.mutable_installed_file()->CopyFrom(file);
    output.WriteRecord(record);
  }

  LOG(INFO) << "Written " << files_.count() << " installed files to "
            << opts_.output_filename;
  return true;
}

}  // namespace analysis
