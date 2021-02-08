#ifndef amiga_c
#define amiga_c
#include <core.h>
#include <amigaos/amiga.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <string.h>

lib_node *__first_lib_node = NULL;

void __ensure_exec() {
	if (SysBase == NULL) {
		SysBase = *((struct ExecBase **)4UL);
	}
	printf("Ensure exec - SysBase: %d\n", (ULONG) SysBase);
}

void * __ensure_library(unsigned char * __lib_name, unsigned int version)
{
	printf("Ensure Library %s\n", __lib_name, (unsigned int) __first_lib_node);

	lib_node * __current_lib_node = __first_lib_node;
	while ( __current_lib_node != NULL) {
		if ( strcmp(__current_lib_node->name, __lib_name) == 0 ) {
			break;
		}
		__current_lib_node = __current_lib_node->next;
	}
	if (__current_lib_node != NULL) {
		return __current_lib_node->lib_base;
	} else {
		__current_lib_node = (lib_node *) malloc(sizeof(lib_node));
		__current_lib_node->name = __lib_name;
		__current_lib_node->next = __first_lib_node;
		__ensure_exec();
		__current_lib_node->lib_base = OpenLibrary(__lib_name, version);
		printf("Library %s ptr %d\n", __lib_name, (unsigned int) __current_lib_node->lib_base);
		__first_lib_node = __current_lib_node;
		return __first_lib_node->lib_base;
	}
}

void __release_libraries() {
	printf("Release libraries");
	lib_node * __current_lib_node = __first_lib_node;
	__first_lib_node = NULL;
	while ( __current_lib_node != NULL) {
		lib_node *next = __current_lib_node->next;
		CloseLibrary(__current_lib_node->lib_base);
		free(__current_lib_node);
		__current_lib_node = next;
	}	
}

#endif
