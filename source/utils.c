#include <stdio.h>
#include <3ds.h>

#include "utils.h"

// code taken from dosbox 3DS and picodrive 3DS

static unsigned int s1, s2, s3, s0;

typedef s32 (*ctr_callback_type)(void);

extern void ctr_clear_cache();

// I don't think the rosalina checks are necessary and this example has it defaulted to the rosalina being true
// and it still works on fine on my new 3DS
static int has_rosalina;

static void check_rosalina(void) {
  int64_t version;
  uint32_t major;

  has_rosalina = 0;

  if (!svcGetSystemInfo(&version, 0x10000, 0)) {
     major = GET_VERSION_MAJOR(version);

     if (major >= 8)
       has_rosalina = 1;
  }
}

static void ctrEnableAllServices(void)
{
	__asm__ volatile("cpsid aif");

	unsigned int *svc_access_control = *(*(unsigned int***)0xFFFF9000 + 0x22) - 0x6;

	s0 = svc_access_control[0];
	s1 = svc_access_control[1];
	s2 = svc_access_control[2];
	s3 = svc_access_control[3];

	svc_access_control[0]=0xFFFFFFFE;
	svc_access_control[1]=0xFFFFFFFF;
	svc_access_control[2]=0xFFFFFFFF;
	svc_access_control[3]=0x3FFFFFFF;
}

//-----------------------------------------------------------------------------
// Sets the permissions for memory.
// You must ensutre that buffer and size are 0x1000-aligned.
//-----------------------------------------------------------------------------
int _SetMemoryPermission(void *buffer, int size, int permission)
{
	Handle currentHandle;
	svcDuplicateHandle(&currentHandle, 0xFFFF8001);
	int res = svcControlProcessMemory(currentHandle, (u32)buffer, 0, size, MEMOP_PROT, permission);
	svcCloseHandle(currentHandle);

	return res;
}

//-----------------------------------------------------------------------------
// Initializes the hack to gain kernel access to all services.
// The one that we really are interested is actually the
// svcControlProcessMemory, because once we have kernel access, we can
// grant read/write/execute access to memory blocks, and which means we
// can do dynamic recompilation.
//-----------------------------------------------------------------------------
int _InitializeSvcHack(void)
{
	svcBackdoor((ctr_callback_type)ctrEnableAllServices);
	svcBackdoor((ctr_callback_type)ctrEnableAllServices);

	if (s0 != 0xFFFFFFFE || s1 != 0xFFFFFFFF || s2 != 0xFFFFFFFF || s3 != 0x3FFFFFFF)
	{
			printf("svcHack failed: svcBackdoor unsuccessful.\n");
			return 0;
	}

	return 1;
}

static void ctr_clean_invalidate_kernel(void)
{
   __asm__ volatile(
      "mrs r1, cpsr\n"
      "cpsid aif\n"                  // disable interrupts
      "mov r0, #0\n"
      "mcr p15, 0, r0, c7, c10, 0\n" // clean dcache
      "mcr p15, 0, r0, c7, c10, 4\n" // DSB
      "mcr p15, 0, r0, c7, c5, 0\n"  // invalidate icache+BTAC
      "msr cpsr_cx, r1\n"            // restore interrupts
      ::: "r0", "r1");
}

void ctr_flush_invalidate_cache(void)
{
//   if (has_rosalina) {
    ctr_clear_cache();
//    } else {
//     svcBackdoor((ctr_callback_type)ctr_clean_invalidate_kernel);
//    }
}
