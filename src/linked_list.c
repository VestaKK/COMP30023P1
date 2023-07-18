#include <linked_list.h>

node* node_create(void* pData) {
    node* new_node = malloc(sizeof(node));
    new_node->data = pData;
    new_node->prev = NULL;
    new_node->next = NULL;
    return new_node;
}

void node_destroy(node** ppNode) {
    if ((*ppNode) == NULL) return;
    (*ppNode)->data = NULL;
    (*ppNode)->prev = NULL;
    (*ppNode)->next = NULL;
    FREE((*ppNode));
}

node* list_pop_node(list* pList, node* pNode) {
    if (pNode == NULL || 
        pList == NULL
        ) return NULL;

    node* reassign = pNode->next;

    if (pNode->prev == NULL) {
        list_pop_head(pList);
        return reassign;
    }

    if (pNode->next == NULL) {
        list_pop_tail(pList);
        return reassign;
    }

    if(pList->should_free_data) {
        FREE(pNode->data);
    }
    pNode->prev->next = pNode->next;
    pNode->next->prev = pNode->prev;
    node_destroy(&pNode);
    return reassign;
}

list* list_create(bool should_free_data) {
    list* pList = malloc(sizeof(list));
    pList->head = NULL;
    pList->tail = NULL;
    pList->should_free_data = should_free_data;
    return pList;
}

void list_insert_tail(list* pList, void* pData) {
    node* pNode = node_create(pData);

    if (pList->head == NULL || 
        pList->tail == NULL
        ) {
        pNode->prev = NULL;
        pNode->next = NULL;

        pList->head = pNode;
        pList->tail = pList->head;
    } else {
        pNode->prev = pList->tail;
        pNode->next = NULL;

        pList->tail->next = pNode;
        pList->tail = pNode;
    }
}

void list_insert_head(list* pList, void* pData) {
    node* pNode = node_create(pData);

    if (pList->head == NULL || 
        pList->tail == NULL
        ) {
        pNode->prev = NULL;
        pNode->next = NULL;

        pList->head = pNode;
        pList->tail = pList->head;
    } else {
        pNode->prev = NULL;
        pNode->next = pList->head;

        pList->head->prev = pNode;
        pList->head = pNode;
    }
}

void list_pop_head(list* pList) {
    if (pList == NULL || 
        pList->head == NULL ||
        pList->tail == NULL
        ) return;

    node* pHead = pList->head;
    if (pList->head == pList->tail) {
        pList->tail = NULL;
        pList->head = NULL;
    } else {
        pList->head = pList->head->next;
        pList->head->prev = NULL;
    }

    if (pList->should_free_data) {
        FREE(pHead->data);
    }
    node_destroy(&pHead);
}

void list_pop_tail(list* pList) {
    if (pList == NULL || 
        pList->head == NULL ||
        pList->tail == NULL 
        ) return;

    node* pTail = pList->tail;
    if (pList->head == pList->tail) {
        pList->tail = NULL;
        pList->head = NULL;
    } else {
        pList->tail = pList->tail->prev;
        pList->tail->next = NULL;
    }
    if (pList->should_free_data) {
        FREE(pTail->data);
    }
    node_destroy(&pTail);
}

void list_insert_sorted(list* pList, void* pData, int32_t (*cmp)(void*, void*)) {
    assert(pList != NULL);
    assert(pData != NULL);
    assert(cmp != NULL);

    node* pNode = pList->head;
    while(pNode != NULL) {
        int32_t result = cmp(pData, pNode->data);
        if (result <= 0) break;
        pNode = pNode->next;
    }

    if (pNode == NULL) {
        list_insert_tail(pList, pData);
    } else if (pNode->prev == NULL) {
        list_insert_head(pList, pData);
    } else {
        node* pPrev = pNode->prev;
        node* pNew = node_create(pData);
        pPrev->next = pNew;
        pNew->prev = pPrev;
        pNew->next = pNode;
        pNode->prev = pNew;
    }
}

void list_destroy(list** ppList) {
    if(*ppList == NULL) return;

    node* pNode = (*ppList)->head;
    while(pNode != NULL) {
        pNode = list_pop_node(*ppList, pNode);
    }

    (*ppList)->head = NULL;
    (*ppList)->tail = NULL;
    FREE((*ppList));
}