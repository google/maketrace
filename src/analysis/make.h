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

#ifndef ANALYSIS_MAKE_H_
#define ANALYSIS_MAKE_H_

#include <memory>

#include "common.h"
#include "graph.h"
#include "installedfilesreader.h"
#include "toolsearchpath.h"
#include "tracenode.h"
#include "tracer.pb.h"
#include "tracereader.h"
#include "utils/recordfile.h"

class GeneratedFileProcessor;

namespace analysis {

class Make {
 public:
  struct Options {
    QString trace_filename;    // Read trace records from this file.
    QString install_filename;  // Read installed file records from this file.
    QString output_filename;   // Write BuildTarget records to this file.

    // If these are not empty, graphs will be written to these files in dot
    // format.
    QString graph_output_filename;
    QString intermediate_graph_output_filename;
  };

  static bool Run(const Options& opts);

  ~Make();

  const pb::MetaData& metadata() const { return trace_.metadata(); }
  const pb::Process& process(int id) const { return trace_.process(id); }
  ToolSearchPath* tool_search_path() { return &tool_search_path_; }

  QString NewTargetName(const QString& filename);
  void CreateReference(const QString& name, pb::Reference* ref) const;
  void CreateReference(const pb::BuildTarget& target, pb::Reference* ref) const;

  const Graph<TraceNode>& graph() const { return graph_; }
  const QList<pb::BuildTarget>& build_targets() const {
    return build_targets_;
  }

 private:
  Make(const Options& opts);

  const pb::BuildTarget& TargetForNode(const QString& node_id) {
    return build_targets_[targets_by_node_id_[node_id]];
  }

  bool ReadInputs();
  bool WriteOutput();

  void ReadInstalledFileRecords(
      std::unique_ptr<utils::RecordFile<pb::Record>> file);
  void BuildGraph();
  void FindCompileTargets();
  void FindLinkTargets();

  void AddEdgeFromFile(const FileEvent& event);

  void GenerateBuildTargets();
  bool RemoveDuplicates();

  void ReplaceDependencyTargetNames();

 private:
  const Options opts_;

  ToolSearchPath tool_search_path_;

  TraceReader trace_;
  InstalledFilesReader installed_files_;

  // Filled by BuildGraph.
  Graph<TraceNode> graph_;

  // Filled by GenerateBuildTargets.
  QList<pb::BuildTarget> build_targets_;
  QMap<QString, int> targets_by_name_;
  QMap<QString, int> targets_by_node_id_;
};

}  // namespace analysis

#endif  // ANALYSIS_MAKE_H_
