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

#ifndef INSTALLEDFILESREADER_H
#define INSTALLEDFILESREADER_H

#include <memory>

#include "tracer.pb.h"
#include "utils/recordfile.h"

class InstalledFilesReader {
 public:
  InstalledFilesReader();

  void Read(std::unique_ptr<utils::RecordFile<pb::Record>> file);
  bool Find(const QString& name,
            const QList<pb::InstalledFile_Type>& types,
            pb::InstalledFile* file) const;
  QList<pb::InstalledFile> AllOfType(pb::InstalledFile_Type type) const;

 private:
  QList<pb::InstalledFile> files_;
};

#endif // INSTALLEDFILESREADER_H
