// Async child-process wrapper for AmigaOS — Phase E.6.
//
// Architecture (proven in HandlerTest/parent.c against child2):
//
//   - We spawn a custom-handler Process. It owns a public MsgPort
//     and serves DOS packets (ACTION_FINDINPUT, ACTION_WRITE,
//     ACTION_READ, ACTION_END, ACTION_DIE, etc.) directly from
//     two shared ring buffers:
//        * out_ring  — child writes go here; AmLang's tryReadOutput
//                      pops from it.
//        * in_ring   — AmLang's writeInput pushes here; child's
//                      ACTION_READ pops from it (or defers).
//
//   - The child is spawned via LoadSeg + CreateNewProcTags with:
//        NP_Input        = FakeFileHandle pointing at handler port
//        NP_Output       = same (separate FH, same handler)
//        NP_ConsoleTask  = handler port  → child's Open("*") routes
//                                          to us (where bebbossh's
//                                          password prompts live)
//        NP_Cli          = TRUE          → libnix's CLI-startup
//                                          path is honoured
//        NP_Arguments    = command-tail string
//        NP_ExitCode     = exit callback → flips state->child_exited
//
//   Why this avoids the prior PIPE:-based design's traps:
//   - Queue-Handler PIPE: buffers all writes until writer-close;
//     interactive children never get to flush. Our handler delivers
//     every byte immediately into the ring buffer (no batching).
//   - SystemTagList derives the child's pr_ConsoleTask from its
//     SYS_Input handler, ignoring NP_ConsoleTask in the tag list.
//     By calling CreateNewProcTags directly we keep NP_ConsoleTask
//     authoritative, so bebbossh's Open("*") lands on us.

#include <libc/core.h>
#include <Am/Lang/RunningProcess.h>
#include <amigaos/Am/Lang/RunningProcess.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Bool.h>
#include <libc/core_inline_functions.h>

#include <amigaos/amiga.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <utility/tagitem.h>

#include <string.h>

// =================================================================
// Diagnostic log (same shape as before — invaluable for debugging
// the dos.library handler protocol)
// =================================================================

static BPTR g_log_fh = 0;
static BOOL g_log_open_attempted = FALSE;
static char g_log_path[64] = { 0 };

// Compile-time gate for the per-packet "hnd RX type=" / wake-stats
// chatter. Once the handler bridge is proven to work end-to-end (we
// passed that point on 2026-06-05 with bebbossh's interactive SSH
// shell), the per-packet logging is pure overhead — bebbossh's idle
// polling fires ACTION_EXAMINE_FH dozens of times per second, and
// each line is a synchronous disk Write that drags amStudio down
// (visibly so on quit/teardown when the log buffer flushes). Define
// `RP_VERBOSE_LOG` at compile time to re-enable for diagnostics.
// Currently ON to chase the post-`dir` Software Failure — every
// packet the handler processes (except ACTION_DIE) gets a log
// line so we can see which one is the last one before the
// crash. Flip back to 0 once the trail is captured.
#ifndef RP_VERBOSE_LOG
#  define RP_VERBOSE_LOG 1
#endif

static const char * const RP_LOG_PATHS[] = {
    "PROGDIR:tty.log",
    "SYS:tty.log",
    "T:amStudio-tty.log",
    "RAM:amStudio-tty.log",
    NULL
};

static void rp_log_open(void) {
    if (g_log_open_attempted) return;
    g_log_open_attempted = TRUE;
    for (int i = 0; RP_LOG_PATHS[i] != NULL; i++) {
        g_log_fh = Open((CONST_STRPTR) RP_LOG_PATHS[i], MODE_NEWFILE);
        if (g_log_fh != 0) {
            const char * src = RP_LOG_PATHS[i];
            int j = 0;
            while (src[j] != 0 && j < (int) sizeof(g_log_path) - 1) {
                g_log_path[j] = src[j]; j++;
            }
            g_log_path[j] = 0;
            return;
        }
    }
    g_log_path[0] = 0;
}

static void rp_log_str(const char * s) {
    if (g_log_fh == 0) return;
    Write(g_log_fh, (APTR) s, (LONG) strlen(s));
    Flush(g_log_fh);
}

static void rp_log_event(const char * msg, LONG val) {
    if (g_log_fh == 0) return;
    char buf[160]; int p = 0;
    while (*msg) buf[p++] = *msg++;
    buf[p++] = ' ';
    BOOL neg = FALSE; LONG v = val;
    if (v < 0) { neg = TRUE; v = -v; }
    char tmp[12]; int t = 0;
    if (v == 0) tmp[t++] = '0';
    while (v > 0) { tmp[t++] = (char)('0' + (v % 10)); v /= 10; }
    if (neg) buf[p++] = '-';
    while (t > 0) buf[p++] = tmp[--t];
    buf[p++] = '\n';
    Write(g_log_fh, buf, p);
    Flush(g_log_fh);
}

// =================================================================
// Ring buffer
// =================================================================

#define RP_RING_SIZE (64 * 1024)

typedef struct rp_ring {
    UBYTE * data;
    ULONG   head, tail, count;
    BOOL    writer_closed;
} rp_ring;

static ULONG rp_push(rp_ring * r, const UBYTE * src, ULONG n) {
    ULONG pushed = 0;
    while (pushed < n && r->count < RP_RING_SIZE) {
        r->data[r->head] = src[pushed];
        r->head = (r->head + 1) % RP_RING_SIZE;
        r->count++; pushed++;
    }
    return pushed;
}

static ULONG rp_pop(rp_ring * r, UBYTE * dst, ULONG n) {
    ULONG popped = 0;
    while (popped < n && r->count > 0) {
        dst[popped] = r->data[r->tail];
        r->tail = (r->tail + 1) % RP_RING_SIZE;
        r->count--; popped++;
    }
    return popped;
}

// =================================================================
// Shared state
// =================================================================

typedef struct rp_state rp_state;
struct rp_state {
    volatile LONG refcount;

    rp_ring in;
    rp_ring out;

    struct Process * handler_proc;
    struct MsgPort * handler_port;

    struct DosPacket * deferred_read_pkt;
    int  open_count;
    BOOL any_open;

    volatile BOOL child_exited;
    volatile BOOL shutdown_requested;

    // ACTION_SCREEN_MODE arg — TRUE = child put stdin into raw mode
    // (SetMode(fh, 1) on AmigaOS, equivalent to terminal raw mode
    // on Unix). Flipped by the SCREEN_MODE packet handler; read by
    // the AmLang side via isRawMode() to switch the CLI panel into
    // char-at-a-time passthrough + terminal-emulator rendering.
    volatile BOOL raw_mode;

    // Reported terminal dimensions, used to answer the bebbossh-
    // style CSI 0 SP q "what's your size?" query that programs send
    // by Write()-ing to stdin (yes, really — the AmigaOS console
    // protocol replies in-band on the same FH). Default 24x80; the
    // AmLang side calls setReportedSize() to override.
    volatile LONG reported_rows;
    volatile LONG reported_cols;

    // Single-linked-list pointer threading every live handler
    // state into `g_handler_list_head`. Used by the runOnExit
    // hook (shutdownAllNative) to find every still-running
    // amStudioTTY Process and tell it to die before the AmLang
    // program's seglist gets unloaded. Pure bookkeeping — the
    // handler itself never touches it.
    struct rp_state * next_handler;
};

// Singly-linked list of every live handler state. Mutated only
// under Forbid(). Walked by shutdownAllNative on exit.
static struct rp_state * g_handler_list_head = NULL;

// Count of handler Processes whose entry function has NOT yet
// returned. Polled by shutdownAllNative — when this reaches 0
// we know no handler is still executing in this program's
// seglist and exit is safe. Tracked separately from the list
// because the list only empties when rp_state is freed, which
// requires _native_release_0 — and that doesn't run if the
// AmLang RunningProcess instance is alive in a static field
// at exit time.
static volatile LONG g_live_handler_count = 0;

// Diagnostic counters that the handler increments at three
// known points. Comparing them at shutdownAllNative tells us
// which segment of the handler's lifecycle is hanging:
//   entered  — first thing in rp_handler_entry (after st check).
//   die_seen — incremented under Forbid the moment a handler
//              observes shutdown_requested == TRUE.
//   exited   — last thing before rp_handler_entry returns.
//
// Pure exec stores — no DOS / IO involved, so they're safe to
// touch from the handler task.
static volatile LONG g_handler_entered_count = 0;
static volatile LONG g_handler_die_seen_count = 0;
static volatile LONG g_handler_exited_count = 0;

// FH captured at first startNative — amStudio's main task's
// Output(), which is the redirected stdout the user reads in
// closedown.log. We hold onto this so handler-task code can
// log there too: handlers are spawned without NP_Output set,
// so calling Output() from inside the handler returns NIL:
// (default) and any writes silently vanish. Capturing once
// from the main task gives every handler a real FH to write
// to.
static BPTR g_parent_stdout = 0;

// Write a NUL-terminated string to the parent task's stdout
// (captured at startNative time). Best-effort — silent no-op
// if we never captured one. Falls back to the current task's
// Output() if the global isn't set yet (e.g. shutdownAllNative
// being called before any handler started — though that path
// is the no-handlers fast-return).
static void rp_stdout_line(const char * msg) {
    BPTR out = g_parent_stdout;
    if (out == 0) {
        out = Output();
    }
    if (out == 0) return;
    int n = 0; while (msg[n] != 0) n++;
    Write(out, (APTR) msg, (LONG) n);
    Write(out, (APTR) "\n", 1);
}

// Add to the global list (call under Forbid).
static void rp_list_add(struct rp_state * st) {
    st->next_handler = g_handler_list_head;
    g_handler_list_head = st;
}

// Remove from the global list (call under Forbid). Tolerates
// st not being on the list (a no-op).
static void rp_list_remove(struct rp_state * st) {
    struct rp_state ** link = &g_handler_list_head;
    while (*link != NULL) {
        if (*link == st) {
            *link = st->next_handler;
            st->next_handler = NULL;
            return;
        }
        link = &(*link)->next_handler;
    }
}

// Process-global wake target — set once by the AmLang main thread
// via setGlobalWake(); used by every handler's ACTION_WRITE to
// Signal() the task whose Wait() the main loop is blocked on. This
// makes streaming output (ping, tail -f, shell prompts) appear in
// the panel within a microsecond of the write, instead of waiting
// for the user's next keystroke. Bounded by `volatile` so the
// handler tasks see updates promptly.
static volatile struct Task * g_wake_task = NULL;
static volatile UBYTE         g_wake_sig_bit = 0;

