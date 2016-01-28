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

#ifndef GEN_BAZEL_BUILDWRITER_H
#define GEN_BAZEL_BUILDWRITER_H

#include <QTextStream>

#include "build.pb.h"

namespace gen {
namespace bazel {

// Like com.google.devtools.build.lib.query2.output.OutputFormatter.BuildOutputFormatter
class BuildWriter {
 public:
  explicit BuildWriter(QTextStream* s);

  void WriteRule(const blaze_query::Rule& rule);
  void WriteAttribute(const blaze_query::Attribute& attr);

 private:
  QTextStream& s_;
};

} // namespace bazel
} // namespace gen

#endif // GEN_BAZEL_BUILDWRITER_H
