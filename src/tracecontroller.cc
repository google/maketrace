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

#include "common.h"
#include "tracecontroller.h"
#include "tracer.h"
#include "tracer.pb.h"
#include "utils/path.h"
#include "utils/recordfile.h"

namespace trace_controller {

bool Run(Options opts) {
  if (opts.working_directory.isEmpty()) {
    opts.working_directory = QDir::currentPath();
  }
  if (opts.project_root.isEmpty()) {
    opts.project_root = opts.working_directory;
  }

  // Open the file.
  auto file = make_unique<utils::RecordFile<pb::Record>>(
      opts.output_filename);
  if (!file->Open(QFile::WriteOnly)) {
    LOG(ERROR) << "Failed to open " << opts.output_filename
               << " for writing";
    return false;
  }

  // Write the metadata record.
  pb::Record metadata_record;
  pb::MetaData* metadata = metadata_record.mutable_metadata();
  metadata->set_project_root(opts.project_root);
  metadata->set_project_name(opts.project_name);

  if (opts.project_root != QDir::currentPath()) {
    metadata->set_build_dir(
        utils::path::MakeRelativeTo(QDir::currentPath(),
                                    opts.project_root));
  }
  file->WriteRecord(metadata_record);

  // Start the trace.
  Tracer t(opts.project_root,
           std::unique_ptr<utils::RecordWriter<pb::Record>>(file.release()));
  if (!t.Start(Tracer::Subprocess(opts.args, opts.working_directory))) {
    return false;
  }
  return t.TraceUntilExit();
}

}  // namespace trace_controller