static volatile ULONG g_wake_signal_count = 0;
static volatile ULONG g_wake_skipped_count = 0;

static void rp_signal_main_wake(void) {
    struct Task * t = (struct Task *) g_wake_task;
    UBYTE bit = g_wake_sig_bit;
    if (t != NULL && bit < 32) {
        Signal(t, 1L << bit);
        g_wake_signal_count++;
    } else {
        g_wake_skipped_count++;
    }
}

typedef struct _running_process_data running_process_data;
struct _running_process_data {
    rp_state * state;
    BPTR  fh_in_bptr;
    BPTR  fh_out_bptr;
    BPTR  fh_err_bptr;
    BPTR  child_seg;        // tracked for cleanup if spawn failed
    BOOL  child_owns_seg;   // TRUE when NP_FreeSeglist=TRUE → child unloads
    BPTR  old_cwd_lock;
    BOOL  has_old_cwd;
};

// =================================================================
// State refcount + cleanup
// =================================================================

static void rp_state_free(rp_state * st) {
    // Unlink from the global handler list so shutdownAllNative
    // doesn't race against a freed pointer. Forbid for the list
    // mutation only — the FreeMems below don't need it.
    Forbid();
    rp_list_remove(st);
    Permit();
    if (st->in.data  != NULL) { FreeMem(st->in.data,  RP_RING_SIZE); st->in.data  = NULL; }
    if (st->out.data != NULL) { FreeMem(st->out.data, RP_RING_SIZE); st->out.data = NULL; }
    FreeMem(st, sizeof(*st));
}

static void rp_state_release(rp_state * st) {
    if (st == NULL) return;
    LONG count;
    Forbid();
    st->refcount--;
    count = st->refcount;
    Permit();
    if (count <= 0) {
        // No rp_log_event here — rp_state_release can be called
        // from the handler's exit path, and DOS calls (Write
        // included) from a custom DOS handler recurse through
        // its own pr_MsgPort and ate the DIE messages, leaving
        // the handler stuck. See banner in rp_handler_entry.
        rp_state_free(st);
    }
}

// =================================================================
// Handler Process
// =================================================================

// Variant of rp_pkt_reply that lets the caller decide whether to
// actually PutMsg the reply back to the original sender. When the
// child has already exited, its Process struct (including the
// port that originally sent the packet) has been freed by exec —
// any PutMsg into that port dereferences mp_SigTask / mp_MsgList
// off freed memory and traps with an illegal instruction, which
// is the #87000004 alert the user was seeing.
//
// `do_reply = TRUE`  → behaves like the original rp_pkt_reply.
// `do_reply = FALSE` → fills in dp_Res1/Res2 + tags as a reply,
//                      but skips the PutMsg. The packet memory was
//                      allocated by the (now-dead) sender; it gets
//                      reclaimed when exec frees the sender's
//                      task-owned memory list.
static void rp_pkt_reply_ex(struct DosPacket * pkt, LONG res1, LONG res2, BOOL do_reply) {
    if (!do_reply) {
        // Sender is dead — don't touch pkt or msg at all. The
        // packet and its containing Message may live on the dead
        // sender's stack/pool; reading dp_Link or writing dp_Res1
        // would dereference freed memory and could corrupt
        // whatever exec has reused that region for. Just drop the
        // whole interaction on the floor — exec frees the
        // sender's memory list when the task is torn down.
        return;
    }
    struct Message * msg = pkt->dp_Link;
    pkt->dp_Res1 = res1;
    pkt->dp_Res2 = res2;
    struct MsgPort * reply_port = pkt->dp_Port;
    struct Process * self = (struct Process *) FindTask(NULL);
    pkt->dp_Port = &self->pr_MsgPort;
    // Tag the reply so the receiving handler can distinguish "fresh
    // packet" from "reply to one of mine". Matters specifically for
    // the fire-and-forget ACTION_DIE we send to ourselves via
    // rp_handler_die — without this the reply loops as a new DIE
    // packet and the handler spins thousands of cycles, never
    // processing the deferred ACTION_READ that bebbossh's password
    // prompt is waiting on.
    msg->mn_Node.ln_Type = NT_REPLYMSG;
    PutMsg(reply_port, msg);
}

static void rp_pkt_reply(struct DosPacket * pkt, LONG res1, LONG res2) {
    rp_pkt_reply_ex(pkt, res1, res2, TRUE);
}

// True when it's UNSAFE to PutMsg back to `pkt->dp_Port`. After
// the child has exited (and its Process / port have been freed)
// any inbound packet from the child's libc / dos cleanup chain
// carries a dangling dp_Port. The only safe target left is our
// own handler port — that's how rp_handler_die's self-DIE works.
static BOOL rp_can_reply(rp_state * st, struct DosPacket * pkt) {
    if (!st->child_exited) {
        return TRUE;
    }
    // Self-reply (e.g. the DIE we sent ourselves) is fine — our
    // own port is alive as long as this handler task is running.
    if (pkt->dp_Port == st->handler_port) {
        return TRUE;
    }
    return FALSE;
}

// Must be called with Forbid() held.
static void rp_fulfil_deferred(rp_state * st) {
    if (st->deferred_read_pkt == NULL) return;
    if (st->in.count == 0 && !st->in.writer_closed && !st->shutdown_requested) return;
    struct DosPacket * pkt = st->deferred_read_pkt;
    UBYTE * dst = (UBYTE *) pkt->dp_Arg2;
    LONG    n   = pkt->dp_Arg3;
    LONG popped = (LONG) rp_pop(&st->in, dst, (ULONG) n);
    st->deferred_read_pkt = NULL;
    // Reply only if the original sender's port is still alive
    // (it won't be if the child has exited — its Process struct
    // and message port have been freed by exec).
    rp_pkt_reply_ex(pkt, popped, 0, rp_can_reply(st, pkt));
}

