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

#include "tracer.h"

#include <fcntl.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <cstdint>

#include <glog/logging.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QMutexLocker>
#include <QtConcurrentMap>

#include "memory.h"
#include "utils/path.h"
#include "utils/recursive_copy.h"
#include "utils/str.h"

namespace {

bool OpenFlagsMightWrite(int flags) {
  return flags & O_WRONLY || flags & O_RDWR;
}

}


// Returned by WaitForChild() when a process changes state.
struct Tracer::ChildEvent {
  enum State {
    kWaiting,  // In a ptrace-stop state.
    kWaitingAfterFork,  // In a ptrace-stop state after a child process has
                        // just been created.  Use PTRACE_GETEVENTMSG to get
                        // the child's PID.
    kWaitingAfterExec,  // In a ptrace-stop state after an exec.
    kStoppedWithSignal,  // In signal-delivery-stop state.
    kExitedNormally,  // exit_code is set with the process' exit code.
    kExitedWithSignal,  // signal is set with the signal that killed it.

    kInvalid,  // A waitpid() error occurred.
  };

  ChildEvent() {}
  ChildEvent(pid_t p, State s) : changed_pid(p), state(s) {}

  pid_t changed_pid = 0;
  State state = kInvalid;
  int exit_code = 0;  // Only set when state == kExitedNormally.
  int signal = 0;  // Only set when state == kExitedWithSignal.
};

// State associated with a file descriptor used by the traced subprocess.
struct Tracer::FileState {
  QString filename;
  QByteArray sha1_before;
  QString renamed_from;

  bool redirected = false;
  bool unlinked = false;

  int open_ordering = 0;
  int close_ordering = 0;
  int ref_count = 1;

  size_t bytes_written = 0;
};

struct Tracer::Registers {
  // Reads the registers from a process in ptrace-stop.
  void FromPid(pid_t pid);

  // Writes the registers to a process in ptrace-stop.
  void ToPid(pid_t pid) const;

  uint64_t syscall = 0;
  uint64_t args[6] = {{0}};
  int64_t return_value = 0;
  uint64_t instruction_pointer = 0;
};

// State associated with a traced subprocess.
struct Tracer::PidState {
  explicit PidState(pid_t pp, pid_t p, int* next_id, int* next_ordering)
      : parent_pid(pp),
        pid(p),
        mem(p),
        process_pb(record_pb.mutable_process()) {
    process_pb->set_id((*next_id)++);
    process_pb->set_begin_ordering((*next_ordering)++);
  }

  const pid_t parent_pid;
  const pid_t pid;

  TraceeMemory mem;

  // This process' proto.
  pb::Record record_pb;
  pb::Process* process_pb;

  // Files touched by this process.  Open files are keyed by FD.
  QMap<int, FileState*> open_files;
  QList<FileState> closed_files;

  // Set by syscall-enter-stop and unset by syscall-exit-stop.
  bool in_syscall = false;

  // The current syscall's path was modified to be within redirect_root_.
  bool path_is_redirected = false;

  // execve is special because it resets the process' address space, making its
  // arguments unreadable when the syscall returns.  They're stored here
  // temporarily so they're available in syscall-exit-stop.
  QString exec_filename;
  QStringList exec_argv;

  // Set in PTRACE_EVENT_EXEC when the exec succeeds and the process' address
  // space is reset.  Unset by the following syscall-exit-stop.
  bool exec_completed = false;

  // Set before an open or unlink system call, so the contents of the file can
  // be recorded before it's truncated by open() or removed by unlink().
  QByteArray file_contents_sha1;

  // An 8k block allocated in the traced process for us to play with.  Used for
  // writing redirected filenames to be passed to syscalls.
  uint64_t scratch_space = 0;
  bool needs_hijack = false;  // Set to true after an exec.
  bool in_hijack = false;  // The current syscall is our mmap.
  Tracer::Registers hijack_registers;  // Original registers before the mmap.
};


