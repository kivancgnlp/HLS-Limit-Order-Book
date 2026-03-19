#include "lob_axi.h"

static inline void LobAxi_WriteReg(LobAxi *instance, u32 offset, u32 value) {
    Xil_Out32(instance->base_addr + offset, value);
}

static inline u32 LobAxi_ReadReg(LobAxi *instance, u32 offset) {
    return Xil_In32(instance->base_addr + offset);
}

void LobAxi_Initialize(LobAxi *instance, UINTPTR base_addr) {
    instance->base_addr = base_addr;
}

void LobAxi_SetAutoRestart(LobAxi *instance, u32 enable) {
    u32 value = LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_AP_CTRL);
    if (enable != 0U) {
        value |= LOB_AXI_AP_AUTO_RESTART_MASK;
    } else {
        value &= ~LOB_AXI_AP_AUTO_RESTART_MASK;
    }
    LobAxi_WriteReg(instance, LOB_AXI_CTRL_ADDR_AP_CTRL, value);
}

u32 LobAxi_IsDone(LobAxi *instance) {
    return (LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_AP_CTRL) & LOB_AXI_AP_DONE_MASK) != 0U;
}

u32 LobAxi_IsIdle(LobAxi *instance) {
    return (LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_AP_CTRL) & LOB_AXI_AP_IDLE_MASK) != 0U;
}

static void LobAxi_Start(LobAxi *instance) {
    u32 control = LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_AP_CTRL);
    LobAxi_WriteReg(instance, LOB_AXI_CTRL_ADDR_AP_CTRL,
                    (control & LOB_AXI_AP_AUTO_RESTART_MASK) | LOB_AXI_AP_START_MASK);
}

static void LobAxi_WaitForDone(LobAxi *instance) {
    while (LobAxi_IsDone(instance) == 0U) {
    }
}

static void LobAxi_LoadInputs(LobAxi *instance, const LobAxiCommand *cmd) {
    LobAxi_WriteReg(instance, LOB_AXI_CTRL_ADDR_CMD_TYPE_DATA, cmd->cmd_type);
    LobAxi_WriteReg(instance, LOB_AXI_CTRL_ADDR_SIDE_DATA, cmd->side);
    LobAxi_WriteReg(instance, LOB_AXI_CTRL_ADDR_ORDER_ID_DATA, cmd->order_id);
    LobAxi_WriteReg(instance, LOB_AXI_CTRL_ADDR_PRICE_DATA, (u32)cmd->price);
    LobAxi_WriteReg(instance, LOB_AXI_CTRL_ADDR_QUANTITY_DATA, (u32)cmd->quantity);
}

static void LobAxi_ReadOutputs(LobAxi *instance, LobAxiResult *result) {
    result->accepted = LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_ACCEPTED_DATA);
    result->result_code = LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_RESULT_CODE_DATA);
    result->touched_price = (s32)LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_TOUCHED_PRICE_DATA);
    result->touched_total_quantity =
        (s32)LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_TOUCHED_TOTAL_QUANTITY_DATA);
    result->touched_order_count =
        LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_TOUCHED_ORDER_COUNT_DATA);
    result->executed_quantity =
        (s32)LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_EXECUTED_QUANTITY_DATA);
    result->cancelled_quantity =
        (s32)LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_CANCELLED_QUANTITY_DATA);
    result->remaining_quantity =
        (s32)LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_REMAINING_QUANTITY_DATA);
    result->last_trade_price =
        (s32)LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_LAST_TRADE_PRICE_DATA);
    result->trade_count = LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_TRADE_COUNT_DATA);
    result->best_bid_price = (s32)LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_BEST_BID_PRICE_DATA);
    result->best_bid_quantity =
        (s32)LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_BEST_BID_QUANTITY_DATA);
    result->best_ask_price = (s32)LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_BEST_ASK_PRICE_DATA);
    result->best_ask_quantity =
        (s32)LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_BEST_ASK_QUANTITY_DATA);
    result->bid_level_count = LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_BID_LEVEL_COUNT_DATA);
    result->ask_level_count = LobAxi_ReadReg(instance, LOB_AXI_CTRL_ADDR_ASK_LEVEL_COUNT_DATA);
}

int LobAxi_ExecuteCommand(LobAxi *instance, const LobAxiCommand *cmd, LobAxiResult *result) {
    if (instance == 0 || cmd == 0 || result == 0) {
        return -1;
    }

    LobAxi_LoadInputs(instance, cmd);
    LobAxi_Start(instance);
    LobAxi_WaitForDone(instance);
    LobAxi_ReadOutputs(instance, result);
    return 0;
}

int LobAxi_Reset(LobAxi *instance, LobAxiResult *result) {
    LobAxiCommand cmd;
    cmd.cmd_type = LOB_CMD_RESET;
    cmd.side = LOB_SIDE_BID;
    cmd.order_id = 0U;
    cmd.price = 0;
    cmd.quantity = 0;
    return LobAxi_ExecuteCommand(instance, &cmd, result);
}

int LobAxi_Add(LobAxi *instance, u32 side, u32 order_id, s32 price, s32 quantity, LobAxiResult *result) {
    LobAxiCommand cmd;
    cmd.cmd_type = LOB_CMD_ADD;
    cmd.side = side;
    cmd.order_id = order_id;
    cmd.price = price;
    cmd.quantity = quantity;
    return LobAxi_ExecuteCommand(instance, &cmd, result);
}

int LobAxi_Cancel(LobAxi *instance, u32 order_id, LobAxiResult *result) {
    LobAxiCommand cmd;
    cmd.cmd_type = LOB_CMD_CANCEL;
    cmd.side = LOB_SIDE_BID;
    cmd.order_id = order_id;
    cmd.price = 0;
    cmd.quantity = 0;
    return LobAxi_ExecuteCommand(instance, &cmd, result);
}
