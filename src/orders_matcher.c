#include "exchange_matcher.h"

/* Executes order, inits and construcs new trade, appends trade to trades list
 */
result trade_order(t_list **head, t_order *order, char side, t_list *trades) {
    t_trade *trade;
    t_list *buf = NULL;
    unsigned volume = 0;
    static unsigned trade_id = 0;
    result res = SUCCESS;

    if (!(*head && (*head)->data && order && trades))
        return (WRONG_DATA);
    volume = min(order->qty, ((t_order *)(*head)->data)->qty);
    if (!(trade = (t_trade *)calloc(1, sizeof(t_trade))))
        return (MEM_ALLOC_FILED);
    order->qty -= volume;
    ((t_order *)(*head)->data)->qty -= volume;
    trade->id = ++trade_id;
    trade->oid1 = ((t_order *)(*head)->data)->oid;
    trade->oid2 = order->oid;
    trade->price[0] = ((t_order *)(*head)->data)->price[0];
    trade->price[1] = ((t_order *)(*head)->data)->price[1];
    trade->qty = volume;
    trade->side = (side == 'B') ? 'S' : 'B';
    if ((res = append_item(trades, trade)) != SUCCESS) {
        free(trade);
        return (res);
    }
    if (!((t_order *)(*head)->data)->qty) {
        if ((*head)->next) {
            buf = *head;
            (*head) = buf->next;
            free(buf->data);
            free(buf);
        } else {
            free((*head)->data);
            (*head)->data = NULL;
        }
    }
    return (SUCCESS);
}

/* Places order to market depth. If order matches, executes order,
   writes trades to trades list.
   Frees order if needed. If returns error, order is already freed.
   If SUCCESS - placed in market depth or fully executed and freed.
   Do not free order after this finction. */
result place_order(char side, t_order *order, t_list **bid, t_list **offer,
                   t_list *trades) {
    t_list **place_side;
    t_list **trade_side;
    result res;

    if (!(order && bid && offer && trades)) {
        if (order)
            free(order);
        return (WRONG_DATA);
    }
    if (side == 'B') {
        place_side = bid;
        trade_side = offer;
    } else if (side == 'S') {
        place_side = offer;
        trade_side = bid;
    } else {
        return (WRONG_DATA);
    }
    if (!(*trade_side)->data) {
        res = insert_item(place_side, order, side);
        if (res != SUCCESS) {
            free(order);
            return (res);
        }
    } else {
        if (is_market_price_better(side, order->price,
                                   ((t_order *)(*trade_side)->data)->price) <
            0) {
            res = insert_item(place_side, order, side);
            if (res != SUCCESS) {
                free(order);
                return (res);
            }
        } else {
            res = trade_order(trade_side, order, side, trades);
            if (res != SUCCESS) {
                free(order);
                return (res);
            }
            if (order->qty)
                return (place_order(side, order, bid, offer, trades));
            else
                free(order);
        }
    }
    return (SUCCESS);
}

/* Removes existing order from market depth. If order id doesn't exist
   in market depth, doesn't do anything. */
result cancel_order(unsigned order_id, t_list **offer, t_list **bid) {
    t_list *buf = NULL;
    t_list *buf_2 = NULL;

    if (offer && *offer && (*offer)->data) {
        buf = *offer;
        if (((t_order *)(*offer)->data)->oid == order_id) {
            (*offer) = buf->next;
            buf->next = NULL;
            free_list(&buf);
            return (SUCCESS);
        }
        for (; buf->next; buf = buf->next) {
            if (buf->next->data &&
                ((t_order *)buf->next->data)->oid == order_id) {
                buf_2 = buf->next;
                buf->next = buf_2->next;
                buf_2->next = NULL;
                free_list(&buf_2);
                return (SUCCESS);
            }
        }
    }
    if (bid)
        return (cancel_order(order_id, bid, NULL));
    return (NOTHING_TO_DO);
}