bool Tracer::Run(const Options& opts) {
  CHECK(!opts.args.empty());

  LOG(INFO) << "Tracing " << opts.args << " in "
            << opts.working_directory;

  pid_t pid = fork();
  switch (pid) {
    case -1:
      LOG(ERROR) << "fork failed: " << strerror(errno);
      return false;
    case 0:
      if (!opts.working_directory.isEmpty() &&
          chdir(opts.working_directory.toUtf8().constData()) != 0) {
        LOG(FATAL) << "Child process failed to change working directory to "
                   << opts.working_directory;
      }
      ExecSubprocess(opts.args);
      return false;  // Never reached.
    default: {
      Tracer t(opts);
      t.pids_[pid] =
          new PidState(0, pid, &t.next_id_, &t.next_ordering_);

      if (t.WaitForChild().state != ChildEvent::kStoppedWithSignal) {
        return false;
      }
      if (!t.SetOptions(pid)) {
        return nullptr;
      }
      return t.TraceUntilExit();
    }
  }
}

Tracer::Tracer(const Options& opts)
    : opts_(opts) {
  // Default to the current directory.
  if (opts_.project_root.isEmpty()) {
    opts_.project_root = opts.working_directory;
  }

  // Guess a project name if we haven't been given one.
  if (opts_.project_name.isEmpty()) {
    opts_.project_name = QFileInfo(opts_.project_root).fileName();
    const int dash = opts_.project_name.indexOf('-');
    if (dash != -1) {
      opts_.project_name = opts_.project_name.left(dash);
    }
  }

  if (!opts_.redirect_root.isEmpty()) {
    QDir().mkpath(opts_.redirect_root);
  }
}

void Tracer::ExecSubprocess(const QStringList& argv) {
  // Don't worry about cleaning up any of this since the process will exit
  // before the function returns.
  char** c_args = new char*[argv.size() + 1];
  for (int i = 0; i < argv.size(); ++i) {
    c_args[i] = strdup(argv[i].toUtf8().constData());
  }
  c_args[argv.size()] = nullptr;

  // Start tracing.
  int ret = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
  if (ret != 0) {
    LOG(ERROR) << "PTRACE_TRACEME failed: " << strerror(errno);
  } else {
    kill(getpid(), SIGSTOP);

    // Start the child process.
    execvp(argv[0].toUtf8().constData(), c_args);
    LOG(ERROR) << "Exec failed: " << strerror(errno);
  }

  google::FlushLogFilesUnsafe(google::GLOG_INFO);
  _exit(1);
}

Tracer::ChildEvent Tracer::WaitForChild() {
  int status = 0;
  // __WALL waits on clones (threads) as well as non-clones.
  pid_t pid = waitpid(-1, &status, __WALL);

  if (pid == -1) {
    LOG(WARNING) << "waitpid failed: " << strerror(errno);
    return ChildEvent();
  }

  if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
    int shifted = status >> 16;
    if (shifted == PTRACE_EVENT_FORK ||
        shifted == PTRACE_EVENT_VFORK ||
        shifted == PTRACE_EVENT_CLONE) {
      return ChildEvent(pid, ChildEvent::kWaitingAfterFork);
    } else if (shifted == PTRACE_EVENT_EXEC) {
      return ChildEvent(pid, ChildEvent::kWaitingAfterExec);
    } else {
      return ChildEvent(pid, ChildEvent::kWaiting);
    }
  }
  if (WIFSTOPPED(status)) {
    ChildEvent ret(pid, ChildEvent::kStoppedWithSignal);
    ret.signal = WSTOPSIG(status);
    return ret;
  }
  if (WIFEXITED(status)) {
    ChildEvent ret(pid, ChildEvent::kExitedNormally);
    ret.exit_code = WEXITSTATUS(status);
    return ret;
  }
  if (WIFSIGNALED(status)) {
    ChildEvent ret(pid, ChildEvent::kExitedWithSignal);
    ret.signal = WTERMSIG(status);
    return ret;
  }

  LOG(ERROR) << "Unknown waitpid() return value " << status;
  return ChildEvent();
}

