#include "../types.h"

struct _system_handle
{
    void (*destructor)(struct _system_handle*);
};

typedef struct _system_handle system_handle;

