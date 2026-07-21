#include <libc/core.h>
#include <Am/Lang/Diagnostics/Arc.h>
#include <libc/Am/Lang/Diagnostics/Arc.h>
#include <Am/Lang/Object.h>
#include <libc/core_inline_functions.h>

#include <stdio.h>
#ifdef __linux__
#include <unistd.h>
#endif

// Diagnostic counters defined in core.c.
extern __amlc_atomic_int __allocation_count; // live objects (allocs - deallocs); atomic under BRC
extern long __wrapper_create_count;   // cross-thread wrappers created
extern long __wrapper_dealloc_count;  // cross-thread wrappers freed

function_result Am_Lang_Diagnostics_Arc__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_Lang_Diagnostics_Arc__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_Lang_Diagnostics_Arc__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_Lang_Diagnostics_Arc_printAllocatedObjects_0()
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	print_allocated_objects();
__exit: ;
	return __result;
};

// Total number of objects ever allocated (monotonic; never decreases).
function_result Am_Lang_Diagnostics_Arc_getAllocationCount_0()
{
	function_result __result = { .has_return_value = true };
	__result.return_value.value.long_value = (long) __allocation_count;
	return __result;
};

// Number of cross-thread wrappers currently alive (created - freed). If this
// keeps climbing, wrappers are leaking.
function_result Am_Lang_Diagnostics_Arc_getLiveWrapperCount_0()
{
	function_result __result = { .has_return_value = true };
	__result.return_value.value.long_value = __wrapper_create_count - __wrapper_dealloc_count;
	return __result;
};

// Current process resident set size in KiB (Linux /proc/self/statm; 0 where
// unavailable). Real memory as seen by the OS.
function_result Am_Lang_Diagnostics_Arc_getResidentMemoryKb_0()
{
	function_result __result = { .has_return_value = true };
	long kb = 0;
#ifdef __linux__
	FILE *f = fopen("/proc/self/statm", "r");
	if (f != NULL) {
		unsigned long total_pages = 0, resident_pages = 0;
		if (fscanf(f, "%lu %lu", &total_pages, &resident_pages) == 2) {
			long page = sysconf(_SC_PAGESIZE);
			if (page <= 0) page = 4096;
			kb = (long) ((unsigned long long) resident_pages * (unsigned long long) page / 1024ULL);
		}
		fclose(f);
	}
#endif
	__result.return_value.value.long_value = kb;
	return __result;
};

