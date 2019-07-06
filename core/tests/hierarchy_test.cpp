//  This file is distributed under the BSD 3-Clause License. See LICENSE for details.

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <vector>
#include <string>

#include "rng.hpp"
#include "tree.hpp"

#include "lgraph.hpp"

using testing::HasSubstr;

class Setup_hierarchy : public ::testing::Test {
protected:
  struct Node_data {
    int create_pos;
    int fwd_pos;
    int bwd_pos;
    bool leaf;
    std::string name;
  };

  Tree<Node_data> tree;

  void populate_tree(int rseed, const int max_depth, const int size, const double leaf_ratio_goal) {

    tree.clear();

    Node_data root_data;
    root_data.create_pos = 0;

    tree.set_root(root_data);

    I(max_depth>1);

    Rng rint(rseed);
    RandomBool rbool;

    int max_level=1;
    int n_leafs = 0;
    for(int i=0;i<size;++i) {
      int level = 0;
      level = rint.uniform<int>(max_level);
      I(level<max_depth);

      Tree_index index(level, rint.uniform<int>(tree.get_tree_width(level)));

      Node_data data;
      data.create_pos = i+1;
      data.fwd_pos = -1;
      data.bwd_pos = -1;

      double leaf_ratio = (double)n_leafs/(1.0 + i);

      //fmt::print("leaf_ratio:{} {} {}\n", leaf_ratio,n_leafs, i);

      if (leaf_ratio < leaf_ratio_goal && index.level) { // Not to root
        tree.add_younger_sibling(index, data);
        n_leafs++;
      }else{
        //index.pos = tree.get_tree_width(index.level)-1; // Add child at the end
        if (!tree.is_leaf(index))
          n_leafs++;

        tree.add_child(index, data);
        if ((index.level+1) == max_level && max_level<max_depth)
          max_level++;
        I(max_level<=max_depth);
      }
    }

    int pos = 0;
    n_leafs = 0;
    for (auto index : tree.depth_preorder()) {
      auto &data = tree.get_data(index);
      data.fwd_pos = pos;
      data.bwd_pos = size-pos;
      data.leaf    = tree.is_leaf(index);
      if (data.leaf) {
        std::string name("leaf_l" + std::to_string(index.level) + "p" + std::to_string(index.pos));
        data.name = name;
        n_leafs++;
      } else {
        std::string name("node_l" + std::to_string(index.level) + "p" + std::to_string(index.pos));
        data.name = name;
      }
      ++pos;
    }

    fmt::print("Tree with {} nodes {} leafs and {} depth\n", size, n_leafs, max_level);

    EXPECT_TRUE(pos == (size+1)); // Missing nodes??? (tree.hpp bug)
  }

  void SetUp() override {
    int rseed=123;
    populate_tree(rseed, 10, 100, 0.5);
  }

  void TearDown() override {
    //Graph_library::sync_all();
  }
};

