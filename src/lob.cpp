#include "lob.hpp"

namespace lob {

static void reset_result(CommandResult &result) {
    result.accepted = false;
    result.code = RES_NONE;
    result.touched_price = INVALID_PRICE;
    result.touched_total_quantity = 0;
    result.touched_order_count = 0;
    result.executed_quantity = 0;
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

    for (std::size_t i = 0; i < MAX_PRICE_LEVELS; ++i) {
        init_level(book.bids[i]);
        init_level(book.asks[i]);
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
    for (std::uint16_t i = 0; i < level_count; ++i) {
        if (side == BID) {
            if (price > levels[i].price) {
                return i;
            }
        } else {
            if (price < levels[i].price) {
                return i;
            }
        }
    }

    return level_count;
}

static bool enqueue_order(PriceLevel &level, const Command &cmd) {
    if (level.order_count >= MAX_ORDERS_PER_LEVEL) {
        return false;
    }

    const std::uint16_t slot = level.tail;
    level.orders[slot].valid = true;
    level.orders[slot].order_id = cmd.order_id;
    level.orders[slot].side = cmd.side;
    level.orders[slot].price = cmd.price;
    level.orders[slot].quantity = cmd.quantity;

    level.tail = static_cast<std::uint16_t>((level.tail + 1) % MAX_ORDERS_PER_LEVEL);
    level.order_count = static_cast<std::uint16_t>(level.order_count + 1);
    level.total_quantity += cmd.quantity;
    return true;
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

static bool is_crossing(Side incoming_side, int incoming_price, int resting_price) {
    if (incoming_side == BID) {
        return incoming_price >= resting_price;
    }

    return incoming_price <= resting_price;
}

static void consume_head_order(PriceLevel &level, int fill_quantity) {
    const std::uint16_t slot = level.head;
    level.orders[slot].quantity -= fill_quantity;
    level.total_quantity -= fill_quantity;

    if (level.orders[slot].quantity == 0) {
        level.orders[slot].valid = false;
        level.orders[slot].order_id = 0;
        level.orders[slot].price = INVALID_PRICE;
        level.order_count = static_cast<std::uint16_t>(level.order_count - 1);
        level.head = static_cast<std::uint16_t>((level.head + 1) % MAX_ORDERS_PER_LEVEL);
    }
}

static ResultCode add_order_to_side(PriceLevel levels[MAX_PRICE_LEVELS],
                                    std::uint16_t &level_count,
                                    const Command &cmd,
                                    int &touched_price,
                                    int &touched_total_quantity,
                                    std::uint16_t &touched_order_count) {
    const std::uint16_t existing_index = find_level_index(levels, level_count, cmd.price);

    if (existing_index < level_count) {
        if (!enqueue_order(levels[existing_index], cmd)) {
            return RES_REJECTED_LEVEL_FULL;
        }

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

    level_count = static_cast<std::uint16_t>(level_count + 1);
    touched_price = levels[insert_index].price;
    touched_total_quantity = levels[insert_index].total_quantity;
    touched_order_count = levels[insert_index].order_count;
    return RES_ACCEPTED;
}

static void match_against_side(PriceLevel levels[MAX_PRICE_LEVELS],
                               std::uint16_t &level_count,
                               const Command &cmd,
                               CommandResult &result,
                               int &remaining_quantity) {
    for (std::size_t level_iter = 0;
         level_iter < MAX_PRICE_LEVELS && remaining_quantity > 0 && level_count > 0;
         ++level_iter) {
        PriceLevel &best_level = levels[0];

        if (!is_crossing(cmd.side, cmd.price, best_level.price)) {
            break;
        }

        result.touched_price = best_level.price;

        for (std::size_t order_iter = 0;
             order_iter < MAX_ORDERS_PER_LEVEL && remaining_quantity > 0 && best_level.order_count > 0;
             ++order_iter) {
            const std::uint16_t slot = best_level.head;

            if (!best_level.orders[slot].valid) {
                break;
            }

            const int resting_quantity = best_level.orders[slot].quantity;
            const int fill_quantity = (remaining_quantity < resting_quantity) ? remaining_quantity : resting_quantity;

            remaining_quantity -= fill_quantity;
            result.executed_quantity += fill_quantity;
            result.last_trade_price = best_level.price;
            result.trade_count = static_cast<std::uint16_t>(result.trade_count + 1);

            consume_head_order(best_level, fill_quantity);
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

void process_command(LimitOrderBook &book, const Command &cmd, CommandResult &result) {
    reset_result(result);

    if (cmd.type == CMD_NOP) {
        result.remaining_quantity = 0;
        summarize_book(book, result.summary);
        return;
    }

    if (cmd.type == CMD_RESET) {
        reset_book(book);
        result.accepted = true;
        result.code = RES_ACCEPTED;
        result.remaining_quantity = 0;
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

    int remaining_quantity = cmd.quantity;
    ResultCode code = RES_ACCEPTED;

    if (cmd.side == BID) {
        match_against_side(book.asks, book.ask_level_count, cmd, result, remaining_quantity);
        if (remaining_quantity > 0) {
            Command residual_cmd = cmd;
            residual_cmd.quantity = remaining_quantity;
            code = add_order_to_side(book.bids,
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
        match_against_side(book.bids, book.bid_level_count, cmd, result, remaining_quantity);
        if (remaining_quantity > 0) {
            Command residual_cmd = cmd;
            residual_cmd.quantity = remaining_quantity;
            code = add_order_to_side(book.asks,
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