static void rp_handler_entry(void) {
    struct Process * self = (struct Process *) FindTask(NULL);
    rp_state * st = (rp_state *) self->pr_ExitData;
    if (st == NULL) return;
    struct MsgPort * port = &self->pr_MsgPort;

    // NO DOS CALLS FROM HERE (Write/Read/Open/Close/printf/etc).
    // This is a custom DOS handler — its main loop reads packets
    // from `port`. Any DOS function we call internally PutMsg's
    // a request to a target FH's handler and then WaitPort's on
    // OUR OWN port for the reply, which can consume DIE/other
    // messages meant for us. The first such call (originally
    // `rp_log_event("handler started", ...)` here) ate the
    // first ACTION_DIE that close() sent right after, so the
    // handler stayed in WaitPort forever and amStudio's
    // seglist got UnLoadSeg'd while the handler was still
    // alive — causing the #80000004 / #87000004 alerts. We
    // log via the still-safe (main-task-only) callers in
    // rp_handler_die / shutdownAllNative instead. Confirmed
    // 2026-06-08 — also a "[rp] handler: shutdown observed"
    // rp_stdout_line was enough to crash with #87000004.
    // crash with #87000004 right after.

    g_handler_entered_count++;

    BOOL running = TRUE;
    while (running) {
        WaitPort(port);
        struct Message * msg;
        while ((msg = GetMsg(port)) != NULL) {
            // Skip replies to our own messages. The fire-and-forget
            // DIE we send via rp_handler_die has its reply port set
            // to our own port; without this guard we'd treat the
            // reply as a new packet and spin forever.
            if (msg->mn_Node.ln_Type == NT_REPLYMSG) {
                FreeMem(msg, msg->mn_Length);
                continue;
            }
            struct DosPacket * pkt = (struct DosPacket *) msg->mn_Node.ln_Name;
            if (pkt == NULL) continue;
            LONG type = pkt->dp_Type;
#if RP_VERBOSE_LOG
            // ACTION_DIE deliberately not logged even in verbose
            // mode — amStudio's main loop spins up tasks whose
            // pr_ConsoleTask inherits from us, and each sends a DIE
            // on exit (tens of thousands per session). Logging the
            // other types is useful for diagnosing handler protocol
            // issues; off by default because bebbossh's idle polling
            // fires ACTION_EXAMINE_FH dozens of times per second.
            if (type != ACTION_DIE) {
                rp_log_event("hnd RX type=", type);
            }
#endif

            Forbid();
            if (st->shutdown_requested) {
                g_handler_die_seen_count++;
                // Reply to the packet that woke us into the
                // shutdown branch — but only if its sender is
                // still alive (rp_can_reply gates this).
                BOOL reply_ok = rp_can_reply(st, pkt);
                switch (type) {
                    case ACTION_READ:       rp_pkt_reply_ex(pkt, 0, 0, reply_ok); break;
                    case ACTION_WRITE:      rp_pkt_reply_ex(pkt, pkt->dp_Arg3, 0, reply_ok); break;
                    case ACTION_FINDINPUT:
                    case ACTION_FINDOUTPUT: rp_pkt_reply_ex(pkt, DOSFALSE, 0, reply_ok); break;
                    default:                rp_pkt_reply_ex(pkt, DOSTRUE, 0, reply_ok); break;
                }
                // Drain whatever is CURRENTLY in the queue with
                // quick replies. Two things matter here:
                //
                // 1) The child's `ACTION_END` packets for its
                //    pr_CIS/pr_COS/pr_CES often arrive shortly
                //    before/after close()'s DIE. If we exit
                //    without replying to them, the FakeFile-
                //    Handles' Close() blocks (or worse, the OS
                //    later does PutMsg to our pr_MsgPort after
                //    the handler Process is gone — instant
                //    #80000004).
                //
                // 2) A run-away flooder (~1.5M packets/sec; root
                //    cause uncertain, possibly pr_ConsoleTask
                //    inheritance) traps a naïve `continue` in
                //    the inner GetMsg loop forever — confirmed
                //    2026-06-08 via HeadlessLoad. So this drain
                //    has a hard cap so we can never spin.
                //
                // SKIP DRAIN entirely when the child has already
                // exited: any packets queued behind the DIE come
                // from the dying child's libc/dos cleanup chain
                // and may sit in memory exec has freed (the
                // child's stack / DOS packet pool). GetMsg's
                // internal RemHead would dereference the freed
                // node's mln_Pred/mln_Succ pointers — instant
                // #87000004. Leaving the messages in the queue
                // is fine: exec reclaims them along with the
                // dying sender's task memory list.
                int drain_left = st->child_exited ? 0 : 64;
                struct Message * drain_msg;
                while (drain_left > 0 && (drain_msg = GetMsg(port)) != NULL) {
                    drain_left--;
                    if (drain_msg->mn_Node.ln_Type == NT_REPLYMSG) {
                        FreeMem(drain_msg, drain_msg->mn_Length);
                        continue;
                    }
                    struct DosPacket * dp = (struct DosPacket *) drain_msg->mn_Node.ln_Name;
                    if (dp == NULL) continue;
                    BOOL drain_reply_ok = rp_can_reply(st, dp);
                    switch (dp->dp_Type) {
                        case ACTION_READ:       rp_pkt_reply_ex(dp, 0, 0, drain_reply_ok); break;
                        case ACTION_WRITE:      rp_pkt_reply_ex(dp, dp->dp_Arg3, 0, drain_reply_ok); break;
                        case ACTION_FINDINPUT:
                        case ACTION_FINDOUTPUT: rp_pkt_reply_ex(dp, DOSFALSE, 0, drain_reply_ok); break;
                        default:                rp_pkt_reply_ex(dp, DOSTRUE, 0, drain_reply_ok); break;
                    }
                }
                running = FALSE;
                Permit();
                break;
            }

            // Post-mortem packet-reply safety. When the child has
            // already died, dp_Port on incoming packets may point
            // at the child's freed Process port. PutMsg into it
            // crashes the handler with #87000004. rp_can_reply
            // returns FALSE in that case; rp_pkt_reply_ex then
            // fills the result fields but skips the actual PutMsg.
            BOOL reply_ok = rp_can_reply(st, pkt);
            switch (type) {
                case ACTION_FINDINPUT:
                case ACTION_FINDOUTPUT: {
                    // After the child has died, pkt may live in the
                    // dead sender's freed memory; dp_Arg1 could be
                    // garbage. The fh writes below dereference
                    // BADDR(dp_Arg1) unconditionally — if we get
                    // unlucky on the recycled value we blast a word
                    // into some random place in exec's heap and
                    // alert with #87000004. Skip the write entirely
                    // when the child is gone.
                    if (!st->child_exited) {
                        struct FileHandle * fh = (struct FileHandle *) BADDR(pkt->dp_Arg1);
                        fh->fh_Type = port;
                        fh->fh_Arg1 = (LONG) st;
                        st->open_count++;
                        st->any_open = TRUE;
                    }
                    rp_pkt_reply_ex(pkt, st->child_exited ? DOSFALSE : DOSTRUE, 0, reply_ok);
                    break;
                }
                case ACTION_END: {
                    if (st->open_count > 0) st->open_count--;
                    rp_pkt_reply_ex(pkt, DOSTRUE, 0, reply_ok);
                    // Self-exit once the child has gone and every
                    // inherited FH is closed — no further legitimate
                    // traffic can arrive from the (now-dead) child, so
                    // hanging around just to read packets out of its
                    // freed memory is asking for an Address Error. The
                    // unconditional BADDR(pkt->dp_Arg*) writes in the
                    // FINDINPUT / FINDOUTPUT / DISK_INFO / EXAMINE_FH
                    // cases would happily blast a random word into
                    // freed-and-reused memory if a post-exit packet
                    // sneaks through; exiting here keeps that window
                    // from ever opening. close() may still PutMsg
                    // ACTION_DIE after this — handler_port is nulled
                    // in the teardown so the next rp_handler_die
                    // sees NULL and skips the PutMsg.
                    if (st->child_exited && st->open_count == 0
                        && st->any_open) {
                        st->shutdown_requested = TRUE;
                        running = FALSE;
                    }
                    break;
                }
                case ACTION_DIE:
                    // Advisory unless AmLang side has requested shutdown.
                    rp_pkt_reply_ex(pkt, DOSTRUE, 0, reply_ok);
                    break;
                case ACTION_WRITE: {
                    // If the child is gone, src+n point into
                    // recycled / freed memory; reading n bytes from
                    // a garbage src for a garbage n could walk off
                    // the end of any mapping. Drop the write
                    // silently — child is dead so there's no
                    // legitimate writer anyway.
                    if (st->child_exited) {
                        rp_pkt_reply_ex(pkt, pkt->dp_Arg3, 0, reply_ok);
                        break;
                    }
                    UBYTE * src = (UBYTE *) pkt->dp_Arg2;
                    LONG    n   = pkt->dp_Arg3;
                    // Intercept the AmigaOS "what's your terminal
                    // size?" query — bebbossh's getConsoleSize()
                    // (and any other libnix program that wants to
                    // know the console dimensions) sends 4 bytes:
                    //   \x9b  '0'  ' '  'q'
                    // (i.e. CSI 0 SP q, DECREQTPARM) via Write on
                    // its stdin FH, then Reads back the response on
                    // the same FH. A real CON: handler reflects the
                    // answer; we have to do the same or the caller
                    // times out and sends rows=cols=0 to the SSH
                    // server, which then opens a 0x0 PTY and nano
                    // / vim refuse to lay out.
                    //
                    // The answer format that bebbossh's parser
                    // expects ([console.cpp:51] in the source tree):
                    //   skip 5 bytes  -> tmp[5] is first rows digit
                    //   read digits until ';'   -> numRows
                    //   skip the ';'
                    //   read digits until ' '   -> numCols
                    // That maps to "CSI 1 ; <rows> ; <cols> SP q"
                    // when CSI is the 2-byte form "ESC [".
                    if (n == 4 && src[0] == 0x9b && src[1] == 0x30
                                 && src[2] == 0x20 && src[3] == 0x71) {
                        UBYTE resp[40];
                        int rp = 0;
                        resp[rp++] = 0x1b; resp[rp++] = '[';
                        resp[rp++] = '1';  resp[rp++] = ';';
                        LONG rows = st->reported_rows;
                        if (rows < 1) rows = 24;
                        // itoa rows
                        {
                            char tmp[12]; int t = 0;
                            if (rows == 0) tmp[t++] = '0';
                            while (rows > 0) { tmp[t++] = (char)('0' + (rows % 10)); rows /= 10; }
                            while (t > 0) resp[rp++] = (UBYTE) tmp[--t];
                        }
                        resp[rp++] = ';';
                        LONG cols = st->reported_cols;
                        if (cols < 1) cols = 80;
                        {
                            char tmp[12]; int t = 0;
                            if (cols == 0) tmp[t++] = '0';
                            while (cols > 0) { tmp[t++] = (char)('0' + (cols % 10)); cols /= 10; }
                            while (t > 0) resp[rp++] = (UBYTE) tmp[--t];
                        }
                        resp[rp++] = ' ';
                        resp[rp++] = 'q';
                        rp_push(&st->in, resp, (ULONG) rp);
                        rp_fulfil_deferred(st);
                        rp_pkt_reply_ex(pkt, n, 0, reply_ok);
                        break;
                    }
                    rp_push(&st->out, src, (ULONG) n);
                    rp_pkt_reply_ex(pkt, n, 0, reply_ok);
                    // Wake the main task AFTER the bytes are in the
                    // ring and the packet is replied. Signal'ing
                    // BEFORE the push lets drainProcess race in,
                    // see an empty ring, reschedule, and miss the
                    // data — the bytes only surface on the user's
                    // next keystroke.
                    rp_signal_main_wake();
                    break;
                }
                case ACTION_READ: {
                    if (st->child_exited) {
                        // Don't write into pkt->dp_Arg2 — after child
                        // exit it may be recycled memory. Reply 0
                        // (EOF) and drop the read.
                        rp_pkt_reply_ex(pkt, 0, 0, reply_ok);
                    } else if (st->in.count > 0) {
                        UBYTE * dst = (UBYTE *) pkt->dp_Arg2;
                        LONG    n   = pkt->dp_Arg3;
                        LONG popped = (LONG) rp_pop(&st->in, dst, (ULONG) n);
                        rp_pkt_reply_ex(pkt, popped, 0, reply_ok);
                    } else if (st->in.writer_closed) {
                        rp_pkt_reply_ex(pkt, 0, 0, reply_ok);
                    } else if (st->raw_mode) {
                        // Raw mode + empty ring -> reply 0 immediately
                        // instead of deferring. bebbossh's interactive
                        // event loop calls Read(stdin, p, 512) right
                        // after SetMode(1) fires (the !stdoutBptr
                        // fallback in handleKeyboard); if we deferred
                        // here, the eventLoop would wedge in that Read
                        // until the user typed a key, and the SSH
                        // socket would never get pumped. Returning 0
                        // makes bebbossh's `if (n <= 0) return;` fire,
                        // handleKeyboard returns, eventLoop runs
                        // WaitSelect, and the banner / shell prompt
                        // streams correctly.
                        //
                        // We still block (defer) when raw_mode is
                        // FALSE — that's how the password fgets in
                        // loginPass() waits for the user to finish
                        // typing.
                        rp_pkt_reply_ex(pkt, 0, 0, reply_ok);
                    } else {
                        if (st->deferred_read_pkt != NULL) {
                            rp_pkt_reply_ex(st->deferred_read_pkt, 0, 0,
                                            rp_can_reply(st, st->deferred_read_pkt));
                        }
                        st->deferred_read_pkt = pkt;
                    }
                    break;
                }
                case ACTION_WAIT_CHAR:
                    // Honest "is data ready right now?" reply.
                    // Returning DOSTRUE unconditionally (the prior
                    // behaviour) wedges bebbossh's post-login
                    // interactive loop: handleKeyboard polls with
                    // WaitForChar(stdinBptr, 1us) every event-loop
                    // tick — we'd say TRUE, it'd call Read(1) which
                    // we then DEFER on the empty ring, and the
                    // outer WaitSelect on the SSH socket never ran.
                    // Result: each user Enter pumped exactly one
                    // SSH packet, banner / shell output stalled.
                    //
                    // The earlier worry (fgets bails with EOF when
                    // WAIT_CHAR is FALSE) doesn't actually fire in
                    // our setup — libnix fgets in our HandlerTest
                    // reproducer never sends a WAIT_CHAR packet at
                    // all; it goes straight to ACTION_READ which we
                    // still defer correctly until writeInput pushes
                    // bytes. If a future code path does drive fgets
                    // through WAIT_CHAR we'll need to defer the
                    // packet with a timer fulfilment, but for the
                    // bebbossh shell loop the immediate honest
                    // answer is exactly what dos.library expects.
                    if (st->in.count > 0 || st->in.writer_closed) {
                        rp_pkt_reply_ex(pkt, DOSTRUE, 0, reply_ok);
                    } else {
                        rp_pkt_reply_ex(pkt, DOSFALSE, 0, reply_ok);
                    }
                    break;
                case ACTION_SCREEN_MODE:
                    // dp_Arg1 = TRUE (1) → raw mode; FALSE (0) → cooked.
                    // bebbossh sends SetMode(stdin, 1) at grabConsole;
                    // restoreConsole sends SetMode(stdin, 0) on exit.
                    // The AmLang side polls isRawMode() to switch CliView
                    // into char-at-a-time passthrough + terminal-emulator
                    // rendering vs the default line-buffered scrollback.
                    //
                    // ALWAYS logged (cheap — fires <= a few times per
                    // session, not in the hot path): without this we
                    // can't tell from a tty.log whether a missing
                    // raw-mode transition is because bebbossh skipped
                    // SetMode or because we mishandled the packet.
                    rp_log_event("[rp] SCREEN_MODE arg=", pkt->dp_Arg1);
                    st->raw_mode = (pkt->dp_Arg1 != 0) ? TRUE : FALSE;
                    rp_pkt_reply_ex(pkt, DOSTRUE, 0, reply_ok);
                    break;
                case ACTION_CHANGE_SIGNAL:
                    rp_pkt_reply_ex(pkt, DOSTRUE, 0, reply_ok);
                    break;
                case ACTION_DISK_INFO: {
                    // IsInteractive() in dos.library V36+ asks the
                    // handler "what disk type are you?" via this
                    // packet, then returns DOSTRUE iff id_DiskType
                    // equals the historical 'CON\0' sentinel
                    // (0x434F4E00). bebbossh's grabConsole() bails
                    // when IsInteractive returns FALSE, so without
                    // this our FHs look like a regular file and
                    // grabConsole never calls SetMode(stdin, 1),
                    // raw_mode never flips, the CLI panel never
                    // switches to passthrough.
                    //
                    // We populate the bare minimum — the disk-state
                    // field gets ID_VALIDATED so any caller that
                    // checks it sees a "healthy" volume rather than
                    // an unmounted error. Block counts stay zero;
                    // they're meaningless for a console stream and
                    // no AmigaOS code we care about reads them.
                    // Skip the BADDR write if the child is gone —
                    // see the FINDINPUT/FINDOUTPUT case above for the
                    // freed-memory / recycled-pointer rationale.
                    if (!st->child_exited) {
                        struct InfoData * id = (struct InfoData *) BADDR(pkt->dp_Arg2);
                        if (id != NULL) {
                            UBYTE * z = (UBYTE *) id;
                            ULONG i;
                            for (i = 0; i < sizeof(struct InfoData); i++) {
                                z[i] = 0;
                            }
                            id->id_DiskType = 0x434F4E00L;
                            id->id_DiskState = ID_VALIDATED;
                        }
                    }
                    rp_pkt_reply_ex(pkt, st->child_exited ? DOSFALSE : DOSTRUE, 0, reply_ok);
                    break;
                }
                case ACTION_SAME_LOCK:
                case ACTION_FH_FROM_LOCK:
                case ACTION_SEEK:           /* 1008 — stream is not seekable */
                    rp_pkt_reply_ex(pkt, -1, ERROR_ACTION_NOT_KNOWN, reply_ok);
                    break;
                case ACTION_EXAMINE_FH: {   /* 1034 */
                    // Two-mode behaviour, gated on raw_mode:
                    //
                    // raw_mode == FALSE (initial state, before
                    // bebbossh's eventLoop has triggered SetMode):
                    //   reply DOSFALSE. This makes bebbossh's
                    //   handleKeyboard take its `else SetMode(stdin,
                    //   1);` branch, which sends ACTION_SCREEN_MODE
                    //   with arg=1, which flips raw_mode to TRUE.
                    //   That's how raw mode actually engages — V40
                    //   IsInteractive does NOT return TRUE for our
                    //   AllocDosObject'd FHs (it uses some private
                    //   check, not ACTION_DISK_INFO as we'd hoped),
                    //   so grabConsole always bails, so the SetMode
                    //   in grabConsole never runs. The handleKeyboard
                    //   fallback is the only entry point left.
                    //
                    // raw_mode == TRUE:
                    //   reply DOSTRUE with fib_Size = current input
                    //   ring depth. This is the libnix non-blocking
                    //   polling idiom — caller does
                    //     ExamineFH; sz = fib_Size;
                    //     if (!sz) return;
                    //     n = Read(fh, p, sz);
                    //   so when the ring is empty (sz=0) the caller
                    //   bails out of handleKeyboard immediately and
                    //   eventLoop's WaitSelect runs on the SSH
                    //   socket, pumping output.
                    //
                    // The IMMEDIATE Read(stdin, p, 512) that follows
                    // SetMode in the !stdoutBptr branch is handled in
                    // ACTION_READ — when raw_mode is TRUE, an empty
                    // ring returns 0 instead of deferring, so the
                    // event loop doesn't wedge on that one Read.
                    if (!st->raw_mode || st->child_exited) {
                        // Plain DOSFALSE reply in the non-raw or
                        // child-gone cases — no BADDR write means
                        // no risk of corrupting memory through a
                        // recycled dp_Arg2 from a dead sender.
                        rp_pkt_reply_ex(pkt, DOSFALSE, 0, reply_ok);
                        break;
                    }
                    struct FileInfoBlock * fib =
                        (struct FileInfoBlock *) BADDR(pkt->dp_Arg2);
                    if (fib != NULL) {
                        UBYTE * z = (UBYTE *) fib;
                        for (ULONG i = 0; i < sizeof(struct FileInfoBlock); i++) {
                            z[i] = 0;
                        }
                        fib->fib_Size = (LONG) st->in.count;
                    }
                    rp_pkt_reply_ex(pkt, DOSTRUE, 0, reply_ok);
                    break;
                }
                default:
                    // No rp_log_event here — DOS-in-handler rule
                    // (see banner at function entry).
                    rp_pkt_reply_ex(pkt, DOSTRUE, 0, reply_ok);
                    break;
            }
            Permit();
        }
    }

    Forbid();
    if (st->deferred_read_pkt != NULL) {
        rp_pkt_reply_ex(st->deferred_read_pkt, 0, 0,
                        rp_can_reply(st, st->deferred_read_pkt));
        st->deferred_read_pkt = NULL;
    }
    // Null handler_port so any subsequent rp_handler_die call
    // (e.g. when the AmLang side gets ARC-released during
    // teardown) sees NULL and skips the PutMsg. Otherwise the
    // pr_MsgPort backing it is freed when this Process exits
    // and the PutMsg goes into freed memory.
    st->handler_port = NULL;
    // Drop the live counter so shutdownAllNative's poll can
    // unblock. We do this BEFORE rp_state_release because
    // release may free `st`.
    g_live_handler_count--;
    g_handler_exited_count++;
    Permit();

    // No rp_log_event here — DOS-in-handler rule (see banner
    // at rp_handler_entry top). rp_state_release also avoids
    // its log line for the same reason.
    rp_state_release(st);
}

