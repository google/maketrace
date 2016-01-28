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

#include "utils/path.h"

#include "utils/logging.h"
#include "utils/str.h"

#include <unistd.h>

#include <QDir>
#include <QFileInfo>

namespace utils {
namespace path {

namespace {

QStringList SplitExtension(const QString& p) {
  QString path(p);

  while (true) {
    QString suffix = QFileInfo(path).suffix();
    if (suffix.isEmpty()) {
      return {path, QString()};
    }

    path = path.left(path.length() - suffix.length() - 1);

    bool ok = false;
    suffix.toInt(&ok);
    if (!ok) {
      return {path, suffix};
    }
  }
}

}

QString Readlink(const QString& p) {
  QString path = p;
  for (int i = 0; i < 10; ++i) {
    const QString old_path = path;
    path = QFile::symLinkTarget(path);
    if (path.isEmpty()) {
      return old_path;
    }
  }
  LOG(FATAL) << "Too many symlink dereferences resolving " << path;
  return "";
}

QString MakeAbsolute(const QString& path, const QString& base) {
  return QFileInfo(QDir(base), path).absoluteFilePath();
}

QString MakeRelativeTo(const QString& absolute_path, const QString& base) {
  if (absolute_path == base) {
    return ".";
  }
  if (absolute_path.startsWith(base)) {
    return absolute_path.right(absolute_path.length() - base.length() - 1);
  }
  return absolute_path;
}

QString Filename(const QString& path) {
  return QFileInfo(path).fileName();
}

QString Extension(const QString& path) {
  return SplitExtension(path)[1];
}

QString PathWithoutExtension(const QString& path) {
  return SplitExtension(path)[0];
}

}  // namespace path
}  // namespace utils

