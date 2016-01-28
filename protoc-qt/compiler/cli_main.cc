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

#include <google/protobuf/compiler/command_line_interface.h>

#include "cpp_generator.h"

int main(int argc, char* argv[]) {
  google::protobuf::compiler::CommandLineInterface cli;
  google::protobuf::compiler::cpp::CppGenerator gen;
  cli.RegisterGenerator("--cpp_out", "--cpp_out", &gen,
                        "Generate C++ header and source with Qt types.");
  cli.RegisterGenerator("--qt_out", "--qt_out", &gen,
                        "Generate C++ header and source with Qt types.");
  return cli.Run(argc, argv);
}
