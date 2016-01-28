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

#ifndef ANALYSIS_TARGETMATCHNODE_H_
#define ANALYSIS_TARGETMATCHNODE_H_

#include "common.h"
#include "analysis/tracenode.h"

namespace analysis {

class TargetMatchNode {
 public:
  QString ID() const { return id_; }

  QString id_;
  QList<TraceNode::Type> types_;
  QStringList process_filename_;
  bool exact_incoming_neighbour_count_;
  bool exact_outgoing_neighbour_count_;

  bool Match(const TraceNode& node) const;
  bool ExactIncomingNeighbourCount() const {
    return exact_incoming_neighbour_count_;
  }
  bool ExactOutgoingNeighbourCount() const {
    return exact_outgoing_neighbour_count_;
  }
};

}  // namespace analysis

#endif // ANALYSIS_TARGETMATCHNODE_H_
