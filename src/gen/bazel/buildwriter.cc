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

#include "buildwriter.h"

#include "utils/logging.h"

namespace gen {
namespace bazel {

namespace {

QString EscapeString(QString s) {
  s.replace('\\', "\\\\");
  s.replace('"', "\\\"");
  return s;
}

}

BuildWriter::BuildWriter(QTextStream* s)
    : s_(*s) {
}

void BuildWriter::WriteRule(const blaze_query::Rule& rule) {
  int colon_idx = rule.name().indexOf(':');
  if (colon_idx == -1) {
    LOG(ERROR) << "Expected bazel rule name to contain a colon: "
               << rule.name();
    return;
  }
  const QString rule_name = rule.name().mid(colon_idx + 1);

  s_ << rule.rule_class() << "(\n"
     << "  name = \"" << rule_name << "\",\n";

  for (const blaze_query::Attribute& attr : rule.attribute()) {
    WriteAttribute(attr);
  }

  s_ << ")\n\n";
}

void BuildWriter::WriteAttribute(const blaze_query::Attribute& attr) {
  s_ << "  " << attr.name() << " = ";

  switch (attr.type()) {
  case blaze_query::Attribute_Discriminator_INTEGER:
    s_ << attr.int_value();
    break;

  case blaze_query::Attribute_Discriminator_STRING:
    s_ << "\"" << EscapeString(attr.string_value()) << "\"";
    break;

  case blaze_query::Attribute_Discriminator_BOOLEAN:
    s_ << (attr.boolean_value() ? "true" : "false");
    break;

  case blaze_query::Attribute_Discriminator_STRING_LIST:
    if (attr.string_list_value().count() == 0) {
      s_ << "[]";
    } else if (attr.string_list_value().count() == 1) {
      s_ << "[\"" << attr.string_list_value()[0] << "\"]";
    } else {
      s_ << "[\n";
      for (const QString& v : attr.string_list_value()) {
        s_ << "    \"" << EscapeString(v) << "\",\n";
      }
      s_ << "  ]";
    }
    break;

  default:
    LOG(ERROR) << "Bazel attribute not supported: " << attr.DebugString();
    break;
  }

  s_ << ",\n";
}

} // namespace bazel
} // namespace gen