bool Tracer::Continue(pid_t pid, int signal) {
  if (ptrace(PTRACE_SYSCALL, pid, nullptr, signal) != 0) {
    LOG(ERROR) << "Continue failed (" << pid << "): " << strerror(errno);
    return false;
  }
  return true;
}

bool Tracer::SetOptions(pid_t pid) {
  if (ptrace(PTRACE_SETOPTIONS, pid, nullptr,
             PTRACE_O_TRACECLONE |
             PTRACE_O_TRACEFORK |
             PTRACE_O_TRACEVFORK |
             PTRACE_O_TRACEEXEC) != 0) {
    LOG(ERROR) << "PTRACE_SETOPTIONS failed for pid " << pid
               << ": " << strerror(errno);
    return false;
  }
  return true;
}

void Tracer::FillMetadata(pb::MetaData* metadata) {
  metadata->set_project_root(opts_.project_root);
  metadata->set_project_name(opts_.project_name);

  if (opts_.project_root != QDir::currentPath()) {
    metadata->set_build_dir(
        utils::path::MakeRelativeTo(QDir::currentPath(),
                                    opts_.project_root));
  }

  if (!opts_.redirect_root.isNull()) {
    metadata->set_redirect_root(opts_.redirect_root);
  }
}

bool Tracer::TraceUntilExit() {
  trace_file_ = make_unique<utils::RecordFile<pb::Record>>(
      opts_.output_filename);
  if (!trace_file_->Open(QFile::WriteOnly)) {
    LOG(ERROR) << "Failed to open " << trace_file_->filename()
               << " for writing";
    return false;
  }

  pb::Record metadata_record;
  FillMetadata(metadata_record.mutable_metadata());
  trace_file_->WriteRecord(metadata_record);

  // Start the process.
  if (pids_.empty()) {
    return false;
  }
  if (!Continue(pids_.begin().key())) {
    return false;
  }

  while (true) {
    if (pids_.empty()) {
      return true;
    }

    ChildEvent event = WaitForChild();
    const pid_t pid = event.changed_pid;
    const bool is_traced = pids_.find(pid) != pids_.end();
    PidState* state = is_traced ? pids_[pid] : nullptr;

    switch (event.state) {
      case ChildEvent::kWaiting:
        CHECK(is_traced) << pid;
        if (state->in_syscall) {
          state->in_syscall = false;
          HandleSyscallEnd(state);
        } else {
          state->in_syscall = true;
          HandleSyscallStart(state);
        }
        if (!Continue(pid)) {
          // Probably exited as part of an exit_group.
          HandleProcessExited(state, -1);
        }
        break;

      case ChildEvent::kWaitingAfterFork: {
        // Get the PID of the newly forked process.
        CHECK(is_traced);
        pid_t new_pid = 0;
        if (ptrace(PTRACE_GETEVENTMSG, pid,
                   nullptr, &new_pid) != 0) {
          LOG(ERROR) << "PTRACE_GETEVENTMSG failed for pid "
                     << pid << ": " << strerror(errno);
          return false;
        }

        // Create a state object for the new process.
        pids_[new_pid] =
            new PidState(state->pid, new_pid, &next_id_, &next_ordering_);
        pids_[new_pid]->process_pb->set_parent_id(state->process_pb->id());
        state->process_pb->add_child_process_id(pids_[new_pid]->process_pb->id());

        // Resume the original process.
        if (!Continue(pid)) {
          return false;
        }

        // If we've seen the SIGSTOP from the new process already, continue it
        // now.
        auto it = stopped_children_.find(new_pid);
        if (it != stopped_children_.end()) {
          if (!Continue(new_pid)) {
            return false;
          }
          stopped_children_.erase(it);
        }
        break;
      }

      case ChildEvent::kWaitingAfterExec: {
        CHECK(is_traced);
        CHECK(state->in_syscall);
        state->exec_completed = true;
        if (state->parent_pid != 0) {
          LOG(INFO) << state->parent_pid << " forked " << pid << " and exec'd "
                    << state->exec_filename;
        }
        if (!Continue(pid)) {
          return false;
        }
        break;
      }

      case ChildEvent::kStoppedWithSignal:
        if (event.signal == SIGSTOP) {
          if (!is_traced) {
            // This is a newly created process, but the parent hasn't seen a
            // kWaitingAfterFork yet so it hasn't created a state object for it.
            // Don't resume it yet - wait for the parent to do it later.
            stopped_children_.insert(pid);
            break;
          }
          // Don't pass SIGSTOP to the child process.
          event.signal = 0;
        }

        if (!Continue(pid, event.signal)) {
          return false;
        }
        break;

      case ChildEvent::kExitedWithSignal:
        event.exit_code = - event.signal;
        // fallthrough
      case ChildEvent::kExitedNormally: {
        if (is_traced) {
          HandleProcessExited(state, event.exit_code);
        }
        break;
      }

      case ChildEvent::kInvalid:
        return false;
    }
  }
}

