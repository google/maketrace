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

#ifndef TOOLSEARCHPATH_H_
#define TOOLSEARCHPATH_H_

#include "common.h"

class ToolSearchPath {
 public:
  QSet<QString> Get(const QString& program);

 private:
  static void GetGcc(const QString& program, QSet<QString>* ret);
  static void GetLd(const QString& program, QSet<QString>* ret);

  QMap<QString, QSet<QString>> cache_;
};

#endif  // TOOLSEARCHPATH_H_
