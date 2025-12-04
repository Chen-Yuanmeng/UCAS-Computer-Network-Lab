#include "tree.h"
#include <stdio.h>
#include <stdlib.h>

#define BIT_LOCATE(x, n) ((x >> (31 - n)) & 0b1)
#define BIT_LOCATE_2(x, n) ((x >> (30 - n)) & 0b11)
#define BIT_LOCATE_4(x, n) ((x >> (28 - n)) & 0b1111)

#define NOT_A_PORT 0xffffffff  // uint32_t max

node_t *root = NULL;
node_advance_t *root_advance = NULL;

// return an array of ip represented by an unsigned integer, the length of array is TEST_SIZE
uint32_t* read_test_data(const char* lookup_file)
{
    // fprintf(stderr,"TODO:%s",__func__);

    uint32_t *arr = (uint32_t *)malloc(TEST_SIZE * sizeof(uint32_t));
    if (arr == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    FILE *fp = fopen(lookup_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", lookup_file);
        free(arr);
        return NULL;
    }

    uint8_t a, b, c, d;

    for (int i = 0; i < TEST_SIZE; i++) {
        if (fscanf(fp, "%hhu.%hhu.%hhu.%hhu\n", &a, &b, &c, &d) != 4) {
            fprintf(stderr, "Invalid IP format in file at line %d\n", i + 1);
            free(arr);
            fclose(fp);
            return NULL;
        }
        arr[i] = (a << 24) | (b << 16) | (c << 8) | d;
    }
    fclose(fp);

    return arr;
}

static node_t *create_new_child(uint32_t port, bool type, node_t *parent, int direction) {
    node_t *new_node = (node_t *)malloc(sizeof(node_t));
    if (new_node == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    if (parent != NULL) {
        if (direction == LEFT) {
            parent->lchild = new_node;
        } else {
            parent->rchild = new_node;
        }
    }
    new_node->port = port;
    new_node->type = type;
    new_node->lchild = NULL;
    new_node->rchild = NULL;
    return new_node;
}


static node_advance_t *create_new_child_advance(uint32_t port, bool type, node_advance_t *parent, int index) {
    node_advance_t *new_node = (node_advance_t *)malloc(sizeof(node_advance_t));
    if (new_node == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    if (parent != NULL) {
        parent->children[index] = new_node;
    }
    new_node->port = port;
    new_node->type = type;
    for (int i = 0; i < 16; i++) {
        new_node->children[i] = NULL;
    }
    return new_node;
}

// Constructing an basic trie-tree to lookup according to `forward_file`
void create_tree(const char* forward_file)
{
    // fprintf(stderr,"TODO:%s",__func__);

    // 1. Initialize empty tree
    root = create_new_child(NOT_A_PORT, I_NODE, NULL, -1);

    // 2. Open the forward_file for reading

    FILE *fp = fopen(forward_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", forward_file);
        return;
    }

    // 3. Read from file and insert into the tree
    for (int i = 0; i < TRAIN_SIZE; ++i) {
        uint8_t a, b, c, d, prefix_len;
        uint32_t port;
        if (fscanf(fp, "%hhu.%hhu.%hhu.%hhu %hhu %u\n", &a, &b, &c, &d, &prefix_len, &port) != 6) {
            fprintf(stderr, "Invalid format in forward file at line %d\n", i + 1);
            fclose(fp);
            return;
        }
        uint32_t ip = (a << 24) | (b << 16) | (c << 8) | d;

        // Insert into the tree
        node_t *current = root;
        for (int j = 0; j < prefix_len; ++j) {
            int bit = BIT_LOCATE(ip, j);
            if (bit == 0) {
                if (current->lchild == NULL) {
                    current->lchild = create_new_child(NOT_A_PORT, I_NODE, current, LEFT);
                }
                current = current->lchild;
            } else {
                if (current->rchild == NULL) {
                    current->rchild = create_new_child(NOT_A_PORT, I_NODE, current, RIGHT);
                }
                current = current->rchild;
            }
        }
        // Update the node to be a match node
        current->type = M_NODE;
        current->port = port;
    }

    // 4. Close the file
    fclose(fp);

    return;
}

// Look up the ports of ip in file `ip_to_lookup.txt` using the basic tree, input is read from `read_test_data` func
uint32_t *lookup_tree(uint32_t* ip_vec)
{
    // fprintf(stderr,"TODO:%s",__func__);
    uint32_t *basic_vec = (uint32_t *)malloc(TEST_SIZE * sizeof(uint32_t));

    if (basic_vec == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    for (int i = 0; i < TEST_SIZE; ++i) {
        uint32_t ip = ip_vec[i];
        node_t *current = root;
        node_t *last_match = NULL;

        for (int j = 0; j < 32; ++j) {
            if (current == NULL) {
                break;  // No further nodes to traverse
            }
            if (current->type == M_NODE) {
                last_match = current;
            }
            int bit = BIT_LOCATE(ip, j);
            if (bit == 0) {
                current = current->lchild;
            } else {
                current = current->rchild;
            }
        }

        if (current) {
            if (current->type == M_NODE) {
                last_match = current;
            }
        }
        basic_vec[i] = (last_match != NULL) ? last_match->port : NOT_A_PORT;
    }

    return basic_vec;
}

// Constructing an advanced trie-tree to lookup according to `forward_file`
void create_tree_advance(const char* forward_file)
{
    // fprintf(stderr,"TODO:%s",__func__);

    // 1. Initialize empty tree
    root_advance = create_new_child_advance(NOT_A_PORT, I_NODE, NULL, -1);

    // 2. Open the forward_file for reading
    FILE *fp = fopen(forward_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", forward_file);
        return;
    }

    fprintf(stdout, "Opened forward file: %s\n", forward_file);

    // 3. Read from file and insert into the tree
    for (int i = 0; i < TRAIN_SIZE; ++i) {
        uint8_t a, b, c, d, prefix_len;
        uint32_t port;
        if (fscanf(fp, "%hhu.%hhu.%hhu.%hhu %hhu %u\n", &a, &b, &c, &d, &prefix_len, &port) != 6) {
            fprintf(stderr, "Invalid format in forward file at line %d\n", i + 1);
            fclose(fp);
            return;
        }
        uint32_t ip = (a << 24) | (b << 16) | (c << 8) | d;

        // Insert into the advanced tree
        node_advance_t *current = root_advance;
        int j = 0;
        for (; j + 4 <= prefix_len; j += 4) {
            int index = BIT_LOCATE_4(ip, j);
            if (current->children[index] == NULL) {
                current->children[index] = create_new_child_advance(NOT_A_PORT, I_NODE, current, index);
            }
            current = current->children[index];
        }

        int remaining_bits = prefix_len - j;
        if (remaining_bits > 0) {
            int nibble = BIT_LOCATE_4(ip, j);
            int fixed_prefix = nibble >> (4 - remaining_bits);
            int base_index = fixed_prefix << (4 - remaining_bits);
            int combinations = 1 << (4 - remaining_bits);
            for (int offset = 0; offset < combinations; ++offset) {
                int child_index = base_index + offset;
                node_advance_t *child = current->children[child_index];
                if (child == NULL) {
                    child = create_new_child_advance(NOT_A_PORT, I_NODE, current, child_index);
                }
                child->type = M_NODE;
                child->port = port;
            }
        } else {
            // Update the node to be a match node
            current->type = M_NODE;
            current->port = port;
        }
    }

    // 4. Close the file
    fclose(fp);

    return;
}

// Look up the ports of ip in file `ip_to_lookup.txt` using the advanced tree input is read from `read_test_data` func
uint32_t *lookup_tree_advance(uint32_t* ip_vec)
{
    // fprintf(stderr,"TODO:%s",__func__);
    uint32_t *advance_vec = (uint32_t *)malloc(TEST_SIZE * sizeof(uint32_t));

    if (advance_vec == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    for (int i = 0; i < TEST_SIZE; ++i) {
        uint32_t ip = ip_vec[i];
        node_advance_t *current = root_advance;
        node_advance_t *last_match = NULL;

        for (int j = 0; j < 32; j += 4) {
            if (current == NULL) {
                break;  // No further nodes to traverse
            }
            if (current->type == M_NODE) {
                last_match = current;
            }
            int index = BIT_LOCATE_4(ip, j);
            current = current->children[index];
        }

        if (current) {
            if (current->type == M_NODE) {
                last_match = current;
            }
        }
        advance_vec[i] = (last_match != NULL) ? last_match->port : NOT_A_PORT;
    }
    return advance_vec;
}
