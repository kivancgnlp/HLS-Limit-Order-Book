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

lob::Command make_cancel(std::uint32_t order_id) {
    lob::Command cmd;
    cmd.type = lob::CMD_CANCEL;
    cmd.side = lob::BID;
    cmd.order_id = order_id;
    cmd.price = 0;
    cmd.quantity = 0;
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

void run_stage2_matching_tests() {
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

void run_stage3_cancel_tests() {
    lob::CommandResult result;

    reset_book();

    lob::lob_top(make_add(lob::BID, 700, 100, 3), result);
    lob::lob_top(make_add(lob::BID, 701, 100, 4), result);
    lob::lob_top(make_cancel(700), result);
    expect(result.accepted, "cancel of head order should be accepted");
    expect(result.code == lob::RES_CANCELLED, "cancel should return cancelled status");
    expect(result.cancelled_quantity == 3, "cancel should report removed quantity");
    expect(result.touched_price == 100, "cancel should touch the correct price level");
    expect(result.touched_total_quantity == 4, "remaining level quantity should be updated");
    expect(result.touched_order_count == 1, "remaining order count should be updated");
    expect(result.summary.best_bid_price == 100, "price level should remain after head cancel");
    expect(result.summary.best_bid_quantity == 4, "remaining order should become new queue head");

    lob::lob_top(make_cancel(701), result);
    expect(result.accepted, "cancel of last order should be accepted");
    expect(result.summary.best_bid_price == lob::INVALID_PRICE, "price level should disappear after last cancel");
    expect(result.summary.bid_level_count == 0, "bid book should be empty after final cancel");

    reset_book();

    lob::lob_top(make_add(lob::ASK, 800, 105, 2), result);
    lob::lob_top(make_add(lob::ASK, 801, 105, 5), result);
    lob::lob_top(make_cancel(801), result);
    expect(result.accepted, "cancel of non-head order should be accepted");
    expect(result.cancelled_quantity == 5, "non-head cancel should remove the target quantity");
    expect(result.summary.best_ask_price == 105, "ask level should remain after non-head cancel");
    expect(result.summary.best_ask_quantity == 2, "head order quantity should remain intact");

    reset_book();

    lob::lob_top(make_add(lob::ASK, 900, 105, 5), result);
    lob::lob_top(make_add(lob::BID, 901, 106, 8), result);
    expect(result.code == lob::RES_MATCHED_AND_RESTED, "residual should rest after partial match");
    lob::lob_top(make_cancel(901), result);
    expect(result.accepted, "rested residual should be cancellable by original order id");
    expect(result.cancelled_quantity == 3, "cancel should remove the residual quantity");
    expect(result.summary.best_bid_price == lob::INVALID_PRICE, "residual bid should be gone after cancel");

    lob::lob_top(make_cancel(123456), result);
    expect(!result.accepted, "cancel of unknown order should fail");
    expect(result.code == lob::RES_REJECTED_NOT_FOUND, "unknown cancel should report not found");
}

void run_duplicate_id_tests() {
    lob::CommandResult result;

    reset_book();

    lob::lob_top(make_add(lob::BID, 1000, 100, 4), result);
    expect(result.accepted, "first order with id 1000 should be accepted");

    lob::lob_top(make_add(lob::ASK, 1000, 105, 2), result);
    expect(!result.accepted, "duplicate active order id should be rejected");
    expect(result.code == lob::RES_REJECTED_DUPLICATE_ID, "duplicate active id should return duplicate error");

    lob::lob_top(make_cancel(1000), result);
    expect(result.accepted, "cancel should release the active id");

    lob::lob_top(make_add(lob::ASK, 1000, 105, 2), result);
    expect(result.accepted, "released order id should be reusable");
}

void run_immediate_match_id_reuse_test() {
    lob::CommandResult result;

    reset_book();

    lob::lob_top(make_add(lob::ASK, 1100, 105, 3), result);
    lob::lob_top(make_add(lob::BID, 1101, 105, 3), result);
    expect(result.code == lob::RES_MATCHED, "fully matched incoming order should not rest");

    lob::lob_top(make_cancel(1101), result);
    expect(!result.accepted, "fully matched incoming order should not be cancellable later");
    expect(result.code == lob::RES_REJECTED_NOT_FOUND, "fully matched order id should not remain in lookup");

    lob::lob_top(make_add(lob::BID, 1101, 100, 1), result);
    expect(result.accepted, "immediately matched order id should be reusable");
}

}  // namespace

int main() {
    run_stage1_regression_tests();
    run_stage2_matching_tests();
    run_stage3_cancel_tests();
    run_duplicate_id_tests();
    run_immediate_match_id_reuse_test();
    std::cout << "All staged testbench scenarios passed.\n";
    return 0;
}
