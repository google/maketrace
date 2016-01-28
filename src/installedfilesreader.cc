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

#include "installedfilesreader.h"

InstalledFilesReader::InstalledFilesReader() {
}

void InstalledFilesReader::Read(
    std::unique_ptr<utils::RecordFile<pb::Record>> file) {
  while (!file->AtEnd()) {
    pb::Record record;
    CHECK(file->ReadRecord(&record));

    if (record.has_installed_file()) {
      files_.append(record.installed_file());
    }
  }
}

bool InstalledFilesReader::Find(
    const QString& name,
    const QList<pb::InstalledFile_Type>& types,
    pb::InstalledFile* file) const {
  for (const pb::InstalledFile& f : files_) {
    if (f.original().name() == name) {
      for (pb::InstalledFile_Type type : types) {
        if (f.type() == type) {
          file->CopyFrom(f);
          return true;
        }
      }
    }
  }
  return false;
}

QList<pb::InstalledFile> InstalledFilesReader::AllOfType(
    pb::InstalledFile_Type type) const {
  QList<pb::InstalledFile> ret;
  for (const pb::InstalledFile& f : files_) {
    if (f.type() == type) {
      ret.append(f);
    }
  }
  return ret;
}
