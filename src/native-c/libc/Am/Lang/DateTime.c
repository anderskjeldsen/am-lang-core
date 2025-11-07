#include <libc/core.h>
#include <time.h>
#include <sys/time.h>
#include <Am/Lang/DateTime.h>
#include <libc/Am/Lang/DateTime.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/UInt.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Bool.h>
#include <Am/Lang/ULong.h>
#include <Am/Lang/Int.h>
#include <libc/core_inline_functions.h>

// External reference to the String class
extern aclass Am_Lang_String;

// Portable timegm implementation for platforms that don't have it (like AmigaOS)
#ifndef HAVE_TIMEGM
static time_t portable_timegm(struct tm *tm) {
    time_t ret;
    char *tz;
    
    tz = getenv("TZ");
    setenv("TZ", "", 1);
    tzset();
    ret = mktime(tm);
    if (tz)
        setenv("TZ", tz, 1);
    else
        unsetenv("TZ");
    tzset();
    return ret;
}
#define timegm portable_timegm
#endif

// Standard object lifecycle methods
function_result Am_Lang_DateTime__native_init_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    bool __returning = false;
    if (this != NULL) {
        __increase_reference_count(this);
    }
__exit: ;
    if (this != NULL) {
        __decrease_reference_count(this);
    }
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

// Get current time in milliseconds since epoch
function_result Am_Lang_DateTime_getMillis_0()
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long milliseconds = tv.tv_sec * 1000LL + tv.tv_usec / 1000;
    __result.return_value.value.ulong_value = milliseconds;

__exit: ;
    return __result;
}

// Get local timezone offset in minutes from UTC
function_result Am_Lang_DateTime_getLocalTimezoneOffset_0()
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    time_t now = time(NULL);
    struct tm* local_tm = localtime(&now);
    struct tm* utc_tm = gmtime(&now);
    
    // Calculate difference in seconds, then convert to minutes
    time_t local_time = mktime(local_tm);
    time_t utc_time = mktime(utc_tm);
    int offset_seconds = (int)(local_time - utc_time);
    int offset_minutes = offset_seconds / 60;
    
    __result.return_value.value.int_value = offset_minutes;

__exit: ;
    return __result;
}

// Helper function to convert epoch millis to tm struct with timezone adjustment
static struct tm epochMillisToTm(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
    time_t epoch_seconds = epochMillis / 1000;
    // Since we store UTC epoch time, just use gmtime directly
    return *gmtime(&epoch_seconds);
}

// Convert date/time components to epoch milliseconds
function_result Am_Lang_DateTime_toEpochMillis_0(int year, int month, int day, int hour, int minute, int second, int timezoneOffsetMinutes)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    struct tm tm_time = {0};
    tm_time.tm_year = year - 1900;  // years since 1900
    tm_time.tm_mon = month - 1;     // months since January (0-11)
    tm_time.tm_mday = day;
    tm_time.tm_hour = hour;
    tm_time.tm_min = minute;
    tm_time.tm_sec = second;
    tm_time.tm_isdst = -1;  // let system determine DST

    // Use timegm (with portable fallback) to get UTC time instead of mktime (which uses local time)
    time_t epoch_time = timegm(&tm_time);
    
    unsigned long long milliseconds = (unsigned long long)epoch_time * 1000LL;
    __result.return_value.value.ulong_value = milliseconds;

__exit: ;
    return __result;
}

// Extract year from epoch milliseconds
function_result Am_Lang_DateTime_extractYear_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    struct tm tm_time = epochMillisToTm(epochMillis, timezoneOffsetMinutes);
    __result.return_value.value.int_value = tm_time.tm_year + 1900;

__exit: ;
    return __result;
}

// Extract month from epoch milliseconds
function_result Am_Lang_DateTime_extractMonth_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    struct tm tm_time = epochMillisToTm(epochMillis, timezoneOffsetMinutes);
    __result.return_value.value.int_value = tm_time.tm_mon + 1;  // Convert from 0-11 to 1-12

