#ifndef __LINKED_LIST_H__
#define __LINKED_LIST_H__

#include "defines.h"

/**
 * In this list implementation, any data that is inserted into the list will either 
 * stay in the list throughout its lifetime until the list is destroyed, or be 
 * destroyed by a list_pop_xyz method. This means you should NOT copy data from nodes 
 * directly, save for loop iterations
*/

typedef struct node node;
struct node {
    void* data;
    node* prev;
    node* next;
};

typedef struct list {
    node* head;
    node* tail;
    bool should_free_data;
} list;

/**
 * @brief 
 * Creates a node that contains a pointer to data. This node should be destroyed by 
 * node_destroy(node** ppNode) when its finished being used.
 * @param pData pointer to desired data.
 * @return 
 * Heap allocated node pointer
*/
node* node_create(void* pData);

/**
 * @brief
 * Destroys node and frees it from the heap. Also sets the value of the pointer 
 * stored by ppNode to NULL. Note that any heap allocated data at node->data must be 
 * freed before destroying the node.
 * @param ppNode address of node pointer
*/
void node_destroy(node** ppNode);

/**
 * @brief 
 * Creates a list. 
 * @param should_free_data 
 * Tell the function if the data inside the list should be freed upon the 
 * destruction of the list and the popping of nodes. 
 * @return 
 * Heap allocated list pointer
*/
list* list_create(bool should_free_data);

/**
 * @brief 
 * Destroys list and frees it from the heap. Also sets the value of the pointer
 * stored by ppList to NULL. Automatically frees data inside nodes if this was
 * specified during list creation.
 * @param ppList address of list pointer
*/
void list_destroy(list** ppList);

/**
 * @brief
 * Inserts data at the tail of the given list
 * @param pList pointer to list
 * @param pData pointer to data
*/
void list_insert_tail(list* pList, void* pData);

/**
 * @brief
 * Inserts data at the head of the given list
 * @param pList pointer to list
 * @param pData pointer to data
*/
void list_insert_head(list* pList, void* pData);

/**
 * @brief
 * Inserts data into list based on a comparison function that follows 
 * the same conventions as GNU cmp functions
 * @param pList pointer to list
 * @param pData pointer to data to be inserted
 * @param cmp pointer to comparison function
*/
void list_insert_sorted(list* pList, void* pData, int32_t (*cmp)(void*, void*));

/**
 * @brief
 * Pops node at head of list. This function will also free data inside 
 * node if this was specified in the list's creation
 * @param pList pointer to list
*/
void list_pop_head(list* pList);

/**
 * @brief
 * Pops node at tail of list. This function will also free data inside
 * node if this was specified in the list's creation
 * @param pList pointer to list
*/
void list_pop_tail(list* pList);

/**
 * @brief
 * Pops a node from the given list. pNode MUST come from the same list
 * as pList. This function will also free data inside node if this 
 * was specified in the list's creation
 * @param pList pointer to list
 * @param pNode pointer to node that will be popped
 * @return
 * Pointer to the next node of pNode
*/
node* list_pop_node(list* pList, node* pNode);

#endif