// =================================================================
// Child-exit callback (NP_ExitCode)
// =================================================================
//
// AmigaOS calls this at child process exit, before the Process
// struct is freed. We flip child_exited on the shared state so the
// AmLang side's isAlive() observes the transition.

// NP_ExitCode hook — AmigaOS calls this with D0=return-code,
// D1=NP_ExitData. Without explicit register binding gcc reads from
// the stack and gets garbage, so the state pointer we set up via
// NP_ExitData arrives NULL and child_exited never flips.
static void __saveds rp_child_exit(
    register LONG status __asm("d0"),
    register LONG data   __asm("d1"))
{
    rp_log_event("rp_child_exit status=", status);
    rp_log_event("rp_child_exit data=",   data);
    rp_state * st = (rp_state *) data;
    if (st == NULL) return;
    Forbid();
    st->child_exited = TRUE;
    // Trip the same shutdown flag that close() would otherwise set
    // a moment later. The handler's shutdown branch responds to the
    // very next packet with safe DOSFALSE/DOSTRUE replies and breaks
    // out of the loop — without this, the handler stays alive in
    // its NORMAL switch for the post-exit packets, and the next
    // FINDINPUT / DISK_INFO / EXAMINE_FH that sneaks in writes
    // through BADDR(pkt->dp_Arg*) into freed memory and alerts with
    // #87000004. close()'s subsequent rp_handler_die becomes a
    // no-op when handler_port is already nulled by the exiting
    // handler, so this is purely additive — the existing path still
    // covers the case where the child crashes without firing
    // NP_ExitCode.
    st->shutdown_requested = TRUE;
    // Wake the handler if it's blocked on a deferred read so the
    // child's read consumers don't sit forever. We turn the
    // writer_closed flag on first, then fulfil any waiting read with
    // whatever's left (zero is OK — child won't read again).
    st->in.writer_closed = TRUE;
    rp_fulfil_deferred(st);
    Permit();
}

// =================================================================
// AmLang-facing
// =================================================================

static running_process_data * rp_data(aobject * const this) {
    if (this == NULL) return NULL;
    return (running_process_data *) this->object_properties.class_object_properties.object_data.value.custom_value;
}

function_result Am_Lang_RunningProcess__native_init_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    if (this != NULL) __increase_reference_count(this);
    running_process_data * d = calloc(1, sizeof(running_process_data));
    if (d != NULL) {
        this->object_properties.class_object_properties.object_data.value.custom_value = d;
    }
    if (this != NULL) __decrease_reference_count(this);
    return __result;
}

function_result Am_Lang_RunningProcess__native_mark_children_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    (void) this;
    return __result;
}