__exit: ;
    return __result;
}

// Extract day from epoch milliseconds
function_result Am_Lang_DateTime_extractDay_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    struct tm tm_time = epochMillisToTm(epochMillis, timezoneOffsetMinutes);
    __result.return_value.value.int_value = tm_time.tm_mday;

__exit: ;
    return __result;
}

// Extract hour from epoch milliseconds
function_result Am_Lang_DateTime_extractHour_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    struct tm tm_time = epochMillisToTm(epochMillis, timezoneOffsetMinutes);
    __result.return_value.value.int_value = tm_time.tm_hour;

__exit: ;
    return __result;
}

// Extract minute from epoch milliseconds
function_result Am_Lang_DateTime_extractMinute_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    struct tm tm_time = epochMillisToTm(epochMillis, timezoneOffsetMinutes);
    __result.return_value.value.int_value = tm_time.tm_min;

__exit: ;
    return __result;
}

// Extract second from epoch milliseconds
function_result Am_Lang_DateTime_extractSecond_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    struct tm tm_time = epochMillisToTm(epochMillis, timezoneOffsetMinutes);
    __result.return_value.value.int_value = tm_time.tm_sec;

__exit: ;
    return __result;
}

// Extract millisecond component
function_result Am_Lang_DateTime_extractMillisecond_0(unsigned long long epochMillis)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    __result.return_value.value.int_value = (int)(epochMillis % 1000);

__exit: ;
    return __result;
}

// Extract day of week (0 = Sunday, 1 = Monday, etc.)
function_result Am_Lang_DateTime_extractDayOfWeek_0(unsigned long long epochMillis, int timezoneOffsetMinutes)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    struct tm tm_time = epochMillisToTm(epochMillis, timezoneOffsetMinutes);
    __result.return_value.value.int_value = tm_time.tm_wday;

__exit: ;
    return __result;
}

// Format date/time according to pattern
function_result Am_Lang_DateTime_formatDateTime_0(unsigned long long epochMillis, int timezoneOffsetMinutes, aobject* pattern)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    if (pattern != NULL) {
        __increase_reference_count(pattern);
    }

    struct tm tm_time = epochMillisToTm(epochMillis, timezoneOffsetMinutes);
    
    // Get pattern string
    string_holder *pattern_holder = pattern->object_properties.class_object_properties.object_data.value.custom_value;
    char* pattern_str = pattern_holder->string_value;
    char formatted[256];
    
    // Simple pattern replacements - extend as needed
    strncpy(formatted, pattern_str, sizeof(formatted) - 1);
    formatted[sizeof(formatted) - 1] = '\0';
    
    // Replace common patterns
    char temp[256];
    char year_str[5], month_str[3], day_str[3], hour_str[3], minute_str[3], second_str[3];
    
    snprintf(year_str, sizeof(year_str), "%04d", tm_time.tm_year + 1900);
    snprintf(month_str, sizeof(month_str), "%02d", tm_time.tm_mon + 1);
    snprintf(day_str, sizeof(day_str), "%02d", tm_time.tm_mday);
    snprintf(hour_str, sizeof(hour_str), "%02d", tm_time.tm_hour);
    snprintf(minute_str, sizeof(minute_str), "%02d", tm_time.tm_min);
    snprintf(second_str, sizeof(second_str), "%02d", tm_time.tm_sec);
    
    // Replace yyyy with year
    char* pos = strstr(formatted, "yyyy");
    if (pos != NULL) {
        size_t before_len = pos - formatted;
        size_t after_len = strlen(pos + 4);
        memmove(pos + 4, pos + 4, after_len + 1);
        memcpy(pos, year_str, 4);
    }
    
    // Replace MM with month
    pos = strstr(formatted, "MM");
    if (pos != NULL) {
        size_t before_len = pos - formatted;
        size_t after_len = strlen(pos + 2);
        memmove(pos + 2, pos + 2, after_len + 1);
        memcpy(pos, month_str, 2);
    }
    
    // Replace dd with day
    pos = strstr(formatted, "dd");
    if (pos != NULL) {
        size_t before_len = pos - formatted;
        size_t after_len = strlen(pos + 2);
        memmove(pos + 2, pos + 2, after_len + 1);
        memcpy(pos, day_str, 2);
    }
    
    // Replace HH with hour
    pos = strstr(formatted, "HH");
    if (pos != NULL) {
        size_t before_len = pos - formatted;
        size_t after_len = strlen(pos + 2);
        memmove(pos + 2, pos + 2, after_len + 1);
        memcpy(pos, hour_str, 2);
    }
    
    // Replace mm with minute
    pos = strstr(formatted, "mm");
    if (pos != NULL) {
        size_t before_len = pos - formatted;
        size_t after_len = strlen(pos + 2);
        memmove(pos + 2, pos + 2, after_len + 1);
        memcpy(pos, minute_str, 2);
    }
    
    // Replace ss with second
    pos = strstr(formatted, "ss");
    if (pos != NULL) {
        size_t before_len = pos - formatted;
        size_t after_len = strlen(pos + 2);
        memmove(pos + 2, pos + 2, after_len + 1);
        memcpy(pos, second_str, 2);
    }
    
    __result.return_value.value.object_value = __create_string(formatted, &Am_Lang_String);

