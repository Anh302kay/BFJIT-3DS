#pragma once
#include <3ds.h>

void ctr_clear_cache();
int _SetMemoryPermission(void *buffer, int size, int permission);
int _InitializeSvcHack(void);
void ctr_flush_invalidate_cache(void);