// Signal the handler to shut down. We DO NOT wait for the ack —
// blocking on WaitPort here would freeze the UI thread when this is
// called from _native_release_0 during GC, and was the source of
// the amStudio freeze observed after `!bebbossh`. Handler picks up
// shutdown_requested on its next packet (often ACTION_END from
// child exit). If it never sees another packet it'll stay alive as
// an orphan — costs one Process slot until amStudio exits, but
// doesn't deadlock anything.
//
// We do send an ACTION_DIE packet to wake the handler out of
// WaitPort, but as a fire-and-forget. Reply port is the handler's
// own pr_MsgPort so the ack just round-trips back to it and gets
// consumed by the next GetMsg / falls on the floor when the handler
// exits.
static void rp_handler_die(rp_state * st) {
    if (st == NULL) {
        rp_stdout_line("[rp] handler_die: st=NULL, skipping");
        return;
    }
    if (st->handler_port == NULL) {
        rp_stdout_line("[rp] handler_die: handler_port=NULL, skipping");
        return;
    }
    rp_stdout_line("[rp] handler_die: sending ACTION_DIE");
    Forbid(); st->shutdown_requested = TRUE; Permit();

    struct StandardPacket * sp = (struct StandardPacket *)
        AllocMem(sizeof(*sp), MEMF_PUBLIC | MEMF_CLEAR);
    if (sp == NULL) return;
    sp->sp_Msg.mn_Node.ln_Type = NT_MESSAGE;
    sp->sp_Msg.mn_Node.ln_Name = (char *) &sp->sp_Pkt;
    sp->sp_Msg.mn_ReplyPort    = st->handler_port; /* self-reply, no waiter */
    sp->sp_Msg.mn_Length       = sizeof(*sp);
    sp->sp_Pkt.dp_Link         = &sp->sp_Msg;
    sp->sp_Pkt.dp_Port         = st->handler_port;
    sp->sp_Pkt.dp_Type         = ACTION_DIE;
    PutMsg(st->handler_port, &sp->sp_Msg);
    /* sp leaks; handler exits before it could free its own. Bounded
     * by handler count = number of CLI commands run in this amStudio
     * session — small. */
}

function_result Am_Lang_RunningProcess__native_release_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    running_process_data * d = rp_data(this);
    if (d != NULL) {
        if (d->state != NULL) {
            rp_handler_die(d->state);
            rp_state_release(d->state);
            d->state = NULL;
        }
        // DON'T FreeDosObject the FakeFileHandles — V40 dos.library
        // (or libnix's exit cleanup, or both) appears to release the
        // FH memory when the child Closes its inherited stdin/stdout/
        // stderr. Calling FreeDosObject again here is a double-free
        // and bus-errors the main task right after the child exits.
        // Confirmed against the [HandlerTest/parent.c] reference: it
        // skips FreeDosObject, runs the same bebbossh child, and
        // exits cleanly. We follow the same convention.
        //
        // The leaked allocation is bounded: one set of 3 FHs per
        // spawn-and-close cycle, ~80 bytes apiece. For an interactive
        // CLI panel that's negligible across an amStudio session.
        // The bptrs are still zeroed so any code inspecting them
        // post-release sees them as "not present".
        d->fh_in_bptr  = 0;
        d->fh_out_bptr = 0;
        d->fh_err_bptr = 0;
        // If the spawn failed before NP_FreeSeglist took ownership,
        // we still hold the seg; unload it.
        if (d->child_seg != 0 && !d->child_owns_seg) {
            UnLoadSeg(d->child_seg);
            d->child_seg = 0;
        }
        free(d);
        this->object_properties.class_object_properties.object_data.value.custom_value = NULL;
    }
    return __result;
}

// Split "cmd args..." into the binary path and the argument tail
// (used as NP_Arguments). The arg tail is NEWLINE-terminated as
// libnix expects — without the trailing \n libnix's ReadArgs-style
// parser sometimes spins.
static void rp_split_cmd(const char * src, char * cmd_out, int cmd_max,
                                          char * args_out, int args_max) {
    int p = 0;
    while (src[p] != 0 && src[p] != ' ' && src[p] != '\t' && p < cmd_max - 1) {
        cmd_out[p] = src[p]; p++;
    }
    cmd_out[p] = 0;
    while (src[p] == ' ' || src[p] == '\t') p++;
    int a = 0;
    while (src[p] != 0 && a < args_max - 2) {
        args_out[a++] = src[p++];
    }
    args_out[a++] = '\n';
    args_out[a]   = 0;
}

// True iff `name` already contains a path separator — either '/' for
// a subdir or ':' for a volume / assign reference. Such names are
// taken verbatim and we don't do any PATH walking on them.
static BOOL rp_name_has_path(const char * name) {
    for (int i = 0; name[i] != 0; i++) {
        if (name[i] == '/' || name[i] == ':') return TRUE;
    }
    return FALSE;
}

// PathNode in AmigaOS dos.library is two BPTRs back-to-back:
//   offset 0: BPTR path_Next   (BPTR to next PathNode, 0 = end)
//   offset 4: BPTR path_Lock   (BPTR lock to the directory)
// The struct isn't in the public NDK headers but the layout is
// documented and stable across V36+. We model it locally.
struct rp_path_node {
    BPTR path_Next;
    BPTR path_Lock;
};

// Locate `name` along the AmigaShell PATH (pr_CLI->cli_CommandDir) +
// the C: assign, then LoadSeg() it. Returns the loaded segment BPTR
// or 0 if no candidate succeeded.
//
// Why we do this ourselves instead of relying on LoadSeg's own
// resolution: V40 dos.library's LoadSeg() honours only the current
// directory; it does NOT walk the Shell's PATH (cli_CommandDir
// chain) the way the Shell itself does when you type a bare
// command name. So a binary that lives in
// `amStudio:extensions/bebbossh/C/bebbosshkeygen`, added to PATH at
// install time via `Path amStudio:extensions/bebbossh/C ADD`, would
// be unspawnable from our handler even though the same bare command
// runs fine from a Shell window.
//
// We try in this order:
//   1. The name verbatim — handles absolute paths, volume-prefixed
//      names, and binaries that happen to be in the current dir.
//   2. Each entry in cli_CommandDir, prepended via NameFromLock so
//      we don't need to flip CurrentDir() per attempt.
//   3. "C:" prefix — every Workbench install has C: assigned and
//      most standard commands live there.
//
// Returns 0 if all attempts failed; the caller logs IoErr() of the
// LAST attempt for diagnostics.
static BPTR rp_loadseg_with_path(const char * name) {
    if (name == NULL || name[0] == 0) return 0;

    // 1. Verbatim. Also catches the case where the caller already
    //    passed a fully-qualified name.
    BPTR seg = LoadSeg((CONST_STRPTR) name);
    if (seg != 0) return seg;

    // If there's already a path separator, don't walk PATH — the
    // caller meant exactly that path; failing was the answer.
    if (rp_name_has_path(name)) return 0;

    // 2. Walk pr_CLI->cli_CommandDir.
    struct Process * self = (struct Process *) FindTask(NULL);
    struct CommandLineInterface * cli = (struct CommandLineInterface *) BADDR(self->pr_CLI);
    if (cli != NULL) {
        struct rp_path_node * node = (struct rp_path_node *) BADDR(cli->cli_CommandDir);
        char buf[260];
        while (node != NULL) {
            BPTR lock = node->path_Lock;
            if (lock != 0) {
                if (NameFromLock(lock, (STRPTR) buf, (LONG) sizeof(buf) - 1) != DOSFALSE) {
                    // Append the command name, inserting a '/' only
                    // when the dir name doesn't already end on a
                    // path separator (volume root "DH1:" ends in
                    // ':' and needs no slash).
                    int len = 0;
                    while (buf[len] != 0 && len < (int) sizeof(buf) - 2) len++;
                    if (len > 0 && buf[len - 1] != '/' && buf[len - 1] != ':'
                            && len < (int) sizeof(buf) - 2) {
                        buf[len++] = '/';
                        buf[len] = 0;
                    }
                    int n = 0;
                    while (name[n] != 0 && len + n < (int) sizeof(buf) - 1) {
                        buf[len + n] = name[n];
                        n++;
                    }
                    buf[len + n] = 0;
                    seg = LoadSeg((CONST_STRPTR) buf);
                    if (seg != 0) return seg;
                }
            }
            node = (struct rp_path_node *) BADDR(node->path_Next);
        }
    }

    // 3. C: fallback. Every standard Workbench install has C:
    //    assigned to wherever the shell commands live.
    char buf[260];
    buf[0] = 'C'; buf[1] = ':';
    int n = 0;
    while (name[n] != 0 && n < (int) sizeof(buf) - 3) {
        buf[2 + n] = name[n];
        n++;
    }
    buf[2 + n] = 0;
    seg = LoadSeg((CONST_STRPTR) buf);
    return seg;  // 0 on failure
}

