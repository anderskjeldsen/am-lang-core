#include <libc/core.h>
#include <amigaos/amiga.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <string.h>
#include <libc/core_inline_functions.h>

lib_node *__first_lib_node = NULL;

void __ensure_exec() {
	if (SysBase == NULL) {
		SysBase = *((struct ExecBase **)4UL);
	}
}

void * __ensure_library(unsigned char * __lib_name, unsigned int version)
{
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
		__first_lib_node = __current_lib_node;
		return __first_lib_node->lib_base;
	}
}

// Auto-runs from the C runtime's atexit chain after main() returns,
// so every library that came in through __ensure_library
// (intuition, asl, cybergraphics, …) gets CloseLibrary'd on shutdown
// without the AmLang side having to plumb #runOnExit for it. The
// constructor/destructor attribute is GCC syntax that amiga-gcc
// honours; m68k crt0 calls the destructor chain on normal exit.
//
// Was orphaned before — defined, never called — so libs we opened
// stayed open across program exit. Wiring it to __attribute__((destructor))
// makes the existing tracker actually do its job.
__attribute__((destructor))
void __release_libraries() {
	lib_node * __current_lib_node = __first_lib_node;
	__first_lib_node = NULL;
	while ( __current_lib_node != NULL) {
		lib_node *next = __current_lib_node->next;
		CloseLibrary(__current_lib_node->lib_base);
		free(__current_lib_node);
		__current_lib_node = next;
	}
}

/*
 * libgcc helper shim. m68k 68000-68030 has no native compare-and-
 * swap, so gcc lowers C11 atomic_compare_exchange on a 1-byte
 * variable to a call to __atomic_compare_exchange_1, which libgcc
 * for our amigaos toolchain doesn't provide. Forbid()/Permit()
 * gives task-level atomicity, which is sufficient for the only
 * caller we have today (Am.Async.Continuation's `done` flag — not
 * touched from interrupts). Signature matches the libgcc form;
 * the weak / memorder args are ignored (always a strong, fully-
 * ordered exchange).
 */
bool __atomic_compare_exchange_1(
	volatile void *ptr, void *expected, unsigned char desired,
	bool weak, int success_memmodel, int failure_memmodel)
{
	volatile unsigned char *p = (volatile unsigned char *) ptr;
	unsigned char *e = (unsigned char *) expected;
	bool ok;
	Forbid();
	if (*p == *e) {
		*p = desired;
		ok = true;
	} else {
		*e = *p;
		ok = false;
	}
	Permit();
	return ok;
}


/* ---- command-search path for spawned children ---------------------- */

/* The Shell's path list is a chain of these; cli_CommandDir points at the
 * first. Declared locally so amiga.h need not drag in dos/dosextens.h. */
struct __sp_node { BPTR pn_Next; BPTR pn_Lock; };

/* Newline-separated, owned here, replaced wholesale by each set call. */
static char * __spawn_dirs = NULL;

void __set_spawn_search_path(const char * dirs)
{
	if (__spawn_dirs != NULL) {
		FreeVec(__spawn_dirs);
		__spawn_dirs = NULL;
	}
	if (dirs == NULL || dirs[0] == 0) {
		return;
	}
	{
		int n = 0;
		while (dirs[n] != 0) n++;
		__spawn_dirs = (char *) AllocVec((ULONG) (n + 1), MEMF_ANY);
		if (__spawn_dirs != NULL) {
			int i = 0;
			for (i = 0; i <= n; i++) __spawn_dirs[i] = dirs[i];
		}
	}
}

void __free_spawn_path(BPTR chain)
{
	struct __sp_node * n = (struct __sp_node *) BADDR(chain);
	while (n != NULL) {
		struct __sp_node * next = (struct __sp_node *) BADDR(n->pn_Next);
		if (n->pn_Lock != 0) UnLock(n->pn_Lock);
		FreeVec(n);
		n = next;
	}
}

