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

#include "reference.h"
#include "utils/logging.h"

void CreateReference(const pb::MetaData& metadata,
                     const QString& name,
                     pb::Reference* ref) {
  if (name.startsWith("//")) {
    ref->set_type(pb::Reference_Type_BUILD_TARGET);
    ref->set_name(name);
  } else if (name.startsWith("-l")) {
    ref->set_type(pb::Reference_Type_LIBRARY);
    ref->set_name(name.mid(2));
  } else if (name.startsWith("/")) {
    if (name.startsWith(metadata.project_root())) {
      ref->set_type(pb::Reference_Type_RELATIVE_TO_PROJECT_ROOT);
      if (name == metadata.project_root()) {
        ref->set_name(".");
      } else {
        ref->set_name(name.mid(metadata.project_root().length() + 1));
      }
    } else if (metadata.has_redirect_root() &&
               name.startsWith(metadata.redirect_root())) {
      ref->set_type(pb::Reference_Type_ABSOLUTE);
      if (name == metadata.redirect_root()) {
        ref->set_name(".");
      } else {
        ref->set_name(name.mid(metadata.redirect_root().length()));
      }
    } else {
      ref->set_type(pb::Reference_Type_ABSOLUTE);
      ref->set_name(name);
    }
  } else {
    if (metadata.has_build_dir() && name.startsWith(metadata.build_dir())) {
      ref->set_type(pb::Reference_Type_RELATIVE_TO_BUILD_DIR);
      if (name == metadata.build_dir()) {
        ref->set_name(".");
      } else {
        ref->set_name(name.mid(metadata.build_dir().length() + 1));
      }
    } else {
      ref->set_type(pb::Reference_Type_RELATIVE_TO_PROJECT_ROOT);
      ref->set_name(name);
    }
  }
}

bool operator<(const pb::Reference& a, const pb::Reference& b) {
#define COMPARE(field) \
  if (a.field() < b.field()) { return true; } \
  if (a.field() > b.field()) { return false; }

  COMPARE(type);
  COMPARE(name);
  return false;

#undef COMPARE
}
