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

#include "rule.h"

namespace gen {
namespace bazel {

Rule::Rule(const Label& label, const QString& type)
    : label_(label),
      type_(type) {
}

void Rule::SetAttribute(const QString& attribute_name,
                        const QSet<QString>& items,
                        blaze_query::Rule* rule) {
  if (items.isEmpty()) {
    return;
  }

  QStringList list = items.toList();
  qSort(list);

  blaze_query::Attribute* attr = rule->add_attribute();
  attr->set_name(attribute_name);
  attr->set_type(blaze_query::Attribute_Discriminator_STRING_LIST);
  attr->set_string_list_value(list);
}

blaze_query::Rule Rule::ToProto() const {
  blaze_query::Rule pb;

  pb.set_rule_class(type_);
  pb.set_name(label_.absolute());

  SetAttribute("srcs", srcs_, &pb);
  SetAttribute("deps", deps_, &pb);
  SetAttribute("copts", copts_, &pb);
  SetAttribute("linkopts", linkopts_, &pb);
  SetAttribute("textual_hdrs", textual_hdrs_, &pb);

  if (!visibility_.isEmpty()) {
    QStringList names;
    for (const Label& label : visibility_) {
      names.append(label.absolute());
    }

    blaze_query::Attribute* attr = pb.add_attribute();
    attr->set_name("visibility");
    attr->set_type(blaze_query::Attribute_Discriminator_STRING_LIST);
    attr->set_string_list_value(names);
  }

  return pb;
}

}  // namespace bazel
}  // namespace gen