function_result Am_Lang_RunningProcess_startNative_0(aobject * const this, aobject * command, aobject * workingDir) {
    function_result __result = { .has_return_value = false };
    if (this != NULL) __increase_reference_count(this);
    if (command != NULL) __increase_reference_count(command);
    if (workingDir != NULL) __increase_reference_count(workingDir);

    rp_log_open();
    rp_log_event("startNative", 0);

    running_process_data * d = rp_data(this);
    if (d == NULL) {
        __throw_simple_exception("RunningProcess: object_data missing", "in startNative", &__result);
        goto __exit;
    }

    rp_state * st = (rp_state *) AllocMem(sizeof(*st), MEMF_PUBLIC | MEMF_CLEAR);
    if (st == NULL) {
        __throw_simple_exception("RunningProcess: AllocMem(state) failed", "in startNative", &__result);
        goto __exit;
    }
    st->refcount = 1;
    st->reported_rows = 24;
    st->reported_cols = 80;
    st->in.data  = (UBYTE *) AllocMem(RP_RING_SIZE, MEMF_PUBLIC);
    st->out.data = (UBYTE *) AllocMem(RP_RING_SIZE, MEMF_PUBLIC);
    if (st->in.data == NULL || st->out.data == NULL) {
        if (st->in.data  != NULL) FreeMem(st->in.data,  RP_RING_SIZE);
        if (st->out.data != NULL) FreeMem(st->out.data, RP_RING_SIZE);
        FreeMem(st, sizeof(*st));
        __throw_simple_exception("RunningProcess: AllocMem(ring) failed", "in startNative", &__result);
        goto __exit;
    }
    d->state = st;

    // Capture the parent task's Output() FH so handler tasks
    // (which inherit NIL: as their stdout) have a real FH to
    // log to. Idempotent — first call wins, subsequent calls
    // are no-ops.
    if (g_parent_stdout == 0) {
        g_parent_stdout = Output();
    }

    // Spawn handler Process.
    Forbid();
    st->handler_proc = CreateNewProcTags(
        NP_Entry,     (ULONG) rp_handler_entry,
        NP_Name,      (ULONG) "amStudioTTY",
        NP_StackSize, 8192,
        TAG_DONE);
    if (st->handler_proc != NULL) {
        st->handler_proc->pr_ExitData = (LONG) st;
        st->handler_port = &st->handler_proc->pr_MsgPort;
        st->refcount++;
        // Register on the global list NOW (still inside Forbid)
        // so shutdownAllNative can find us if the AmLang program
        // exits before this RunningProcess is otherwise released.
        rp_list_add(st);
        // Independent counter shutdownAllNative polls. The list
        // alone isn't enough because rp_state_release only frees
        // (and thus unlinks) when refcount hits 0 — which means
        // the AmLang object also has to be released. At runOnExit
        // the AmLang object may still be live in a static field.
        g_live_handler_count++;
    }
    Permit();
    if (st->handler_proc == NULL) {
        __throw_simple_exception("RunningProcess: CreateNewProc(handler) failed", "in startNative", &__result);
        goto __exit;
    }
    rp_log_event("[rp] handler_port=", (LONG) st->handler_port);

    // Allocate FakeFileHandles via AllocDosObject(DOS_FILEHANDLE).
    // Plain AllocMem(sizeof(struct FileHandle)) gives only the public
    // 44-byte struct, but V40 dos.library has PRIVATE fields beyond
    // that which FPuts (LVO -342) requires. Without those, FPuts
    // silently drops writes — which is why bebbossh's amiprintf
    // (which routes through FPuts via amistdio.h) was invisible
    // to our handler. Confirmed via child5 in HandlerTest harness.
    struct FileHandle * fh_in  = (struct FileHandle *) AllocDosObject(DOS_FILEHANDLE, NULL);
    struct FileHandle * fh_out = (struct FileHandle *) AllocDosObject(DOS_FILEHANDLE, NULL);
    struct FileHandle * fh_err = (struct FileHandle *) AllocDosObject(DOS_FILEHANDLE, NULL);
    if (fh_in == NULL || fh_out == NULL || fh_err == NULL) {
        if (fh_in  != NULL) FreeDosObject(DOS_FILEHANDLE, fh_in);
        if (fh_out != NULL) FreeDosObject(DOS_FILEHANDLE, fh_out);
        if (fh_err != NULL) FreeDosObject(DOS_FILEHANDLE, fh_err);
        __throw_simple_exception("RunningProcess: AllocDosObject(fh) failed", "in startNative", &__result);
        goto __exit;
    }
    fh_in->fh_Type  = st->handler_port;
    fh_in->fh_Port  = st->handler_port;        // non-NULL → IsInteractive=TRUE
    fh_in->fh_Arg1  = (LONG) st;
    fh_out->fh_Type = st->handler_port;
    fh_out->fh_Port = st->handler_port;
    fh_out->fh_Arg1 = (LONG) st;
    fh_err->fh_Type = st->handler_port;
    fh_err->fh_Port = st->handler_port;
    fh_err->fh_Arg1 = (LONG) st;
    d->fh_in_bptr  = MKBADDR(fh_in);
    d->fh_out_bptr = MKBADDR(fh_out);
    d->fh_err_bptr = MKBADDR(fh_err);
    st->open_count = 3;
    st->any_open   = TRUE;

    // Banner — lands in the panel as the first thing the user sees.
    {
        char banner[160]; int p = 0;
        const char * pre = "[amStudio] tty ready, log=";
        while (*pre) banner[p++] = *pre++;
        if (g_log_path[0] != 0) {
            for (int i = 0; g_log_path[i] != 0; i++) banner[p++] = g_log_path[i];
        } else {
            const char * np = "(none)"; while (*np) banner[p++] = *np++;
        }
        banner[p++] = '\n';
        Forbid();
        rp_push(&st->out, (const UBYTE *) banner, (ULONG) p);
        Permit();
    }

    // CWD swap (LoadSeg honours current dir).
    if (workingDir != NULL) {
        string_holder * wd_holder = (string_holder *) (workingDir + 1);
        const char * wd_str = wd_holder->string_value;
        if (wd_str != NULL && wd_str[0] != 0) {
            BPTR new_lock = Lock((CONST_STRPTR) wd_str, ACCESS_READ);
            if (new_lock != 0) {
                d->old_cwd_lock = CurrentDir(new_lock);
                d->has_old_cwd = TRUE;
            }
        }
    }

    // Parse "binary args..." and LoadSeg.
    string_holder * cmd_holder = (string_holder *) (command + 1);
    const char * cmd_str = (cmd_holder != NULL) ? cmd_holder->string_value : NULL;
    if (cmd_str == NULL || cmd_str[0] == 0) {
        if (d->has_old_cwd) {
            BPTR nl = CurrentDir(d->old_cwd_lock);
            if (nl != 0) UnLock(nl);
            d->has_old_cwd = FALSE;
        }
        __throw_simple_exception("RunningProcess: empty command", "in startNative", &__result);
        goto __exit;
    }
    static char g_cmd_buf[256];
    static char g_arg_buf[512];
    rp_split_cmd(cmd_str, g_cmd_buf, sizeof(g_cmd_buf), g_arg_buf, sizeof(g_arg_buf));
    rp_log_str("[rp] LoadSeg "); rp_log_str(g_cmd_buf); rp_log_str("\n");
    rp_log_str("[rp] args=");    rp_log_str(g_arg_buf);

    BPTR seg = rp_loadseg_with_path(g_cmd_buf);
    if (seg == 0) {
        rp_log_event("[rp] LoadSeg failed, IoErr=", IoErr());
        if (d->has_old_cwd) {
            BPTR nl = CurrentDir(d->old_cwd_lock);
            if (nl != 0) UnLock(nl);
            d->has_old_cwd = FALSE;
        }
        __throw_simple_exception("RunningProcess: LoadSeg failed (binary not found)", "in startNative", &__result);
        goto __exit;
    }
    d->child_seg = seg;

    // Spawn the child. NP_FreeSeglist=TRUE so the child unloads its
    // own seg on exit.
    Forbid();
    struct Process * child = CreateNewProcTags(
        NP_Seglist,     (ULONG) seg,
        NP_FreeSeglist, TRUE,
        NP_Cli,         TRUE,
        NP_Input,       (ULONG) d->fh_in_bptr,
        NP_Output,      (ULONG) d->fh_out_bptr,
        NP_Error,       (ULONG) d->fh_err_bptr,    /* ignored on V40, see below */
        NP_ConsoleTask, (ULONG) st->handler_port,
        NP_Arguments,   (ULONG) g_arg_buf,
        NP_Name,        (ULONG) "amStudioChild",
        NP_StackSize,   32768,
        NP_ExitCode,    (ULONG) rp_child_exit,
        NP_ExitData,    (LONG)  st,
        TAG_DONE);
    // NDK note in dostags.h: "V40 DID NOT, unlike claimed, support
    // NP_Error and NP_CloseError." On Kickstart 3.1 (V40) NP_Error
    // is silently dropped — pr_CES stays 0, and libnix's stdio
    // init then opens CONSOLE: (a fresh visible window) as stderr.
    // bebbossh writes its cert / password prompt to that stderr,
    // so we never see it in the panel and the user gets a popup
    // CON: window instead. Manually set pr_CES while Forbid is
    // held so the child hasn't run yet.
    if (child != NULL) {
        // V40 fix: NP_Error silently dropped, set pr_CES manually.
        child->pr_CES = d->fh_err_bptr;

        // V40 fix: CLI struct's standard I/O fields aren't fully
        // populated from NP_Input/NP_Output either — some programs
        // (incl. stock C:Version) write via cli_CurrentOutput rather
        // than pr_COS, and that field's left as whatever dos.library
        // happened to inherit. Set them all explicitly so the child
        // has no ambiguity.
        struct CommandLineInterface * cli =
            (struct CommandLineInterface *) BADDR(child->pr_CLI);
        if (cli != NULL) {
            cli->cli_StandardInput  = d->fh_in_bptr;
            cli->cli_CurrentInput   = d->fh_in_bptr;
            cli->cli_StandardOutput = d->fh_out_bptr;
            cli->cli_CurrentOutput  = d->fh_out_bptr;
            rp_log_event("[rp] CLI patched, cli=", (LONG) cli);
        } else {
            rp_log_event("[rp] no CLI on child (pr_CLI=", (LONG) child->pr_CLI);
        }
    }
    Permit();

    // Restore CWD regardless of spawn outcome.
    if (d->has_old_cwd) {
        BPTR nl = CurrentDir(d->old_cwd_lock);
        if (nl != 0) UnLock(nl);
        d->has_old_cwd = FALSE;
        d->old_cwd_lock = 0;
    }

    if (child == NULL) {
        rp_log_event("[rp] CreateNewProc(child) failed", 0);
        UnLoadSeg(seg);
        d->child_seg = 0;
        __throw_simple_exception("RunningProcess: CreateNewProc(child) failed", "in startNative", &__result);
        goto __exit;
    }
    d->child_owns_seg = TRUE;
    // Confirm the new process actually got our FH wiring + handler
    // port. If any of these don't match what we passed in NP_*, the
    // tag wasn't honoured for some reason and we'll see writes go
    // elsewhere.
    rp_log_event("[rp] child pr_CIS=",         (LONG) child->pr_CIS);
    rp_log_event("[rp] child pr_COS=",         (LONG) child->pr_COS);
    rp_log_event("[rp] child pr_CES=",         (LONG) child->pr_CES);
    rp_log_event("[rp] child pr_ConsoleTask=", (LONG) child->pr_ConsoleTask);
    rp_log_event("[rp] expected fh_out_bptr=", (LONG) d->fh_out_bptr);
    rp_log_event("[rp] expected handler_port=",(LONG) st->handler_port);

__exit: ;
    if (this != NULL) __decrease_reference_count(this);
    if (command != NULL) __decrease_reference_count(command);
    if (workingDir != NULL) __decrease_reference_count(workingDir);
    return __result;
}

