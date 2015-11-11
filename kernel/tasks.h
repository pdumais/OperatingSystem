// We cant use bit 63 because it is used to return invalid task addresses (set to -1)
#define TASK_ENABLE_BIT 62
#define TASK_RUNNING_BIT 61
#define TASK_DEAD_BIT 60
#define CLEANCR3ADDRESS(x) \
    shl $8,x; \
    shr $(8+12),x; \
    shl $12,x

// We might want to only clear control flags in a register 
// before loading cr3. The lower 12bits represent the PCID
// so We must not clear it.
#define STRIPCONTROLFROMCR3(x) \
    shl $8,x; \
    shr $8,x
