#include "lob.hpp"

namespace lob {

extern "C" void lob_top(const Command &cmd, CommandResult &result) {
    static LimitOrderBook book;
    static bool initialized = false;

    if (!initialized) {
        // HLS note: keeping book state static makes this function behave like a
        // stateful kernel invocation stream rather than a pure software helper.
        reset_book(book);
        initialized = true;
    }

    // HLS note: interface pragmas are intentionally left as comments-only guidance
    // in this project stage; in Vitis HLS this wrapper can be adapted to AXI-Lite
    // or AXI-Stream control/data interfaces depending on integration goals.
    process_command(book, cmd, result);
}

}  // namespace lob
