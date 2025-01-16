#include "exchange_matcher.h"

t_list *new_list(void *data) {
    t_list *list;

    if (!(list = (t_list *)calloc(1, sizeof(t_list))))
        return NULL;
    list->data = data;
    list->next = NULL;
    return (list);
}

result append_item(t_list *list, void *data) {
    while (list) {
        if (!list->next) {
            list->next = new_list(data);
            if (!list->next)
                return (MEM_ALLOC_FILED);
            return (SUCCESS);
        }
        list = list->next;
    }
    return (WRONG_DATA);
}

void free_list(t_list **list) {
    t_list *buf = *list;
    t_list *next;

    while (buf) {
        next = buf->next;
        if (buf->data)
            free(buf->data);
        free(buf);
        buf = next;
    }
    *list = NULL;
}

result insert_item(t_list **head, t_order *order, char side) {
    t_list *buf = NULL;
    t_list *new_item = NULL;

    if (!(new_item = new_list(order)))
        return (MEM_ALLOC_FILED);
    buf = *head;
    if (!buf->data) {
        *head = new_item;
        if (buf->next)
            free_list(&buf->next);
        return (SUCCESS);
    }
    if (is_market_price_better(side, order->price,
                               ((t_order *)buf->data)->price) > 0) {
        new_item->next = buf;
        *head = new_item;
    } else {
        while (buf->next && is_market_price_better(
                                side, order->price,
                                ((t_order *)buf->next->data)->price) <= 0) {
            buf = buf->next;
        }
        new_item->next = buf->next;
        buf->next = new_item;
    }
    return (SUCCESS);
}
