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

void run_stage1_tests() {
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

    lob::lob_top(make_add(lob::BID, 1, 100, 10), result);
    expect(result.accepted, "first bid add should be accepted");
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

}  // namespace

int main() {
    run_stage1_tests();
    std::cout << "Stage 1 testbench passed.\n";
    return 0;
}
