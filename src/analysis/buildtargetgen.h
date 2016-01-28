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

#ifndef ANALYSIS_BUILDTARGETGEN_H_
#define ANALYSIS_BUILDTARGETGEN_H_

#include "analysis/tracenode.h"
#include "tracer.pb.h"

namespace analysis {

class BuildTargetGen {
 public:
  virtual ~BuildTargetGen() {}

  void Init(Make* make);

  virtual bool Gen(const TraceNode& node, pb::BuildTarget* target) = 0;

 protected:
  int AddInputs(const TraceNode& node,
                pb::BuildTarget* target,
                const QSet<QString>& valid_extensions,
                int limit);
  int AddOutputs(const TraceNode& node,
                 pb::BuildTarget* target,
                 const QSet<QString>& valid_extensions,
                 int limit);

  Make* make_ = nullptr;
};

}  // namespace analysis

#endif  // ANALYSIS_BUILDTARGETGEN_H_
