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

#include "analysis/tracenode.h"

#include "tracer.pb.h"
#include "analysis/make.h"
#include "utils/logging.h"
#include "utils/str.h"

namespace analysis {

TraceNode::TraceNode()
    : TraceNode(nullptr, Type::Unknown, "", 0, "", 0) {}

TraceNode TraceNode::SourceFile(Make* m,
                                const QString& source_filename) {
  return TraceNode(m, Type::SourceFile, source_filename, 0, "", 0);
}

TraceNode TraceNode::GeneratedFile(Make* m,
                                   int process_id,
                                   int file_index,
                                   const QByteArray& sha1) {
  return TraceNode(m, Type::GeneratedFile, "", file_index, sha1, process_id);
}

TraceNode TraceNode::Process(Make* m, int process_id) {
  return TraceNode(m, Type::Process, "", 0, "", process_id);
}

TraceNode TraceNode::CompileStep(Make* m, int process_id) {
  return TraceNode(m, Type::CompileStep, "", 0, "", process_id);
}

TraceNode TraceNode::DynamicLinkStep(Make* m, int process_id) {
  return TraceNode(m, Type::DynamicLinkStep, "", 0, "", process_id);
}

TraceNode TraceNode::StaticLinkStep(Make* m, int process_id) {
  return TraceNode(m, Type::StaticLinkStep, "", 0, "", process_id);
}

TraceNode::TraceNode(Make* m, Type type, const QString& source_filename,
                     int file_index, const QByteArray& sha1, int process_id)
    : make_(m),
      type_(type),
      source_filename_(source_filename),
      file_index_(file_index),
      sha1_(sha1),
      process_id_(process_id) {
}


QString TraceNode::ID() const {
  switch (type_) {
    case Type::SourceFile:
      return "source/" + Filename();
    case Type::GeneratedFile:
      return "gen/" + sha1_.toHex() + ":" + Filename();
    case Type::Process:
      return "proc/" + QString::number(process_id_);
    case Type::CompileStep:
      return "compile/" + QString::number(process_id_);
    case Type::DynamicLinkStep:
      return "dlink/" + QString::number(process_id_);
    case Type::StaticLinkStep:
      return "slink/" + QString::number(process_id_);
    default:
      LOG(FATAL) << "Unknown type: " << int(type_);
      return "";
  }
}

QString TraceNode::Filename() const {
  switch (type_) {
    case Type::SourceFile:
      return source_filename_;
    case Type::GeneratedFile: {
      const pb::Process* proc = &make_->process(process_id_);
      const pb::File* file = &proc->files(file_index_);
      return file->filename();
    }
    default:
      LOG(FATAL) << "Filename() called on node " << ID();
      return "";
  }
}

void TraceNode::WriteDot(QTextStream& os) const {
  switch (type_) {
    case TraceNode::Type::GeneratedFile:
      os << "shape=box,label=\"" << Filename() << "\"";
      break;
    case TraceNode::Type::Process: {
      const pb::Process* proc = &make_->process(process_id_);
      os << "shape=ellipse,label=\""
         << proc->argv()[0] << " (" << proc->id() << ")\"";
      break;
    }
    case TraceNode::Type::SourceFile:
      os << "shape=box,style=dashed,label=\"" << Filename() << "\"";
      break;
    case TraceNode::Type::CompileStep: {
      const pb::Process* proc = &make_->process(process_id_);
      os << "shape=ellipse,style=filled,fillcolor=yellow,label=\"Compile "
         << proc->argv()[0] << " (" << proc->id() << ")\"";
      break;
    }
    case TraceNode::Type::StaticLinkStep:
    case TraceNode::Type::DynamicLinkStep: {
      const pb::Process* proc = &make_->process(process_id_);
      os << "shape=ellipse,style=filled,fillcolor=red,label=\"Link "
         << proc->argv()[0] << " (" << proc->id() << ")\"";
      break;
    }
    default:
      LOG(FATAL) << "Unknown node type: " << int(type_);
      break;
  }
}

}  // namespace analysis
