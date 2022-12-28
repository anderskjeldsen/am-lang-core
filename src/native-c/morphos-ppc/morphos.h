#ifndef amiga_h
#define amiga_h
#include <exec/types.h>
#include <proto/exec.h>
#include <string.h>

typedef struct _lib_node lib_node;

struct _lib_node {
    char *name;
	lib_node * next;
	void * lib_base;
};


void __ensure_exec();
void * __ensure_library(unsigned char * __lib_name, unsigned int version);
void __release_libraries();

#endif
