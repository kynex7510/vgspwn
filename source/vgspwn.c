#include "vgspwn.h"

#include <string.h>

extern void* gspSharedMem; // Needs custom libctru

static Thread g_Thread = NULL;
static u8 g_Target = -1;

static LightLock g_TriggerLock;
static CondVar g_TriggerCV;
static bool g_Exit = false;
static bool g_Trigger = false;

static LightLock g_DoneLock;
static CondVar g_DoneCV;
static bool g_Done = false;

#define CmdQueue(id) (u32*)(((u8*)gspSharedMem) + 0x800 + ((id) * 0x200))

static void addCommand(u32* q, const u32* cmd) {
    u32 index = (*q >> 8) & 0xFF;
    u32* dst = &q[8 * (index + 1)];
    memcpy(dst, cmd, 8 * sizeof(u32));
    *q = (*q & 0xFFFF00FF) | ((index + 1) << 8);
}

static void threadMain(void*) {
    LightLock_Lock(&g_TriggerLock);

    while (true) {
        // Wait for incoming commands.
        while (!g_Exit && !g_Trigger)
            CondVar_Wait(&g_TriggerCV, &g_TriggerLock);

        if (g_Exit)
            break;

        g_Trigger = false;

        // Wait for app switch.
        svcSleepThread(500 * 1000 * 1000);

        // Trigger command processing on the target process.
        GSPGPU_TriggerCmdReqQueue();
        svcSleepThread(5 * 1000 * 1000);

        // Cleanup shared memory to consistent state.
        do {
            __ldrex((s32*)CmdQueue(g_Target));
        } while (__strex((s32*)CmdQueue(g_Target), 0));

        // Signal that we're done.
        LightLock_Lock(&g_DoneLock);
        g_Done = true;
        LightLock_Unlock(&g_DoneLock);
        CondVar_Signal(&g_DoneCV);
    }

    LightLock_Unlock(&g_TriggerLock);
}

void vgspwnInit(void) {
    if (R_FAILED(gspInit()))
        svcBreak(USERBREAK_PANIC);
 
    LightLock_Init(&g_TriggerLock);
    LightLock_Init(&g_DoneLock);
    CondVar_Init(&g_DoneCV);
    CondVar_Init(&g_TriggerCV);

    g_Exit = false;
    g_Trigger = false;
    g_Done = false;

    g_Thread = threadCreate(threadMain, NULL, 0x1000, 0x18, -2, true);
    if (!g_Thread)
        svcBreak(USERBREAK_PANIC);
}

void vgspwnExit(void) {
    LightLock_Lock(&g_TriggerLock);
    g_Exit = true;
    LightLock_Unlock(&g_TriggerLock);

    CondVar_Signal(&g_TriggerCV);

    threadJoin(g_Thread, U64_MAX);

    g_Thread = NULL;

    gspExit();
}

void vgspwnSelect(u8 t) {
    g_Target = t;

    u32* q = CmdQueue(t);

    // Avoid race conditions.
    do {
        __ldrex((s32*)q);
    } while (__strex((s32*)q, 0xFFFFFFFF));

    q[0] = q[1] = 0;
}

void vgspwnAddTransfer(u32 src, u32 dst, size_t size) {
    const u32 dmaReq[8] = {
        0x0,
        src,
        dst,
        size,
        0,
        0,
        0,
        1
    };

    addCommand(CmdQueue(g_Target), dmaReq);
}

void vgspwnCommit(void) {
    GSPGPU_FlushDataCache(CmdQueue(g_Target), 0x200);

    // Trigger command processing.
    LightLock_Lock(&g_TriggerLock);
    g_Trigger = true;
    LightLock_Unlock(&g_TriggerLock);

    CondVar_Signal(&g_TriggerCV);

    // Switch app.
    aptJumpToHomeMenu();

    // Wait for completion.
    LightLock_Lock(&g_DoneLock);

    while (!g_Done)
        CondVar_Wait(&g_DoneCV, &g_DoneLock);

    g_Done = false;
    LightLock_Unlock(&g_DoneLock);
}