void Tracer::HandleProcessExited(PidState* state, int exit_code) {
  WriteFileProtos(state);

  state->process_pb->set_exit_code(exit_code);
  state->process_pb->set_end_ordering(next_ordering_++);
  trace_file_->WriteRecord(state->record_pb);
  pids_.remove(state->pid);
  delete state;
}

void Tracer::WriteFileProtos(PidState* state) {
  // Treat all open file handles as closed now.
  for (FileState* value : state->open_files) {
    if (--value->ref_count == 0) {
      value->close_ordering = next_ordering_++;
      state->closed_files.push_back(*value);
      delete value;
    }
  }
  state->open_files.empty();

  // Combine all the entries for a filename into one pb - so if a file is opened
  // and closed multiple times the output pb will hold its final state.  For
  // example a file opened once for reading and then deleted will have access()
  // == DELETED.
  QMap<QString, pb::File*> file_protos;
  for (const FileState& file : state->closed_files) {
    pb::File* fpb = file_protos[file.filename];
    if (fpb == nullptr) {
      fpb = new pb::File;
      file_protos[file.filename] = fpb;

      fpb->set_filename(file.filename);
      fpb->set_open_ordering(file.open_ordering);
      if (file.sha1_before.isEmpty()) {
        fpb->set_access(pb::File_Access_CREATED);
      } else {
        fpb->set_access(pb::File_Access_READ);
        fpb->set_sha1_before(file.sha1_before);
      }
    }

    fpb->set_close_ordering(file.close_ordering);

    if (file.redirected) {
      fpb->set_redirected(true);
    }

    if (file.unlinked) {
      fpb->set_access(pb::File_Access_DELETED);
    } else if (!file.renamed_from.isEmpty()) {
      fpb->set_open_ordering(file.open_ordering);
      fpb->set_renamed_from(file.renamed_from);
    } else if (file.bytes_written != 0 &&
               fpb->access() != pb::File_Access_CREATED) {
      fpb->set_access(pb::File_Access_MODIFIED);
    }
  }

  // If a file was created and then renamed, treat it as created.
  for (pb::File* fpb : file_protos) {
    if (fpb->renamed_from().isEmpty()) {
      continue;
    }

    const auto from_fpb_it = file_protos.find(fpb->renamed_from());
    if (from_fpb_it == file_protos.end()) {
      continue;
    }

    pb::File* from_fpb = from_fpb_it.value();
    if (from_fpb->access() == pb::File_Access_CREATED) {
      delete from_fpb;
      file_protos.erase(from_fpb_it);
      fpb->clear_renamed_from();
    }
  }

  // Hash all the files in their final state.  Do this in parallel since the
  // sha1 is expensive.
  QMutex process_pb_lock;
  QtConcurrent::blockingMap(file_protos,
                            [this, &state, &process_pb_lock](pb::File* value) {
    // Delete the fpb (allocated above) when we're done.
    std::unique_ptr<pb::File> fpb(value);

    // Dereference any symlinks and make paths relative to the project root.
    const QString absolute_path = utils::path::Readlink(fpb->filename());
    fpb->set_filename(utils::path::MakeRelativeTo(
        absolute_path, opts_.project_root));
    if (fpb->has_renamed_from()) {
      fpb->set_renamed_from(utils::path::MakeRelativeTo(
          utils::path::Readlink(fpb->renamed_from()),
          opts_.project_root));
    }

    // Hash the file's contents.
    const QByteArray sha1 = Sha1Hash(absolute_path);
    if (!sha1.isEmpty()) {
      fpb->set_sha1_after(sha1);
    }

    if (fpb->access() == pb::File_Access_READ &&
        fpb->sha1_before() != fpb->sha1_after()) {
      // If a file wasn't created by this process, we determine whether it was
      // modified by comparing the hash before and after.
      fpb->set_access(pb::File_Access_MODIFIED);
    } else if (fpb->access() == pb::File_Access_MODIFIED &&
               fpb->sha1_before() == fpb->sha1_after()) {
      // If a file had bytes written to it but didn't change, maybe it already
      // existed and was rewritten with the same contents.
      fpb->set_access(pb::File_Access_WRITTEN_BUT_UNCHANGED);
    } else if (fpb->access() == pb::File_Access_CREATED &&
               fpb->sha1_before().isEmpty() &&
               fpb->sha1_after().isEmpty()) {
      // For non-regular files that aren't hashed at all.
      fpb->set_access(pb::File_Access_READ);
    }

    QMutexLocker l(&process_pb_lock);
    state->process_pb->add_files()->CopyFrom(*fpb);
  });
}

