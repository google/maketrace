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

#include "analysis/buildtargetgen.h"

#include "analysis/make.h"
#include "utils/path.h"

namespace analysis {

void BuildTargetGen::Init(Make* make) {
  make_ = make;
}

int BuildTargetGen::AddInputs(const TraceNode& node,
                              pb::BuildTarget* target,
                              const QSet<QString>& valid_extensions,
                              int limit) {
  int ret = 0;
  for (const TraceNode& input : make_->graph().Incoming(node)) {
    if ((input.type_ != TraceNode::Type::SourceFile &&
         input.type_ != TraceNode::Type::GeneratedFile) ||
        (!valid_extensions.isEmpty() &&
         !valid_extensions.contains(
           utils::path::Extension(input.Filename())))) {
      LOG(WARNING) << "Unknown input node to process " << input.ID();
      continue;
    }
    make_->CreateReference(input.Filename(), target->add_srcs());
    ++ret;
    if (limit > 0 && ret == limit) {
      break;
    }
  }
  return ret;
}

int BuildTargetGen::AddOutputs(const TraceNode& node,
                               pb::BuildTarget* target,
                               const QSet<QString>& valid_extensions,
                               int limit) {
  int ret = 0;
  for (const TraceNode& output : make_->graph().Outgoing(node)) {
    if (output.type_ != TraceNode::Type::GeneratedFile ||
        (!valid_extensions.isEmpty() &&
         !valid_extensions.contains(
           utils::path::Extension(output.Filename())))) {
      LOG(WARNING) << "Unknown output node from process " << output.ID();
      continue;
    }
    make_->CreateReference(output.Filename(), target->add_outputs());
    ++ret;
    if (limit > 0 && ret == limit) {
      break;
    }
  }
  return ret;
}

}  // namespace analysis
