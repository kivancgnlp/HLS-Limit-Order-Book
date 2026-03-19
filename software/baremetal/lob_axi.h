#ifndef LOB_AXI_H
#define LOB_AXI_H

#include "xil_io.h"
#include "xil_types.h"

#include "lob_axi_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOB_SIDE_BID = 0,
    LOB_SIDE_ASK = 1
} LobAxiSide;

typedef enum {
    LOB_CMD_NOP = 0,
    LOB_CMD_RESET = 1,
    LOB_CMD_ADD = 2,
    LOB_CMD_CANCEL = 3
} LobAxiCommandType;

typedef enum {
    LOB_RES_NONE = 0,
    LOB_RES_ACCEPTED = 1,
    LOB_RES_MATCHED = 2,
    LOB_RES_MATCHED_AND_RESTED = 3,
    LOB_RES_CANCELLED = 4,
    LOB_RES_REJECTED_BAD_PRICE = 5,
    LOB_RES_REJECTED_BAD_QUANTITY = 6,
    LOB_RES_REJECTED_BOOK_FULL = 7,
    LOB_RES_REJECTED_LEVEL_FULL = 8,
    LOB_RES_REJECTED_DUPLICATE_ID = 9,
    LOB_RES_REJECTED_NOT_FOUND = 10,
    LOB_RES_UNSUPPORTED = 11
} LobAxiResultCode;

typedef struct {
    u32 cmd_type;
    u32 side;
    u32 order_id;
    s32 price;
    s32 quantity;
} LobAxiCommand;

typedef struct {
    u32 accepted;
    u32 result_code;
    s32 touched_price;
    s32 touched_total_quantity;
    u32 touched_order_count;
    s32 executed_quantity;
    s32 cancelled_quantity;
    s32 remaining_quantity;
    s32 last_trade_price;
    u32 trade_count;
    s32 best_bid_price;
    s32 best_bid_quantity;
    s32 best_ask_price;
    s32 best_ask_quantity;
    u32 bid_level_count;
    u32 ask_level_count;
} LobAxiResult;

typedef struct {
    UINTPTR base_addr;
} LobAxi;

void LobAxi_Initialize(LobAxi *instance, UINTPTR base_addr);
void LobAxi_SetAutoRestart(LobAxi *instance, u32 enable);
u32 LobAxi_IsDone(LobAxi *instance);
u32 LobAxi_IsIdle(LobAxi *instance);
int LobAxi_ExecuteCommand(LobAxi *instance, const LobAxiCommand *cmd, LobAxiResult *result);
int LobAxi_Reset(LobAxi *instance, LobAxiResult *result);
int LobAxi_Add(LobAxi *instance, u32 side, u32 order_id, s32 price, s32 quantity, LobAxiResult *result);
int LobAxi_Cancel(LobAxi *instance, u32 order_id, LobAxiResult *result);

#ifdef __cplusplus
}
#endif

#endif
