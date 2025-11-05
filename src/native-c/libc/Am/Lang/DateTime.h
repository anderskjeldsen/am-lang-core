#pragma once
#include <libc/core.h>
#include <Am/Lang/DateTime.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/UInt.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Bool.h>
#include <Am/Lang/ULong.h>
#include <Am/Lang/Int.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Native function declarations
/* these should be generated 
function_result Am_Lang_DateTime_getMillis_0();
function_result Am_Lang_DateTime_getLocalTimezoneOffset_0();
function_result Am_Lang_DateTime_toEpochMillis_0(int year, int month, int day, int hour, int minute, int second, int timezoneOffsetMinutes);
function_result Am_Lang_DateTime_extractYear_0(unsigned long long epochMillis, int timezoneOffsetMinutes);
function_result Am_Lang_DateTime_extractMonth_0(unsigned long long epochMillis, int timezoneOffsetMinutes);
function_result Am_Lang_DateTime_extractDay_0(unsigned long long epochMillis, int timezoneOffsetMinutes);
function_result Am_Lang_DateTime_extractHour_0(unsigned long long epochMillis, int timezoneOffsetMinutes);
function_result Am_Lang_DateTime_extractMinute_0(unsigned long long epochMillis, int timezoneOffsetMinutes);
function_result Am_Lang_DateTime_extractSecond_0(unsigned long long epochMillis, int timezoneOffsetMinutes);
function_result Am_Lang_DateTime_extractMillisecond_0(unsigned long long epochMillis);
function_result Am_Lang_DateTime_extractDayOfWeek_0(unsigned long long epochMillis, int timezoneOffsetMinutes);
function_result Am_Lang_DateTime_formatDateTime_0(unsigned long long epochMillis, int timezoneOffsetMinutes, aobject* pattern);
function_result Am_Lang_DateTime_parseDateTime_0(aobject* dateString, aobject* pattern);
function_result Am_Lang_DateTime_checkLeapYear_0(int year);
function_result Am_Lang_DateTime_calculateDaysInMonth_0(int year, int month);

// Standard DateTime object methods
function_result Am_Lang_DateTime__native_init_0(aobject * const this);
function_result Am_Lang_DateTime__native_release_0(aobject * const this);
function_result Am_Lang_DateTime__native_mark_children_0(aobject * const this);
*/