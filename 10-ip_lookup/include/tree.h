#ifndef __TREE_H__
#define __TREE_H__

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

// do not change it
#define TEST_SIZE 100000

#define TRAIN_SIZE 697882 
#define I_NODE 0 // internal node
#define M_NODE 1 // match node
#define LEFT 0
#define RIGHT 1

#define MASK(x,y) (((x) & 0x000000ff) << (y))
typedef struct node{
    bool type; //I_NODE or M_NODE
    uint32_t port;
    struct node* lchild;
    struct node* rchild;
} node_t;


typedef struct node_advance{
    bool type; //I_NODE or M_NODE
    uint32_t port;
    struct node_advance* children[16];
} node_advance_t;

void create_tree(const char*);
uint32_t *lookup_tree(uint32_t *);
void create_tree_advance(const char*);
uint32_t *lookup_tree_advance(uint32_t *);

uint32_t* read_test_data(const char* lookup_file);

#endif
