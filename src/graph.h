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

#ifndef GRAPH_H
#define GRAPH_H

#include <utility>

#include <glog/logging.h>

#include <QFile>
#include <QTextStream>

#include "common.h"
#include "utils/logging.h"

template <typename N>
class Graph {
 public:
  Graph() {}

  using IDType = QString;
  using EdgeType = QPair<IDType, IDType>;
  using NodeType = N;

  void AddNode(const NodeType& node);
  void AddEdge(const NodeType& from, const NodeType& to);
  void AddEdgeByID(const IDType& from, const IDType& to);
  void RemoveEdge(const NodeType& from, const NodeType& to);
  void RemoveEdgeByID(const IDType& from, const IDType& to);
  void RemoveNode(const NodeType& node);

  // Adds all the nodes, and adds edges from one node to the next.
  void AddEdges(std::initializer_list<NodeType> nodes);

  bool HasNode(const NodeType& node) const;

  QList<NodeType> AllNodes() const;
  QList<EdgeType> AllEdges() const;
  QList<NodeType> Incoming(const NodeType& node) const;
  QList<NodeType> Outgoing(const NodeType& node) const;

  bool empty() const { return nodes_.empty(); }
  int count() const { return nodes_.count(); }

  // Subgraph must be a connected graph, and Subgraph::NodeType must have:
  //   bool Match(NodeType)  - where NodeType is from *this* graph.
  //   bool ExactIncomingNeighbourCount()
  //   bool ExactOutgoingNeighbourCount()
  // Tries to place the subgraph over every position in this graph and returns
  // any configurations where Match returns true for every node.
  // The return type is a QList of matches.  Each match maps the ID of the
  // subgraph node to the corresponding node in this graph.
  template <typename Subgraph>
  QList<QMap<typename Subgraph::IDType, NodeType>> FindSubgraphMatches(
      const Subgraph& subgraph) const;

  // Removes all the given nodes from this graph, and replaces them with one
  // replacement node.  All edges to/from removed nodes are connected to the
  // replacement node instead.
  void ReplaceSubgraph(std::initializer_list<NodeType> nodes,
                       const NodeType& replacement);

  template <typename Iterator>
  void ReplaceSubgraph(Iterator begin, Iterator end,
                       const NodeType& replacement);

  // Repeatedly calls FindSubgraphMatches and uses replace_fn to remove them.
  // Continues until there are no more matches.  ReplaceFn should be a function
  // (or lambda) that takes a map<typename Subgraph::IDType, NodeType>.  It must
  // remove or replace all the matches from the graph.
  template <typename Subgraph, typename ReplaceFn>
  void FindAndReplaceSubgraph(const Subgraph& subgraph, ReplaceFn replace_fn);

  // Writes a DOT representation of this graph to the stream.  Nodes must have
  // a WriteDot function to write their own state.
  void WriteDot(QTextStream& os) const;
  void WriteDotToFile(const QString& filename) const;

 private:
  bool HasNodeByID(const IDType& id) const;

  QList<NodeType> Neighbours(const NodeType& node,
                             const QMap<IDType, QSet<IDType>>& direction) const;

  template <typename Subgraph>
  bool MatchRecursive(
      const NodeType& node,
      const Subgraph& subgraph,
      const typename Subgraph::NodeType& subgraph_node,
      QMap<typename Subgraph::IDType, NodeType>* match) const;

  QMap<IDType, N> nodes_;
  QSet<EdgeType> edges_;
  QMap<IDType, QSet<IDType>> incoming_edges_;
  QMap<IDType, QSet<IDType>> outgoing_edges_;
};


template <typename NodeType>
void Graph<NodeType>::AddNode(const NodeType& node) {
  nodes_[node.ID()] = node;
}

template <typename NodeType>
void Graph<NodeType>::AddEdge(const NodeType& from, const NodeType& to) {
  if (!HasNode(from)) {
    AddNode(from);
  }
  if (!HasNode(to)) {
    AddNode(to);
  }
  AddEdgeByID(from.ID(), to.ID());
}

template <typename NodeType>
void Graph<NodeType>::AddEdgeByID(const IDType& from, const IDType& to) {
  CHECK(HasNodeByID(from));
  CHECK(HasNodeByID(to));

  edges_.insert(qMakePair(from, to));
  outgoing_edges_[from].insert(to);
  incoming_edges_[to].insert(from);
}

