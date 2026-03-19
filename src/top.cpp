#include "lob.hpp"

namespace lob {

static Side decode_side(std::uint32_t side_code) {
    return (side_code == static_cast<std::uint32_t>(ASK)) ? ASK : BID;
}

extern "C" void lob_top(const Command &cmd, CommandResult &result) {
    static LimitOrderBook book;
    static bool initialized = false;

    if (!initialized) {
        reset_book(book);
        initialized = true;
    }

    process_command(book, cmd, result);
}

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
                                   std::uint32_t &ask_level_count) {
#ifdef __SYNTHESIS__
#pragma HLS INTERFACE s_axilite port=cmd_type bundle=CTRL
#pragma HLS INTERFACE s_axilite port=side bundle=CTRL
#pragma HLS INTERFACE s_axilite port=order_id bundle=CTRL
#pragma HLS INTERFACE s_axilite port=price bundle=CTRL
#pragma HLS INTERFACE s_axilite port=quantity bundle=CTRL
#pragma HLS INTERFACE s_axilite port=accepted bundle=CTRL
#pragma HLS INTERFACE s_axilite port=result_code bundle=CTRL
#pragma HLS INTERFACE s_axilite port=touched_price bundle=CTRL
#pragma HLS INTERFACE s_axilite port=touched_total_quantity bundle=CTRL
#pragma HLS INTERFACE s_axilite port=touched_order_count bundle=CTRL
#pragma HLS INTERFACE s_axilite port=executed_quantity bundle=CTRL
#pragma HLS INTERFACE s_axilite port=cancelled_quantity bundle=CTRL
#pragma HLS INTERFACE s_axilite port=remaining_quantity bundle=CTRL
#pragma HLS INTERFACE s_axilite port=last_trade_price bundle=CTRL
#pragma HLS INTERFACE s_axilite port=trade_count bundle=CTRL
#pragma HLS INTERFACE s_axilite port=best_bid_price bundle=CTRL
#pragma HLS INTERFACE s_axilite port=best_bid_quantity bundle=CTRL
#pragma HLS INTERFACE s_axilite port=best_ask_price bundle=CTRL
#pragma HLS INTERFACE s_axilite port=best_ask_quantity bundle=CTRL
#pragma HLS INTERFACE s_axilite port=bid_level_count bundle=CTRL
#pragma HLS INTERFACE s_axilite port=ask_level_count bundle=CTRL
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL
#endif

    Command cmd;
    cmd.type = static_cast<CommandType>(cmd_type);
    cmd.side = decode_side(side);
    cmd.order_id = order_id;
    cmd.price = price;
    cmd.quantity = quantity;

    CommandResult result;
    lob_top(cmd, result);

    accepted = result.accepted ? 1U : 0U;
    result_code = static_cast<std::uint32_t>(result.code);
    touched_price = result.touched_price;
    touched_total_quantity = result.touched_total_quantity;
    touched_order_count = result.touched_order_count;
    executed_quantity = result.executed_quantity;
    cancelled_quantity = result.cancelled_quantity;
    remaining_quantity = result.remaining_quantity;
    last_trade_price = result.last_trade_price;
    trade_count = result.trade_count;
    best_bid_price = result.summary.best_bid_price;
    best_bid_quantity = result.summary.best_bid_quantity;
    best_ask_price = result.summary.best_ask_price;
    best_ask_quantity = result.summary.best_ask_quantity;
    bid_level_count = result.summary.bid_level_count;
    ask_level_count = result.summary.ask_level_count;
}

}  // namespace lob
