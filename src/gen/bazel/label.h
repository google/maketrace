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

#ifndef GEN_BAZEL_LABEL_H
#define GEN_BAZEL_LABEL_H

#include <QString>

namespace gen {
namespace bazel {

class Label {
 public:
  Label(const QString& package, const QString& target);

  static Label FromAbsolute(const QString& name);

  QString package() const { return package_; }
  QString target() const { return target_; }
  QString absolute() const { return "//" + package_ + ":" + target_; }

  // If this label and the other label are both in the same package, returns
  // :other.target(), otherwise returns other.absolute().
  QString RelativeTarget(const Label& other) const;

 private:
  QString package_;
  QString target_;
};

}  // namespace bazel
}  // namespace gen

#endif // GEN_BAZEL_LABEL_H
