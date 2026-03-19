#include <cstdlib>
#include <iostream>

#include "lob.hpp"

namespace {

void expect(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "TEST FAILED: " << message << '\n';
        std::exit(1);
    }
}

lob::Command make_add(lob::Side side, std::uint32_t order_id, int price, int quantity) {
    lob::Command cmd;
    cmd.type = lob::CMD_ADD;
    cmd.side = side;
    cmd.order_id = order_id;
    cmd.price = price;
    cmd.quantity = quantity;
    return cmd;
}

void reset_book() {
    lob::CommandResult result;
    lob::Command reset_cmd;
    reset_cmd.type = lob::CMD_RESET;
    reset_cmd.side = lob::BID;
    reset_cmd.order_id = 0;
    reset_cmd.price = 0;
    reset_cmd.quantity = 0;
    lob::lob_top(reset_cmd, result);
    expect(result.accepted, "reset should be accepted");
    expect(result.summary.bid_level_count == 0, "reset should clear bid levels");
    expect(result.summary.ask_level_count == 0, "reset should clear ask levels");
}

void run_stage1_regression_tests() {
    lob::CommandResult result;

    reset_book();

    lob::lob_top(make_add(lob::BID, 1, 100, 10), result);
    expect(result.accepted, "first bid add should be accepted");
    expect(result.code == lob::RES_ACCEPTED, "resting bid should return accepted");
    expect(result.summary.best_bid_price == 100, "best bid should be 100");
    expect(result.summary.best_bid_quantity == 10, "best bid quantity should be 10");
    expect(result.summary.bid_level_count == 1, "there should be one bid level");

    lob::lob_top(make_add(lob::BID, 2, 99, 5), result);
    expect(result.accepted, "second bid add should be accepted");
    expect(result.summary.best_bid_price == 100, "lower bid must not replace best bid");
    expect(result.summary.bid_level_count == 2, "there should be two bid levels");

    lob::lob_top(make_add(lob::BID, 3, 101, 7), result);
    expect(result.accepted, "better priced bid should be accepted");
    expect(result.summary.best_bid_price == 101, "higher bid should become best bid");
    expect(result.summary.best_bid_quantity == 7, "best bid quantity should match inserted order");
    expect(result.summary.bid_level_count == 3, "there should be three bid levels");

    lob::lob_top(make_add(lob::ASK, 10, 105, 4), result);
    expect(result.accepted, "first ask add should be accepted");
    expect(result.summary.best_ask_price == 105, "best ask should be 105");
    expect(result.summary.best_ask_quantity == 4, "best ask quantity should be 4");
    expect(result.summary.ask_level_count == 1, "there should be one ask level");

    lob::lob_top(make_add(lob::ASK, 11, 103, 6), result);
    expect(result.accepted, "better ask should be accepted");
    expect(result.summary.best_ask_price == 103, "lower ask should become best ask");
    expect(result.summary.best_ask_quantity == 6, "best ask quantity should be 6");
    expect(result.summary.ask_level_count == 2, "there should be two ask levels");

    lob::lob_top(make_add(lob::BID, 4, 100, 3), result);
    expect(result.accepted, "same-price bid should be accepted");
    expect(result.touched_price == 100, "touched price level should be 100");
    expect(result.touched_total_quantity == 13, "same-price bid should aggregate quantity");
    expect(result.touched_order_count == 2, "same-price level should now contain two orders");
    expect(result.summary.best_bid_price == 101, "best bid should remain 101");
}

void run_full_fill_test() {
    lob::CommandResult result;

    reset_book();

    lob::lob_top(make_add(lob::ASK, 100, 105, 4), result);
    lob::lob_top(make_add(lob::ASK, 101, 106, 6), result);

    lob::lob_top(make_add(lob::BID, 200, 106, 10), result);
    expect(result.accepted, "crossing bid should be accepted");
    expect(result.code == lob::RES_MATCHED, "full aggressive fill should report matched");
    expect(result.executed_quantity == 10, "full fill should execute all incoming quantity");
    expect(result.remaining_quantity == 0, "full fill should leave no residual quantity");
    expect(result.trade_count == 2, "full fill should consume two resting orders");
    expect(result.last_trade_price == 106, "last trade price should equal final resting level");
    expect(result.summary.best_ask_price == lob::INVALID_PRICE, "all asks should be consumed");
    expect(result.summary.ask_level_count == 0, "ask book should be empty after full fill");
}

void run_partial_fill_and_rest_test() {
    lob::CommandResult result;

    reset_book();

    lob::lob_top(make_add(lob::ASK, 300, 105, 5), result);
    lob::lob_top(make_add(lob::ASK, 301, 107, 3), result);

    lob::lob_top(make_add(lob::BID, 400, 106, 8), result);
    expect(result.accepted, "partially marketable bid should be accepted");
    expect(result.code == lob::RES_MATCHED_AND_RESTED, "partial match plus residual rest should be reported");
    expect(result.executed_quantity == 5, "only crossed liquidity should execute");
    expect(result.remaining_quantity == 0, "residual should be rested, not left pending");
    expect(result.trade_count == 1, "only one resting order should trade");
    expect(result.last_trade_price == 105, "trade should occur at resting ask price");
    expect(result.summary.best_bid_price == 106, "residual bid should rest at its limit price");
    expect(result.summary.best_bid_quantity == 3, "resting residual should keep remaining quantity");
    expect(result.summary.best_ask_price == 107, "uncrossed ask should remain");
}

void run_fifo_within_level_test() {
    lob::CommandResult result;

    reset_book();

    lob::lob_top(make_add(lob::ASK, 500, 105, 2), result);
    lob::lob_top(make_add(lob::ASK, 501, 105, 4), result);

    lob::lob_top(make_add(lob::BID, 600, 105, 3), result);
    expect(result.accepted, "same-price crossing bid should be accepted");
    expect(result.code == lob::RES_MATCHED, "same-price match should report matched");
    expect(result.executed_quantity == 3, "incoming quantity should execute");
    expect(result.trade_count == 2, "fill should consume the first order then part of the second");
    expect(result.last_trade_price == 105, "trade price should remain on the matched level");
    expect(result.summary.best_ask_price == 105, "partially consumed ask level should remain");
    expect(result.summary.best_ask_quantity == 3, "remaining ask quantity should stay on the level");
    expect(result.summary.ask_level_count == 1, "same-price queue should remain one level");
}

}  // namespace

int main() {
    run_stage1_regression_tests();
    run_full_fill_test();
    run_partial_fill_and_rest_test();
    run_fifo_within_level_test();
    std::cout << "Stage 2 testbench passed.\n";
    return 0;
}