template <typename NodeType>
void Graph<NodeType>::RemoveEdge(const NodeType& from, const NodeType& to) {
  RemoveEdgeByID(from.ID(), to.ID());
}

template <typename NodeType>
void Graph<NodeType>::RemoveEdgeByID(const IDType& from, const IDType& to) {
  CHECK(HasNodeByID(from));
  CHECK(HasNodeByID(to));

  edges_.remove(qMakePair(from, to));
  outgoing_edges_[from].remove(to);
  incoming_edges_[to].remove(from);
}

template <typename NodeType>
void Graph<NodeType>::RemoveNode(const NodeType& node) {
  const IDType id = node.ID();
  nodes_.remove(id);
  for (const IDType& neighbour_id : incoming_edges_[id]) {
    edges_.remove(EdgeType(neighbour_id, id));
    outgoing_edges_[neighbour_id].remove(id);
  }
  for (const IDType& neighbour_id : outgoing_edges_[id]) {
    edges_.remove(EdgeType(id, neighbour_id));
    incoming_edges_[neighbour_id].remove(id);
  }
  incoming_edges_.remove(id);
  outgoing_edges_.remove(id);
}

template <typename NodeType>
void Graph<NodeType>::AddEdges(std::initializer_list<NodeType> nodes) {
  int i = 0;
  IDType last_id;
  for (auto it = nodes.begin(); it != nodes.end(); ++it, ++i) {
    if (!HasNode(*it)) {
      AddNode(*it);
    }
    if (i != 0) {
      AddEdgeByID(last_id, it->ID());
    }
    last_id = it->ID();
  }
}

template <typename NodeType>
bool Graph<NodeType>::HasNode(const NodeType& node) const {
  return HasNodeByID(node.ID());
}

template <typename NodeType>
bool Graph<NodeType>::HasNodeByID(const IDType& id) const {
  return nodes_.find(id) != nodes_.end();
}

template <typename NodeType>
QList<NodeType> Graph<NodeType>::AllNodes() const {
  return nodes_.values();
}

template <typename NodeType>
QList<typename Graph<NodeType>::EdgeType> Graph<NodeType>::AllEdges() const {
  return edges_.values();
}

template <typename NodeType>
void Graph<NodeType>::ReplaceSubgraph(std::initializer_list<NodeType> nodes,
                                      const NodeType& replacement) {
  ReplaceSubgraph(nodes.begin(), nodes.end(), replacement);
}

template <typename NodeType>
template <typename Iterator>
void Graph<NodeType>::ReplaceSubgraph(Iterator begin, Iterator end,
                                      const NodeType& replacement) {
  QSet<IDType> removing;
  QSet<IDType> incoming;
  QSet<IDType> outgoing;
  for (Iterator it = begin; it != end; ++it) {
    const NodeType& node = *it;

    // Remove the node.
    removing.insert(node.ID());
    nodes_.remove(node.ID());

    // Record any incoming edges.
    for (const IDType& neighbour_id : incoming_edges_[node.ID()]) {
      incoming.insert(neighbour_id);
      edges_.remove(qMakePair(neighbour_id, node.ID()));
      outgoing_edges_[neighbour_id].remove(node.ID());
    }

    // Record any outgoing edges.
    for (const IDType& neighbour_id : outgoing_edges_[node.ID()]) {
      outgoing.insert(neighbour_id);
      edges_.remove(qMakePair(node.ID(), neighbour_id));
      incoming_edges_[neighbour_id].remove(node.ID());
    }

    incoming_edges_.remove(node.ID());
    outgoing_edges_.remove(node.ID());
  }

  // Add the replacement node.
  AddNode(replacement);

  // Re-add the edges to neighbouring nodes, excluding internal edges between
  // nodes that were removed.
  for (const IDType& neighbour_id : incoming) {
    if (!removing.contains(neighbour_id)) {
      AddEdgeByID(neighbour_id, replacement.ID());
    }
  }
  for (const IDType& neighbour_id : outgoing) {
    if (!removing.contains(neighbour_id)) {
      AddEdgeByID(replacement.ID(), neighbour_id);
    }
  }
}

template <typename NodeType>
QList<NodeType> Graph<NodeType>::Neighbours(
    const NodeType& node,
    const QMap<IDType, QSet<IDType>>& direction) const {
  QList<NodeType> ret;
  auto it = direction.find(node.ID());
  if (it == direction.end()) {
    return ret;
  }
  for (const IDType& id : it.value()) {
    ret.push_back(nodes_[id]);
  }
  return ret;
}

