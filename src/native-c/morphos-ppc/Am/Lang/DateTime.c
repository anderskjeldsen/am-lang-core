#include <libc/core.h>
#include <Am/Lang/DateTime.h>
#include <morphos-ppc/Am/Lang/DateTime.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/ULong.h>
#include <Am/Lang/Int.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Bool.h>
#include <Am/Lang/UInt.h>
#include <libc/core_inline_functions.h>

#include <morphos-ppc/morphos.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#define UNIX_EPOCH_SECS_PER_DAY 86400ULL

static int is_leap(int year)
{
	return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static int days_in_year(int year)
{
	return is_leap(year) ? 366 : 365;
}

static int days_in_month_for(int year, int month)
{
	static const int dim[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (month < 1 || month > 12) return 30;
	if (month == 2 && is_leap(year)) return 29;
	return dim[month - 1];
}

// Broken-down representation in UTC. Year is the full year (e.g. 2026).
// dayOfWeek: 0 = Sunday … 6 = Saturday — same convention as struct tm.tm_wday.
typedef struct {
	int year, month, day, hour, minute, second, dayOfWeek;
} broken_down_t;

// Decompose Unix epoch milliseconds (+ tz offset minutes) into broken-down time.
// tzOffsetMinutes is added to UTC seconds before breakdown so the caller sees
// fields in the chosen timezone.
static void decompose(unsigned long long epochMillis, int tzOffsetMinutes,
                      broken_down_t *out)
{
	long long total_secs = (long long)(epochMillis / 1000ULL)
	                     + (long long) tzOffsetMinutes * 60LL;
	if (total_secs < 0) total_secs = 0;

	long long days = total_secs / 86400LL;
	int secs_today = (int)(total_secs - days * 86400LL);

	out->hour = secs_today / 3600;
	out->minute = (secs_today % 3600) / 60;
	out->second = secs_today % 60;

	// Walk forward from 1970-01-01.
	int year = 1970;
	long long remaining = days;
	while (remaining >= days_in_year(year)) {
		remaining -= days_in_year(year);
		year++;
	}
	int month = 1;
	while (remaining >= days_in_month_for(year, month)) {
		remaining -= days_in_month_for(year, month);
		month++;
	}
	out->year = year;
	out->month = month;
	out->day = (int) remaining + 1;

	// 1970-01-01 was a Thursday (= 4).
	out->dayOfWeek = (int)((4 + days % 7 + 7) % 7);
}

// Compose Unix epoch seconds (UTC) from broken-down fields. Treats input as
// being in the timezone identified by tzOffsetMinutes; subtracts that offset
// to produce UTC.
static long long compose_unix_secs(int year, int month, int day,
                                    int hour, int minute, int second,
                                    int tzOffsetMinutes)
{
	if (year < 1970) return 0;
	long long days = 0;
	for (int y = 1970; y < year; y++) {
		days += days_in_year(y);
	}
	for (int m = 1; m < month; m++) {
		days += days_in_month_for(year, m);
	}
	days += (day - 1);
	long long secs = days * 86400LL
	               + (long long) hour * 3600LL
	               + (long long) minute * 60LL
	               + (long long) second
	               - (long long) tzOffsetMinutes * 60LL;
	return secs < 0 ? 0 : secs;
}

// Standard object lifecycle methods

function_result Am_Lang_DateTime__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_DateTime__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_DateTime__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

// Current time in milliseconds since Unix epoch (1970-01-01 UTC).
// MorphOS epoch is 1978-01-01; offset is 8 years + 2 leap days = 252,460,800 s.
function_result Am_Lang_DateTime_getMillis_0()
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	struct DateStamp ds;
	DateStamp(&ds);

	unsigned long long morphos_seconds =
		  (unsigned long long) ds.ds_Days * 86400ULL
		+ (unsigned long long) ds.ds_Minute * 60ULL
		+ (unsigned long long) ds.ds_Tick / (unsigned long long) TICKS_PER_SECOND;
	unsigned long long ms_remainder =
		((unsigned long long)(ds.ds_Tick % TICKS_PER_SECOND) * 1000ULL)
		/ (unsigned long long) TICKS_PER_SECOND;

	__result.return_value.value.ulong_value =
		(morphos_seconds + 252460800ULL) * 1000ULL + ms_remainder;

__exit: ;
	return __result;
}

function_result Am_Lang_DateTime_getLocalTimezoneOffset_0()
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value.value.int_value = 0;
__exit: ;
	return __result;
}

function_result Am_Lang_DateTime_toEpochMillis_0(int year, int month, int day,
                                                  int hour, int minute, int second,
                                                  int timezoneOffsetMinutes)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	long long unix_secs = compose_unix_secs(year, month, day, hour, minute, second, timezoneOffsetMinutes);
	__result.return_value.value.ulong_value = (unsigned long long) unix_secs * 1000ULL;

__exit: ;
	return __result;
}

function_result Am_Lang_DateTime_extractYear_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	broken_down_t bd;
	decompose(epochMillis, timezoneOffsetMinutes, &bd);
	__result.return_value.value.int_value = bd.year;
__exit: ;
	return __result;
}

function_result Am_Lang_DateTime_extractMonth_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	broken_down_t bd;
	decompose(epochMillis, timezoneOffsetMinutes, &bd);
	__result.return_value.value.int_value = bd.month;
__exit: ;
	return __result;
}

