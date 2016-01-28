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

#ifndef UTILS_PATH_H
#define UTILS_PATH_H

#include "common.h"

namespace utils {
namespace path {

// Sensible wrapper around readlink().
QString Readlink(const QString& path);

QString MakeAbsolute(const QString& path, const QString& base);
QString MakeRelativeTo(const QString& absolute_path, const QString& base);

QString Filename(const QString& path);
QString Extension(const QString& path);
QString PathWithoutExtension(const QString& path);

}  // namespace path
}  // namespace utils

#endif // UTILS_PATH_H