template <typename NodeType>
QList<NodeType> Graph<NodeType>::Incoming(const NodeType& node) const {
  return Neighbours(node, incoming_edges_);
}

template <typename NodeType>
QList<NodeType> Graph<NodeType>::Outgoing(const NodeType& node) const {
  return Neighbours(node, outgoing_edges_);
}

template <typename NodeType>
template <typename Subgraph>
bool Graph<NodeType>::MatchRecursive(
    const NodeType& node,
    const Subgraph& subgraph,
    const typename Subgraph::NodeType& subgraph_node,
    QMap<typename Subgraph::IDType, NodeType>* match) const {
  // Try to match this node.
  if (!subgraph_node.Match(node)) {
    return false;
  }

  QMap<typename Subgraph::IDType, NodeType> child_match = *match;
  child_match[subgraph_node.ID()] = node;

  auto match_neighbours_fn = [&child_match, &subgraph, &subgraph_node, this](
      const QList<NodeType>& neighbours,
      const QList<typename Subgraph::NodeType>& subgraph_neighbours,
      bool expects_exact_count) {
    if (expects_exact_count &&
        neighbours.size() != subgraph_neighbours.size()) {
      return false;
    }

    for (const auto& subgraph_neighbour : subgraph_neighbours) {
      if (child_match.find(subgraph_neighbour.ID()) != child_match.end()) {
        // We've already matched this node.
        continue;
      }

      bool found = false;
      for (const auto& neighbour : neighbours) {
        if (MatchRecursive(neighbour,
                           subgraph,
                           subgraph_neighbour,
                           &child_match)) {
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
    }
    return true;
  };

  // Match all incoming and outgoing nodes.
  if (!match_neighbours_fn(Incoming(node),
                           subgraph.Incoming(subgraph_node),
                           subgraph_node.ExactIncomingNeighbourCount())) {
    return false;
  }
  if (!match_neighbours_fn(Outgoing(node),
                           subgraph.Outgoing(subgraph_node),
                           subgraph_node.ExactOutgoingNeighbourCount())) {
    return false;
  }

  *match = child_match;
  return true;
}

template <typename NodeType>
template <typename Subgraph>
QList<QMap<typename Subgraph::IDType, NodeType>>
Graph<NodeType>::FindSubgraphMatches(const Subgraph& subgraph) const {
  // TODO: check whether the subgraph is connected.

  QList<QMap<typename Subgraph::IDType, NodeType>> ret;
  if (subgraph.empty()) {
    LOG(WARNING) << "Empty subgraph";
    return ret;
  }

  const typename Subgraph::NodeType start_node = *subgraph.AllNodes().begin();

  for (const NodeType& node : nodes_) {
    QMap<typename Subgraph::IDType, NodeType> match;
    if (MatchRecursive(node, subgraph, start_node, &match)) {
      ret.append(match);
    }
  }
  return ret;
}

template <typename NodeType>
void Graph<NodeType>::WriteDot(QTextStream& os) const {
  os << "digraph {\n";
  for (const NodeType& node : AllNodes()) {
    os << "  \"" << node.ID() << "\" [";
    node.WriteDot(os);
    os << "];\n";
  }
  for (const auto& edge : AllEdges()) {
    os << "  \"" << edge.first << "\" -> \"" << edge.second << "\";\n";
  }
  os << "}\n";
}

template <typename NodeType>
void Graph<NodeType>::WriteDotToFile(const QString& filename) const {
  QFile out(filename);
  if (!out.open(QIODevice::WriteOnly)) {
    LOG(ERROR) << "Failed to open " << filename << " for writing";
  } else {
    QTextStream os(&out);
    WriteDot(os);
    LOG(INFO) << "Written graph (" << count() << " nodes) to " << filename;
  }
}

template <typename NodeType>
template <typename Subgraph, typename ReplaceFn>
void Graph<NodeType>::FindAndReplaceSubgraph(
    const Subgraph& subgraph,
    ReplaceFn replace_fn) {
  for (int i = 0; i < 100; ++i) {
    const auto& matches = FindSubgraphMatches(subgraph);
    if (matches.empty()) {
      return;
    }

    for (const auto& match : matches) {
      replace_fn(match);
    }
  }
  LOG(FATAL) << "FindAndReplaceSubgraph matched too many times - maybe the "
                "replace_fn isn't removing the matches from the graph.";
}

#endif // GRAPH_H
