#include <libc/core.h>
#include <Am/Lang/Date.h>
#include <amigaos/Am/Lang/Date.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/ULong.h>
#include <Am/Lang/UInt.h>
#include <Am/Lang/String.h>
#include <libc/core_inline_functions.h>

#include <amigaos/amiga.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/datetime.h>
#include <proto/exec.h>
#include <proto/dos.h>

// Seconds between Unix epoch (1970-01-01 UTC) and AmigaOS epoch (1978-01-01).
// 8 years, 2 leap days (1972, 1976) = 2922 days = 252,460,800 seconds.
#define UNIX_TO_AMIGA_EPOCH_SECONDS 252460800ULL

function_result Am_Lang_Date__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Date__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Date__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

// Current time in milliseconds since Unix epoch (1970-01-01 UTC).
// Source: dos.library DateStamp() — gives days/minutes/ticks since AmigaOS epoch
// (1978-01-01). AmigaOS has no built-in timezone, so DateStamp is treated as UTC.
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

	unsigned long long unix_ms =
		(amiga_seconds + UNIX_TO_AMIGA_EPOCH_SECONDS) * 1000ULL + ms_remainder;

	__result.return_value.value.ulong_value = unix_ms;

__exit: ;
	return __result;
}
