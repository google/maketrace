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

#ifndef ANALYSIS_TRACENODE_H_
#define ANALYSIS_TRACENODE_H_

#include <QTextStream>

#include "common.h"

namespace analysis {

class Make;

class TraceNode {
 public:
  enum class Type {
    Unknown,

    SourceFile,
    GeneratedFile,
    Process,

    CompileStep,
    DynamicLinkStep,
    StaticLinkStep,
  };

  TraceNode();

  static TraceNode SourceFile(Make* m, const QString& source_filename);
  static TraceNode GeneratedFile(Make* m, int process_id,
                                 int file_index, const QByteArray& sha1);
  static TraceNode Process(Make* m, int process_id);
  static TraceNode CompileStep(Make* m, int process_id);
  static TraceNode DynamicLinkStep(Make* m, int process_id);
  static TraceNode StaticLinkStep(Make* m, int process_id);

  QString ID() const;
  void WriteDot(QTextStream& os) const;

  Make* make_;
  Type type_;

  // For source files only.
  QString source_filename_;

  // For generated files only.
  int file_index_;
  QByteArray sha1_;

  // For source files and generated files only.
  QString Filename() const;

  // For generated files and processes.
  int process_id_;

  // For compile steps.
  int compiler_frontend_process_id_;

 private:
  TraceNode(Make* m, Type type, const QString& source_filename,
            int file_index, const QByteArray& sha1, int process_id);
};

}  // namespace analysis

#endif  // ANALYSIS_TRACENODE_H_
