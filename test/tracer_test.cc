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

#include <gtest/gtest.h>

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
  f.open();
  f.close();

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
  f.open();
  f.write("foo");
  f.close();

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
  f.open();
  f.write("foo");
  f.close();

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
  f.open();
  f.write("hello");
  f.close();

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
  f.open();
  f.close();

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
  f.open();
  f.close();

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
