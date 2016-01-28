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

#include "gen/ninja/ninjagenerator.h"

#include <gflags/gflags.h>

#include <QFile>
#include <QTextStream>

#include "utils/str.h"

DEFINE_string(ninja_build_directory, "build",
              "Build directory to write generated files");

NinjaGenerator::NinjaGenerator() {
}

void NinjaGenerator::Generate(const QList<pb::BuildTarget>& targets) {
  for (const pb::BuildTarget& target : targets) {
    targets_[target.qualified_name()] = target;
  }

  QFile file("build.ninja");
  file.open(QFile::WriteOnly);

  QTextStream s(&file);

  s << "builddir = " << utils::str::StlToQt(FLAGS_ninja_build_directory)
    << "\n";
  s << "c_compiler = gcc\n";
  s << "cc_compiler = g++\n";
  s << "c_compiler_flags = -fPIC\n";
  s << "c_link_library_flags = \n";
  s << "c_link_binary_flags = \n";
  s << "\n";
  s << "rule c_compile\n";
  s << "  command = $c_compiler $c_compiler_flags $flags $definitions $header_search_path -c $in -o $out\n";
  s << "\n";
  s << "rule c_link_library\n";
  s << "  command = $c_compiler $c_link_library_flags $flags -shared $library_search_path $in $libs -o $out\n";
  s << "\n";
  s << "rule c_link_binary\n";
  s << "  command = $c_compiler $c_link_binary_flags $flags $library_search_path $in $libs -o $out\n";
  s << "\n";
  s << "rule cc_compile\n";
  s << "  command = $cc_compiler $c_compiler_flags $flags $definitions $header_search_path -c $in -o $out\n";
  s << "\n";
  s << "rule cc_link_library\n";
  s << "  command = $cc_compiler $c_link_library_flags $flags -shared $library_search_path $in $libs -o $out\n";
  s << "\n";
  s << "rule cc_link_binary\n";
  s << "  command = $cc_compiler $c_link_binary_flags $flags $library_search_path $in $libs -o $out\n";
  s << "\n";

  for (const pb::BuildTarget& target : targets) {
    if (target.has_c_compile()) {
      WriteCompileTarget(target, s);
    } else if (target.has_c_link()) {
      WriteLinkTarget(target, s);
    } else {
      continue;
    }
    s << "\n\n";
  }
}

QStringList NinjaGenerator::OutputFilenames(
    const pb::BuildTarget& target) const {
  QStringList ret;
  for (const QString& output : target.outputs()) {
    ret.append("$builddir/" + output);
  }
  return ret;
}

QStringList NinjaGenerator::InputFilenames(
    const pb::BuildTarget& target) const {
  // Write any object files before any libraries, so that linking order is
  // correct.
  QStringList files, targets;
  for (const QString& input : target.srcs()) {
    if (input.startsWith("//__lib__/")) {
      continue;
    } else if (input.startsWith("//")) {
      const pb::BuildTarget& input_target = targets_[input];
      QStringList* out = input_target.has_c_link() ? &targets : &files;
      out->append(OutputFilenames(input_target));
    } else {
      files.append(input);
    }
  }
  return files + targets;
}

void NinjaGenerator::WriteCompileTarget(const pb::BuildTarget& target,
                                        QTextStream& s) {
  const pb::CCompile& compile = target.c_compile();

  s << "build " << OutputFilenames(target).join(" ") << ": ";
  if (compile.is_cc()) {
    s << "cc_compile";
  } else {
    s << "c_compile";
  }
  s << " " << InputFilenames(target).join(" ") << "\n";

  s << "  flags =";
  for (const QString& flag : compile.flag()) {
    s << " " << flag;
  }
  s << "\n";
  s << "  definitions =";
  for (const pb::Definition& defintion : compile.definition()) {
    s << " -D" << defintion.name();
    if (defintion.has_value()) {
      s << "=" << defintion.value();
    }
  }
  s << "\n";
  s << "  header_search_path =";
  for (const QString& path : compile.header_search_path()) {
    s << " -I";
    if (path.isEmpty()) {
      s << ".";
    } else {
      s << path;
    }
  }
}

void NinjaGenerator::WriteLinkTarget(const pb::BuildTarget& target,
                                     QTextStream& s) {
  const pb::CLink& link = target.c_link();

  s << "build " << OutputFilenames(target).join(" ") << ": ";
  if (link.is_cc()) {
    s << "cc_";
  } else {
    s << "c_";
  }
  if (link.is_library()) {
    s << "link_library";
  } else {
    s << "link_binary";
  }
  s << " " << InputFilenames(target).join(" ") << "\n";

  s << "  flags =";
  for (const QString& flag : link.flag()) {
    s << " " << flag;
  }
  s << "\n";
  s << "  library_search_path =";
  for (const QString& path : link.library_search_path()) {
    s << " -L";
    if (path.isEmpty()) {
      s << ".";
    } else {
      s << path;
    }
  }
  s << "\n";
  s << "  libs =";
  for (const QString& src : target.srcs()) {
    if (src == "//__lib__/pthread") {
      s << " -pthread";
    } else if (src.startsWith("//__lib__/")) {
      s << " -l" << src.mid(QString("//__lib__/").length());
    }
  }
}