void Tracer::Registers::FromPid(pid_t pid) {
  #define GET_REGISTER(field, reg) \
    field = ptrace(PTRACE_PEEKUSER, pid, \
                   offsetof(user_regs_struct, reg), nullptr)

  // See http://man7.org/linux/man-pages/man2/syscall.2.html#NOTES.
  GET_REGISTER(syscall, orig_rax);
  GET_REGISTER(args[0], rdi);
  GET_REGISTER(args[1], rsi);
  GET_REGISTER(args[2], rdx);
  GET_REGISTER(args[3], r10);
  GET_REGISTER(args[4], r8);
  GET_REGISTER(args[5], r9);
  GET_REGISTER(return_value, rax);
  GET_REGISTER(instruction_pointer, rip);

  #undef GET_REGISTER
}

void Tracer::Registers::ToPid(pid_t pid) const {
  #define SET_REGISTER(field, reg) \
    ptrace(PTRACE_POKEUSER, pid, offsetof(user_regs_struct, reg), field)

  // See http://man7.org/linux/man-pages/man2/syscall.2.html#NOTES.
  SET_REGISTER(syscall, orig_rax);
  SET_REGISTER(args[0], rdi);
  SET_REGISTER(args[1], rsi);
  SET_REGISTER(args[2], rdx);
  SET_REGISTER(args[3], r10);
  SET_REGISTER(args[4], r8);
  SET_REGISTER(args[5], r9);
  SET_REGISTER(return_value, rax);
  SET_REGISTER(instruction_pointer, rip);

  #undef SET_REGISTER
}

