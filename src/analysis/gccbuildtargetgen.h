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

#ifndef ANALYSIS_GCCBUILDTARGETGEN_H_
#define ANALYSIS_GCCBUILDTARGETGEN_H_

#include "analysis/buildtargetgen.h"

namespace analysis {

class GccBuildTargetGen : public BuildTargetGen {
 public:
  GccBuildTargetGen();

  bool Gen(const TraceNode& node, pb::BuildTarget* target);

 private:
  const QSet<QString> cc_compile_input_extensions_;
  const QSet<QString> cc_compile_output_extensions_;
  const QSet<QString> cc_link_input_extensions_;
  const QSet<QString> cc_link_standard_libs_;
};

}  // namespace analysis

#endif // ANALYSIS_GCCBUILDTARGETGEN_H_
