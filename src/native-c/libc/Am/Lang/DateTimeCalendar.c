/* Shared signed Gregorian arithmetic. No time_t width dependency or TZ mutation.
 * Apple supplies named zones; other targets explicitly expose UTC only. */
#include <libc/core.h>
#include <libc/core_inline_functions.h>
#include <Am/Lang/DateTimeCalendar.h>
#include <Am/Lang/String.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#if defined(__APPLE__) && !defined(AM_CALENDAR_UTC_ONLY)
#include <CoreFoundation/CoreFoundation.h>
#define CALENDAR_APPLE 1
#endif
#define CALENDAR_LIMIT 8640000000000000.0
static const char *cal_text(aobject *s) { return ((string_holder *)((char *)s+sizeof(aobject)))->string_value; }
static function_result cal_number(double n) { function_result r={.has_return_value=true}; r.return_value.value.double_value=n; return r; }
static function_result cal_string(const char *s) { function_result r={.has_return_value=true}; r.return_value.value.object_value=__create_string(s,&Am_Lang_String); return r; }
function_result Am_Lang_DateTimeCalendar__native_init_0(aobject *const self) { return (function_result){0}; }
function_result Am_Lang_DateTimeCalendar__native_release_0(aobject *const self) { return (function_result){0}; }
function_result Am_Lang_DateTimeCalendar__native_mark_children_0(aobject *const self) { return (function_result){0}; }
static long long cal_floor(long long a,long long b) { long long q=a/b,r=a%b;return q-(r<0); }
/* Gregorian eras make conversion constant-time even at the supported limits. */
static long long cal_days(long long y,int m,int d) {
 y-=m<=2;long long era=cal_floor(y,400);int yo=(int)(y-era*400);
 int doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
 return era*146097+yo*365+yo/4-yo/100+doy-719468;
}
static void cal_utc_parts(double ms,int *p) {
 long long n=(long long)trunc(ms),days=cal_floor(n,86400000),within=n-days*86400000;
 long long z=days+719468,era=cal_floor(z,146097);int doe=(int)(z-era*146097);
 int yo=(doe-doe/1460+doe/36524-doe/146096)/365;long long y=yo+era*400;
 int doy=doe-(365*yo+yo/4-yo/100),mp=(5*doy+2)/153;
 p[2]=doy-(153*mp+2)/5+1;p[1]=mp+(mp<10?3:-9);p[0]=(int)(y+(p[1]<=2));
 p[3]=(int)(within/3600000);p[4]=(int)(within/60000%60);p[5]=(int)(within/1000%60);p[6]=(int)(within%1000);
 p[7]=(int)((days+4)%7);if(p[7]<0)p[7]+=7;p[8]=0;
}
#ifdef CALENDAR_APPLE
static CFTimeZoneRef cal_zone(const char *s) {
 if(!*s)return CFTimeZoneCopyDefault();
 CFStringRef name=CFStringCreateWithCString(NULL,s,kCFStringEncodingUTF8);if(!name)return NULL;
 CFArrayRef known=CFTimeZoneCopyKnownNames();
 bool valid=!strcmp(s,"UTC")||!strcmp(s,"GMT")||CFArrayContainsValue(known,CFRangeMake(0,CFArrayGetCount(known)),name);
 CFRelease(known);CFTimeZoneRef zone=valid?CFTimeZoneCreateWithName(NULL,name,true):NULL;CFRelease(name);return zone;
}
static void cal_utf8(CFStringRef s,char *b,size_t n) { b[0]=0;if(s)CFStringGetCString(s,b,n,kCFStringEncodingUTF8); }
#else
static bool cal_valid_zone(const char *s) { return !*s||!strcmp(s,"UTC")||!strcmp(s,"GMT"); }
#endif
static bool cal_parts(double ms,const char *zone,int *p) {
 if(!isfinite(ms)||fabs(ms)>CALENDAR_LIMIT)return false;
#ifdef CALENDAR_APPLE
 CFTimeZoneRef tz=cal_zone(zone);if(!tz)return false;
 double offset=CFTimeZoneGetSecondsFromGMT(tz,trunc(ms)/1000-kCFAbsoluteTimeIntervalSince1970);CFRelease(tz);
 cal_utc_parts(trunc(ms)+offset*1000,p);p[8]=(int)(offset/60);
#else
 if(!cal_valid_zone(zone))return false;cal_utc_parts(ms,p);
#endif
 return true;
}
static double cal_compose(double year,double month,double day,double hour,double minute,double second,double milli,const char *zone) {
 double fields[]={year,month,day,hour,minute,second,milli};
 for(int i=0;i<7;i++)if(!isfinite(fields[i])||fabs(fields[i])>1000000000.0)return NAN;
 long long y=(long long)trunc(year),m=(long long)trunc(month)-1;
 y+=cal_floor(m,12);m-=cal_floor(m,12)*12;
 double ms=(double)cal_days(y,(int)m+1,1)*86400000+(trunc(day)-1)*86400000+trunc(hour)*3600000+trunc(minute)*60000+trunc(second)*1000+trunc(milli);
 if(fabs(ms)>CALENDAR_LIMIT+86400000)return NAN;
#ifdef CALENDAR_APPLE
 CFTimeZoneRef tz=cal_zone(zone);if(!tz)return NAN;
 if(strcmp(zone,"UTC")&&strcmp(zone,"GMT")) {
  int p[9];cal_utc_parts(ms,p);CFGregorianDate d={(SInt32)p[0],(SInt8)p[1],(SInt8)p[2],(SInt8)p[3],(SInt8)p[4],(double)p[5]};
  ms=round((CFGregorianDateGetAbsoluteTime(d,tz)+kCFAbsoluteTimeIntervalSince1970)*1000)+p[6];
 }
 CFRelease(tz);
#else
 if(!cal_valid_zone(zone))return NAN;
#endif
 return fabs(ms)<=CALENDAR_LIMIT?ms:NAN;
}
function_result Am_Lang_DateTimeCalendar_parts_0(double ms,aobject *zone,aobject *output) {
 function_result r={.has_return_value=true};array_holder *a=(array_holder *)((char *)output+sizeof(aobject));
 r.return_value.value.bool_value=a->size>=9&&a->item_size==sizeof(int)&&cal_parts(ms,cal_text(zone),(int *)((char *)a+sizeof(array_holder)));return r;
}
function_result Am_Lang_DateTimeCalendar_compose_0(double y,double m,double d,double h,double mi,double s,double milli,aobject *zone) { return cal_number(cal_compose(y,m,d,h,mi,s,milli,cal_text(zone))); }
function_result Am_Lang_DateTimeCalendar_zoneName_0(aobject *zone) {
#ifdef CALENDAR_APPLE
 CFTimeZoneRef tz=cal_zone(cal_text(zone));char name[256]="";if(tz){cal_utf8(CFTimeZoneGetName(tz),name,sizeof(name));CFRelease(tz);}return cal_string(name);
#else
 return cal_string(cal_valid_zone(cal_text(zone))?"UTC":"");
#endif
}
function_result Am_Lang_DateTimeCalendar_abbreviation_0(double ms,aobject *zone) {
#ifdef CALENDAR_APPLE
 CFTimeZoneRef tz=cal_zone(cal_text(zone));char name[256]="";if(tz){if(isfinite(ms)&&fabs(ms)<=CALENDAR_LIMIT){CFStringRef n=CFTimeZoneCopyAbbreviation(tz,ms/1000-kCFAbsoluteTimeIntervalSince1970);cal_utf8(n,name,sizeof(name));if(n)CFRelease(n);}CFRelease(tz);}return cal_string(name);
#else
 return cal_string(cal_valid_zone(cal_text(zone))&&isfinite(ms)&&fabs(ms)<=CALENDAR_LIMIT?"UTC":"");
#endif
}
function_result Am_Lang_DateTimeCalendar_zones_0(void) {
#ifdef CALENDAR_APPLE
 CFArrayRef names=CFTimeZoneCopyKnownNames();CFStringRef joined=CFStringCreateByCombiningStrings(NULL,names,CFSTR("\n"));
 CFIndex size=CFStringGetMaximumSizeForEncoding(CFStringGetLength(joined),kCFStringEncodingUTF8)+1;
 char *text=malloc((size_t)size);if(!text){CFRelease(joined);CFRelease(names);return cal_string("UTC");}
 cal_utf8(joined,text,(size_t)size);function_result r=cal_string(text);free(text);CFRelease(joined);CFRelease(names);return r;
#else
 return cal_string("UTC");
#endif
}
static int cal_digits(const char **s,int count) { int n=0;for(int i=0;i<count;i++){if(**s<'0'||**s>'9')return -1;n=n*10+*(*s)++-'0';}return n; }
static bool cal_take(const char **s,char c) { if(**s!=c)return false;(*s)++;return true; }
static double cal_parse(const char *s) {
 int sign=1,digits=4;if(*s=='+'||*s=='-'){sign=*s++=='-'?-1:1;digits=6;}
 int y=cal_digits(&s,digits);if(y<0||(digits==6&&sign<0&&y==0))return NAN;y*=sign;
 int m=1,d=1,h=0,mi=0,sec=0,ms=0,offset=0;bool dateOnly=true,utc=false,fullDate=false;
 if(*s=='-'){s++;m=cal_digits(&s,2);if(*s=='-'){s++;d=cal_digits(&s,2);fullDate=true;}}
 if(*s=='T'||*s==' '){if(!fullDate)return NAN;dateOnly=false;s++;h=cal_digits(&s,2);if(!cal_take(&s,':'))return NAN;mi=cal_digits(&s,2);
  if(*s==':'){s++;sec=cal_digits(&s,2);if(*s=='.'){s++;int scale=100;if(*s<'0'||*s>'9')return NAN;while(*s>='0'&&*s<='9'){ms+=(*s++-'0')*scale;scale/=10;}}}
  if(*s=='Z'){utc=true;s++;}else if(*s=='+'||*s=='-'){utc=true;int os=*s++=='-'?-1:1;int oh=cal_digits(&s,2);if(*s==':')s++;int om=cal_digits(&s,2);if(oh<0||oh>23||om<0||om>59)return NAN;offset=os*(oh*60+om);}
 }
 if(*s||m<1||m>12||d<1||d>31||h<0||h>24||mi<0||mi>59||sec<0||sec>59||(h==24&&(mi||sec||ms)))return NAN;
 double result=cal_compose(y,m,d,h,mi,sec,ms,dateOnly||utc?"UTC":"")-offset*60000.0;
 return fabs(result)<=CALENDAR_LIMIT?result:NAN;
}
function_result Am_Lang_DateTimeCalendar_parse_0(aobject *text) {
 const char *s=cal_text(text);double result=cal_parse(s);
#ifdef CALENDAR_APPLE
 /* Preserve the browser's legacy local/RFC input formats on the existing target.
  * ISO input never falls back after failed validation. */
 if(isnan(result)&&(strchr(s,'/')||(*s>='A'&&*s<='Z'))) {
  const char *formats[]={"%m/%d/%Y, %H:%M:%S","%m/%d/%Y","%Y/%m/%d","%a %b %d %Y %H:%M:%S GMT","%a, %d %b %Y %H:%M:%S GMT",NULL};
  for(int i=0;formats[i];i++) {
   struct tm fields={0};fields.tm_mday=1;char *end=strptime(s,formats[i],&fields);
   if(end&&!*end){result=cal_compose(fields.tm_year+1900,fields.tm_mon+1,fields.tm_mday,fields.tm_hour,fields.tm_min,fields.tm_sec,0,i>=3?"UTC":"");break;}
  }
 }
#endif
 return cal_number(result);
}
