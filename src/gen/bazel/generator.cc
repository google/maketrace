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

#include "generator.h"

#include <QDir>
#include <QRegularExpression>

#include "build.pb.h"
#include "gen/bazel/buildwriter.h"
#include "gen/bazel/label.h"
#include "utils/logging.h"

namespace gen {
namespace bazel {

static const char* kGeneratedFilePrefix = "_generated/";
static const char* kPublicHeaderPrefix = "public_headers/";

bool Generator::Run(const Options& opts) {
  // Read the targets.
  auto make_fh =
      make_unique<utils::RecordFile<pb::Record>>(opts.target_filename);
  if (!make_fh->Open(QIODevice::ReadOnly)) {
    LOG(ERROR) << "Failed to open " << make_fh->filename() << " for reading";
    return false;
  }

  // Read the installed files.
  auto installed_files_fh =
      make_unique<utils::RecordFile<pb::Record>>(opts.installed_files_filename);
  if (!installed_files_fh->Open(QIODevice::ReadOnly)) {
    LOG(ERROR) << "Failed to open " << installed_files_fh->filename()
               << " for reading";
    return false;
  }

  Generator gen(opts);
  gen.Generate(std::move(make_fh), std::move(installed_files_fh));
  return true;
}

Generator::Generator(const Options& opts)
    : opts_(opts) {
}

void Generator::AddTargetRecursive(const pb::BuildTarget& target, Rule* rule,
                                   Rule* binary_rule) {
  if (binary_rule == nullptr) {
    binary_rule = rule;
  }

  for (const pb::Reference& ref : target.srcs()) {
    switch (ref.type()) {
      case pb::Reference_Type_LIBRARY:
        if (ref.name() == "pthread") {
          binary_rule->add_linkopt("-pthread");
        } else {
          binary_rule->add_linkopt("-l" + ref.name());
        }
        break;

      case pb::Reference_Type_BUILD_TARGET:
        if (targets_.contains(ref.name())) {
          const pb::BuildTarget& dep = targets_[ref.name()];
          const Label dep_label = Label::FromAbsolute(dep.qualified_name());
          if (dep.has_c_compile()) {
            // Add .c and .h files directly.
            AddTargetRecursive(dep, rule, binary_rule);
          } else {
            // Otherwise add a dependency on the target.
            rule->add_src(ConvertLabel(dep_label));
          }
        } else {
          LOG(ERROR) << "Target " << target.qualified_name()
                     << " has unknown src " << ref.name();
        }
        break;

      case pb::Reference_Type_ABSOLUTE:
        LOG(WARNING) << "Ignoring absolute target src: " << ref.name();
        break;

      case pb::Reference_Type_RELATIVE_TO_PROJECT_ROOT:
        rule->add_src(ref.name());
        source_files_.insert(ref);
        break;

      case pb::Reference_Type_RELATIVE_TO_BUILD_DIR:
        rule->add_src(kGeneratedFilePrefix + ref.name());
        source_files_.insert(ref);
        break;
    }
  }

  for (const pb::Reference& ref : target.c_compile().headers()) {
    QString filename;
    switch (ref.type()) {
      case pb::Reference_Type_ABSOLUTE:
        break;

      case pb::Reference_Type_RELATIVE_TO_PROJECT_ROOT:
        filename = ref.name();
        source_files_.insert(ref);
        break;

      case pb::Reference_Type_RELATIVE_TO_BUILD_DIR:
        filename = kGeneratedFilePrefix + ref.name();
        break;

      default:
        LOG(FATAL) << "Bad type for header reference: "
                   << ref.ShortDebugString();
        break;
    }
    if (!filename.isEmpty()) {
      if (filename.endsWith(".h") ||
          filename.endsWith(".hh") ||
          filename.endsWith(".hpp") ||
          filename.endsWith(".hxx") ||
          filename.endsWith(".inc")) {
        rule->add_src(filename);
      } else {
        rule->add_textual_hdr(filename);
      }
      source_files_.insert(ref);
    }
  }

  for (const pb::Definition& def : target.c_compile().definition()) {
    if (def.has_value()) {
      rule->add_copt("-D" + def.name() + "=" + def.value());
    } else {
      rule->add_copt("-D" + def.name());
    }
  }

  for (const pb::Reference& ref : target.c_compile().header_search_path()) {
    switch (ref.type()) {
      case pb::Reference_Type_ABSOLUTE:
        rule->add_copt("-I" + ref.name());
        break;

      case pb::Reference_Type_RELATIVE_TO_PROJECT_ROOT:
        rule->add_copt("-I" + package_ + "/" + ref.name());
        break;

      case pb::Reference_Type_RELATIVE_TO_BUILD_DIR:
        rule->add_copt("-I" + package_ + "/" + kGeneratedFilePrefix +
                       ref.name());
        break;

      default:
        LOG(FATAL) << "Bad type for header search path reference: "
                   << ref.ShortDebugString();
        break;
    }
  }

  for (const QString& flag : target.c_compile().flag()) {
    rule->add_copt(flag);
    binary_rule->add_linkopt(flag);
  }
}

void Generator::Generate(
    std::unique_ptr<utils::RecordFile<pb::Record>> target_records,
    std::unique_ptr<utils::RecordFile<pb::Record>> installed_file_records) {
  installed_files_.Read(std::move(installed_file_records));

  // Read all the build targets.
  while (!target_records->AtEnd()) {
    pb::Record record;
    CHECK(target_records->ReadRecord(&record));

    if (record.has_metadata()) {
      metadata_ = record.metadata();
      if (!opts_.project_root.isEmpty()) {
        metadata_.set_project_root(opts_.project_root);
      }
    } else if (record.has_build_target()) {
      targets_[record.build_target().qualified_name()] = record.build_target();
    }
  }

  package_ = metadata_.project_name();
  package_.replace(QRegularExpression("[^a-zA-Z0-9_]"), "_");

  // Generate 1 rule for each link target.
  QList<blaze_query::Rule> rules;
  for (const pb::BuildTarget& target : targets_.values()) {
    if (!target.has_c_link()) {
      continue;
    }

    // Pick a name for the target
    const Label label = ConvertLabel(
        Label::FromAbsolute(target.qualified_name()));

    Rule rule(label);
    rule.set_type("cc_library");
    std::unique_ptr<Rule> binary_rule;

    if (!target.c_link().is_library()) {
      // cc_binary rules can't have textual_hdrs, so we need to generate 2
      // rules for binary targets.
      rule.set_label(Label(label.package(), label.target() + "_binary_lib"));

      binary_rule.reset(new Rule(label));
      binary_rule->set_type("cc_binary");
      binary_rule->add_dep(rule.label());
      if (target.install()) {
        binary_rule->add_visibility(Label("visibility", "public"));
      }
    } else {
      if (target.install()) {
        rule.add_visibility(Label("visibility", "public"));
      }
    }

    AddTargetRecursive(target, &rule, binary_rule.get());

    rules.append(rule.ToProto());
    if (binary_rule) {
      rules.append(binary_rule->ToProto());
    }
  }

  // Copy all the source files into the workspace.
  const QString package_dir = opts_.workspace_path + "/" + package_;
  for (const pb::Reference& ref : source_files_) {
    const QString source = AbsoluteSourceFilePath(ref);
    QString destination;

    switch (ref.type()) {
      case pb::Reference_Type_RELATIVE_TO_PROJECT_ROOT:
        destination = package_dir + "/" + ref.name();
        break;

      case pb::Reference_Type_RELATIVE_TO_BUILD_DIR:
        destination = package_dir + "/" + kGeneratedFilePrefix + ref.name();
        break;

      default:
        LOG(FATAL) << "Invalid source file reference type: "
                   << ref.ShortDebugString();
        break;
    }

    CopyFile(source, destination);
  }

  // Copy all the installed headers to the workspace, and make a filegroup rule
  // for them.
  const QList<pb::InstalledFile> installed_headers =
      installed_files_.AllOfType(pb::InstalledFile_Type_HEADER);
  Rule headers_rule(Label(metadata_.project_name(), "public_headers"),
                    "filegroup");
  headers_rule.add_visibility(Label("visibility", "public"));
  for (const pb::InstalledFile& header : installed_headers) {
    const QString dest_filename = kPublicHeaderPrefix +
                                  RemoveInstalledFilePrefix(header);
    const QString source = AbsoluteSourceFilePath(header.original());
    const QString destination = package_dir + "/" + dest_filename;

    CopyFile(source, destination);
    headers_rule.add_src(dest_filename);
  }

  if (headers_rule.has_srcs()) {
    rules.append(headers_rule.ToProto());
  }

  // Create the BUILD file for the package and write the rules.
  CHECK(QDir().mkpath(package_dir));

  const QString filename = package_dir + "/BUILD";
  QFile file(filename);
  CHECK(file.open(QFile::WriteOnly));

  QTextStream s(&file);
  BuildWriter w(&s);

  for (const blaze_query::Rule& rule : rules) {
    w.WriteRule(rule);
  }

  LOG(INFO) << "Wrote " << rules.count() << " rules to " << filename;
}

Label Generator::ConvertLabel(const Label& label) {
  return Label(
      metadata_.project_name(),
      label.package().replace('/', '_') + "_" + label.target());
}

QString Generator::AbsoluteSourceFilePath(const pb::Reference& ref) const {
  switch (ref.type()) {
    case pb::Reference_Type_RELATIVE_TO_PROJECT_ROOT:
      return metadata_.project_root() + "/" + ref.name();

    case pb::Reference_Type_RELATIVE_TO_BUILD_DIR:
      return metadata_.project_root() + "/" + metadata_.build_dir() + "/" +
             ref.name();

    default:
      LOG(FATAL) << "Invalid source file reference type: "
                 << ref.ShortDebugString();
      return QString();
  }
}

QString Generator::RemoveInstalledFilePrefix(const pb::InstalledFile& file) {
  for (const QString& prefix : {"/usr/local/include/", "/usr/include/"}) {
    if (file.target().name().startsWith(prefix)) {
      return file.target().name().mid(prefix.length());
    }
  }
  return file.target().name();
}

void Generator::CopyFile(const QString& source, const QString& dest) {
  // Create the destination directory.
  QDir dir(QFileInfo(dest).absolutePath());
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  // Copy the file.
  if (!QFile::exists(dest)) {
    if (QFile::copy(source, dest)) {
      LOG(INFO) << "Copied " << source << " to " << dest;
    } else {
      LOG(WARNING) << "Failed to copy " << source << " to " << dest;
    }
  }
}

}  // namespace bazel
}  // namespace gen
