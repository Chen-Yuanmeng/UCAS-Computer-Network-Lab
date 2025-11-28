#ifndef __MOSPF_DATABASE_H__
#define __MOSPF_DATABASE_H__

#include "base.h"
#include "list.h"

#include "mospf_proto.h"

extern struct list_head mospf_db;

typedef struct {
	struct list_head list;
	u32	rid;
	u16	seq;
	int nadv;
	int alive;
	struct mospf_lsa *array;
} mospf_db_entry_t;

extern int node_num;

void init_mospf_db();
void update_routing_table_from_database();
void check_mospf_db_timeout();
int update_mospf_db(char *lsu_msg);
void print_mospf_db();

#endif
