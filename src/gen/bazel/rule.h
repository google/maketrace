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

#ifndef GEN_BAZEL_RULE_H
#define GEN_BAZEL_RULE_H

#include "build.pb.h"
#include "gen/bazel/label.h"

#include <QSet>
#include <QString>

namespace gen {
namespace bazel {

class Rule {
 public:
  explicit Rule(const Label& label, const QString& type = QString());

  Label label() const { return label_; }

  void set_label(const Label& label) { label_ = label; }
  void set_type(const QString& type) { type_ = type; }
  void add_visibility(const Label& l) { visibility_.append(l); }
  void add_src(const QString& filename) { srcs_.insert(filename); }
  void add_src(const Label& l) { srcs_.insert(label_.RelativeTarget(l)); }
  void add_dep(const QString& filename) { deps_.insert(filename); }
  void add_dep(const Label& l) { deps_.insert(label_.RelativeTarget(l)); }
  void add_textual_hdr(const Label& l) {
    textual_hdrs_.insert(label_.RelativeTarget(l)); }
  void add_textual_hdr(const QString& filename) {
    textual_hdrs_.insert(filename); }
  void add_copt(const QString& copt) { copts_.insert(copt); }
  void add_linkopt(const QString& linkopt) { linkopts_.insert(linkopt); }

  bool has_srcs() const { return !srcs_.isEmpty(); }

  blaze_query::Rule ToProto() const;

 private:
  static void SetAttribute(const QString& attribute_name,
                           const QSet<QString>& items,
                           blaze_query::Rule* rule);
  static QSet<QString> EscapeShellArgs(const QSet<QString>& args);

  Label label_;
  QString type_;
  QList<Label> visibility_;
  QSet<QString> copts_;
  QSet<QString> linkopts_;
  QSet<QString> deps_;
  QSet<QString> srcs_;
  QSet<QString> textual_hdrs_;
};

}  // namespace bazel
}  // namespace gen

#endif // GEN_BAZEL_RULE_H
