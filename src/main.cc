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

#include <array>
#include <cstdio>
#include <functional>
#include <iostream>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <QCoreApplication>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>
#include <QTextStream>

#include "fromapt.h"
#include "tracer.h"
#include "analysis/configure.h"
#include "analysis/install.h"
#include "analysis/make.h"
#include "gen/bazel/generator.h"
#include "utils/recordfile.h"
#include "utils/str.h"
#include "utils/subcommand.h"


DEFINE_string(project_name, "", "The name of the project.  Default is to guess "
              "from the name of the project_root directory");
DEFINE_string(project_root, "", "The directory containing the source code, if "
              "different to the current directory");

namespace {

bool Trace(const QStringList& args) {
  Tracer::Options opts;
  opts.output_filename = args[0] + ".trace";
  opts.args = args.mid(1);
  opts.working_directory = QDir::currentPath();

  if (!FLAGS_project_name.empty()) {
    opts.project_name = utils::str::StlToQt(FLAGS_project_name);
  }
  if (!FLAGS_project_root.empty()) {
    opts.project_root = utils::str::StlToQt(FLAGS_project_root);
  }

  return Tracer::Run(opts);
}


bool AnalyzeConf(const QStringList& args) {
  analysis::Configure::Options opts;
  opts.trace_filename = args[0] + ".trace";
  opts.output_filename = args[0] + ".outputs";

  return analysis::Configure::Run(opts);
}


bool AnalyzeMake(const QStringList& args) {
  analysis::Make::Options opts;
  opts.trace_filename = args[0] + ".trace";
  opts.output_filename = args[0] + ".targets";
  opts.graph_output_filename = args[0] + ".dot";
  opts.intermediate_graph_output_filename = args[0] + ".intermediate.dot";
  opts.install_filename = args[1] + ".files";

  return analysis::Make::Run(opts);
}


bool AnalyzeInstall(const QStringList& args) {
  analysis::Install::Options opts;
  opts.trace_filename = args[0] + ".trace";
  opts.output_filename = args[0] + ".files";

  return analysis::Install::Run(opts);
}


template <typename T>
bool TryDump(const QString& filename) {
  utils::RecordFile<T> fh(filename);
  if (!fh.Open(QIODevice::ReadOnly)) {
    LOG(ERROR) << "Failed to open " << filename << " for reading";
    return false;
  }

  while (!fh.AtEnd()) {
    T msg;
    if (!fh.ReadRecord(&msg)) {
      return false;
    }
    if (!msg.GetReflection()->GetUnknownFields(msg).empty()) {
      return false;
    }
  }

  fh.Open(QIODevice::ReadOnly);
  while (!fh.AtEnd()) {
    T msg;
    fh.ReadRecord(&msg);
    msg.PrintDebugString();
    std::cout << "\n";
  }
  return true;
}


bool Dump(const QStringList& args) {
  const QString filename = args[0];

  if (TryDump<pb::Record>(filename)) return true;

  LOG(ERROR) << "Couldn't parse " << filename;
  return false;
}


bool Graph(const QStringList& args) {
  const QString filename = args[0];

  QTemporaryFile svg_file(QDir::tempPath() + "/maketrace-XXXXXX.svg");
  svg_file.setAutoRemove(false);
  svg_file.open();

  if (QProcess::execute("dot", QStringList{"-Tsvg", "-o" + svg_file.fileName(),
                                           filename}) != 0) {
    LOG(ERROR) << "dot command failed";
    return false;
  }

  QProcess::execute("google-chrome", QStringList{svg_file.fileName()});
  return true;
}


bool FromAptCommand(const QStringList& args) {
  const QString package_name = args[0];

  return FromApt(package_name).Run();
}


bool GenBazel(const QStringList& args) {
  gen::bazel::Generator::Options opts;
  opts.target_filename = args[0] + ".targets";
  opts.installed_files_filename = args[1] + ".files";
  opts.workspace_path = args[2];
  return gen::bazel::Generator::Run(opts);
}


const std::array<utils::SubcommandSpec, 8> kSubcommands = {{
  {"trace", "<name> <command> [<arg> ...]",
   "Runs a command and writes a trace file.\n"
   "\n"
   "Output is written to <name>.trace - give the same name to analyze-conf\n"
   "and analyze-make commands later.",
   2,
   Trace,
  },
  {"analyze-conf", "<name>",
   "Analyzes the trace of a configure step.",
   1,
   AnalyzeConf,
  },
  {"analyze-make", "<make-name> <install-name>",
   "Analyzes the trace of a compile.  analyze-install must have been run first.",
   2,
   AnalyzeMake,
  },
  {"analyze-install", "<name>",
   "Analyzes the trace of a 'make install'.",
   1,
   AnalyzeInstall,
  },
  {"gen-bazel", "<make-name> <install-name> <workspace>",
   "Writes bazel BUILD files into the given workspace.\n"
   "\n"
   "<conf-name> is the name of a trace that has had analyze-conf run on it.\n"
   "<make-name> is the name of a trace that has had analyze-make run on it.\n",
   3,
   GenBazel,
  },
  {"dump", "<filename>",
   "Prints a human-readable representation of a protobuf record file.",
   1,
   Dump,
  },
  {"graph", "<filename>",
   "Converts a .dot file to an SVG and opens it in a browser.",
   1,
   Graph,
  },
  {"fromapt", "<package name>",
   "Downloads and traces the build of a debian package.",
   1,
   FromAptCommand,
  },
}};


}  // namespace


int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;

  QCoreApplication app(argc, argv);

  return utils::RunSubcommand(argc, argv, kSubcommands);
}