function_result Am_Lang_RunningProcess_tryReadOutput_0(aobject * const this) {
    function_result __result = { .has_return_value = true };
    if (this != NULL) __increase_reference_count(this);
    __result.return_value.value.object_value = __create_string("", &Am_Lang_String);

#if RP_VERBOSE_LOG
    // Periodic Signal-counter dump for verifying the wake mechanism
    // (Forbid-block logging from the handler hot path crashed
    // earlier; doing it here on the main thread is safe). Off by
    // default — fires on every drainProcess tick, which is N times
    // per second of an active session.
    static ULONG s_last_sig = 0;
    static ULONG s_last_skip = 0;
    if (g_wake_signal_count != s_last_sig || g_wake_skipped_count != s_last_skip) {
        char lbuf[80]; int lp = 0;
        const char * lbl = "[wake-stats] sig="; while (*lbl) lbuf[lp++] = *lbl++;
        ULONG v = g_wake_signal_count;
        char tmp[12]; int t = 0;
        if (v == 0) tmp[t++] = '0';
        while (v > 0) { tmp[t++] = (char)('0' + (v % 10)); v /= 10; }
        while (t > 0) lbuf[lp++] = tmp[--t];
        const char * lbl2 = " skipped="; while (*lbl2) lbuf[lp++] = *lbl2++;
        v = g_wake_skipped_count; t = 0;
        if (v == 0) tmp[t++] = '0';
        while (v > 0) { tmp[t++] = (char)('0' + (v % 10)); v /= 10; }
        while (t > 0) lbuf[lp++] = tmp[--t];
        lbuf[lp++] = '\n';
        if (g_log_fh != 0) { Write(g_log_fh, lbuf, lp); Flush(g_log_fh); }
        s_last_sig = g_wake_signal_count;
        s_last_skip = g_wake_skipped_count;
    }
#endif

    running_process_data * d = rp_data(this);
    if (d == NULL || d->state == NULL) goto __exit;

    UBYTE buf[2048];
    Forbid();
    ULONG n = rp_pop(&d->state->out, buf, sizeof(buf) - 1);
    Permit();
    if (n > 0) {
        buf[n] = 0;
        __result.return_value.value.object_value = __create_string((char const *) buf, &Am_Lang_String);
    }

__exit: ;
    if (this != NULL) __decrease_reference_count(this);
    return __result;
}

function_result Am_Lang_RunningProcess_writeInput_0(aobject * const this, aobject * text) {
    function_result __result = { .has_return_value = false };
    if (this != NULL) __increase_reference_count(this);
    if (text != NULL) __increase_reference_count(text);

    running_process_data * d = rp_data(this);
    if (d == NULL || d->state == NULL || text == NULL) goto __exit;
    rp_state * st = d->state;
    if (st->child_exited) goto __exit;

    string_holder * h = (string_holder *) (text + 1);
    if (h == NULL || h->string_value == NULL) goto __exit;
    // AmLang strings are NOT necessarily \0-terminated — use the
    // explicit `length` field. Using strlen here previously made
    // writeInput silently truncate at the first incidental \0 in
    // adjacent memory.
    LONG len = (LONG) h->length;
    if (len <= 0) goto __exit;

    // Log the actual writeInput bytes — length-bounded so we use
    // h->length (AmLang strings are NOT \0-terminated). Both a
    // textual line and a hex dump help spot stray control chars.
    {
        char tbuf[200]; int tp = 0;
        const char * lbl = "[rp] writeInput[";
        while (*lbl) tbuf[tp++] = *lbl++;
        // numeric length
        LONG v = len; int dt = 0; char tmp[12];
        if (v == 0) tmp[dt++] = '0';
        while (v > 0) { tmp[dt++] = (char)('0' + (v % 10)); v /= 10; }
        while (dt > 0) tbuf[tp++] = tmp[--dt];
        tbuf[tp++] = ']'; tbuf[tp++] = ':'; tbuf[tp++] = ' ';
        LONG i = 0;
        while (i < len && tp < 180) {
            UBYTE b = (UBYTE) h->string_value[i];
            tbuf[tp++] = (b >= 32 && b < 127) ? (char) b : '.';
            i++;
        }
        tbuf[tp++] = '\n';
        if (g_log_fh != 0) Write(g_log_fh, tbuf, tp);

        char hbuf[200]; int hp = 0;
        const char * pre = "[rp]   bytes=";
        while (*pre) hbuf[hp++] = *pre++;
        i = 0;
        while (i < len && hp < 180) {
            UBYTE b = (UBYTE) h->string_value[i];
            const char * hx = "0123456789abcdef";
            hbuf[hp++] = hx[(b >> 4) & 0xF];
            hbuf[hp++] = hx[b & 0xF];
            hbuf[hp++] = ' ';
            i++;
        }
        hbuf[hp++] = '\n';
        if (g_log_fh != 0) Write(g_log_fh, hbuf, hp);
    }
    Forbid();
    rp_push(&st->in, (const UBYTE *) h->string_value, (ULONG) len);
    rp_fulfil_deferred(st);
    Permit();

__exit: ;
    if (this != NULL) __decrease_reference_count(this);
    if (text != NULL) __decrease_reference_count(text);
    return __result;
}

function_result Am_Lang_RunningProcess_isAlive_0(aobject * const this) {
    function_result __result = { .has_return_value = true };
    if (this != NULL) __increase_reference_count(this);
    running_process_data * d = rp_data(this);
    BOOL alive = FALSE;
    if (d != NULL && d->state != NULL) {
        Forbid();
        BOOL exited = d->state->child_exited;
        ULONG queued = d->state->out.count;
        Permit();
        if (!exited || queued > 0) alive = TRUE;
    }
    __result.return_value.value.bool_value = alive ? true : false;
    if (this != NULL) __decrease_reference_count(this);
    return __result;
}

function_result Am_Lang_RunningProcess_setReportedSize_0(aobject * const this, int var_rows, int var_cols) {
    function_result __result = { .has_return_value = false };
    if (this != NULL) __increase_reference_count(this);
    running_process_data * d = rp_data(this);
    if (d != NULL && d->state != NULL) {
        LONG r = (LONG) var_rows;
        LONG c = (LONG) var_cols;
        if (r < 1) r = 1;
        if (c < 1) c = 1;
        d->state->reported_rows = r;
        d->state->reported_cols = c;
    }
    if (this != NULL) __decrease_reference_count(this);
    return __result;
}

function_result Am_Lang_RunningProcess_isRawMode_0(aobject * const this) {
    function_result __result = { .has_return_value = true };
    if (this != NULL) __increase_reference_count(this);
    running_process_data * d = rp_data(this);
    BOOL raw = FALSE;
    if (d != NULL && d->state != NULL) {
        raw = d->state->raw_mode;
    }
    __result.return_value.value.bool_value = raw ? true : false;
    if (this != NULL) __decrease_reference_count(this);
    return __result;
}

function_result Am_Lang_RunningProcess_close_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    if (this != NULL) __increase_reference_count(this);
    running_process_data * d = rp_data(this);
    if (d != NULL && d->state != NULL) {
        // These two log_event calls run on the AmLang main task
        // (not the handler), so the dos-in-handler rule doesn't
        // apply — Write to g_log_fh is safe. They bracket the
        // close so a tty.log that ends after "close: enter" but
        // before "close: done" tells us the AmLang side is the
        // one crashing, not the handler.
        rp_log_event("[rp] close: enter, live=", g_live_handler_count);
        rp_handler_die(d->state);
        rp_state_release(d->state);
        d->state = NULL;
        rp_log_event("[rp] close: done, live=", g_live_handler_count);
    }
    if (this != NULL) __decrease_reference_count(this);
    return __result;
}

// Static method — sets the process-global wake target for ALL future
// (and current) handler tasks. Pass taskPtr=0 to clear.
function_result Am_Lang_RunningProcess_setGlobalWake_0(long long var_taskPtr, int var_sigBit) {
    function_result __result = { .has_return_value = false };
    g_wake_task = (struct Task *) (ULONG) var_taskPtr;
    if (var_sigBit >= 0 && var_sigBit < 32) {
        g_wake_sig_bit = (UBYTE) var_sigBit;
    } else {
        g_wake_sig_bit = 0;
        g_wake_task = NULL;
    }
    rp_log_event("[rp] setGlobalWake taskPtr=", (LONG) var_taskPtr);
    rp_log_event("[rp] setGlobalWake sigBit=", (LONG) var_sigBit);
    return __result;
}

// Force every still-running handler Process to terminate, then
// wait briefly for each to actually go away. Called from the
// `#runOnExit` hook in RunningProcess.aml so it fires after
// `main()` returns but before the runtime tears down statics.
//
// Why we can't just rely on per-instance release: when the
// AmLang program exits "the hard way" (window-close → main
// returns without explicitly close()-ing each CliApp's
// RunningProcess), the AmLang GC never runs and the handler
// Processes stay in WaitPort forever. The C runtime then
// UnLoadSeg()s the program; the handlers' code pages get
// freed; the next time an idle handler is dispatched it
// executes garbage and the user sees an alert with the stale
// `amStudioTTY` name. Solving that needs a sweep at exit time
// that walks every live handler and tells it to die.
//
// Strategy: snapshot the list of handler Processes (so we
// don't have to hold Forbid for the duration of the wait),
// send each one an ACTION_DIE + flag shutdown_requested, then
// poll-with-Delay until the global list empties. Capped at
// ~5s total wait — generous enough that the only handlers
// still alive past this point are genuinely wedged, in
// which case we move on to the RemTask backstop below
// rather than just sleeping.
#define RP_SHUTDOWN_TIMEOUT_TICKS    250  // 250 ticks = ~5 seconds
#define RP_SHUTDOWN_POLL_TICKS         2  // 2 ticks   = ~40 ms per poll
// After the polite wait we do up to two re-DIE rounds before
// resorting to RemTask. Each round retries the polite DIE
// path so a handler that was simply behind on processing has
// one more chance to exit cleanly. The Signal() nudge wakes
// any task that's mysteriously not seeing PutMsg's signal
// (defensive — shouldn't happen, but cheap).
#define RP_SHUTDOWN_REDIE_ROUNDS       2
#define RP_SHUTDOWN_REDIE_TICKS       50  // ~1s per round