void Tracer::HandleSyscallStart(PidState* state) {
  Registers regs;
  regs.FromPid(state->pid);

  if (state->needs_hijack &&
      !opts_.redirect_root.isNull() &&
      state->scratch_space == 0 &&
      !state->in_hijack) {
    state->in_hijack = true;
    state->hijack_registers = regs;
    state->hijack_registers.instruction_pointer -= 2;  // length of syscall

    regs.syscall = __NR_mmap;
    regs.args[0] = 0;  // addr
    regs.args[1] = 8 * 1024;  // length
    regs.args[2] = PROT_READ | PROT_WRITE;  // prot
    regs.args[3] = MAP_PRIVATE | MAP_ANONYMOUS;  // flags
    regs.args[4] = 0;  // fd
    regs.args[5] = 0;  // offset
    regs.ToPid(state->pid);
    return;
  }

  // When the exec syscall returns this data won't be accessible any more, so
  // read it now so we can use it in HandleSyscallEnd.
  if (regs.syscall == __NR_execve) {
    state->exec_filename = state->mem.ReadNullTerminatedUtf8(
        reinterpret_cast<void*>(regs.args[0]));
    state->exec_argv = state->mem.ReadNullTerminatedUtf8Array(
        reinterpret_cast<void*>(regs.args[1]));
  }

  state->path_is_redirected = false;
  if (!opts_.redirect_root.isNull() && state->scratch_space) {
    if (RedirectSyscall(*state, &regs)) {
      state->path_is_redirected = true;
    }
  }

  // Hash the contents of a file being opened or unlinked before the system
  // call actually happens.
  int filename_arg_index = -1;
  switch (regs.syscall) {
    case __NR_open:
    case __NR_unlink:
      filename_arg_index = 0; break;
    case __NR_rename:
      // Hash the file that's being overwritten since this is the "before" sha1.
      filename_arg_index = 1; break;
    case __NR_unlinkat:
      // We CHECK fd == AT_FDCWD in HandleSyscallEnd;
      filename_arg_index = 1; break;
  }

  if (filename_arg_index != -1) {
    const QString filename = ReadAbsolutePath(
        state->pid, reinterpret_cast<void*>(regs.args[filename_arg_index]));
    state->file_contents_sha1 = Sha1Hash(filename);
  }
}

void Tracer::HandleSyscallEnd(PidState* state) {
  Registers regs;
  regs.FromPid(state->pid);

  if (state->in_hijack) {
    state->in_hijack = false;
    if (regs.return_value == -1) {
      LOG(FATAL) << "mmap failed";
    }
    state->scratch_space = *reinterpret_cast<uint64_t*>(&regs.return_value);
    LOG(INFO) << state->pid << " mmap at 0x"
              << std::hex << state->scratch_space;

    // Do the original syscall again.
    state->hijack_registers.ToPid(state->pid);
    return;
  }

  switch (regs.syscall) {
    case __NR_openat:
    case __NR_open: {
      const int fd = regs.return_value;

      if (fd >= 0) {
        CHECK(!state->open_files.contains(fd));
        FileState* file = new FileState;
        state->open_files[fd] = file;

        if (regs.syscall == __NR_open) {
          file->filename = ReadAbsolutePath(
              state->pid, reinterpret_cast<void*>(regs.args[0]));
        } else {
          file->filename = ReadPathAt(
              state, regs.args[0], reinterpret_cast<void*>(regs.args[1]));
        }

        file->redirected = state->path_is_redirected;
        file->sha1_before = state->file_contents_sha1;
        file->open_ordering = next_ordering_++;
      }

      break;
    }
    case __NR_close: {
      if (regs.return_value == 0) {
        HandleCloseFd(state, regs.args[0]);
      }
      break;
    }
    case __NR_execve: {
      if (state->exec_completed && regs.return_value == 0) {
        state->process_pb->set_filename(state->exec_filename);
        state->process_pb->set_working_directory(ReadCwd(state->pid));
        state->process_pb->clear_argv();
        state->process_pb->mutable_argv()->append(state->exec_argv);
        state->exec_completed = false;
        state->scratch_space = 0;
        state->needs_hijack = true;
        state->in_hijack = false;
      }
      break;
    }
    case __NR_unlinkat:
    case __NR_unlink:
      if (regs.return_value == 0) {
        FileState file;

        if (regs.syscall == __NR_unlink) {
          file.filename = ReadAbsolutePath(
              state->pid, reinterpret_cast<void*>(regs.args[0]));
        } else {
          file.filename = ReadPathAt(
              state, regs.args[0], reinterpret_cast<void*>(regs.args[1]));
        }

        file.sha1_before = state->file_contents_sha1;
        file.redirected = state->path_is_redirected;
        file.unlinked = true;
        file.open_ordering = next_ordering_++;
        file.close_ordering = file.open_ordering;
        state->closed_files.push_back(file);
      }
      break;
    case __NR_rename:
      if (regs.return_value == 0) {
        FileState file;
        file.renamed_from = ReadAbsolutePath(
            state->pid, reinterpret_cast<void*>(regs.args[0]));
        file.filename = ReadAbsolutePath(
            state->pid, reinterpret_cast<void*>(regs.args[1]));
        file.redirected = state->path_is_redirected;
        file.sha1_before = state->file_contents_sha1;
        file.open_ordering = next_ordering_++;
        file.close_ordering = file.open_ordering;
        state->closed_files.push_back(file);
      }
      break;
    case __NR_fcntl:
      if (regs.args[1] == F_DUPFD && regs.return_value != -1) {
        HandleDupFd(state, regs.syscall, regs.args[0], regs.return_value);
      }
      break;
    case __NR_dup:
      if (regs.return_value != -1) {
        HandleDupFd(state, regs.syscall, regs.args[0], regs.return_value);
      }
      break;
    case __NR_dup2:
    case __NR_dup3:
      if (regs.return_value != -1 &&
          regs.args[0] != regs.args[1]) {
        HandleCloseFd(state, regs.args[1]);
        HandleDupFd(state, regs.syscall, regs.args[0], regs.args[1]);
      }
      break;
    case __NR_write: {
      const int fd = regs.args[0];
      if (regs.return_value > 0 && state->open_files.contains(fd)) {
        state->open_files[fd]->bytes_written += regs.return_value;
      }
      break;
    }
    default:
      break;
  }
}

