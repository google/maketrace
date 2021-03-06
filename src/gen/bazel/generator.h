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

#ifndef GEN_BAZEL_GENERATOR_H
#define GEN_BAZEL_GENERATOR_H

#include "common.h"
#include "installedfilesreader.h"
#include "tracer.pb.h"
#include "gen/bazel/rule.h"
#include "utils/recordfile.h"

namespace gen {
namespace bazel {

class Label;

class Generator {
 public:
  struct Options {
    // Read build target records from this file.
    QString target_filename;

    // Read InstalledFile records from this file.
    QString installed_files_filename;

    // Write BUILD files to this directory.
    QString workspace_path;

    // If this is set, the project_root metadata field will be changed to this.
    QString project_root;
  };

  static bool Run(const Options& opts);

 private:
  Generator(const Options& opts);

  void Generate(
      std::unique_ptr<utils::RecordFile<pb::Record>> target_records,
      std::unique_ptr<utils::RecordFile<pb::Record>> installed_file_records);

  Label ConvertLabel(const Label& label);
  void AddTargetRecursive(const pb::BuildTarget& target, Rule* rule,
                          Rule* binary_rule);

  static QString RemoveInstalledFilePrefix(const pb::InstalledFile& file);
  QString AbsoluteSourceFilePath(const pb::Reference& ref) const;
  static void CopyFile(const QString& source, const QString& dest);

  const Options opts_;
  QString package_;

  pb::MetaData metadata_;
  InstalledFilesReader installed_files_;

  QMap<QString, pb::BuildTarget> targets_;
  QSet<pb::Reference> source_files_;
};

}  // namespace bazel
}  // namespace gen

#endif // GEN_BAZEL_GENERATOR_H
