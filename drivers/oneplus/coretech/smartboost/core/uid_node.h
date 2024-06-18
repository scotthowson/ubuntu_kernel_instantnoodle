#ifndef UID_NODE_H
#define UID_NODE_H

#include <linux/list.h>  // For struct list_head

struct uid_node {
    uid_t uid;
    int hot_count;
    struct list_head page_cache_list;
    struct uid_node *next;
};

#endif // UID_NODE_H