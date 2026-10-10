#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rbtree_tree rbtree_tree;

typedef int (*tree_fcompare)(void *key1, void *key2, void *opt);
typedef void (*tree_fforeach)(void *key, void *value, void *opt);

rbtree_tree *rbtree_create(tree_fcompare key_cmp, void *opt_data);

int rbtree_insert(rbtree_tree *tree, void *key, void *value);
void *rbtree_lookup(rbtree_tree *tree, void *key);
void *rbtree_remove(rbtree_tree *tree, void *key);

/* Callbacks may free key/value data, but must not modify the tree. */
int rbtree_foreach(rbtree_tree *tree, tree_fforeach user_fn, void *opt);

int rbtree_node_count(rbtree_tree *tree);

/* Keys and values remain caller-owned. */
void rbtree_delete(rbtree_tree *tree);
void rbtree_purge(rbtree_tree *tree);

#ifdef __cplusplus
}
#endif
