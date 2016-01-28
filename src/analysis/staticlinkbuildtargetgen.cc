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

#include "analysis/staticlinkbuildtargetgen.h"

#include "analysis/make.h"
#include "utils/logging.h"

namespace analysis {

bool StaticLinkBuildTargetGen::Gen(const TraceNode& node,
                                   pb::BuildTarget* target) {
  if (node.type_ != TraceNode::Type::StaticLinkStep) {
    return false;
  }

  if (AddInputs(node, target, {"o"}, 0) < 1) {
    LOG(INFO) << "Not enough inputs to " << node.ID();
    return false;
  }
  if (AddOutputs(node, target, {"a"}, 1) < 1) {
    LOG(INFO) << "Not enough outputs from " << node.ID();
    return false;
  }

  target->mutable_c_link()->set_is_library(true);
  target->set_qualified_name(make_->NewTargetName(target->outputs(0).name()));
  return true;
}

}  // namespace analysis