__exit: ;
    if (pattern != NULL) {
        __decrease_reference_count(pattern);
    }
    return __result;
}

// Parse date/time string according to pattern
function_result Am_Lang_DateTime_parseDateTime_0(aobject* dateString, aobject* pattern)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    if (dateString != NULL) {
        __increase_reference_count(dateString);
    }
    if (pattern != NULL) {
        __increase_reference_count(pattern);
    }

    // Basic parsing for "yyyy-MM-dd HH:mm:ss" format
    string_holder *date_holder = dateString->object_properties.class_object_properties.object_data.value.custom_value;
    char* date_str = date_holder->string_value;
    
    struct tm tm_time = {0};
    int year, month, day, hour = 0, minute = 0, second = 0;
    
    // Try to parse different formats
    if (sscanf(date_str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) >= 3) {
        tm_time.tm_year = year - 1900;
        tm_time.tm_mon = month - 1;
        tm_time.tm_mday = day;
        tm_time.tm_hour = hour;
        tm_time.tm_min = minute;
        tm_time.tm_sec = second;
        tm_time.tm_isdst = -1;
        
        time_t epoch_time = mktime(&tm_time);
        unsigned long long milliseconds = (unsigned long long)epoch_time * 1000LL;
        __result.return_value.value.ulong_value = milliseconds;
    } else {
        // If parsing fails, return current time
        struct timeval tv;
        gettimeofday(&tv, NULL);
        unsigned long long milliseconds = tv.tv_sec * 1000LL + tv.tv_usec / 1000;
        __result.return_value.value.ulong_value = milliseconds;
    }

__exit: ;
    if (dateString != NULL) {
        __decrease_reference_count(dateString);
    }
    if (pattern != NULL) {
        __decrease_reference_count(pattern);
    }
    return __result;
}

// Check if year is a leap year
function_result Am_Lang_DateTime_checkLeapYear_0(int year)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    __result.return_value.value.bool_value = is_leap;

__exit: ;
    return __result;
}

// Calculate days in month
function_result Am_Lang_DateTime_calculateDaysInMonth_0(int year, int month)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (month < 1 || month > 12) {
        __result.return_value.value.int_value = 30;  // Default fallback
        goto __exit;
    }
    
    int days = days_in_month[month - 1];
    
    // Adjust for February in leap years
    if (month == 2) {
        bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (is_leap) {
            days = 29;
        }
    }
    
    __result.return_value.value.int_value = days;

__exit: ;
    return __result;
}