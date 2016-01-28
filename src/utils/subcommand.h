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

#ifndef UTILS_SUBCOMMAND_H
#define UTILS_SUBCOMMAND_H

#include <QStringList>
#include <QTextStream>

#include <gflags/gflags.h>

namespace utils {

struct SubcommandSpec {
  QString name;
  QString usage;
  QString description;
  int required_args;
  std::function<bool (const QStringList& args)> func;

  void PrintUsage(const QString& program, QTextStream& s) const;
};


template <typename ArrayType>
int RunSubcommand(int argc, char** argv, ArrayType subcommands);


namespace {

template <typename ArrayType>
bool Find(const QString& name, ArrayType subcommands, SubcommandSpec* s) {
  for (const SubcommandSpec& spec: subcommands) {
    if (spec.name == name) {
      *s = spec;
      return true;
    }
  }
  return false;
}

template <typename ArrayType>
void PrintCommandList(ArrayType subcommands, QTextStream& s) {
  s << "Usage: " << gflags::GetArgv0() << " <command> [options ...]\n"
    << "\n"
    << "Commands:\n";

  for (const SubcommandSpec& spec : subcommands) {
    s << "    " << qSetFieldWidth(16) << spec.name << qSetFieldWidth(0)
      << spec.description.split('\n')[0] << "\n";
  }
}

}  // namespace


template <typename ArrayType>
int RunSubcommand(int argc, char** argv, ArrayType subcommands) {
  QStringList args;
  for (int i = 1; i < argc; ++i) {
    args.append(QString::fromUtf8(argv[i]));
  }

  QTextStream s(stdout);
  s.setFieldAlignment(QTextStream::AlignLeft);

  if (args.empty() ||
      (args.count() == 1 && args[0] == "help")) {
    // Show a list of commands.
    PrintCommandList(subcommands, s);
    return 0;
  } else if (args[0] == "help") {
    // Show usage for this command, if it was found.
    SubcommandSpec cmd;
    if (Find(args[1], subcommands, &cmd)) {
      cmd.PrintUsage(gflags::GetArgv0(), s);
    } else {
      s << "Unknown command: " << args[0] << "\n";
    }
  } else {
    // Run the command if the required number of arguments was provided.
    SubcommandSpec cmd;
    if (Find(args[0], subcommands, &cmd)) {
      if (args.count() < cmd.required_args + 1) {
        cmd.PrintUsage(gflags::GetArgv0(), s);
      } else {
        return cmd.func(args.mid(1)) ? 0 : 1;
      }
    } else {
      PrintCommandList(subcommands, s);
    }
  }
  return 1;
}

}  // namespace utils

#endif // SUBCOMMAND_H
