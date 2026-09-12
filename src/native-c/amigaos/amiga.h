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
 * Directories a spawned child should search for BARE command names, on top
 * of whatever the parent already has. Set from AmLang via
 * Process.setSpawnSearchPath() (newline-separated, empty clears); consumed
 * by every spawn site, which mints a FRESH chain per spawn and hands it to
 * the child via NP_Path.
 *
 * Measured on m68k (see the probe notes): NP_Path transfers OWNERSHIP -- the
 * child frees the whole chain, locks included, when it exits. Reusing one
 * chain for a second spawn hands over freed memory and HANGS THE MACHINE, so
 * never cache the result. NP_Path also REPLACES the child's inherited path
 * rather than adding to it, which is why __build_spawn_path() copies the
 * parent's own entries in as well -- without that, every command spawned from
 * the IDE would lose whatever the user had on their own Path.
 */
void __set_spawn_search_path(const char * dirs);

/* Fresh chain = our dirs + a DupLock'd copy of the parent's entries.
 * Returns 0 when no dirs are configured; the caller must then OMIT the
 * NP_Path tag entirely (passing 0 would mean "empty path" and strip the
 * child's default inheritance). Hand the result to NP_Path and never touch
 * it again -- the child owns it. */
BPTR __build_spawn_path(void);
/* Own CLI, or the Workbench/Ambient process's when started from an icon. */
struct CommandLineInterface;
struct CommandLineInterface * __effective_path_cli(void);

/* Only for the error path, when the chain was built but never handed over. */
void __free_spawn_path(BPTR chain);
