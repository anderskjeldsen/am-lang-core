#pragma once

#include <exec/types.h>
#include <proto/exec.h>
#include <dos/dos.h>
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


/* ---- command-search path for spawned children ----------------------
 *
 * Mirror of the AmigaOS implementation in amiga.h/amiga.c -- MorphOS shares
 * the dos.library command-resolution model, so the same mechanism applies.
 *
 * NOTE: the behaviour was MEASURED on m68k under amiberry, not on MorphOS.
 * The API is compatible, but if bare extension commands still fail here,
 * verify NP_Path on the morphos-qemu rig before assuming this file is wrong.
 *
 * Set from AmLang via Process.setSpawnSearchPath() (newline-separated, empty
 * clears); consumed by every spawn site, which mints a FRESH chain per spawn
 * and hands it to the child via NP_Path.
 *
 * NP_Path transfers OWNERSHIP -- the child frees the chain, locks included,
 * when it exits, so a cached chain is freed under you and the next spawn
 * hands over dead memory. It also REPLACES the child's inherited path rather
 * than adding to it, which is why the parent's own entries are copied in.
 */
void __set_spawn_search_path(const char * dirs);

/* Fresh chain = our dirs + a DupLock'd copy of the parent's entries.
 * 0 when no dirs are configured; the caller must then OMIT the NP_Path tag
 * (passing 0 would mean "empty path" and strip the child's inheritance). */
BPTR __build_spawn_path(void);
/* Own CLI, or the Workbench/Ambient process's when started from an icon. */
struct CommandLineInterface;
struct CommandLineInterface * __effective_path_cli(void);

/* Only for the error path, when the chain was built but never handed over. */
void __free_spawn_path(BPTR chain);