TEST_F(Setup_hierarchy, setup_tree) {

  std::vector<Tree_index> index_order;
  for (const auto &index : tree.depth_preorder()) {
#ifndef NDEBUG
    const auto &data       = tree.get_data(index);
    fmt::print(" level:{} pos:{} create_pos:{} fwd:{} bwd:{} leaf:{}\n", index.level, index.pos, data.create_pos, data.fwd_pos, data.bwd_pos, data.leaf);
#endif

    if (index.level || index.pos)
      index_order.emplace_back(index);
  }

  LGraph *lg_root = LGraph::create("lgdb_hierarchy_test", "node_l0p0", "hierarchy_test");
  lg_root->add_graph_output("o0",0,17);
  lg_root->add_graph_input("i0",1,31);

  std::vector<Node> node_order;

  int rseed = 123;
  Rng rint(rseed);
  RandomBool rbool;

  for(const auto &index:index_order) {
    const auto &data       = tree.get_data(index);

    I(index.level || index.pos); // skip root

    auto parent_index = tree.get_parent(index);
    const auto &parent_data = tree.get_data(parent_index);

    LGraph *parent_lg = LGraph::open("lgdb_hierarchy_test", parent_data.name);
    I(parent_lg);
    Node node;
    if (data.leaf) {
      node = parent_lg->create_node(Sum_Op,10);
    } else {
      node = parent_lg->create_node_sub(data.name);
      LGraph *sub_lg = LGraph::create("lgdb_hierarchy_test", data.name, "hierarchy_test");
      I(sub_lg);
      I(node.get_class_lgraph() == parent_lg);
      I(node.get_type_sub() == sub_lg->get_lgid());

      int n_inputs  = rint.uniform<int>(1)+1; // At least one
      int n_outputs = rint.uniform<int>(1)+1; // At least one
      int max_pos = 0;
      for(int i=0;i<n_inputs;++i) {
        std::string name = std::string("i") + std::to_string(i);
        int pos = max_pos+rint.uniform<int>(4)+1;
        max_pos = pos;
        sub_lg->add_graph_input(name, pos, rint.uniform<int>(60));
      }
      for(int i=0;i<n_outputs;++i) {
        std::string name = std::string("o") + std::to_string(i);
        int pos = max_pos+rint.uniform<int>(4)+1;
        max_pos = pos;
        sub_lg->add_graph_output(name, pos, rint.uniform<int>(60));
      }
    }
    //fmt::print("create {} class {}\n", node.debug_name(), node.get_class_lgraph()->get_name());
    if (rbool(rint))
      node.set_name(std::to_string(data.create_pos));
    node_order.emplace_back(node);
  }

  for (size_t i=1;i<node_order.size();++i) {
    const auto &curr_index = index_order[i];
    const auto &prev_index = index_order[i-1];
    const auto &curr_data = tree.get_data(curr_index);
    const auto &prev_data = tree.get_data(prev_index);

    //const auto &parent_index = tree.get_parent(curr_index);
    //const auto &parent_data = tree.get_data(parent_index);
    //LGraph *parent_lg = LGraph::open("lgdb_hierarchy_test", parent_data.name);

    auto &curr_node = node_order[i];
    auto &prev_node = node_order[i-1];

    fmt::print("prev   {} class {}\n", curr_node.debug_name(), curr_node.get_class_lgraph()->get_name());
    fmt::print("curr   {} class {}\n", prev_node.debug_name(), prev_node.get_class_lgraph()->get_name());

    Node_pin dpin;
    if (prev_data.leaf) {
      dpin = prev_node.setup_driver_pin(0);
      I(prev_node.get_type().op == Sum_Op);
    }else{
      LGraph *prev_lg = LGraph::open("lgdb_hierarchy_test", prev_data.name);
      I(prev_node.get_class_lgraph() != prev_lg);
      auto d_pid = prev_node.get_class_lgraph()->get_self_sub_node().get_instance_pid("o0");
      dpin = prev_node.setup_driver_pin(d_pid);
      I(prev_node.get_type().op == SubGraph_Op);
    }

    Node_pin spin;
    if (curr_data.leaf) {
      spin = curr_node.setup_sink_pin(0);
      I(curr_node.get_type().op == Sum_Op);
    }else{
      LGraph *curr_lg = LGraph::open("lgdb_hierarchy_test", curr_data.name);
      I(curr_node.get_class_lgraph() != curr_lg);
      auto s_pid = curr_node.get_class_lgraph()->get_self_sub_node().get_instance_pid("i0");
      spin = curr_node.setup_sink_pin(s_pid);
      I(curr_node.get_type().op == SubGraph_Op);
    }

    if (prev_node.get_class_lgraph() == curr_node.get_class_lgraph()) {
      dpin.connect_sink(spin);
    }else{
      // Connect local to input
      auto local_dpin = curr_node.get_class_lgraph()->get_graph_input("i0");
      local_dpin.connect_sink(spin);

      // Connect last to output
      auto prev_spin = prev_node.get_class_lgraph()->get_graph_output("o0");
      dpin.connect_sink(prev_spin);
    }

#if 0
    if (rbool(rint) && curr_data.leaf) { // Add edge randomly to parent (should not change order)
      auto dpin = curr_node.get_class_lgraph()->get_graph_input("i0");
      curr_node.get_sink_pin(s_pid).connect_driver(dpin);

      auto spin = curr_node.get_class_lgraph()->get_graph_output("o0");
      curr_node.get_driver_pin(d_pid).connect_sink(spin);
    }
#endif
  }

  EXPECT_TRUE(true);
}

