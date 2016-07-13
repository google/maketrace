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

#include <fcntl.h>
#include <gtest/gtest.h>
#include <syscall.h>

#include <QTemporaryDir>
#include <QTemporaryFile>

#include "tracer.h"

class TracerTest : public ::testing::Test {
 protected:
  void SetUp() {
    tracer_.reset(
        new Tracer("/foo",
        std::unique_ptr<utils::RecordWriter<pb::Record>>(
            new utils::MemoryRecordWriter<pb::Record>(&records_))));
  }

  void Run(Tracer::Tracee tracee) {
    ASSERT_TRUE(tracer_->Start(tracee));
    ASSERT_TRUE(tracer_->TraceUntilExit());

    for (const pb::Record& record : records_) {
      processes_.append(record.process());
    }
  }

  void WriteFile(QFile* file, const QString& contents) {
    file->open(QFile::WriteOnly);
    file->write(contents.toUtf8());
    file->close();
  }

  std::unique_ptr<Tracer> tracer_;
  QList<pb::Record> records_;
  QList<pb::Process> processes_;
};

TEST_F(TracerTest, ExitCode) {
  Run([]() {
    exit(42);
  });

  ASSERT_EQ(1, processes_.count());
  EXPECT_EQ(42, processes_[0].exit_code());
}

TEST_F(TracerTest, OpensNoFiles) {
  Run([]() {});

  ASSERT_EQ(1, processes_.count());
  EXPECT_EQ(0, processes_[0].files_size());
}

TEST_F(TracerTest, OpensOneFileForReading) {
  QTemporaryFile f;
  WriteFile(&f, QString());

  Run([&f]() {
    QFile f2(f.fileName());
    f2.open(QFile::ReadOnly);
  });

  ASSERT_EQ(1, processes_.count());
  ASSERT_EQ(1, processes_[0].files_size());
  EXPECT_EQ(f.fileName(), processes_[0].files(0).filename());
  EXPECT_EQ(pb::File_Access_READ, processes_[0].files(0).access());
}

TEST_F(TracerTest, OpensOneFileForWritingButNotWritten) {
  QTemporaryFile f;
  WriteFile(&f, "foo");

  Run([&f]() {
    QFile f2(f.fileName());
    f2.open(QFile::Append);
  });

  ASSERT_EQ(1, processes_.count());
  ASSERT_EQ(1, processes_[0].files_size());
  EXPECT_EQ(f.fileName(), processes_[0].files(0).filename());
  EXPECT_EQ(pb::File_Access_READ, processes_[0].files(0).access());
}

TEST_F(TracerTest, OpensOneFileForWritingAndWritten) {
  QTemporaryFile f;
  WriteFile(&f, "foo");

  Run([&f]() {
    QFile f2(f.fileName());
    f2.open(QFile::WriteOnly);
    f2.write("hello");
  });

  ASSERT_EQ(1, processes_.count());
  ASSERT_EQ(1, processes_[0].files_size());
  EXPECT_EQ(f.fileName(), processes_[0].files(0).filename());
  EXPECT_EQ(pb::File_Access_MODIFIED, processes_[0].files(0).access());
}

TEST_F(TracerTest, OpensOneFileForWritingAndWrittenButUnchanged) {
  QTemporaryFile f;
  WriteFile(&f, "hello");

  Run([&f]() {
    QFile f2(f.fileName());
    f2.open(QFile::WriteOnly);
    f2.write("hello");
  });

  ASSERT_EQ(1, processes_.count());
  ASSERT_EQ(1, processes_[0].files_size());
  EXPECT_EQ(f.fileName(), processes_[0].files(0).filename());
  EXPECT_EQ(pb::File_Access_WRITTEN_BUT_UNCHANGED,
            processes_[0].files(0).access());
}

TEST_F(TracerTest, CreatesOneFile) {
  QTemporaryDir dir;
  Run([&dir]() {
    QFile f(dir.path() + "/foo");
    f.open(QFile::WriteOnly);
    f.write("hello");
  });

  ASSERT_EQ(1, processes_.count());
  ASSERT_EQ(1, processes_[0].files_size());
  EXPECT_EQ(pb::File_Access_CREATED, processes_[0].files(0).access());
}

TEST_F(TracerTest, DeletesOneFile) {
  QTemporaryFile f;
  WriteFile(&f, QString());

  Run([&f]() {
    QFile::remove(f.fileName());
  });

  ASSERT_EQ(1, processes_.count());
  ASSERT_EQ(1, processes_[0].files_size());
  EXPECT_EQ(f.fileName(), processes_[0].files(0).filename());
  EXPECT_EQ(pb::File_Access_DELETED, processes_[0].files(0).access());
}

TEST_F(TracerTest, RenamesOneFile) {
  QTemporaryFile f;
  WriteFile(&f, QString());

  Run([&f]() {
    QFile::rename(f.fileName(), f.fileName() + "2");
  });
  QFile::remove(f.fileName() + "2");

  ASSERT_EQ(1, processes_.count());
  ASSERT_EQ(1, processes_[0].files_size());
  EXPECT_EQ(f.fileName() + "2", processes_[0].files(0).filename());
  EXPECT_EQ(f.fileName(), processes_[0].files(0).renamed_from());
  EXPECT_EQ(pb::File_Access_READ, processes_[0].files(0).access());
}

TEST_F(TracerTest, OpenAtDirectory) {
  QTemporaryDir dir;
  QFile file(dir.path() + "/foo");
  WriteFile(&file, "foo");

  Run([&dir, &file]() {
    QFile f2(file.fileName());
    f2.open(QFile::ReadOnly);
    f2.close();

    const int fd = openat(AT_FDCWD, dir.path().toUtf8().constData(),
                          O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC);
    char buf[1024];
    while (syscall(SYS_getdents, fd, buf, sizeof(buf))) {}
    close(fd);
  });

  ASSERT_EQ(1, processes_.count());
  ASSERT_EQ(2, processes_[0].files_size());

  pb::File f, d;
  if (processes_[0].files(0).filename() == file.fileName()) {
    f = processes_[0].files(0);
    d = processes_[0].files(1);
  } else {
    f = processes_[0].files(1);
    d = processes_[0].files(0);
  }

  EXPECT_EQ(pb::File_Access_READ, f.access());
  EXPECT_TRUE(f.has_sha1_before());
  EXPECT_TRUE(f.has_sha1_after());

  EXPECT_EQ(dir.path(), d.filename());
  EXPECT_EQ(pb::File_Access_READ, d.access());
  EXPECT_FALSE(d.has_sha1_before());
  EXPECT_FALSE(d.has_sha1_after());
}