function_result Am_Lang_RunningProcess_shutdownAllNative_0(void) {
    function_result __result = { .has_return_value = false };

    // Mirror the per-stage counters into tty.log so a quit-time
    // crash leaves a trail in the file (rp_stdout_line goes to
    // the parent shell's stdout, which evaporates if you launched
    // amStudio from Workbench).
    rp_log_event("[rp] shutdownAll: enter live=", g_live_handler_count);
    rp_log_event("[rp] shutdownAll: entered=",    g_handler_entered_count);
    rp_log_event("[rp] shutdownAll: die_seen=",   g_handler_die_seen_count);
    rp_log_event("[rp] shutdownAll: exited=",     g_handler_exited_count);

    // Print a single combined diagnostic line covering all four
    // counters so we can tell at a glance which segment of the
    // handler lifecycle is hanging:
    //   live     — handlers tracked as alive (++start, --end)
    //   entered  — handler entry function actually ran
    //   die_seen — handler observed shutdown_requested
    //   exited   — handler reached its exit cleanup
    {
        char msg[160]; int p = 0;
        const char * pre = "[rp] shutdownAll: live=";
        while (*pre) msg[p++] = *pre++;
        LONG values[4] = {
            g_live_handler_count,
            g_handler_entered_count,
            g_handler_die_seen_count,
            g_handler_exited_count
        };
        const char * labels[4] = { "", " entered=", " die_seen=", " exited=" };
        for (int k = 0; k < 4; k++) {
            const char * lab = labels[k];
            while (*lab) msg[p++] = *lab++;
            LONG v = values[k]; BOOL neg = (v < 0); if (neg) v = -v;
            char tmp[12]; int t = 0;
            if (v == 0) tmp[t++] = '0';
            while (v > 0) { tmp[t++] = (char)('0' + v % 10); v /= 10; }
            if (neg) msg[p++] = '-';
            while (t > 0) msg[p++] = tmp[--t];
        }
        msg[p] = 0;
        rp_stdout_line(msg);
    }

    LONG live = g_live_handler_count;
    if (live <= 0) {
        rp_stdout_line("[rp] shutdownAll: no live handlers");
        return __result;
    }

    // Walk the list once under Forbid, mark every handler for
    // shutdown, and snapshot the ports we still need to DIE
    // into a local array. We send the DIEs OUTSIDE Forbid
    // because rp_handler_die calls AllocMem internally.
    //
    // 64 handlers is way more than any realistic amStudio
    // session — if you exceed this you've got bigger problems
    // than menu-item cleanup. Excess handlers fall through to
    // the poll loop and time out unkilled.
    #define RP_MAX_SHUTDOWN_TARGETS 64
    rp_state * targets[RP_MAX_SHUTDOWN_TARGETS];
    int n_targets = 0;

    // Snapshot live handlers under Forbid AND pin each by
    // bumping refcount. Without the pin a handler that exits
    // between our Permit() and the rp_handler_die() call would
    // free its rp_state, leaving us pointing at freed memory.
    // The matching rp_state_release at the end drops the pin.
    Forbid();
    rp_state * cur = g_handler_list_head;
    while (cur != NULL && n_targets < RP_MAX_SHUTDOWN_TARGETS) {
        cur->shutdown_requested = TRUE;
        if (cur->handler_port != NULL) {
            cur->refcount++;
            targets[n_targets++] = cur;
        }
        cur = cur->next_handler;
    }
    Permit();

    // Probe whether the handler Task we tracked is actually
    // still in exec's task list. FindTask returns NULL if no
    // task with that name exists. If we see live=1 but
    // FindTask returns NULL, the handler died via some path
    // that bypassed our counter decrement (e.g. a crash /
    // RemTask). If FindTask returns non-NULL, the handler is
    // alive but ignoring DIE.
    {
        struct Task * found = FindTask((STRPTR) "amStudioTTY");
        char msg[80]; int p = 0;
        const char * pre = "[rp] shutdownAll: FindTask(amStudioTTY)=";
        while (*pre) msg[p++] = *pre++;
        if (found == NULL) {
            const char * tag = "NULL";
            while (*tag) msg[p++] = *tag++;
        } else {
            const char * tag = "ALIVE";
            while (*tag) msg[p++] = *tag++;
        }
        msg[p] = 0;
        rp_stdout_line(msg);
    }

    // Send DIE to each captured handler. Each one wakes the
    // handler out of WaitPort; it sees shutdown_requested and
    // exits its main loop, ending with g_live_handler_count--.
    for (int i = 0; i < n_targets; i++) {
        rp_handler_die(targets[i]);
    }

    // Poll the live counter. Each tick is ~20ms (50 Hz);
    // RP_SHUTDOWN_TIMEOUT_TICKS=250 → ~5s cap. In practice a
    // handler picks up the DIE and exits within one or two
    // ticks, so we typically return well under 100ms — the
    // cap is generous so that a behind-on-processing handler
    // can still complete its drain rather than being killed.
    ULONG waited = 0;
    while (waited < RP_SHUTDOWN_TIMEOUT_TICKS) {
        if (g_live_handler_count <= 0) break;
        Delay(RP_SHUTDOWN_POLL_TICKS);
        waited += RP_SHUTDOWN_POLL_TICKS;
    }

    // Re-DIE rounds: any handler still alive past the polite
    // wait gets another ACTION_DIE plus a Signal() nudge in
    // case its first DIE went into a packet queue we couldn't
    // get past, or the handler somehow missed the port's wake
    // signal. Each round waits a smaller window for results.
    if (g_live_handler_count > 0) {
        for (int round = 0; round < RP_SHUTDOWN_REDIE_ROUNDS; round++) {
            if (g_live_handler_count <= 0) break;
            // Snapshot live handlers again — between rounds, some
            // may have exited and unlinked themselves, so the
            // original `targets[]` array can have stale pointers.
            rp_state * roundTargets[RP_MAX_SHUTDOWN_TARGETS];
            int roundN = 0;
            Forbid();
            rp_state * walk = g_handler_list_head;
            while (walk != NULL && roundN < RP_MAX_SHUTDOWN_TARGETS) {
                walk->shutdown_requested = TRUE;
                if (walk->handler_port != NULL) {
                    walk->refcount++;
                    roundTargets[roundN++] = walk;
                }
                walk = walk->next_handler;
            }
            Permit();
            for (int i = 0; i < roundN; i++) {
                rp_handler_die(roundTargets[i]);
                // Wake the handler's task by raising any pending
                // signal it might be sleeping on. The handler's
                // WaitPort waits on its message port's signal bit;
                // PutMsg already raises that bit, but we Signal()
                // SIGBREAKF_CTRL_C as a defensive nudge — if the
                // port's signal is stuck for any reason, this
                // forces a Wait() return. The handler's main loop
                // re-checks shutdown_requested on every packet, so
                // a spurious wake-without-message is harmless.
                if (roundTargets[i]->handler_proc != NULL) {
                    Signal((struct Task *) roundTargets[i]->handler_proc,
                           SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_F);
                }
            }
            ULONG round_waited = 0;
            while (round_waited < RP_SHUTDOWN_REDIE_TICKS) {
                if (g_live_handler_count <= 0) break;
                Delay(RP_SHUTDOWN_POLL_TICKS);
                round_waited += RP_SHUTDOWN_POLL_TICKS;
            }
            waited += round_waited;
            // Drop the pins; matched to the refcount++ above.
            for (int i = 0; i < roundN; i++) {
                rp_state_release(roundTargets[i]);
            }
        }
    }

    // Drop the pins taken in the initial snapshot. Matched to the
    // refcount++ inside the original Forbid block above.
    for (int i = 0; i < n_targets; i++) {
        rp_state_release(targets[i]);
    }

    // Last resort: any handler still alive at this point is
    // genuinely wedged. Letting it survive past UnLoadSeg means
    // the next packet that wakes it will execute freed code and
    // pop the #87000004 Software Failure alert under the stale
    // `amStudioTTY` name. RemTask removes the task before that
    // can happen. Resources tracked via TC_MemEntry (the stack,
    // any allocations the task did via its own pool) get freed
    // by exec on RemTask; our rp_state / ring buffers leak (no
    // callback path to reclaim them once RemTask hits), but the
    // process is exiting anyway so this is just a clean-exit
    // tradeoff.
    if (g_live_handler_count > 0) {
        rp_stdout_line("[rp] shutdownAll: RemTask on wedged handlers");
        rp_log_event("[rp] shutdownAll: RemTask wedged, live=", g_live_handler_count);
        Forbid();
        rp_state * cur2 = g_handler_list_head;
        while (cur2 != NULL) {
            rp_state * next = cur2->next_handler;
            struct Process * hp = cur2->handler_proc;
            // Defensive guard: only RemTask if the proc pointer is
            // still set AND we haven't already nulled handler_port
            // (which the handler exit path does after Permit, so a
            // null handler_port + non-null handler_proc means the
            // handler is between Permit and function-return — give
            // it one more tick rather than RemTasking mid-cleanup).
            if (hp != NULL && cur2->handler_port != NULL) {
                cur2->handler_port = NULL;
                RemTask((struct Task *) hp);
                g_live_handler_count--;
            }
            cur2 = next;
        }
        Permit();
    }

    {
        char msg[200]; int p = 0;
        const char * pre = "[rp] shutdownAll: done, remaining=";
        while (*pre) msg[p++] = *pre++;
        LONG values[5] = {
            g_live_handler_count, (LONG) waited,
            g_handler_entered_count, g_handler_die_seen_count, g_handler_exited_count
        };
        const char * labels[5] = {
            "", " waited_ticks=", " entered=", " die_seen=", " exited="
        };
        for (int k = 0; k < 5; k++) {
            const char * lab = labels[k];
            while (*lab) msg[p++] = *lab++;
            LONG v = values[k]; BOOL neg = (v < 0); if (neg) v = -v;
            char tmp[12]; int t = 0;
            if (v == 0) tmp[t++] = '0';
            while (v > 0) { tmp[t++] = (char)('0' + v % 10); v /= 10; }
            if (neg) msg[p++] = '-';
            while (t > 0) msg[p++] = tmp[--t];
        }
        msg[p] = 0;
        rp_stdout_line(msg);
    }

    // Mirror the final state into tty.log alongside the
    // parent-stdout breadcrumb, so a Workbench-launched session
    // can still see the cleanup outcome after exit.
    rp_log_event("[rp] shutdownAll: done remaining=", g_live_handler_count);
    rp_log_event("[rp] shutdownAll: waited_ticks=",   (LONG) waited);
    rp_log_event("[rp] shutdownAll: final entered=",  g_handler_entered_count);
    rp_log_event("[rp] shutdownAll: final die_seen=", g_handler_die_seen_count);
    rp_log_event("[rp] shutdownAll: final exited=",   g_handler_exited_count);

    return __result;
}
