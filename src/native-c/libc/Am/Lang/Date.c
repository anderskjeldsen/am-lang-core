#include <libc/core.h>
#include <Am/Lang/Date.h>
#include <libc/Am/Lang/Date.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/UInt.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Bool.h>
#include <Am/Lang/ULong.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_Date__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_Lang_Date__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_Lang_Date__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

#ifdef __MORPHOS__
// MorphOS PPC: ppc-morphos-gcc + -noixemul doesn't ship clock_gettime,
// so route through dos.library DateStamp() — same approach as the
// amigaos m68k port. Mirrors src/native-c/amigaos/Am/Lang/Date.c.
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/datetime.h>
#include <proto/dos.h>

// Unix epoch (1970-01-01) to Amiga epoch (1978-01-01): 2922 days.
#define AM_LANG_DATE_UNIX_TO_AMIGA_EPOCH_SECONDS 252460800ULL

function_result Am_Lang_Date_getMillis_0()
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	struct DateStamp ds;
	DateStamp(&ds);

	unsigned long long amiga_seconds =
		  (unsigned long long) ds.ds_Days * 86400ULL
		+ (unsigned long long) ds.ds_Minute * 60ULL
		+ (unsigned long long) ds.ds_Tick / (unsigned long long) TICKS_PER_SECOND;
	unsigned long long ms_remainder =
		((unsigned long long)(ds.ds_Tick % TICKS_PER_SECOND) * 1000ULL)
		/ (unsigned long long) TICKS_PER_SECOND;

	__result.return_value.value.ulong_value =
		(amiga_seconds + AM_LANG_DATE_UNIX_TO_AMIGA_EPOCH_SECONDS) * 1000ULL + ms_remainder;
__exit: ;
	return __result;
};
#else
function_result Am_Lang_Date_getMillis_0()
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

//	struct timeval tv;
	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);
//    gettimeofday(&tv, NULL);
    unsigned long long milliseconds = tv.tv_sec * 1000LL + tv.tv_nsec / 1000000;
	__result.return_value.value.ulong_value = milliseconds;
//	__result.return_value.flags = PRIMITIVE_ULONG;
__exit: ;
	return __result;
};
#endif