/* Reads from input FILE (in), parses data to order, sends orders to
   market depth (place_order()) or cancels orders (cancel_orders).
   Reads trades from trades list, serializes trade data to string
   And sends strings to output FILE (out) */
result process(FILE *in, FILE *out) {
    char order_line[ORDER_LINE_SIZE] = {0};
    char trade_line[TRADE_LINE_SIZE] = {0};
    t_order *order = NULL;
    t_list *trades = NULL;
    t_list *trade = NULL;
    t_list *bid = NULL;
    t_list *offer = NULL;
    char type = 0;
    char side = 0;
    unsigned order_id = 0;
    char ceil_1 = '0';
    char ceil_2 = '0';
    unsigned ceil = 0;
    result res = SUCCESS;

    if (!(trades = new_list(NULL)))
        return (MEM_ALLOC_FILED);
    if (!(bid = new_list(NULL))) {
        free_list(&trades);
        return (MEM_ALLOC_FILED);
    }
    if (!(offer = new_list(NULL))) {
        free_list(&trades);
        free_list(&bid);
        return (MEM_ALLOC_FILED);
    }
    while (fgets(order_line, sizeof(order_line), in)) {
        if (isspace(order_line[0]))
            break;
        if (order_line[0] == 'O') {
            if (!(order = (t_order *)calloc(1, sizeof(t_order)))) {
                free_list(&trades);
                free_list(&bid);
                free_list(&offer);
                return (MEM_ALLOC_FILED);
            }
            if (sscanf(order_line, "%c,%u,%c,%u,%u.%c%c", &type, &order->oid,
                       &side, &order->qty, &order->price[0], &ceil_1,
                       &ceil_2) < 0) {
                free_list(&trades);
                free_list(&bid);
                free_list(&offer);
                free(order);
                return (END_OF_DATA);
            }
            ceil_1 = (ceil_1 > '9' || ceil_1 < '0') ? '0' : ceil_1;
            ceil_2 = (ceil_2 > '9' || ceil_2 < '0') ? '0' : ceil_2;
            order->price[1] = (ceil_1 - 48) * 10 + ceil_2 - 48;
            if (place_order(side, order, &bid, &offer, trades) == SUCCESS) {
                order = NULL;
                trade = trades->next;
                while (trade) {
                    ceil = ((t_trade *)trade->data)->price[1];
                    if (!(ceil % 10))
                        ceil = ceil / 10;
                    if (snprintf(trade_line, TRADE_LINE_SIZE,
                                 "T,%u,%c,%u,%u,%u,%u.%u\n",
                                 ((t_trade *)trade->data)->id,
                                 ((t_trade *)trade->data)->side,
                                 ((t_trade *)trade->data)->oid1,
                                 ((t_trade *)trade->data)->oid2,
                                 ((t_trade *)trade->data)->qty,
                                 ((t_trade *)trade->data)->price[0],
                                 ceil) < 0) {
                        free_list(&trades);
                        free_list(&bid);
                        free_list(&offer);
                        return (TRADELINE_OVERFLOW);
                    }
                    fputs(trade_line, out);
                    trade = trade->next;
                }
                free_list(&(trades->next));
                trades->next = NULL;
            } else {
                free_list(&trades);
                free_list(&bid);
                free_list(&offer);
                return (WRONG_DATA);
            }
        } else if (order_line[0] == 'C') {
            errno = 0;
            order_id = atoi(order_line + 2);
            if ((res = cancel_order(order_id, &offer, &bid)) == SUCCESS) {
                sprintf(trade_line, "X,%u\n", order_id);
                fputs(trade_line, out);
            }
            if (errno) {
                free_list(&trades);
                free_list(&bid);
                free_list(&offer);
                return (res);
            }
        }
    }
    free_list(&trades);
    free_list(&bid);
    free_list(&offer);
    return (SUCCESS);
}
