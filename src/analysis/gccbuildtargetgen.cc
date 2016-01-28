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

#include "analysis/gccbuildtargetgen.h"

#include "reference.h"
#include "analysis/make.h"
#include "utils/path.h"
#include "utils/str.h"

namespace analysis {

GccBuildTargetGen::GccBuildTargetGen()
    : cc_compile_input_extensions_({
        "c",
        "C",
        "cc",
        "cpp",
        "cxx",
        "h",
        "hh",
        "hpp",
        "hxx",
        "inc",
        "S",
      }),
      cc_compile_output_extensions_({
        "o",
        "lo",
      }),
      cc_link_input_extensions_({
        "a",
        "lo",
        "o",
        "so",
      }),
      cc_link_standard_libs_({
        "c",
        "gcc",
        "gcc_s",
        "stdc++",
      }) {
}

bool GccBuildTargetGen::Gen(const TraceNode& node, pb::BuildTarget* target) {
  if (node.type_ != TraceNode::Type::CompileStep &&
      node.type_ != TraceNode::Type::DynamicLinkStep) {
    return false;
  }

  const pb::Process& proc = make_->process(node.process_id_);

  QStringList flags;
  QSet<QString> library_search_path;
  QSet<QString> header_search_path;
  QSet<QString> deps;
  QMap<QString, QPair<bool, QString>> definitions;
  bool is_compile = false;
  bool is_library = false;

  const auto CanonicalizePath = [this, &proc](const QString& path) {
    return utils::path::MakeRelativeTo(
        utils::path::MakeAbsolute(path, proc.working_directory()),
        make_->metadata().project_root());
  };

  const QSet<QString> standard_library_search_path =
      make_->tool_search_path()->Get(proc.filename());

  const bool is_cc = proc.filename().endsWith("++");

  for (int i = 1; i < proc.argv_size(); ++i) {
    const QString arg = proc.argv()[i];
    QString next_arg;
    if (i < proc.argv_size() - 1) {
      next_arg = proc.argv()[i + 1];
    }

    if (arg.startsWith("-Wl,") ||
        arg.startsWith("-M") ||
        arg.startsWith("-O") ||
        arg.startsWith("--sysroot=") ||
        arg.startsWith("--hash-style=") ||
        arg.startsWith("-soname=") ||
        arg == "-g" ||
        arg == "-m" ||
        arg == "-pg" ||
        arg == "-fPIC" ||
        arg == "-nostdlib" ||
        arg == "--eh-frame-hdr" ||
        arg == "--build-id" ||
        arg == "--as-needed" ||
        arg == "--no-as-needed" ||
        arg == "-dynamic-linker") {
      // Ignore.
    } else if (arg.startsWith("-W") ||
               arg.startsWith("-f") ||
               arg.startsWith("-std")) {
      flags.push_back(arg);
    } else if (arg.startsWith("-D")) {
      const QString name_value = arg.mid(2);
      const int equals_pos = name_value.indexOf('=');
      if (equals_pos == -1) {
        definitions.insert(name_value, {false, ""});
      } else {
        definitions.insert(name_value.left(equals_pos),
                           {true, name_value.mid(equals_pos + 1)});
      }
    } else if (arg.startsWith("-U")) {
      definitions.remove(arg.mid(2));
    } else if (arg.startsWith("-L")) {
      const QString path = CanonicalizePath(arg.mid(2));
      if (!standard_library_search_path.contains(path)) {
        library_search_path.insert(path);
      }
    } else if (arg.startsWith("-I")) {
      header_search_path.insert(CanonicalizePath(arg.mid(2)));
    } else if (arg.startsWith("-l")) {
      if (!cc_link_standard_libs_.contains(arg.mid(2))) {
        deps.insert(arg);
      }
    } else if (!arg.startsWith("-")) {
      // TODO: handle relative paths.
    } else if (arg == "-o") {
      // TODO: handle relative paths.
      ++i;
    } else if (arg == "-c") {
      is_compile = true;
    } else if (arg == "-shared") {
      is_library = true;
    } else if (arg == "-pthread") {
      deps.insert("-lpthread");
    } else if (arg == "-MF" ||
               arg == "-MT" ||
               arg == "-MQ" ||
               arg == "-z" ||
               arg == "-soname") {
      // Ignore this and the next one.
      ++i;
    } else {
      LOG(WARNING) << "Unknown GCC argument: " << arg;
      return false;
    }
  }

  CHECK((is_compile && node.type_ == TraceNode::Type::CompileStep) ||
        (!is_compile && node.type_ == TraceNode::Type::DynamicLinkStep));

  // Add the flags and header/library search paths.
  if (is_compile) {
    pb::CCompile* compile = target->mutable_c_compile();
    compile->mutable_flag()->append(flags);
    compile->set_is_cc(is_cc);
    for (const QString& path : header_search_path.values()) {
      make_->CreateReference(path, compile->add_header_search_path());
    }
    for (auto it = definitions.begin(); it != definitions.end(); ++it) {
      pb::Definition* definition = compile->add_definition();
      definition->set_name(it.key());
      if (it.value().first) {
        definition->set_value(it.value().second);
      }
    }

    const pb::Process& frontend =
        make_->process(node.compiler_frontend_process_id_);

    QSet<QString> headers;
    for (const pb::File& file : frontend.files()) {
      if (file.access() == pb::File_Access_READ &&
          file.filename().endsWith(".h")) {
        headers.insert(file.filename());
      }
    }
    for (const QString& path : headers.toList()) {
      make_->CreateReference(path, compile->add_headers());
    }
    qSort(*compile->mutable_headers());
  } else {
    pb::CLink* link = target->mutable_c_link();
    link->mutable_flag()->append(flags);
    link->set_is_library(is_library);
    link->set_is_cc(is_cc);
    for (const QString& path : library_search_path.values()) {
      make_->CreateReference(path, link->add_library_search_path());
    }
  }

  if (AddInputs(node, target, is_compile ? cc_compile_input_extensions_ :
                                           cc_link_input_extensions_,
                is_compile ? 1 : 0) < 1) {
    LOG(INFO) << "Not enough inputs to " << node.ID();
    return false;
  }
  if (AddOutputs(node, target, is_compile ? cc_compile_output_extensions_ :
                                            QSet<QString>(),
                 1) < 1) {
    LOG(INFO) << "Not enough outputs from " << node.ID();
    return false;
  }

  // Add dependencies.
  for (const QString& dep : deps.values()) {
    make_->CreateReference(dep, target->add_srcs());
  }

  if (is_compile) {
    target->set_qualified_name(make_->NewTargetName(target->srcs(0).name()));
  } else {
    target->set_qualified_name(make_->NewTargetName(target->outputs(0).name()));
  }
  return true;
}

}  // namespace analysis