/* Append a node owning `lock`. On allocation failure the lock is released
 * and 0 returned, so the caller can unwind the partial chain. */
static struct __sp_node * __sp_append(struct __sp_node ** head,
                                      struct __sp_node ** tail, BPTR lock)
{
	struct __sp_node * n = (struct __sp_node *)
		AllocVec((ULONG) sizeof(struct __sp_node), MEMF_CLEAR | MEMF_PUBLIC);
	if (n == NULL) {
		UnLock(lock);
		return NULL;
	}
	n->pn_Next = 0;
	n->pn_Lock = lock;
	if (*tail != NULL) {
		(*tail)->pn_Next = MKBADDR(n);
		*tail = n;
	} else {
		*head = n;
		*tail = n;
	}
	return n;
}


/* The CLI whose cli_CommandDir is this process's effective command path.
 * A process started from a Shell has its own. One started from an ICON
 * (Workbench / Ambient) has pr_CLI == 0 and therefore NO path list at all:
 * bare names resolve only via C:, and every child we spawn inherits the
 * same nothing -- "Path entries are not forwarded when starting from an
 * icon". The desktop process keeps a CLI structure whose path LoadWB seeded
 * from the startup shell; that is what "Execute Command..." and an
 * icon-launched Shell use, so we borrow it the same way. */

/* Length of a CLI's path list (0 when the CLI is absent). */
static int __path_len(struct CommandLineInterface * cli)
{
	int cnt = 0;
	struct __sp_node * pn;
	if (cli == NULL || cli->cli_CommandDir == 0) return 0;
	pn = (struct __sp_node *) BADDR(cli->cli_CommandDir);
	while (pn != NULL && cnt < 256) {
		cnt++;
		pn = (struct __sp_node *) BADDR(pn->pn_Next);
	}
	return cnt;
}

/* Case-insensitive "does `name` contain `word`". */
static int __name_contains(const char * name, const char * word)
{
	int i;
	if (name == NULL) return 0;
	for (i = 0; name[i] != 0; i++) {
		int j = 0;
		while (word[j] != 0 && name[i + j] != 0) {
			char a = name[i + j], b = word[j];
			if (a >= 'A' && a <= 'Z') a = (char) (a + 32);
			if (b >= 'A' && b <= 'Z') b = (char) (b + 32);
			if (a != b) break;
			j++;
		}
		if (word[j] == 0) return 1;
	}
	return 0;
}

/* Remember `n`'s CLI when it is a process with a longer path than `*best`.
 * A desktop process (name contains "workbench" / "ambient") with any path
 * wins outright: that is the list the user's startup scripts built. */
static void __consider_path_owner(struct Node * n,
                                  struct CommandLineInterface ** best, int * best_n)
{
	struct CommandLineInterface * c;
	int len;
	if (n == NULL || n->ln_Type != NT_PROCESS) return;
	c = (struct CommandLineInterface *) BADDR(((struct Process *) n)->pr_CLI);
	len = __path_len(c);
	if (len <= 0) return;
	if (*best_n < 100000
			&& (__name_contains(n->ln_Name, "workbench") || __name_contains(n->ln_Name, "ambient"))) {
		*best_n = 100000;
		*best = c;
		return;
	}
	if (len > *best_n) {
		*best_n = len;
		*best = c;
	}
}

