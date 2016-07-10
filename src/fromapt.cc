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

#include "fromapt.h"

#include "tracer.h"
#include "analysis/configure.h"
#include "analysis/install.h"
#include "analysis/make.h"
#include "utils/logging.h"

#include <QDir>
#include <QProcess>
#include <QTemporaryDir>

FromApt::FromApt(const QString& package)
    : package_(package) {
}

bool FromApt::Run() {
  QTemporaryDir dir;
  CHECK(dir.isValid());
  dir.setAutoRemove(false);
  LOG(INFO) << "Using temporary directory " << dir.path();

  if (!RunCommand(dir.path(), {"apt-get", "source", package_})) {
    LOG(ERROR) << "apt-get source failed";
    return false;
  }

  // Find the directory that apt-get source created.
  const QStringList entries =
      QDir(dir.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  if (entries.empty()) {
    LOG(ERROR) << "apt-get source didn't create any directories";
    return false;
  }
  package_dir_ = QDir(dir.path());
  package_dir_.cd(entries[0]);
  if (entries.count() != 1) {
    LOG(WARNING) << "apt-get source created multiple directories, using "
                 << package_dir_.path();
  }

  if (QFileInfo(package_dir_, "configure").isExecutable()) {
    LOG(INFO) << "Detected autotools build system";
    if (!Autotools()) {
      return false;
    }
  } else {
    LOG(ERROR) << "Unknown build system";
    return false;
  }

  analysis::Configure::Options conf_opts;
  conf_opts.trace_filename = "configure.trace";
  conf_opts.output_filename = "configure.files";
  if (!analysis::Configure::Run(conf_opts)) {
    return false;
  }

  analysis::Install::Options install_opts;
  install_opts.trace_filename = "install.trace";
  install_opts.output_filename = "install.files";
  if (!analysis::Install::Run(install_opts)) {
    return false;
  }

  analysis::Make::Options make_opts;
  make_opts.trace_filename = "make.trace";
  make_opts.install_filename = install_opts.output_filename;
  make_opts.output_filename = "make.targets";
  if (!analysis::Make::Run(make_opts)) {
    return false;
  }

  return true;
}

bool FromApt::Autotools() {
  if (!TraceCommand("configure.trace", {"./configure"}, package_dir_.path())) {
    LOG(ERROR) << "configure failed";
    return false;
  }

  if (!TraceCommand("make.trace", {"make"}, package_dir_.path())) {
    LOG(ERROR) << "make failed";
    return false;
  }

  if (!TraceCommand("install.trace",
                    {"make", "install"},
                    package_dir_.path())) {
    LOG(ERROR) << "install failed";
    return false;
  }
  return true;
}

bool FromApt::RunCommand(const QString& working_directory,
                         const QStringList& args) const {
  LOG(INFO) << "Running " << args << " in " << working_directory;
  QProcess proc;
  proc.setWorkingDirectory(working_directory);
  proc.setProgram(args[0]);
  proc.setArguments(args.mid(1));
  proc.start();
  proc.waitForStarted(-1);
  if (proc.state() != QProcess::Running) {
    return false;
  }
  proc.waitForFinished(-1);
  return proc.exitStatus() == QProcess::NormalExit;
}

bool FromApt::TraceCommand(const QString& output_name,
                           const QStringList& args,
                           const QString& working_directory) const {
  Tracer::Options opts;
  opts.output_filename = output_name;
  opts.args = args;
  opts.working_directory = working_directory;
  opts.project_name = package_;

  return Tracer::Run(opts);
}