void Tracer::HandleDupFd(PidState* state, uint64_t syscall, int old_fd,
                         int new_fd) {
  if (!state->open_files.contains(old_fd)) {
    // Probably a pipe or a socket.
    return;
  }

  state->open_files[new_fd] = state->open_files[old_fd];
  state->open_files[new_fd]->ref_count ++;
}

void Tracer::HandleCloseFd(PidState* state, int fd) {
  if (!state->open_files.contains(fd)) {
    return;
  }

  FileState* file = state->open_files.take(fd);
  if (--file->ref_count == 0) {
    file->close_ordering = next_ordering_++;
    state->closed_files.push_back(*file);
    delete file;
  }
}

QString Tracer::ReadCwd(pid_t pid) {
  return utils::path::Readlink("/proc/" + QString::number(pid) + "/cwd");
}

QString Tracer::ReadAbsolutePath(pid_t pid, void* client_addr) {
  const QString filename =
      TraceeMemory(pid).ReadNullTerminatedUtf8(client_addr);
  if (filename.isEmpty()) {
    return filename;
  }

  const QString cwd = ReadCwd(pid);

  return utils::path::MakeAbsolute(filename, cwd);
}

QString Tracer::ReadPathAt(const PidState* state, int fd, void* client_addr) {
  if (fd == AT_FDCWD) {
    return ReadAbsolutePath(state->pid, client_addr);
  }

  CHECK(state->open_files.contains(fd));
  const FileState* dirstate = state->open_files[fd];

  const QString filename =
      state->mem.ReadNullTerminatedUtf8(client_addr);
  if (filename.isEmpty()) {
    return filename;
  }

  return utils::path::MakeAbsolute(filename, dirstate->filename);
}

QByteArray Tracer::Sha1Hash(const QString& absolute_path) {
  QFile file(absolute_path);
  QFileInfo info(file);

  if (!info.isFile() || info.size() == 0 ||
      file.fileName().startsWith("/sys/") ||
      file.fileName().startsWith("/proc/")) {
    // Don't try to hash empty files, devices or other things that aren't files.
    return "";
  }

  if (!file.open(QIODevice::ReadOnly)) {
    return "";
  }

  QCryptographicHash h(QCryptographicHash::Sha1);
  while (!file.atEnd()) {
    h.addData(file.read(4096));
  }

  return h.result();
}

