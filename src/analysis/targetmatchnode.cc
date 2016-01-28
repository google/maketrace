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

#include "analysis/targetmatchnode.h"

#include "analysis/make.h"
#include "utils/path.h"

namespace analysis {

bool TargetMatchNode::Match(const TraceNode& node) const {
  bool type_match = false;
  for (TraceNode::Type expected_type : types_) {
    if (node.type_ == expected_type) {
      type_match = true;
      break;
    }
  }
  if (!type_match) {
    return false;
  }

  if (node.type_ == TraceNode::Type::Process && !process_filename_.isEmpty()) {
    bool filename_match = false;
    const QString node_proc_filename = utils::path::Filename(
        node.make_->process(node.process_id_).filename());
    for (const QString& expected_filename : process_filename_) {
      if (expected_filename == node_proc_filename) {
        filename_match = true;
        break;
      }
    }
    if (!filename_match) {
      return false;
    }
  }
  return true;
}

}  // namespace analysis
