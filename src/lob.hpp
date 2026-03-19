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
    CMD_ADD = 2,
    CMD_CANCEL = 3
};

enum ResultCode {
    RES_NONE = 0,
    RES_ACCEPTED = 1,
    RES_MATCHED = 2,
    RES_MATCHED_AND_RESTED = 3,
    RES_CANCELLED = 4,
    RES_REJECTED_BAD_PRICE = 5,
    RES_REJECTED_BAD_QUANTITY = 6,
    RES_REJECTED_BOOK_FULL = 7,
    RES_REJECTED_LEVEL_FULL = 8,
    RES_REJECTED_DUPLICATE_ID = 9,
    RES_REJECTED_NOT_FOUND = 10,
    RES_UNSUPPORTED = 11
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

struct OrderLookupEntry {
    bool active;
    std::uint32_t order_id;
    Side side;
    int price;
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
    int executed_quantity;
    int cancelled_quantity;
    int remaining_quantity;
    int last_trade_price;
    std::uint16_t trade_count;
    BookSummary summary;
};

struct LimitOrderBook {
    PriceLevel bids[MAX_PRICE_LEVELS];
    PriceLevel asks[MAX_PRICE_LEVELS];
    OrderLookupEntry order_lookup[MAX_ORDERS];
    std::uint16_t bid_level_count;
    std::uint16_t ask_level_count;
    std::uint16_t active_order_count;
};

void init_level(PriceLevel &level);
void reset_book(LimitOrderBook &book);
void summarize_book(const LimitOrderBook &book, BookSummary &summary);
void process_command(LimitOrderBook &book, const Command &cmd, CommandResult &result);

extern "C" void lob_top(const Command &cmd, CommandResult &result);
extern "C" void lob_axi_peripheral(std::uint32_t cmd_type,
                                   std::uint32_t side,
                                   std::uint32_t order_id,
                                   int price,
                                   int quantity,
                                   std::uint32_t &accepted,
                                   std::uint32_t &result_code,
                                   int &touched_price,
                                   int &touched_total_quantity,
                                   std::uint32_t &touched_order_count,
                                   int &executed_quantity,
                                   int &cancelled_quantity,
                                   int &remaining_quantity,
                                   int &last_trade_price,
                                   std::uint32_t &trade_count,
                                   int &best_bid_price,
                                   int &best_bid_quantity,
                                   int &best_ask_price,
                                   int &best_ask_quantity,
                                   std::uint32_t &bid_level_count,
                                   std::uint32_t &ask_level_count);

}  // namespace lob

#endif
