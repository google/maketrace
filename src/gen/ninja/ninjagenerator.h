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

#ifndef NINJAGENERATOR_H
#define NINJAGENERATOR_H

#include "common.h"
#include "tracer.pb.h"

#include <QTextStream>

class NinjaGenerator {
 public:
  NinjaGenerator();

  void Generate(const QList<pb::BuildTarget>& targets);

 private:
  void WriteCompileTarget(const pb::BuildTarget& target, QTextStream& s);
  void WriteLinkTarget(const pb::BuildTarget& target, QTextStream& s);

  QStringList OutputFilenames(const pb::BuildTarget& target) const;
  QStringList InputFilenames(const pb::BuildTarget& target) const;

  QMap<QString, pb::BuildTarget> targets_;
};

#endif // NINJAGENERATOR_H