struct CommandLineInterface * __effective_path_cli(void)
{
	struct Process * self = (struct Process *) FindTask(NULL);
	struct CommandLineInterface * own =
		(struct CommandLineInterface *) BADDR(self->pr_CLI);
	if (own != NULL && own->cli_CommandDir != 0) {
		return own;
	}
	{
		static const char * const desktop_names[] = { "Workbench", "Ambient", NULL };
		int i;
		for (i = 0; desktop_names[i] != NULL; i++) {
			struct Task * t;
			Forbid();
			t = FindTask((CONST_STRPTR) desktop_names[i]);
			if (t != NULL && t->tc_Node.ln_Type == NT_PROCESS) {
				struct CommandLineInterface * wb =
					(struct CommandLineInterface *) BADDR(((struct Process *) t)->pr_CLI);
				if (wb != NULL && wb->cli_CommandDir != 0) {
					Permit();
					return wb;
				}
			}
			Permit();
		}
	}
	/* No desktop process by name: take the process with the LONGEST path
	 * list in the system. That is the boot shell / desktop on any real
	 * setup (the only ones that ran the startup Path commands), and it is
	 * what a headless boot without a desktop has. Task lists are walked
	 * under Forbid; only pointers are read, no DOS calls. */
	{
		struct CommandLineInterface * best = NULL;
		int best_n = 0;
		struct Node * n;
		Forbid();
		__consider_path_owner((struct Node *) SysBase->ThisTask, &best, &best_n);
		for (n = SysBase->TaskReady.lh_Head; n->ln_Succ != NULL; n = n->ln_Succ) {
			__consider_path_owner(n, &best, &best_n);
		}
		for (n = SysBase->TaskWait.lh_Head; n->ln_Succ != NULL; n = n->ln_Succ) {
			__consider_path_owner(n, &best, &best_n);
		}
		Permit();
		if (best != NULL) {
			return best;
		}
	}
	return own;
}

BPTR __build_spawn_path(void)
{
	struct __sp_node * head = NULL;
	struct __sp_node * tail = NULL;
	struct Process * self;
	struct CommandLineInterface * cli;

	/* No extra dirs -> no tag, so the child keeps its default inheritance --
	 * but only when we HAVE a path to inherit. Started from an icon there is
	 * none, and the chain must then carry the desktop's path (see
	 * __effective_path_cli) or the child cannot find anything but C:. */
	if (__spawn_dirs == NULL || __spawn_dirs[0] == 0) {
		struct Process * me = (struct Process *) FindTask(NULL);
		struct CommandLineInterface * own =
			(struct CommandLineInterface *) BADDR(me->pr_CLI);
		if (own != NULL && own->cli_CommandDir != 0) {
			return 0;
		}
	}

	/* Ours first, so an extension's tool wins over a same-named system one. */
	if (__spawn_dirs != NULL) {
		const char * p = __spawn_dirs;
		char buf[256];
		while (*p != 0) {
			int i = 0;
			while (*p != 0 && *p != '\n' && i < (int) sizeof(buf) - 1) {
				buf[i++] = *p++;
			}
			buf[i] = 0;
			while (*p == '\n') p++;
			if (i > 0) {
				BPTR l = Lock((CONST_STRPTR) buf, ACCESS_READ);
				/* A drawer that has gone away is skipped, not fatal. */
				if (l != 0) {
					if (__sp_append(&head, &tail, l) == NULL) {
						__free_spawn_path(head ? MKBADDR(head) : 0);
						return 0;
					}
				}
			}
		}
	}

	/* Then a DupLock'd copy of the parent's own entries: NP_Path REPLACES,
	 * so without this the child loses the user's Path. Each copy needs its
	 * own lock because the child unlocks everything it frees. */
	self = (struct Process *) FindTask(NULL);
	(void) self;
	cli = __effective_path_cli();
	if (cli != NULL) {
		struct __sp_node * p = (struct __sp_node *) BADDR(cli->cli_CommandDir);
		while (p != NULL) {
			if (p->pn_Lock != 0) {
				BPTR d = DupLock(p->pn_Lock);
				if (d != 0) {
					if (__sp_append(&head, &tail, d) == NULL) {
						__free_spawn_path(head ? MKBADDR(head) : 0);
						return 0;
					}
				}
			}
			p = (struct __sp_node *) BADDR(p->pn_Next);
		}
	}

	return head != NULL ? MKBADDR(head) : 0;
}
