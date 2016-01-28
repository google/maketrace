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

#include "toolsearchpath.h"

#include "utils/logging.h"
#include "utils/path.h"
#include "utils/str.h"

#include <QDir>
#include <QProcess>

QSet<QString> ToolSearchPath::Get(const QString& program) {
  const auto& it = cache_.find(program);
  if (it != cache_.end()) {
    return it.value();
  }

  QSet<QString> ret;
  const QString filename = utils::path::Filename(program);
  if (filename == "gcc" || filename == "g++") {
    GetGcc(program, &ret);
  } else if (filename == "ld") {
    GetLd(program, &ret);
  }

  LOG(INFO) << "Library search path for " << program << ":";
  for (const QString& path : ret) {
    LOG(INFO) << "  " << path;
  }

  cache_.insert(program, ret);
  return ret;
}

void ToolSearchPath::GetGcc(const QString& program, QSet<QString>* ret) {
  QProcess proc;
  proc.start(program,
             QStringList() << "-print-search-dirs",
             QProcess::ReadOnly);
  if (!proc.waitForFinished(1000)) {
    LOG(WARNING) << "Failed to run " << program << " to find library search "
                    "path";
    return;
  }

  const QString kLinePrefix("libraries: ");
  for (const QString& line : QString::fromUtf8(proc.readAll()).split('\n')) {
    if (!line.startsWith(kLinePrefix)) {
      continue;
    }
    for (QString path : line.mid(kLinePrefix.length()).split(':')) {
      if (path.startsWith('=')) {
        path = path.mid(1);
      }
      const QString canonical = QDir(path).canonicalPath();
      if (!canonical.isEmpty()) {
        ret->insert(canonical);
      }
    }
    break;
  }
}

void ToolSearchPath::GetLd(const QString& program, QSet<QString>* ret) {
  QProcess proc;
  proc.start(program,
             QStringList() << "--verbose",
             QProcess::ReadOnly);
  if (!proc.waitForFinished(1000)) {
    LOG(WARNING) << "Failed to run " << program << " to find library search "
                    "path";
    return;
  }

  QRegExp re("SEARCH_DIR\\(\"=*([^\"]+)\"\\);",
             Qt::CaseSensitive,
             QRegExp::RegExp2);
  for (const QString& line : QString::fromUtf8(proc.readAll()).split('\n')) {
    int pos = 0;

    while ((pos = re.indexIn(line, pos)) != -1) {
      const QString canonical = QDir(re.cap(1)).canonicalPath();
      if (!canonical.isEmpty()) {
        ret->insert(canonical);
      }
      pos += re.matchedLength();
    }
  }
}

