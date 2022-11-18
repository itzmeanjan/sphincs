#pragma once
#include "wots.hpp"
#include <cassert>
#include <stack>

// Fixed Input-Length XMSS, used in SPHINCS+
namespace sphincs_xmss {

// Internal/ Leaf Node of Main Merkle Tree, where each node is n -bytes wide
//
// Read section 4.1.3 of SPHINCS+ specification
// https://sphincs.org/data/sphincs+-r3.1-specification.pdf to understand why
// height of node needs to be kept track of.
template<const size_t n>
struct node_t
{
  uint8_t data[n]{};
  uint32_t height = 0u;
};

// Computes n -bytes root node of a subtree of height `n_height` with leftmost
// leaf node being WOTS+ compressed public key at index `s_idx`, using algorithm
// 7, described in section 4.1.3 of SPHINCS+ specification
// https://sphincs.org/data/sphincs+-r3.1-specification.pdf
template<const size_t n, const size_t w, const sphincs_hashing::variant v>
inline static void
treehash(
  const uint8_t* const __restrict sk_seed, // n -bytes secret key seed
  const uint32_t s_idx,                    // 4 -bytes start index
  const uint32_t n_height,                 // 4 -bytes target node height
  const uint8_t* const __restrict pk_seed, // n -bytes public key seed
  const sphincs_adrs::adrs_t adrs, // 32 -bytes address of containing tree
  uint8_t* const __restrict root   // n -bytes root of subtree of height
                                   // `n_height`
)
{
  // # -of leafs in the subtree
  const uint32_t leaf_cnt = 1u << n_height;
  assert((s_idx % leaf_cnt) == 0);

  // Stack which will hold at max `n_height` many intermediate nodes
  std::stack<node_t<n>> stack;

  for (uint32_t i = 0; i < leaf_cnt; i++) {
    sphincs_adrs::wots_hash_t hash_adrs{ adrs };

    hash_adrs.set_type(sphincs_adrs::type_t::WOTS_HASH);
    hash_adrs.set_keypair_address(s_idx + i);

    node_t<n> node{};
    sphincs_wots::pkgen<n, w, v>(sk_seed, pk_seed, hash_adrs, node.data);
    node.height = 1u;

    sphincs_adrs::tree_t tree_adrs{ adrs };

    tree_adrs.set_type(sphincs_adrs::type_t::TREE);
    tree_adrs.set_tree_height(1u);
    tree_adrs.set_tree_index(s_idx + i);

    // Two consecutive nodes, each of n -bytes width
    //
    // Used for computing parent node of binary Merkle Tree, from two children
    uint8_t c_nodes[n + n]{};

    while (!stack.empty()) {
      const auto top = stack.top();
      if (top.height != node.height) {
        break;
      }

      tree_adrs.set_tree_index((tree_adrs.get_tree_index() - 1u) >> 1);

      std::memcpy(c_nodes + 0, top.data, n);
      std::memcpy(c_nodes + n, node.data, n);

      sphincs_hashing::h<n, v>(pk_seed, tree_adrs.data, c_nodes, node.data);
      node.height = tree_adrs.get_tree_height() + 1u;

      tree_adrs.set_tree_height(tree_adrs.get_tree_height() + 1u);
      stack.pop();
    }

    stack.push(node);
  }

  const node_t<n> top = stack.top();
  std::memcpy(root, top.data, n);
  stack.pop(); // stack must be empty now !
}

}
