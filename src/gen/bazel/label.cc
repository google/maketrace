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

#include "label.h"

#include "utils/logging.h"

namespace gen {
namespace bazel {

Label::Label(const QString& package, const QString& target)
    : package_(package),
      target_(target) {
}

Label Label::FromAbsolute(const QString& name) {
  CHECK(name.startsWith("//")) << name.toStdString();

  const int colon = name.indexOf(':');
  CHECK(colon != -1) << name.toStdString();

  return Label(name.mid(2, colon - 2), name.mid(colon + 1));
}

QString Label::RelativeTarget(const Label& other) const {
  if (package() == other.package()) {
    return ":" + other.target();
  }
  return other.absolute();
}

}  // namespace bazel
}  // namespace gen

