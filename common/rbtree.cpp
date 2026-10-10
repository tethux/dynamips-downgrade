#include "rbtree.h"

#include <limits>
#include <map>

struct rbtree_compare {
  tree_fcompare compare_fn;
  void *context;

  bool operator()(void *lhs, void *rhs) const {
    return compare_fn(lhs, rhs, context) < 0;
  }
};

struct rbtree_tree {
  std::map<void *, void *, rbtree_compare> entries;

  rbtree_tree(tree_fcompare cmp, void *context)
      : entries(rbtree_compare{cmp, context}) {}
};

extern "C" {

rbtree_tree *rbtree_create(tree_fcompare key_cmp, void *opt_data) {
  if (!key_cmp)
    return nullptr;

  try {
    return new rbtree_tree(key_cmp, opt_data);
  } catch (...) {
    return nullptr;
  }
}

int rbtree_insert(rbtree_tree *tree, void *key, void *value) {
  if (!tree)
    return -1;

  try {
    if (tree->entries.size() >=
            static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
        !tree->entries.contains(key))
      return -1;

    tree->entries.insert_or_assign(key, value);
    return 0;
  } catch (...) {
    return -1;
  }
}

void *rbtree_lookup(rbtree_tree *tree, void *key) {
  if (!tree)
    return nullptr;

  try {
    const auto it = tree->entries.find(key);
    return it == tree->entries.end() ? nullptr : it->second;
  } catch (...) {
    return nullptr;
  }
}

void *rbtree_remove(rbtree_tree *tree, void *key) {
  if (!tree)
    return nullptr;

  try {
    const auto it = tree->entries.find(key);
    if (it == tree->entries.end())
      return nullptr;

    void *value = it->second;
    tree->entries.erase(it);
    return value;
  } catch (...) {
    return nullptr;
  }
}

int rbtree_foreach(rbtree_tree *tree, tree_fforeach user_fn, void *opt) {
  if (!tree || !user_fn)
    return -1;

  try {
    for (const auto &[key, value] : tree->entries)
      user_fn(key, value, opt);

    return 0;
  } catch (...) {
    return -1;
  }
}

int rbtree_node_count(rbtree_tree *tree) {
  return tree ? static_cast<int>(tree->entries.size()) : 0;
}

void rbtree_purge(rbtree_tree *tree) {
  if (tree)
    tree->entries.clear();
}

void rbtree_delete(rbtree_tree *tree) { delete tree; }
}
