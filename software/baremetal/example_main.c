#include "xil_printf.h"
#include "xparameters.h"

#include "lob_axi.h"

#ifndef XPAR_LOB_AXI_PERIPHERAL_0_S_AXI_CTRL_BASEADDR
#define XPAR_LOB_AXI_PERIPHERAL_0_S_AXI_CTRL_BASEADDR XPAR_LOB_AXI_PERIPHERAL_0_BASEADDR
#endif

static void print_result(const char *label, const LobAxiResult *result) {
    xil_printf("%s\r\n", label);
    xil_printf("  accepted=%lu code=%lu\r\n", (unsigned long)result->accepted,
               (unsigned long)result->result_code);
    xil_printf("  executed=%ld cancelled=%ld remaining=%ld trade_count=%lu\r\n",
               (long)result->executed_quantity, (long)result->cancelled_quantity,
               (long)result->remaining_quantity, (unsigned long)result->trade_count);
    xil_printf("  best_bid=(%ld,%ld) best_ask=(%ld,%ld)\r\n",
               (long)result->best_bid_price, (long)result->best_bid_quantity,
               (long)result->best_ask_price, (long)result->best_ask_quantity);
}

int main() {
    LobAxi ip;
    LobAxiResult result;

    LobAxi_Initialize(&ip, XPAR_LOB_AXI_PERIPHERAL_0_S_AXI_CTRL_BASEADDR);

    xil_printf("LOB AXI-Lite bare-metal example\r\n");

    LobAxi_Reset(&ip, &result);
    print_result("After reset", &result);

    LobAxi_Add(&ip, LOB_SIDE_BID, 1U, 100, 10, &result);
    print_result("After add bid id=1 px=100 qty=10", &result);

    LobAxi_Add(&ip, LOB_SIDE_ASK, 2U, 105, 6, &result);
    print_result("After add ask id=2 px=105 qty=6", &result);

    LobAxi_Add(&ip, LOB_SIDE_BID, 3U, 105, 4, &result);
    print_result("After crossing bid id=3 px=105 qty=4", &result);

    LobAxi_Cancel(&ip, 1U, &result);
    print_result("After cancel id=1", &result);

    while (1) {
    }

    return 0;
}
