#include "lob.hpp"

namespace lob {

static void reset_result(CommandResult &result) {
    result.accepted = false;
    result.code = RES_NONE;
    result.touched_price = INVALID_PRICE;
    result.touched_total_quantity = 0;
    result.touched_order_count = 0;
    result.executed_quantity = 0;
    result.cancelled_quantity = 0;
    result.remaining_quantity = 0;
    result.last_trade_price = INVALID_PRICE;
    result.trade_count = 0;
    result.summary.best_bid_price = INVALID_PRICE;
    result.summary.best_bid_quantity = 0;
    result.summary.best_ask_price = INVALID_PRICE;
    result.summary.best_ask_quantity = 0;
    result.summary.bid_level_count = 0;
    result.summary.ask_level_count = 0;
}

static void init_lookup_entry(OrderLookupEntry &entry) {
    entry.active = false;
    entry.order_id = 0;
    entry.side = BID;
    entry.price = INVALID_PRICE;
}

void init_level(PriceLevel &level) {
    level.active = false;
    level.price = INVALID_PRICE;
    level.total_quantity = 0;
    level.order_count = 0;
    level.head = 0;
    level.tail = 0;

    for (std::size_t i = 0; i < MAX_ORDERS_PER_LEVEL; ++i) {
        level.orders[i].valid = false;
        level.orders[i].order_id = 0;
        level.orders[i].side = BID;
        level.orders[i].price = INVALID_PRICE;
        level.orders[i].quantity = 0;
    }
}

void reset_book(LimitOrderBook &book) {
    book.bid_level_count = 0;
    book.ask_level_count = 0;
    book.active_order_count = 0;

    for (std::size_t i = 0; i < MAX_PRICE_LEVELS; ++i) {
        init_level(book.bids[i]);
        init_level(book.asks[i]);
    }

    for (std::size_t i = 0; i < MAX_ORDERS; ++i) {
        init_lookup_entry(book.order_lookup[i]);
    }
}

void summarize_book(const LimitOrderBook &book, BookSummary &summary) {
    summary.best_bid_price = INVALID_PRICE;
    summary.best_bid_quantity = 0;
    summary.best_ask_price = INVALID_PRICE;
    summary.best_ask_quantity = 0;
    summary.bid_level_count = book.bid_level_count;
    summary.ask_level_count = book.ask_level_count;

    if (book.bid_level_count > 0) {
        summary.best_bid_price = book.bids[0].price;
        summary.best_bid_quantity = book.bids[0].total_quantity;
    }

    if (book.ask_level_count > 0) {
        summary.best_ask_price = book.asks[0].price;
        summary.best_ask_quantity = book.asks[0].total_quantity;
    }
}

static std::uint16_t find_level_index(const PriceLevel levels[MAX_PRICE_LEVELS],
                                      std::uint16_t level_count,
                                      int price) {
    // HLS note: this bounded search is a natural candidate for loop pipelining.
    for (std::uint16_t i = 0; i < level_count; ++i) {
        if (levels[i].active && levels[i].price == price) {
            return i;
        }
    }

    return level_count;
}

static std::uint16_t find_insert_index(const PriceLevel levels[MAX_PRICE_LEVELS],
                                       std::uint16_t level_count,
                                       Side side,
                                       int price) {
    // HLS note: with a bounded number of levels, linear scan keeps control simple.
    for (std::uint16_t i = 0; i < level_count; ++i) {
        if (side == BID) {
            if (price > levels[i].price) {
                return i;
            }
        } else if (price < levels[i].price) {
            return i;
        }
    }

    return level_count;
}

static void shift_levels_right(PriceLevel levels[MAX_PRICE_LEVELS],
                               std::uint16_t level_count,
                               std::uint16_t insert_index) {
    for (int i = static_cast<int>(level_count); i > static_cast<int>(insert_index); --i) {
        levels[i] = levels[i - 1];
    }
}

static void shift_levels_left(PriceLevel levels[MAX_PRICE_LEVELS],
                              std::uint16_t &level_count,
                              std::uint16_t remove_index) {
    for (std::uint16_t i = remove_index; i + 1 < level_count; ++i) {
        levels[i] = levels[i + 1];
    }

    if (level_count > 0) {
        init_level(levels[level_count - 1]);
        level_count = static_cast<std::uint16_t>(level_count - 1);
    }
}

static std::uint16_t find_lookup_index(const LimitOrderBook &book, std::uint32_t order_id) {
    for (std::uint16_t i = 0; i < MAX_ORDERS; ++i) {
        if (book.order_lookup[i].active && book.order_lookup[i].order_id == order_id) {
            return i;
        }
    }

    return MAX_ORDERS;
}

static std::uint16_t find_free_lookup_index(const LimitOrderBook &book) {
    for (std::uint16_t i = 0; i < MAX_ORDERS; ++i) {
        if (!book.order_lookup[i].active) {
            return i;
        }
    }

    return MAX_ORDERS;
}

static bool register_resting_order(LimitOrderBook &book, const Command &cmd) {
    if (book.active_order_count >= MAX_ORDERS) {
        return false;
    }

    const std::uint16_t lookup_index = find_free_lookup_index(book);
    if (lookup_index >= MAX_ORDERS) {
        return false;
    }

    book.order_lookup[lookup_index].active = true;
    book.order_lookup[lookup_index].order_id = cmd.order_id;
    book.order_lookup[lookup_index].side = cmd.side;
    book.order_lookup[lookup_index].price = cmd.price;
    book.active_order_count = static_cast<std::uint16_t>(book.active_order_count + 1);
    return true;
}

static void release_lookup_entry(LimitOrderBook &book, std::uint32_t order_id) {
    const std::uint16_t lookup_index = find_lookup_index(book, order_id);
    if (lookup_index < MAX_ORDERS) {
        init_lookup_entry(book.order_lookup[lookup_index]);
        book.active_order_count = static_cast<std::uint16_t>(book.active_order_count - 1);
    }
}

static void advance_head_to_valid(PriceLevel &level) {
    while (level.order_count > 0 && !level.orders[level.head].valid) {
        level.head = static_cast<std::uint16_t>((level.head + 1) % MAX_ORDERS_PER_LEVEL);
    }
}

static bool find_next_free_slot(const PriceLevel &level, std::uint16_t &slot) {
    // HLS note: this search avoids linked-list freelists and keeps storage static.
    for (std::uint16_t offset = 0; offset < MAX_ORDERS_PER_LEVEL; ++offset) {
        const std::uint16_t candidate =
            static_cast<std::uint16_t>((level.tail + offset) % MAX_ORDERS_PER_LEVEL);
        if (!level.orders[candidate].valid) {
            slot = candidate;
            return true;
        }
    }

    return false;
}

static bool enqueue_order(PriceLevel &level, const Command &cmd) {
    if (level.order_count >= MAX_ORDERS_PER_LEVEL) {
        return false;
    }

    std::uint16_t slot = 0;
    if (!find_next_free_slot(level, slot)) {
        return false;
    }

    level.orders[slot].valid = true;
    level.orders[slot].order_id = cmd.order_id;
    level.orders[slot].side = cmd.side;
    level.orders[slot].price = cmd.price;
    level.orders[slot].quantity = cmd.quantity;

    level.tail = static_cast<std::uint16_t>((slot + 1) % MAX_ORDERS_PER_LEVEL);
    level.order_count = static_cast<std::uint16_t>(level.order_count + 1);
    level.total_quantity += cmd.quantity;
    if (level.order_count == 1) {
        level.head = slot;
    }
    return true;
}

static bool is_crossing(Side incoming_side, int incoming_price, int resting_price) {
    if (incoming_side == BID) {
        return incoming_price >= resting_price;
    }

    return incoming_price <= resting_price;
}

static void consume_head_order(LimitOrderBook &book, PriceLevel &level, int fill_quantity) {
    advance_head_to_valid(level);

    const std::uint16_t slot = level.head;
    level.orders[slot].quantity -= fill_quantity;
    level.total_quantity -= fill_quantity;

    if (level.orders[slot].quantity == 0) {
        const std::uint32_t completed_order_id = level.orders[slot].order_id;
        level.orders[slot].valid = false;
        level.orders[slot].order_id = 0;
        level.orders[slot].price = INVALID_PRICE;
        level.orders[slot].quantity = 0;
        level.order_count = static_cast<std::uint16_t>(level.order_count - 1);
        release_lookup_entry(book, completed_order_id);
        if (level.order_count > 0) {
            level.head = static_cast<std::uint16_t>((level.head + 1) % MAX_ORDERS_PER_LEVEL);
            advance_head_to_valid(level);
        }
    }
}

static ResultCode add_resting_order(LimitOrderBook &book,
                                    PriceLevel levels[MAX_PRICE_LEVELS],
                                    std::uint16_t &level_count,
                                    const Command &cmd,
                                    int &touched_price,
                                    int &touched_total_quantity,
                                    std::uint16_t &touched_order_count) {
    if (book.active_order_count >= MAX_ORDERS || find_free_lookup_index(book) >= MAX_ORDERS) {
        return RES_REJECTED_BOOK_FULL;
    }

    const std::uint16_t existing_index = find_level_index(levels, level_count, cmd.price);

    if (existing_index < level_count) {
        if (!enqueue_order(levels[existing_index], cmd)) {
            return RES_REJECTED_LEVEL_FULL;
        }
        register_resting_order(book, cmd);

        touched_price = levels[existing_index].price;
        touched_total_quantity = levels[existing_index].total_quantity;
        touched_order_count = levels[existing_index].order_count;
        return RES_ACCEPTED;
    }

    if (level_count >= MAX_PRICE_LEVELS) {
        return RES_REJECTED_BOOK_FULL;
    }

    const std::uint16_t insert_index = find_insert_index(levels, level_count, cmd.side, cmd.price);
    shift_levels_right(levels, level_count, insert_index);
    init_level(levels[insert_index]);
    levels[insert_index].active = true;
    levels[insert_index].price = cmd.price;

    if (!enqueue_order(levels[insert_index], cmd)) {
        return RES_REJECTED_LEVEL_FULL;
    }
    register_resting_order(book, cmd);

    level_count = static_cast<std::uint16_t>(level_count + 1);
    touched_price = levels[insert_index].price;
    touched_total_quantity = levels[insert_index].total_quantity;
    touched_order_count = levels[insert_index].order_count;
    return RES_ACCEPTED;
}

static void match_against_side(LimitOrderBook &book,
                               PriceLevel levels[MAX_PRICE_LEVELS],
                               std::uint16_t &level_count,
                               const Command &cmd,
                               CommandResult &result,
                               int &remaining_quantity) {
    // HLS note: this outer loop walks at most MAX_PRICE_LEVELS best-price entries.
    for (std::size_t level_iter = 0;
         level_iter < MAX_PRICE_LEVELS && remaining_quantity > 0 && level_count > 0;
         ++level_iter) {
        PriceLevel &best_level = levels[0];

        if (!is_crossing(cmd.side, cmd.price, best_level.price)) {
            break;
        }

        result.touched_price = best_level.price;

        // HLS note: FIFO within a level is preserved by consuming from head only.
        for (std::size_t order_iter = 0;
             order_iter < MAX_ORDERS_PER_LEVEL && remaining_quantity > 0 && best_level.order_count > 0;
             ++order_iter) {
            advance_head_to_valid(best_level);

            if (best_level.order_count == 0 || !best_level.orders[best_level.head].valid) {
                break;
            }

            const int resting_quantity = best_level.orders[best_level.head].quantity;
            const int fill_quantity = (remaining_quantity < resting_quantity) ? remaining_quantity : resting_quantity;

            remaining_quantity -= fill_quantity;
            result.executed_quantity += fill_quantity;
            result.last_trade_price = best_level.price;
            result.trade_count = static_cast<std::uint16_t>(result.trade_count + 1);

            consume_head_order(book, best_level, fill_quantity);
        }

        if (best_level.order_count == 0) {
            result.touched_total_quantity = 0;
            result.touched_order_count = 0;
            shift_levels_left(levels, level_count, 0);
        } else {
            result.touched_total_quantity = best_level.total_quantity;
            result.touched_order_count = best_level.order_count;
        }
    }
}

static bool cancel_from_level(LimitOrderBook &book,
                              PriceLevel levels[MAX_PRICE_LEVELS],
                              std::uint16_t &level_count,
                              const OrderLookupEntry &lookup_entry,
                              CommandResult &result) {
    const std::uint16_t level_index = find_level_index(levels, level_count, lookup_entry.price);
    if (level_index >= level_count) {
        return false;
    }

    PriceLevel &level = levels[level_index];

    for (std::uint16_t slot = 0; slot < MAX_ORDERS_PER_LEVEL; ++slot) {
        if (!level.orders[slot].valid || level.orders[slot].order_id != lookup_entry.order_id) {
            continue;
        }

        result.cancelled_quantity = level.orders[slot].quantity;
        level.total_quantity -= level.orders[slot].quantity;
        level.orders[slot].valid = false;
        level.orders[slot].order_id = 0;
        level.orders[slot].price = INVALID_PRICE;
        level.orders[slot].quantity = 0;
        level.order_count = static_cast<std::uint16_t>(level.order_count - 1);
        release_lookup_entry(book, lookup_entry.order_id);

        if (level.order_count == 0) {
            result.touched_price = lookup_entry.price;
            result.touched_total_quantity = 0;
            result.touched_order_count = 0;
            shift_levels_left(levels, level_count, level_index);
        } else {
            if (slot == level.head) {
                advance_head_to_valid(level);
            }
            result.touched_price = level.price;
            result.touched_total_quantity = level.total_quantity;
            result.touched_order_count = level.order_count;
        }

        return true;
    }

    return false;
}

void process_command(LimitOrderBook &book, const Command &cmd, CommandResult &result) {
    reset_result(result);

    if (cmd.type == CMD_NOP) {
        summarize_book(book, result.summary);
        return;
    }

    if (cmd.type == CMD_RESET) {
        reset_book(book);
        result.accepted = true;
        result.code = RES_ACCEPTED;
        summarize_book(book, result.summary);
        return;
    }

    if (cmd.type == CMD_CANCEL) {
        const std::uint16_t lookup_index = find_lookup_index(book, cmd.order_id);
        if (lookup_index >= MAX_ORDERS) {
            result.code = RES_REJECTED_NOT_FOUND;
            summarize_book(book, result.summary);
            return;
        }

        const OrderLookupEntry lookup_entry = book.order_lookup[lookup_index];
        bool cancelled = false;

        if (lookup_entry.side == BID) {
            cancelled = cancel_from_level(book, book.bids, book.bid_level_count, lookup_entry, result);
        } else {
            cancelled = cancel_from_level(book, book.asks, book.ask_level_count, lookup_entry, result);
        }

        result.accepted = cancelled;
        result.code = cancelled ? RES_CANCELLED : RES_REJECTED_NOT_FOUND;
        summarize_book(book, result.summary);
        return;
    }

    if (cmd.type != CMD_ADD) {
        result.code = RES_UNSUPPORTED;
        summarize_book(book, result.summary);
        return;
    }

    if (cmd.price <= 0) {
        result.remaining_quantity = cmd.quantity;
        result.code = RES_REJECTED_BAD_PRICE;
        summarize_book(book, result.summary);
        return;
    }

    if (cmd.quantity <= 0) {
        result.remaining_quantity = cmd.quantity;
        result.code = RES_REJECTED_BAD_QUANTITY;
        summarize_book(book, result.summary);
        return;
    }

    if (find_lookup_index(book, cmd.order_id) < MAX_ORDERS) {
        result.remaining_quantity = cmd.quantity;
        result.code = RES_REJECTED_DUPLICATE_ID;
        summarize_book(book, result.summary);
        return;
    }

    int remaining_quantity = cmd.quantity;
    ResultCode code = RES_ACCEPTED;

    if (cmd.side == BID) {
        match_against_side(book, book.asks, book.ask_level_count, cmd, result, remaining_quantity);
        if (remaining_quantity > 0) {
            Command residual_cmd = cmd;
            residual_cmd.quantity = remaining_quantity;
            code = add_resting_order(book,
                                     book.bids,
                                     book.bid_level_count,
                                     residual_cmd,
                                     result.touched_price,
                                     result.touched_total_quantity,
                                     result.touched_order_count);
            if (code == RES_ACCEPTED) {
                remaining_quantity = 0;
            }
        } else {
            code = RES_MATCHED;
        }
    } else {
        match_against_side(book, book.bids, book.bid_level_count, cmd, result, remaining_quantity);
        if (remaining_quantity > 0) {
            Command residual_cmd = cmd;
            residual_cmd.quantity = remaining_quantity;
            code = add_resting_order(book,
                                     book.asks,
                                     book.ask_level_count,
                                     residual_cmd,
                                     result.touched_price,
                                     result.touched_total_quantity,
                                     result.touched_order_count);
            if (code == RES_ACCEPTED) {
                remaining_quantity = 0;
            }
        } else {
            code = RES_MATCHED;
        }
    }

    if (result.executed_quantity > 0 && code == RES_ACCEPTED) {
        code = RES_MATCHED_AND_RESTED;
    }

    result.code = code;
    result.accepted = (result.executed_quantity > 0 ||
                       code == RES_ACCEPTED ||
                       code == RES_MATCHED ||
                       code == RES_MATCHED_AND_RESTED);
    result.remaining_quantity = remaining_quantity;
    summarize_book(book, result.summary);
}

}  // namespace lob
