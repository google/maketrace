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

#ifndef TRACER_H
#define TRACER_H

#include <memory>

#include "common.h"
#include "tracer.pb.h"
#include "utils/recordfile.h"

class Tracer {
 public:
  struct Options {
    // Run this command in this directory.
    QStringList args;
    QString working_directory;

    // Write trace records to this file.
    QString output_filename;

    // The name of the project to put in the metadata.  If unset, defaults to
    // the name of the project root directory, with numbers and punctuation
    // removed.
    QString project_name;

    // All filenames will be made relative to this directory.  If unset,
    // defaults to the current directory.
    QString project_root;
  };

  // Starts the executable and runs it until it terminates.
  static bool Run(const Options& opts);

 private:
  struct ChildEvent;
  struct FileState;
  struct PidState;
  struct Registers;

  Tracer(const Options& opts);

  bool TraceUntilExit();

  // Wrapper for exec().
  static void ExecSubprocess(const QStringList& argv);

  // Uses waitpid() to wait for any child process to change state.
  ChildEvent WaitForChild();

  // Sets default ptrace options on the process.  Only needs to be done once.
  bool SetOptions(pid_t pid);

  // Resumes a stopped process, optionally sending it a signal.
  bool Continue(pid_t pid, int signal = 0);

  // Handles syscall-enter-stop and syscall-exit-stop events.
  void HandleSyscallStart(PidState* state);
  void HandleSyscallEnd(PidState* state);

  // Handles explicit process terminations as well as clone deaths in an
  // exit_group.
  void HandleProcessExited(PidState* state, int exit_code);

  // Increases the ref count on the FD and adds it to open_files.
  void HandleDupFd(PidState* state, uint64_t syscall, int old_fd, int new_fd);
  void HandleCloseFd(PidState* state, int fd);

  // Reads a string from a traced process' address space and converts it to an
  // absolute path relative to that process' working directory.
  QString ReadAbsolutePath(pid_t pid, void* client_addr);

  // Reads a path relative to the directory FD.  Handles AT_FDCWD correctly.
  QString ReadPathAt(const PidState* state, int fd, void* client_addr);

  // Reads /proc/.../cwd of another process.
  static QString ReadCwd(pid_t pid);

  // Calculates the sha1 hash of the contents of the file.
  static QByteArray Sha1Hash(const QString& absolute_path);

  // Called when the process exits.  De-dupes file accesses and writes the final
  // state to the pb::Process.
  void WriteFileProtos(PidState* state);

  // Fills the metadata proto that will be written at the start of the file.
  void FillMetadata(pb::MetaData* metadata);

  Options opts_;
  std::unique_ptr<utils::RecordFile<pb::Record>> trace_file_;
  QMap<pid_t, PidState*> pids_;
  QSet<pid_t> stopped_children_;

  int next_id_ = 0;
  int next_ordering_ = 0;

};

#endif // TRACER_H
