#include "lob.hpp"

namespace lob {

extern "C" void lob_top(const Command &cmd, CommandResult &result) {
    static LimitOrderBook book;
    static bool initialized = false;

    if (!initialized) {
        reset_book(book);
        initialized = true;
    }

    process_command(book, cmd, result);
}

}  // namespace lob