bool Tracer::RedirectSyscall(const Tracer::PidState& state,
                             Tracer::Registers* regs) {
  bool modified_regs = false;
  switch (regs->syscall) {
    case __NR_open:
      modified_regs |= RedirectSyscallArg(
          state, 0, OpenFlagsMightWrite(regs->args[1]), regs);
      break;
    case __NR_rename:
      modified_regs |= RedirectSyscallArg(state, 0, true, regs);
      modified_regs |= RedirectSyscallArg(state, 1, true, regs);
      break;
    case __NR_stat:
    case __NR_lstat:
      modified_regs |= RedirectSyscallArg(state, 0, false, regs);
      break;
    case __NR_mkdir:
    case __NR_rmdir:
    case __NR_creat:
    case __NR_chmod:
    case __NR_chown:
    case __NR_lchown:
    case __NR_unlink:
    case __NR_chdir:
    case __NR_utime:
    case __NR_utimes:
      modified_regs |= RedirectSyscallArg(state, 0, true, regs);
      break;
    case __NR_link:
    case __NR_symlink:
      modified_regs |= RedirectSyscallArg(state, 1, true, regs);
      break;

    case __NR_openat:
      modified_regs |= RedirectSyscallArgAt(
          state, 1, 0, OpenFlagsMightWrite(regs->args[1]), regs);
      break;
    case __NR_mkdirat:
    case __NR_fchownat:
    case __NR_fchmodat:
    case __NR_unlinkat:
      modified_regs |= RedirectSyscallArgAt(state, 1, 0, true, regs);
      break;
    case __NR_renameat:
      modified_regs |= RedirectSyscallArgAt(state, 1, 0, true, regs);
      modified_regs |= RedirectSyscallArgAt(state, 3, 2, true, regs);
      break;
    case __NR_linkat:
      modified_regs |= RedirectSyscallArgAt(state, 3, 2, true, regs);
      break;
    case __NR_symlinkat:
      modified_regs |= RedirectSyscallArgAt(state, 2, 1, true, regs);
      break;
    case __NR_faccessat:
    case __NR_newfstatat:
      modified_regs |= RedirectSyscallArgAt(state, 1, 0, false, regs);
      break;
  }

  if (modified_regs) {
    regs->ToPid(state.pid);
  }
  return modified_regs;
}

bool Tracer::RedirectSyscallArg(const Tracer::PidState& state, int arg_index,
                                bool might_write, Tracer::Registers* regs,
                                QString filename) {
  if (filename.isNull()) {
    filename = ReadAbsolutePath(
        state.pid, reinterpret_cast<void*>(regs->args[arg_index]));
  }
  while (filename.startsWith(opts_.redirect_root)) {
    filename = filename.mid(opts_.redirect_root.length());
  }
  if (filename.startsWith(opts_.project_root)) {
    // Never redirect files in the project directory.
    return false;
  }

  const QString redirected_filename = opts_.redirect_root + filename;

  if (!might_write && !QFile::exists(redirected_filename)) {
    // Only redirect read-only opens if the file was already written to.
    return false;
  }

  if (QFile::exists(filename) && !QFile::exists(redirected_filename)) {
    if (QFileInfo(filename).isDir()) {
      LOG(INFO) << "Creating directory " << redirected_filename;
      QDir().mkpath(redirected_filename);
    } else {
      if (!utils::RecursiveCopy(filename, redirected_filename)) {
        return false;
      }
    }
  }

  const QString orig_dir = QFileInfo(filename).path();
  const QString redirected_dir = QFileInfo(redirected_filename).path();
  if (QFile::exists(orig_dir) && !QFile::exists(redirected_dir)) {
    LOG(INFO) << "Creating directory " << redirected_dir;
    QDir().mkpath(redirected_dir);
  }

  // Write the new filename to the process' memory.
  state.mem.WriteNullTerminatedUtf8(
      redirected_filename, reinterpret_cast<void*>(state.scratch_space));
  regs->args[arg_index] = state.scratch_space;

  LOG(INFO) << "Redirecting syscall " << regs->syscall << " for " << filename;
  return true;
}

bool Tracer::RedirectSyscallArgAt(const PidState& state, int arg_index,
                                  int at_index, bool might_write,
                                  Registers *regs) {
  const QString& filename = ReadPathAt(
      &state, regs->args[at_index],
      reinterpret_cast<void*>(regs->args[arg_index]));
  return RedirectSyscallArg(state, arg_index, might_write, regs, filename);
}