function_result Am_Lang_DateTime_extractDay_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	broken_down_t bd;
	decompose(epochMillis, timezoneOffsetMinutes, &bd);
	__result.return_value.value.int_value = bd.day;
__exit: ;
	return __result;
}

function_result Am_Lang_DateTime_extractHour_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	broken_down_t bd;
	decompose(epochMillis, timezoneOffsetMinutes, &bd);
	__result.return_value.value.int_value = bd.hour;
__exit: ;
	return __result;
}

function_result Am_Lang_DateTime_extractMinute_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	broken_down_t bd;
	decompose(epochMillis, timezoneOffsetMinutes, &bd);
	__result.return_value.value.int_value = bd.minute;
__exit: ;
	return __result;
}

function_result Am_Lang_DateTime_extractSecond_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	broken_down_t bd;
	decompose(epochMillis, timezoneOffsetMinutes, &bd);
	__result.return_value.value.int_value = bd.second;
__exit: ;
	return __result;
}

function_result Am_Lang_DateTime_extractMillisecond_0(unsigned long long epochMillis)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value.value.int_value = (int)(epochMillis % 1000ULL);
__exit: ;
	return __result;
}

function_result Am_Lang_DateTime_extractDayOfWeek_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	broken_down_t bd;
	decompose(epochMillis, timezoneOffsetMinutes, &bd);
	__result.return_value.value.int_value = bd.dayOfWeek;
__exit: ;
	return __result;
}

// Write a zero-padded decimal of `value` into `buf` (no NUL).
static void write_padded(char *buf, int value, int width)
{
	if (value < 0) value = -value;
	for (int i = width - 1; i >= 0; i--) {
		buf[i] = (char)('0' + (value % 10));
		value /= 10;
	}
}

// In-place pattern substitution of fixed-width tokens. Each token is replaced
// once, with a zero-padded numeric value of the same width. Avoids snprintf so
// the MorphOS build doesn't need to drag in stdio formatting. Comment: stdio is already dragged in.
static void replace_token(char *str, const char *token, int token_len, int value)
{
	for (char *p = str; *p != 0; p++) {
		int matched = 1;
		for (int i = 0; i < token_len; i++) {
			if (p[i] != token[i]) { matched = 0; break; }
		}
		if (matched) {
			write_padded(p, value, token_len);
			return;
		}
	}
}

function_result Am_Lang_DateTime_formatDateTime_0(unsigned long long epochMillis,
                                                   int timezoneOffsetMinutes,
                                                   aobject *pattern)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	broken_down_t bd;
	decompose(epochMillis, timezoneOffsetMinutes, &bd);

	string_holder *pat_holder = (string_holder *) (pattern + 1);
	const char *pat_str = pat_holder->string_value;

	char formatted[256];
	int i = 0;
	while (i < (int)(sizeof(formatted) - 1) && pat_str[i] != 0) {
		formatted[i] = pat_str[i];
		i++;
	}
	formatted[i] = 0;

	replace_token(formatted, "yyyy", 4, bd.year);
	replace_token(formatted, "MM",   2, bd.month);
	replace_token(formatted, "dd",   2, bd.day);
	replace_token(formatted, "HH",   2, bd.hour);
	replace_token(formatted, "mm",   2, bd.minute);
	replace_token(formatted, "ss",   2, bd.second);

	__result.return_value.value.object_value = __create_string(formatted, &Am_Lang_String);

__exit: ;
	return __result;
}

// Read up to `width` consecutive digits starting at *pos, advance *pos past them.
static int parse_int(const char *str, int *pos, int max_width)
{
	int v = 0;
	int read = 0;
	while (read < max_width && str[*pos] >= '0' && str[*pos] <= '9') {
		v = v * 10 + (str[*pos] - '0');
		(*pos)++;
		read++;
	}
	return v;
}

// Minimal "yyyy-MM-dd[ HH:mm:ss]" / "yyyy-MM-ddTHH:mm:ss" parser. Treats input
// as UTC. Anything that doesn't match returns 0.
function_result Am_Lang_DateTime_parseDateTime_0(aobject *dateString, aobject *pattern)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *date_holder = (string_holder *) (dateString + 1);
	const char *date_str = date_holder->string_value;

	int pos = 0;
	int year = parse_int(date_str, &pos, 4);
	if (date_str[pos] == '-') pos++;
	int month = parse_int(date_str, &pos, 2);
	if (date_str[pos] == '-') pos++;
	int day = parse_int(date_str, &pos, 2);
	int hour = 0, minute = 0, second = 0;
	if (date_str[pos] == ' ' || date_str[pos] == 'T') {
		pos++;
		hour = parse_int(date_str, &pos, 2);
		if (date_str[pos] == ':') pos++;
		minute = parse_int(date_str, &pos, 2);
		if (date_str[pos] == ':') pos++;
		second = parse_int(date_str, &pos, 2);
	}

	long long unix_secs = compose_unix_secs(year, month, day, hour, minute, second, 0);
	__result.return_value.value.ulong_value = (unsigned long long) unix_secs * 1000ULL;

__exit: ;
	return __result;
}

function_result Am_Lang_DateTime_checkLeapYear_0(int year)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value.value.bool_value = is_leap(year) ? true : false;
__exit: ;
	return __result;
}

function_result Am_Lang_DateTime_calculateDaysInMonth_0(int year, int month)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value.value.int_value = days_in_month_for(year, month);
__exit: ;
	return __result;
}
