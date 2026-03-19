#ifndef LOB_HPP
#define LOB_HPP

#include <cstddef>
#include <cstdint>

namespace lob {

static const std::size_t MAX_ORDERS = 128;
static const std::size_t MAX_PRICE_LEVELS = 16;
static const std::size_t MAX_ORDERS_PER_LEVEL = 8;
static const int INVALID_PRICE = -1;

enum Side {
    BID = 0,
    ASK = 1
};

enum CommandType {
    CMD_NOP = 0,
    CMD_RESET = 1,
    CMD_ADD = 2
};

enum ResultCode {
    RES_NONE = 0,
    RES_ACCEPTED = 1,
    RES_REJECTED_BAD_PRICE = 2,
    RES_REJECTED_BAD_QUANTITY = 3,
    RES_REJECTED_BOOK_FULL = 4,
    RES_REJECTED_LEVEL_FULL = 5,
    RES_UNSUPPORTED = 6
};

struct Order {
    bool valid;
    std::uint32_t order_id;
    Side side;
    int price;
    int quantity;
};

struct PriceLevel {
    bool active;
    int price;
    int total_quantity;
    std::uint16_t order_count;
    std::uint16_t head;
    std::uint16_t tail;
    Order orders[MAX_ORDERS_PER_LEVEL];
};

struct Command {
    CommandType type;
    Side side;
    std::uint32_t order_id;
    int price;
    int quantity;
};

struct BookSummary {
    int best_bid_price;
    int best_bid_quantity;
    int best_ask_price;
    int best_ask_quantity;
    std::uint16_t bid_level_count;
    std::uint16_t ask_level_count;
};

struct CommandResult {
    bool accepted;
    ResultCode code;
    int touched_price;
    int touched_total_quantity;
    std::uint16_t touched_order_count;
    BookSummary summary;
};

struct LimitOrderBook {
    PriceLevel bids[MAX_PRICE_LEVELS];
    PriceLevel asks[MAX_PRICE_LEVELS];
    std::uint16_t bid_level_count;
    std::uint16_t ask_level_count;
};

void init_level(PriceLevel &level);
void reset_book(LimitOrderBook &book);
void summarize_book(const LimitOrderBook &book, BookSummary &summary);
void process_command(LimitOrderBook &book, const Command &cmd, CommandResult &result);

extern "C" void lob_top(const Command &cmd, CommandResult &result);

}  // namespace lob

#endif
