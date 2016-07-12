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

#include "analysis/make.h"

#include <iostream>

#include "buildtargetgen.h"
#include "common.h"
#include "gccbuildtargetgen.h"
#include "reference.h"
#include "staticlinkbuildtargetgen.h"
#include "targetmatchnode.h"
#include "utils/path.h"
#include "utils/str.h"

#include <QRegularExpression>

using utils::path::Extension;
using utils::path::Filename;
using utils::path::PathWithoutExtension;

namespace analysis {

namespace {

bool CheckExtensions(const TraceNode& node,
                     const QSet<QString>& valid_extensions) {
  return valid_extensions.empty() ||
      valid_extensions.contains(Extension(node.Filename()));
}

void ReplaceStrings(QStringList* field,
                    const QString& original,
                    const QString& replacement) {
  for (int i = 0; i < field->size(); ++i) {
    if (field->at(i) == original) {
      field->replace(i, replacement);
    }
  }
}

}  // namespace


Make::Make(const Options& opts)
    : opts_(opts) {
  trace_.IgnoreFileExtensions({
      "h", "hpp", "Plo", "Po", "Tpo", "la", "lai", "loT"});
  trace_.IgnoreProcessFilenames({
      "bash", "cat", "cmake", "grep", "make", "sed", "sh"});
}

Make::~Make() {
}

void Make::AddEdgeFromFile(const FileEvent& event) {
  const pb::Process* pb = &process(event.process_id);
  const pb::File* file = &pb->files(event.file_index);

  const TraceNode proc_node = TraceNode::Process(this, pb->id());

  // If it was a file we read from the project root, and that file hadn't
  // previously been generated, then it's a source file.
  const TraceNode gen_node = TraceNode::GeneratedFile(this, pb->id(),
                                                      event.file_index,
                                                      file->sha1_before());
  if (graph_.HasNode(gen_node)) {
    graph_.AddEdge(gen_node, proc_node);
  } else if (!file->filename().startsWith("/")) {
    graph_.AddEdge(TraceNode::SourceFile(this, file->filename()), proc_node);
  }
}

void Make::BuildGraph() {
  for (const FileEvent& event : trace_.events()) {
    const pb::Process* pb = &process(event.process_id);
    const pb::File* file = &pb->files(event.file_index);

    const TraceNode proc_node = TraceNode::Process(this, pb->id());
    graph_.AddNode(proc_node);

    if (file->has_renamed_from()) {
      // Find the original node and replace its path.
      for (const TraceNode& node : graph_.AllNodes()) {
        if ((node.type_ == TraceNode::Type::SourceFile ||
             node.type_ == TraceNode::Type::GeneratedFile) &&
            node.Filename() == file->renamed_from()) {
          TraceNode replacement = node;
          if (replacement.type_ == TraceNode::Type::SourceFile) {
            replacement.source_filename_ = file->filename();
          } else {
            replacement.process_id_ = pb->id();
            replacement.file_index_ = event.file_index;
          }
          LOG(INFO) << "Replacing " << node.Filename() << " with " << replacement.Filename();
          graph_.ReplaceSubgraph({node}, replacement);
          break;
        }
      }
    } else {
      switch (file->access()) {
        case pb::File_Access_READ:
          AddEdgeFromFile(event);
          break;
        case pb::File_Access_MODIFIED:
        case pb::File_Access_WRITTEN_BUT_UNCHANGED:
          AddEdgeFromFile(event);
          // Fallthrough
        case pb::File_Access_CREATED:
          graph_.AddEdge(proc_node,
                         TraceNode::GeneratedFile(this, pb->id(),
                                                  event.file_index,
                                                  file->sha1_after()));
          break;
        case pb::File_Access_DELETED:
          break;
      }
    }
  }

  // Remove process nodes that don't have any edges.
  for (const TraceNode& node : graph_.AllNodes()) {
    if (graph_.Incoming(node).empty() &&
        graph_.Outgoing(node).empty()) {
      graph_.RemoveNode(node);
    }
  }
}

void Make::FindCompileTargets() {
  Graph<TargetMatchNode> subgraph;
  subgraph.AddEdges({
      {"input", {{TraceNode::Type::SourceFile, TraceNode::Type::GeneratedFile}}, {}, false, false},
      {"cc1", {{TraceNode::Type::Process}}, {"cc1", "cc1plus"}, false, true},
      {"asm", {{TraceNode::Type::GeneratedFile}}, {}, true, true},
      {"as", {{TraceNode::Type::Process}}, {"as"}, true, true},
      {"object", {{TraceNode::Type::GeneratedFile}}, {}, true, false},
  });

  graph_.FindAndReplaceSubgraph(subgraph, [this](
      const QMap<Graph<TargetMatchNode>::IDType, TraceNode>& match) {
    const int parent_process_id =
        process(match["cc1"].process_id_).parent_id();
    TraceNode node = TraceNode::CompileStep(this, parent_process_id);
    node.compiler_frontend_process_id_ = match["cc1"].process_id_;

    graph_.ReplaceSubgraph({
        match["cc1"],
        match["asm"],
        match["as"],
    }, node);
  });
}

void Make::FindLinkTargets() {
  Graph<TargetMatchNode> static_subgraph_ranlib;
  static_subgraph_ranlib.AddEdges({
      {"input", {{TraceNode::Type::GeneratedFile}}, {}, false, false},
      {"ar", {{TraceNode::Type::Process}}, {"ar"}, false, true},
      {"output", {{TraceNode::Type::GeneratedFile}}, {}, true, false},
      {"ranlib", {{TraceNode::Type::Process}}, {"ranlib"}, true, false},
  });
  graph_.FindAndReplaceSubgraph(static_subgraph_ranlib, [this](
      const QMap<typename Graph<TargetMatchNode>::IDType, TraceNode>& match) {
    const TraceNode replacement = TraceNode::StaticLinkStep(
        this, match["ar"].process_id_);
    graph_.ReplaceSubgraph({
        match["ar"],
        match["ranlib"],
    }, replacement);
    graph_.RemoveEdge(match["output"], replacement);
  });

  Graph<TargetMatchNode> static_subgraph;
  static_subgraph.AddEdges({
      {"input", {{TraceNode::Type::GeneratedFile}}, {}, false, false},
      {"ar", {{TraceNode::Type::Process}}, {"ar"}, false, true},
      {"output", {{TraceNode::Type::GeneratedFile}}, {}, true, false},
  });
  graph_.FindAndReplaceSubgraph(static_subgraph, [this](
      const QMap<typename Graph<TargetMatchNode>::IDType, TraceNode>& match) {
    const TraceNode replacement = TraceNode::StaticLinkStep(
        this, match["ar"].process_id_);
    graph_.ReplaceSubgraph({
        match["ar"],
    }, replacement);
    graph_.RemoveEdge(match["output"], replacement);
  });

  Graph<TargetMatchNode> dynamic_subgraph;
  dynamic_subgraph.AddEdges({
      {"input", {{TraceNode::Type::GeneratedFile}}, {}, false, false},
      {"ld", {{TraceNode::Type::Process}}, {"ld"}, false, true},
      {"output", {{TraceNode::Type::GeneratedFile}}, {}, true, false},
  });
  graph_.FindAndReplaceSubgraph(dynamic_subgraph, [this](
      const QMap<typename Graph<TargetMatchNode>::IDType, TraceNode>& match) {
    // Usually dynamic links are gcc -> collect2 -> ld.  Try to find the gcc
    // parent.
    int process_id = match["ld"].process_id_;
    while (true) {
      int parent_id = process(process_id).parent_id();
      const QString program = Filename(process(parent_id).filename());
      if (program == "gcc" || program == "g++" || program == "collect2") {
        process_id = parent_id;
        continue;
      }
      break;
    }

    graph_.ReplaceSubgraph({
        match["ld"],
    }, TraceNode::DynamicLinkStep(this, process_id));
  });
}

QString Make::NewTargetName(const QString& filename) {
  static QRegularExpression sInvalidCharsRe("[^a-zA-Z0-9_/]");

  pb::Reference ref;
  CreateReference(filename, &ref);
  if (ref.type() != pb::Reference_Type_RELATIVE_TO_BUILD_DIR &&
      ref.type() != pb::Reference_Type_RELATIVE_TO_PROJECT_ROOT) {
    LOG(FATAL) << "Expected " << filename << " to be relative to the project "
               << "root, got: " << ref.ShortDebugString();
  }

  QString ret = PathWithoutExtension(ref.name());
  ret.replace(sInvalidCharsRe, "_");

  const int last_slash = ret.lastIndexOf('/');
  if (last_slash != -1) {
    ret.replace(last_slash, 1, ':');
  } else {
    ret.prepend(":");
  }

  ret.prepend("//");

  const QString base = ret;
  int suffix = 1;
  while (targets_by_name_.contains(ret)) {
    ret = base + "_" + QString::number(suffix);
    suffix ++;
  }

  LOG(INFO) << "Using name " << ret << " for " << filename;
  return ret;
}

void Make::GenerateBuildTargets() {
  build_targets_.clear();
  targets_by_name_.clear();
  targets_by_node_id_.clear();

  vector<std::unique_ptr<BuildTargetGen>> generators;
  generators.emplace_back(new GccBuildTargetGen);
  generators.emplace_back(new StaticLinkBuildTargetGen);

  for (const auto& gen : generators) {
    gen->Init(this);
  }

  for (const TraceNode& node : graph_.AllNodes()) {
    for (const auto& gen : generators) {
      pb::BuildTarget target;
      if (gen->Gen(node, &target)) {
        CHECK(target.has_qualified_name());

        // Does this build target install a file?
        for (const pb::Reference& output : target.outputs()) {
          pb::InstalledFile installed;
          if (installed_files_.Find(
                output.name(),
                {pb::InstalledFile_Type_BINARY,
                 pb::InstalledFile_Type_LIBRARY},
                &installed)) {
            target.set_install(true);
            break;
          }
        }

        build_targets_.push_back(target);
        const int index = build_targets_.size() - 1;

        targets_by_name_[target.qualified_name()] = index;
        targets_by_node_id_[node.ID()] = index;

        break;
      }
    }
  }
}

bool Make::RemoveDuplicates() {
  bool replaced_any = false;

  QMap<string, QList<TraceNode>> nodes_by_canonical_target;
  for (const TraceNode& node : graph_.AllNodes()) {
    if (node.type_ != TraceNode::Type::CompileStep &&
        node.type_ != TraceNode::Type::DynamicLinkStep &&
        node.type_ != TraceNode::Type::StaticLinkStep) {
        continue;
    }

    const pb::BuildTarget& orig = TargetForNode(node.ID());

    pb::BuildTarget canon;
    canon.mutable_srcs()->CopyFrom(orig.srcs());
    if (orig.has_c_compile()) {
      canon.mutable_c_compile()->set_flag(orig.c_compile().flag());
      canon.mutable_c_compile()->mutable_headers()->CopyFrom(
          orig.c_compile().headers());
      for (const pb::Definition& def : orig.c_compile().definition()) {
        if (def.name().contains("PIC") ||
            def.name().contains("SHARED") ||
            def.name().contains("STATIC")) {
          continue;
        }
        canon.mutable_c_compile()->add_definition()->MergeFrom(def);
      }
    }
    if (orig.has_c_link()) {
      canon.mutable_c_link()->set_flag(orig.c_link().flag());
      canon.mutable_c_link()->mutable_library_search_path()->CopyFrom(
          orig.c_link().library_search_path());
      canon.mutable_c_link()->set_is_library(orig.c_link().is_library());
    }

    nodes_by_canonical_target[canon.SerializeAsString()].append(node);
  }

  for (const QList<TraceNode>& nodes : nodes_by_canonical_target.values()) {
    if (nodes.count() <= 1) {
      continue;
    }
    replaced_any = true;

    // We'll pick one process node to keep and remove the others.  Keep the
    // one whose output has the shortest name (eg: libfoo.a rather than
    // .deps/libfoo.so).
    TraceNode replacement;
    QList<TraceNode> replacement_inputs;
    TraceNode replacement_output;

    QList<TraceNode> all_outputs;
    QStringList output_connections;
    for (const TraceNode& node : nodes) {
      const QList<TraceNode> outputs = graph_.Outgoing(node);
      if (outputs.length() > 1) {
        LOG(FATAL) << "Duplicate process node " << node.ID()
                   << " had more than one output";
      }

      all_outputs.append(outputs[0]);

      for (const TraceNode& connection : graph_.Outgoing(outputs[0])) {
        output_connections.append(connection.ID());
      }

      if (replacement.type_ == TraceNode::Type::Unknown ||
          outputs[0].Filename().length() <
          replacement_output.Filename().length()) {
        replacement = node;
        replacement_inputs = graph_.Incoming(node);
        replacement_output = outputs[0];
      }
    }

    QStringList inputs_list;
    for (const TraceNode& node : replacement_inputs) {
      inputs_list.append(node.Filename());
    }
    LOG(INFO) << "Inputs " << inputs_list << "are processed by "
              << nodes.count() << " targets - replacing with the target that "
              << "outputs " << replacement_output.Filename();

    // Remove all the process nodes and outputs.
    for (const TraceNode& node : nodes) { graph_.RemoveNode(node); }
    for (const TraceNode& node : all_outputs) { graph_.RemoveNode(node); }

    // Re-add the chosen process and its outputs.
    graph_.AddEdges({replacement, replacement_output});
    for (const TraceNode& input : replacement_inputs) {
      graph_.AddEdge(input, replacement);
    }
    for (const QString& connection : output_connections) {
      graph_.AddEdgeByID(replacement_output.ID(), connection);
    }
  }

  return replaced_any;
}

void Make::ReplaceDependencyTargetNames() {
  // Replace generated src file names with the names of their build targets.
  QMap<pb::Reference, int> targets_by_output;
  for (int target_i = 0; target_i < build_targets_.size(); ++target_i) {
    const pb::BuildTarget& target = build_targets_[target_i];
    for (const pb::Reference& ref : target.outputs()) {
      if (targets_by_output.contains(ref)) {
        LOG(WARNING) << "Output " << ref.ShortDebugString()
                     << " was generated by multiple targets: "
                     << target.qualified_name() << " and "
                     << build_targets_[
                            targets_by_output[ref]].qualified_name();
        continue;
      }
      targets_by_output[ref] = target_i;
    }
  }

  for (int target_i = 0; target_i < build_targets_.size(); ++target_i) {
    pb::BuildTarget* target = &build_targets_[target_i];
    for (int src_i = 0; src_i < target->srcs_size(); ++src_i) {
      const pb::Reference& ref = target->srcs(src_i);
      if ((ref.type() != pb::Reference_Type_RELATIVE_TO_BUILD_DIR &&
           ref.type() != pb::Reference_Type_RELATIVE_TO_PROJECT_ROOT) ||
          !targets_by_output.contains(ref)) {
        continue;
      }

      pb::BuildTarget* dependency = &build_targets_[targets_by_output[ref]];

      if (dependency == target) {
        LOG(WARNING) << "Target " << target->qualified_name() << " generates "
                        "its own source file " << ref.ShortDebugString();
        continue;
      }

      CreateReference(*dependency, target->mutable_srcs(src_i));
    }
  }
}

bool Make::Run(const Options& opts) {
  Make make(opts);

  if (!make.ReadInputs()) {
    return false;
  }

  make.BuildGraph();
  Graph<TraceNode> intermediate_graph = make.graph();
  if (!opts.intermediate_graph_output_filename.isEmpty()) {
    intermediate_graph.WriteDotToFile(opts.intermediate_graph_output_filename);
  }

  make.FindCompileTargets();
  make.FindLinkTargets();

  forever {
    make.GenerateBuildTargets();
    if (!make.RemoveDuplicates()) {
      break;
    }
  }

  make.ReplaceDependencyTargetNames();

  if (!make.WriteOutput()) {
    return false;
  }

  if (!opts.graph_output_filename.isEmpty()) {
    make.graph().WriteDotToFile(opts.graph_output_filename);
  }
  return true;
}

bool Make::ReadInputs() {
  // Read the trace.
  auto trace = make_unique<utils::RecordFile<pb::Record>>(opts_.trace_filename);
  if (!trace->Open(QIODevice::ReadOnly)) {
    LOG(ERROR) << "Failed to open " << opts_.trace_filename << " for reading";
    return false;
  }

  // Read the installed files.
  auto installed_files =
      make_unique<utils::RecordFile<pb::Record>>(opts_.install_filename);
  if (!installed_files->Open(QIODevice::ReadOnly)) {
    LOG(ERROR) << "Failed to open " << opts_.install_filename << " for reading";
    return false;
  }

  trace_.Read(std::move(trace));
  installed_files_.Read(std::move(installed_files));
  return true;
}

bool Make::WriteOutput() {
  utils::RecordFile<pb::Record> output(opts_.output_filename);
  if (!output.Open(QIODevice::WriteOnly)) {
    LOG(ERROR) << "Failed to open " << opts_.output_filename << " for writing";
    return false;
  }

  pb::Record metadata_record;
  metadata_record.mutable_metadata()->CopyFrom(metadata());
  output.WriteRecord(metadata_record);

  for (const pb::BuildTarget& target : build_targets()) {
    pb::Record record;
    record.mutable_build_target()->CopyFrom(target);
    output.WriteRecord(record);
  }

  LOG(INFO) << "Written " << build_targets().count() << " targets to "
            << opts_.output_filename;
  return true;
}

void Make::CreateReference(const QString& name, pb::Reference* ref) const {
  ::CreateReference(trace_.metadata(), name, ref);
}

void Make::CreateReference(const pb::BuildTarget& target,
                                pb::Reference* ref) const {
  ref->set_type(pb::Reference_Type_BUILD_TARGET);
  ref->set_name(target.qualified_name());
}

}  // namespace analysis
