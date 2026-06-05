// Async child-process wrapper for AmigaOS, built on a custom
// dos.library file-system handler.
//
// Architecture
// ------------
// Each RunningProcess instance spawns TWO Processes:
//   1. A handler Process — `amStudioTTY-<n>` — that owns a public
//      MsgPort and responds to DOS packets. It maintains a stdin
//      ring buffer (the AmLang side pushes, the child pulls) and a
//      stdout ring buffer (the child pushes, the AmLang side pulls).
//      The handler is what the child sees as its Input() / Output()
//      handles; it's effectively a virtual CON: window.
//   2. The actual child Process — spawned via SystemTagList with
//      SYS_Asynch=TRUE, with SYS_Input and SYS_Output BPTRs that
//      point to FakeFileHandles whose fh_Type is the handler's
//      MsgPort. The child writes to its Output() → DOS packets
//      arrive at the handler MsgPort → handler appends to the
//      stdout ring. The child reads from its Input() → DOS packets
//      arrive at the handler MsgPort → handler pulls from the
//      stdin ring (or defers the reply until bytes are available).
//
// The AmLang side communicates with the handler purely through
// shared memory (the two ring buffers + a couple of state words
// guarded by Forbid()/Permit()) — no signalling needed.
//
// The handler signals "child exited" by detecting ACTION_END
// arriving for both the child's Input and Output BPTRs.
//
// Buffers
// -------
// 64 KB stdin + 64 KB stdout, allocated with MEMF_PUBLIC because the
// handler Process is a different Task and must be able to read/write
// them with the AmLang parent.

#include <libc/core.h>
#include <Am/Lang/RunningProcess.h>
#include <morphos-ppc/Am/Lang/RunningProcess.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Bool.h>
#include <libc/core_inline_functions.h>

#include <morphos-ppc/morphos.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <utility/tagitem.h>

// =================================================================
// Diagnostic logging
// =================================================================
//
// While the handler is being shaken out we log every packet and
// every AmLang-side call. The file is opened once at the first
// startNative call and reused; the handler logs via Write() to the
// same BPTR (DOS handlers are reentrant for disk-backed paths, so
// writing from inside our handler is safe — the packet goes to the
// disk filesystem handler, not us).
//
// Path: PROGDIR:tty.log so the log lands next to the AmIde binary
// itself, where the host filesystem can find it directly (the
// amStudio install drawer the user double-clicks from). T: would
// only be visible from inside the emulator session, and on some
// configs it's mapped to in-memory RAM:T which isn't browsable
// from the host.

static BPTR g_log_fh = 0;
static BOOL g_log_open_attempted = FALSE;
static char g_log_path[64] = { 0 };  // path that actually succeeded

// Try each candidate path in turn until one Open succeeds. RAM: is
// the last resort — always writable on a sane Workbench install,
// even if it's not visible from the host filesystem. SYS: lands on
// the boot drive and IS host-visible (typically `Workbench/` on
// FS-UAE / Amiberry).
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
            // Stash the path so we can echo it into the CLI panel.
            const char * src = RP_LOG_PATHS[i];
            int j = 0;
            while (src[j] != 0 && j < (int) sizeof(g_log_path) - 1) {
                g_log_path[j] = src[j];
                j++;
            }
            g_log_path[j] = 0;
            return;
        }
    }
    g_log_path[0] = 0;  // none worked
}

// Append a decimal number to buf at offset *p, advance *p.
static void rp_log_append_long(char * buf, int * p, LONG v) {
    if (v < 0) { buf[(*p)++] = '-'; v = -v; }
    char tmp[12]; int t = 0;
    if (v == 0) { buf[(*p)++] = '0'; return; }
    while (v > 0) { tmp[t++] = (char)('0' + (v % 10)); v /= 10; }
    while (t > 0) { buf[(*p)++] = tmp[--t]; }
}

static void rp_log_append_hex(char * buf, int * p, ULONG v) {
    static const char hex[] = "0123456789abcdef";
    int leading = 1;
    for (int nibble = 7; nibble >= 0; nibble--) {
        char c = hex[(v >> (nibble * 4)) & 0xF];
        if (leading && c == '0' && nibble > 0) continue;
        leading = 0;
        buf[(*p)++] = c;
    }
    if (leading) buf[(*p)++] = '0';
}

static void rp_log_append_str(char * buf, int * p, const char * s) {
    while (*s) buf[(*p)++] = *s++;
}

static void rp_log_packet(const char * label, LONG type, LONG arg1, LONG arg2, LONG arg3) {
    if (g_log_fh == 0) return;
    char buf[160]; int p = 0;
    rp_log_append_str(buf, &p, label);
    rp_log_append_str(buf, &p, " type=");
    rp_log_append_long(buf, &p, type);
    rp_log_append_str(buf, &p, " arg1=0x");
    rp_log_append_hex(buf, &p, (ULONG) arg1);
    rp_log_append_str(buf, &p, " arg2=0x");
    rp_log_append_hex(buf, &p, (ULONG) arg2);
    rp_log_append_str(buf, &p, " arg3=");
    rp_log_append_long(buf, &p, arg3);
    buf[p++] = '\n';
    Write(g_log_fh, buf, p);
    // Flush via Seek-current so each line shows up in the file
    // immediately rather than sitting in DOS's per-FH write buffer
    // — critical if the IDE crashes mid-session, since the buffer
    // would otherwise be lost.
    Flush(g_log_fh);
}

static void rp_log_event(const char * msg, LONG val) {
    if (g_log_fh == 0) return;
    char buf[160]; int p = 0;
    rp_log_append_str(buf, &p, msg);
    rp_log_append_str(buf, &p, " ");
    rp_log_append_long(buf, &p, val);
    buf[p++] = '\n';
    Write(g_log_fh, buf, p);
    Flush(g_log_fh);
}

// =================================================================
// Shared state between the AmLang parent and the handler Process
// =================================================================

#define RP_RING_SIZE (64 * 1024)

typedef struct rp_ring rp_ring;
struct rp_ring {
    UBYTE * data;            // MEMF_PUBLIC buffer of RP_RING_SIZE bytes
    ULONG   head;            // next write position
    ULONG   tail;            // next read position
    ULONG   count;           // number of bytes currently buffered
    BOOL    writer_closed;   // set when the writer has done ACTION_END
};

// Forward decl — the handler entry uses the layout.
typedef struct rp_state rp_state;
struct rp_state {
    // Lock: a single critical section guards every mutation below.
    // Forbid()/Permit() is sufficient on AmigaOS since signal-driven
    // task switches honour it; the IDE doesn't run preemptive
    // threads at this granularity.

    rp_ring stdin_buf;       // AmLang writes -> child reads
    rp_ring stdout_buf;      // child writes -> AmLang reads

    // Set TRUE by the handler when the child has issued ACTION_END
    // on its last open BPTR (= effectively exited / closed both
    // pipes). AmLang side reads this flag from isAlive().
    volatile BOOL child_exited;

    // Set TRUE by the AmLang side from close(); the handler sees it
    // on the next packet and starts winding down (replies to any
    // pending packets, exits its main loop, releases resources).
    volatile BOOL shutdown_requested;

    // Count of open child handles. Incremented on ACTION_FINDINPUT
    // and ACTION_FINDOUTPUT, decremented on ACTION_END. child_exited
    // flips when this hits zero after at least one open.
    int open_handle_count;
    BOOL any_handle_was_open;

    // Deferred-read state. When ACTION_READ arrives but stdin_buf is
    // empty AND not yet writer_closed, we stash the packet here and
    // reply only once writeInput() has pushed enough bytes (or
    // shutdown is requested). Only one deferred read is supported
    // at a time — the child should never have two concurrent reads
    // on one handle, and we use distinct handles per FIND.
    struct DosPacket * deferred_read_pkt;
    LONG               deferred_read_arg_buf;  // dp_Arg2 = APTR buffer
    LONG               deferred_read_arg_len;  // dp_Arg3 = LONG length
    struct Message *   deferred_read_msg;      // the wrapping Message (for ReplyMsg)
};

// The handler Process needs its own copy of these handles. We
// allocate one FakeFileHandle per open Find on the child side.
struct FakeFileHandle {
    struct FileHandle real;  // first member so BPTR points to it correctly
    rp_state * state;
};

typedef struct _running_process_data running_process_data;
struct _running_process_data {
    rp_state *        state;
    struct Process *  handler_proc;
    struct MsgPort *  handler_port;
    BPTR              child_in_bptr;   // FakeFileHandle for child stdin
    BPTR              child_out_bptr;  // FakeFileHandle for child stdout
    BPTR              child_err_bptr;  // FakeFileHandle for child stderr
    BPTR              old_cwd_lock;    // restored after spawn
    BOOL              has_old_cwd;
    BOOL              child_spawned;
    // Reply port we use to send our ACTION_DIE packet to the
    // handler at shutdown. Created in startNative, deleted in
    // close after the handler acks. Stays separate from the
    // handler's own port so we can wait on it cleanly.
    struct MsgPort *  shutdown_reply_port;
};

// =================================================================
// Ring-buffer helpers (caller must Forbid()/Permit() around them)
// =================================================================

static ULONG rp_ring_push(rp_ring * r, const UBYTE * src, ULONG n) {
    ULONG pushed = 0;
    while (pushed < n && r->count < RP_RING_SIZE) {
        r->data[r->head] = src[pushed];
        r->head = (r->head + 1) % RP_RING_SIZE;
        r->count++;
        pushed++;
    }
    return pushed;
}

static ULONG rp_ring_pop(rp_ring * r, UBYTE * dst, ULONG n) {
    ULONG popped = 0;
    while (popped < n && r->count > 0) {
        dst[popped] = r->data[r->tail];
        r->tail = (r->tail + 1) % RP_RING_SIZE;
        r->count--;
        popped++;
    }
    return popped;
}

// =================================================================
// Handler Process entry
// =================================================================
//
// Receives DOS packets on its public MsgPort and routes them through
// the shared state. The packet protocol (selected subset):
//
//   ACTION_FINDINPUT  (1005): open a handle for input.
//       dp_Arg1 = FileHandle * to fill in (we set fh_Arg1 = state,
//                                          fh_Type  = our port)
//       dp_Arg2 = Lock BPTR (cwd; ignored)
//       dp_Arg3 = filename (CSTR; ignored, our handler is anonymous)
//       Reply: dp_Res1 = DOSTRUE on success, FALSE on failure.
//   ACTION_FINDOUTPUT (1006): same shape, opens for output.
//   ACTION_READ        (R, 'R'<<24|'D'<<...; symbol ACTION_READ = R):
//       dp_Arg1 = FileHandle.fh_Arg1 (we stashed `state` there)
//       dp_Arg2 = APTR  buf
//       dp_Arg3 = LONG  length
//       Reply: dp_Res1 = bytes read (0 = EOF, -1 = error).
//   ACTION_WRITE       (W): same args as READ but writes from buf.
//   ACTION_END        (1007): close the handle.
//       dp_Arg1 = fh_Arg1
//       Reply: dp_Res1 = DOSTRUE.
//   ACTION_WAIT_CHAR (20): "is at least one char available within
//       timeout?". dp_Arg1 = fh_Arg1, dp_Arg2 = timeout (usec).
//       For simplicity we treat timeout=0 as "right now" and ignore
//       non-zero timeouts (return immediately).
//       Reply: dp_Res1 = DOSTRUE if char available OR EOF, else FALSE.

static struct DosPacket * rp_get_packet(struct MsgPort * port) {
    struct Message * msg = WaitPort(port);
    if (msg == NULL) return NULL;
    msg = (struct Message *) GetMsg(port);
    if (msg == NULL) return NULL;
    return (struct DosPacket *) msg->mn_Node.ln_Name;
}

static void rp_reply_packet(struct DosPacket * pkt, LONG res1, LONG res2) {
    struct Message * msg = pkt->dp_Link;
    pkt->dp_Res1 = res1;
    pkt->dp_Res2 = res2;
    // Bounce the reply back to the sender's port. Save the sender's
    // port (caller's reply port) BEFORE overwriting dp_Port with our
    // own — the convention is that the reply tells the caller where
    // future round-trips should go, which is OUR Process's MsgPort.
    struct MsgPort * reply_port = pkt->dp_Port;
    struct Process * self = (struct Process *) FindTask(NULL);
    pkt->dp_Port = &self->pr_MsgPort;
    PutMsg(reply_port, msg);
}

// Try to fulfil a deferred ACTION_READ now that bytes have arrived
// (or shutdown was requested). Called with Forbid() held.
static void rp_try_fulfil_deferred_read(rp_state * st) {
    if (st->deferred_read_pkt == NULL) return;
    rp_ring * r = &st->stdin_buf;
    if (r->count == 0 && !r->writer_closed && !st->shutdown_requested) {
        return;  // still nothing, keep waiting
    }
    UBYTE * dst = (UBYTE *) st->deferred_read_arg_buf;
    LONG  n   = st->deferred_read_arg_len;
    LONG popped = (LONG) rp_ring_pop(r, dst, (ULONG) n);
    // 0 popped + writer_closed = EOF; otherwise return what we got.
    struct DosPacket * pkt = st->deferred_read_pkt;
    st->deferred_read_pkt = NULL;
    st->deferred_read_msg = NULL;
    rp_reply_packet(pkt, popped, 0);
}

static void rp_handler_entry(void) {
    struct Process * self = (struct Process *) FindTask(NULL);
    rp_state * st = (rp_state *) self->pr_ExitData;  // we'll stash state ptr here pre-launch

    struct MsgPort * port = &self->pr_MsgPort;

    rp_log_event("handler started", 0);
    BOOL running = TRUE;
    while (running) {
        struct DosPacket * pkt = rp_get_packet(port);
        if (pkt == NULL) continue;

        LONG type = pkt->dp_Type;
        rp_log_packet("RX", type, pkt->dp_Arg1, pkt->dp_Arg2, pkt->dp_Arg3);

        Forbid();
        if (st->shutdown_requested) {
            // Reply with a clean EOF / DOSTRUE-ish for every packet,
            // then break the loop.
            switch (type) {
                case ACTION_READ:       rp_reply_packet(pkt, 0, 0); break;
                case ACTION_WRITE:      rp_reply_packet(pkt, pkt->dp_Arg3, 0); break;
                case ACTION_FINDINPUT:  rp_reply_packet(pkt, DOSFALSE, 0); break;
                case ACTION_FINDOUTPUT: rp_reply_packet(pkt, DOSFALSE, 0); break;
                case ACTION_END:        rp_reply_packet(pkt, DOSTRUE,  0); break;
                default:                rp_reply_packet(pkt, DOSTRUE,  0); break;
            }
            running = FALSE;
            Permit();
            continue;
        }

        switch (type) {
            case ACTION_FINDINPUT:
            case ACTION_FINDOUTPUT: {
                // dp_Arg1 holds a BPTR to a FileHandle the caller
                // wants us to populate. We fill fh_Type with our
                // port and fh_Arg1 with the state pointer so
                // subsequent READ/WRITE/END packets can find us.
                struct FileHandle * fh = (struct FileHandle *) BADDR(pkt->dp_Arg1);
                fh->fh_Type = port;
                fh->fh_Arg1 = (LONG) st;
                st->open_handle_count++;
                st->any_handle_was_open = TRUE;
                rp_reply_packet(pkt, DOSTRUE, 0);
                break;
            }
            case ACTION_END: {
                if (st->open_handle_count > 0) {
                    st->open_handle_count--;
                }
                if (st->open_handle_count == 0 && st->any_handle_was_open) {
                    st->child_exited = TRUE;
                    st->stdin_buf.writer_closed = TRUE;
                    rp_try_fulfil_deferred_read(st);
                }
                rp_reply_packet(pkt, DOSTRUE, 0);
                break;
            }
            case ACTION_DIE: {
                // MorphOS SDK procmessages.c: the proper way to
                // shut down a handler. Caller (us, from rp_close)
                // sends ACTION_DIE; we acknowledge, drop out of
                // the packet loop, and exit. The wiki / SDK both
                // call out that leaving messages queued at our
                // pr_MsgPort when we exit causes the next caller
                // (often dos.library itself or the AmigaOS Shell)
                // to Alert(), which is exactly the "Shell crashes
                // after running bebbossh" symptom we hit.
                rp_reply_packet(pkt, DOSTRUE, 0);
                running = FALSE;
                break;
            }
            case ACTION_READ: {
                UBYTE * dst = (UBYTE *) pkt->dp_Arg2;
                LONG    n   = pkt->dp_Arg3;
                if (st->stdin_buf.count > 0) {
                    LONG popped = (LONG) rp_ring_pop(&st->stdin_buf, dst, (ULONG) n);
                    rp_reply_packet(pkt, popped, 0);
                } else if (st->stdin_buf.writer_closed) {
                    rp_reply_packet(pkt, 0, 0);  // EOF
                } else {
                    // Defer until writeInput pushes bytes (or close).
                    // Stash and wait — the AmLang side polls
                    // tryReadOutput; whenever it calls writeInput it
                    // sends us a wake packet (ACTION_FLUSH below).
                    //
                    // If there's already a deferred read pending,
                    // reply EOF to it so the new caller can take its
                    // place. This shouldn't happen under normal use
                    // (one Read at a time per FH), but if a child
                    // task cancels and retries we'd otherwise leak
                    // the previous packet forever.
                    if (st->deferred_read_pkt != NULL) {
                        rp_reply_packet(st->deferred_read_pkt, 0, 0);
                    }
                    st->deferred_read_pkt = pkt;
                    st->deferred_read_arg_buf = pkt->dp_Arg2;
                    st->deferred_read_arg_len = pkt->dp_Arg3;
                    st->deferred_read_msg = pkt->dp_Link;
                }
                break;
            }
            case ACTION_WRITE: {
                UBYTE * src = (UBYTE *) pkt->dp_Arg2;
                LONG    n   = pkt->dp_Arg3;
                LONG pushed = (LONG) rp_ring_push(&st->stdout_buf, src, (ULONG) n);
                rp_reply_packet(pkt, pushed, 0);
                break;
            }
            case ACTION_WAIT_CHAR: {
                // Always claim a char is "available". We don't have
                // a timer to honour the dp_Arg1 timeout, and lying
                // optimistically is the lesser evil: callers like
                // ssh's password loop poll WaitForChar with a
                // short timeout, get DOSFALSE, conclude "no input
                // came" and send an empty password 3 times. By
                // saying TRUE we make the caller go on to ACTION_READ,
                // which we properly defer until real data arrives.
                rp_reply_packet(pkt, DOSTRUE, 0);
                break;
            }
            case ACTION_FLUSH:
                // Sent by AmLang side to wake a deferred read.
                rp_try_fulfil_deferred_read(st);
                rp_reply_packet(pkt, DOSTRUE, 0);
                break;
            case ACTION_SCREEN_MODE: {
                // This is what IsInteractive() sends to discover
                // whether the file handle is a tty. SSH and other
                // interactive programs gate the password prompt on
                // this returning DOSTRUE — without that they treat
                // stdin as non-interactive and bail out before
                // prompting. The dp_Arg1 is RAW(TRUE)/COOKED(FALSE)
                // when a CON: handler is being toggled to/from raw
                // mode; we accept either silently.
                rp_reply_packet(pkt, DOSTRUE, 0);
                break;
            }
            case ACTION_DISK_INFO:
            case ACTION_SAME_LOCK:
            case ACTION_FH_FROM_LOCK:
                // Common queries some programs make against their
                // stdin. We're not a real filesystem; respond
                // failure so callers fall back to "it's a terminal,
                // skip filesystem-style ops".
                rp_reply_packet(pkt, DOSFALSE, 0);
                break;
            case ACTION_CHANGE_SIGNAL:
                // Caller wants us to send a specific signal when
                // input is available. We don't (yet) support this;
                // pretend we set it up so the program continues.
                rp_reply_packet(pkt, DOSTRUE, 0);
                break;
            default:
                // Unknown packet. Be permissive — reply DOSTRUE so
                // brittle callers (libc init, ssh's open("/dev/tty"))
                // don't bail out just because we don't know what
                // ACTION_QUEUED_READ_OPEN means. The log entry above
                // captures the exact packet type so we can add
                // explicit handling later if anything misbehaves
                // because of the optimistic reply.
                rp_log_event("WARN unhandled packet type", type);
                rp_reply_packet(pkt, DOSTRUE, 0);
                break;
        }
        Permit();
    }

    // Drained out. Reply to any still-pending deferred read.
    Forbid();
    if (st->deferred_read_pkt != NULL) {
        rp_reply_packet(st->deferred_read_pkt, 0, 0);
        st->deferred_read_pkt = NULL;
    }
    Permit();
}

// =================================================================
// AmLang-facing native functions
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

static void rp_close_internal(running_process_data * d) {
    if (d == NULL) return;

    // Step 1: reply EOF to any deferred Read so the child wakes
    // up and gets out of its read loop. We do this from the
    // AmLang side directly (not via a packet) so the handler
    // doesn't have to do anything special.
    if (d->state != NULL) {
        Forbid();
        d->state->shutdown_requested = TRUE;
        if (d->state->deferred_read_pkt != NULL) {
            struct DosPacket * pkt = d->state->deferred_read_pkt;
            d->state->deferred_read_pkt = NULL;
            d->state->deferred_read_msg = NULL;
            struct Message * msg = pkt->dp_Link;
            pkt->dp_Res1 = 0;
            pkt->dp_Res2 = 0;
            struct MsgPort * reply_port = pkt->dp_Port;
            pkt->dp_Port = d->handler_port;
            PutMsg(reply_port, msg);
        }
        Permit();
    }

    // Step 2: kill the handler process via ACTION_DIE. MorphOS SDK
    // procmessages.c rule: "Leaving any messages to this port is
    // illegal and will cause Alert()" — we must drain the handler
    // before we free anything. Send a real heap-allocated DIE
    // packet (NOT stack-allocated; the handler runs on a different
    // task and reads it asynchronously) with a real reply port so
    // we can WaitPort() for the ack.
    if (d->handler_port != NULL && d->shutdown_reply_port != NULL) {
        struct StandardPacket * sp = (struct StandardPacket *)
            AllocMem(sizeof(*sp), MEMF_PUBLIC | MEMF_CLEAR);
        if (sp != NULL) {
            sp->sp_Msg.mn_Node.ln_Type   = NT_MESSAGE;
            sp->sp_Msg.mn_Node.ln_Name   = (char *) &sp->sp_Pkt;
            sp->sp_Msg.mn_ReplyPort      = d->shutdown_reply_port;
            sp->sp_Msg.mn_Length         = sizeof(*sp);
            sp->sp_Pkt.dp_Link           = &sp->sp_Msg;
            sp->sp_Pkt.dp_Port           = d->shutdown_reply_port;
            sp->sp_Pkt.dp_Type           = ACTION_DIE;
            PutMsg(d->handler_port, &sp->sp_Msg);
            // Wait for the handler to acknowledge.
            WaitPort(d->shutdown_reply_port);
            GetMsg(d->shutdown_reply_port);
            FreeMem(sp, sizeof(*sp));
        }
        // Handler has now exited its loop. Its Process struct
        // self-destructs on Exit (CreateNewProc / Exit
        // contract).
        DeleteMsgPort(d->shutdown_reply_port);
        d->shutdown_reply_port = NULL;
    }

    // Step 3: free the FileHandle structs we AllocMem'd for the
    // child. SystemTagList with SYS_Asynch=TRUE closed them at
    // child exit (per the MorphOS dos.aros.doc NOTES quote), but
    // dos.library only closes them in the FH sense — it doesn't
    // free our AllocMem block. Free them here.
    if (d->child_in_bptr  != 0) { FreeMem(BADDR(d->child_in_bptr),  sizeof(struct FileHandle));  d->child_in_bptr  = 0; }
    if (d->child_out_bptr != 0) { FreeMem(BADDR(d->child_out_bptr), sizeof(struct FileHandle));  d->child_out_bptr = 0; }
    if (d->child_err_bptr != 0) { FreeMem(BADDR(d->child_err_bptr), sizeof(struct FileHandle));  d->child_err_bptr = 0; }

    // Step 4: free ring buffers + state struct.
    if (d->state != NULL) {
        if (d->state->stdin_buf.data  != NULL) FreeMem(d->state->stdin_buf.data,  RP_RING_SIZE);
        if (d->state->stdout_buf.data != NULL) FreeMem(d->state->stdout_buf.data, RP_RING_SIZE);
        FreeMem(d->state, sizeof(*d->state));
        d->state = NULL;
    }
    d->handler_port = NULL;
    d->handler_proc = NULL;
}

function_result Am_Lang_RunningProcess__native_release_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    running_process_data * d = rp_data(this);
    if (d != NULL) {
        rp_close_internal(d);
        free(d);
        this->object_properties.class_object_properties.object_data.value.custom_value = NULL;
    }
    return __result;
}

function_result Am_Lang_RunningProcess_startNative_0(aobject * const this, aobject * command, aobject * workingDir) {
    function_result __result = { .has_return_value = false };
    if (this != NULL) __increase_reference_count(this);
    if (command != NULL) __increase_reference_count(command);
    if (workingDir != NULL) __increase_reference_count(workingDir);

    rp_log_open();
    rp_log_event("startNative entered", 0);

    running_process_data * d = rp_data(this);

    if (d == NULL) {
        __throw_simple_exception("RunningProcess: object_data missing", "in Am_Lang_RunningProcess_startNative_0", &__result);
        goto __exit;
    }

    // === Allocate shared state ===
    d->state = (rp_state *) AllocMem(sizeof(*d->state), MEMF_PUBLIC | MEMF_CLEAR);
    if (d->state == NULL) {
        __throw_simple_exception("RunningProcess: AllocMem(state) failed", "in startNative", &__result);
        goto __exit;
    }
    d->state->stdin_buf.data  = (UBYTE *) AllocMem(RP_RING_SIZE, MEMF_PUBLIC);
    d->state->stdout_buf.data = (UBYTE *) AllocMem(RP_RING_SIZE, MEMF_PUBLIC);
    if (d->state->stdin_buf.data == NULL || d->state->stdout_buf.data == NULL) {
        __throw_simple_exception("RunningProcess: AllocMem(ring) failed", "in startNative", &__result);
        goto __exit;
    }

    // === Create reply port for ACTION_DIE handshake at close ===
    // Used in rp_close_internal so we can synchronously wait for
    // the handler to acknowledge shutdown before we free anything.
    d->shutdown_reply_port = CreateMsgPort();
    if (d->shutdown_reply_port == NULL) {
        __throw_simple_exception("RunningProcess: CreateMsgPort failed", "in startNative", &__result);
        goto __exit;
    }

    // === Launch the handler Process ===
    // pr_ExitData is repurposed to carry the state pointer to the
    // handler entry. The Forbid() wrap is critical: without it,
    // the new process is scheduled the instant CreateNewProcTags
    // returns and may execute rp_handler_entry — which reads
    // pr_ExitData — before we get a chance to write the pointer.
    // Forbid prevents any task switch, so the new task can't run
    // until Permit, by which time pr_ExitData is set.
    Forbid();
    d->handler_proc = CreateNewProcTags(
        NP_Entry,     (ULONG) rp_handler_entry,
        NP_Name,      (ULONG) "amStudioTTY",
        NP_StackSize, 8192,
        TAG_DONE);
    if (d->handler_proc != NULL) {
        d->handler_proc->pr_ExitData = (LONG) d->state;
    }
    Permit();
    if (d->handler_proc == NULL) {
        __throw_simple_exception("RunningProcess: CreateNewProc failed", "in startNative", &__result);
        goto __exit;
    }
    d->handler_port = &d->handler_proc->pr_MsgPort;

    // === Build fake FileHandles for child stdin / stdout / stderr ===
    // The child will see these as its Input() / Output() / pr_CES.
    // fh_Type points to our handler's port; subsequent Read/Write
    // packets from the child land at the handler. We allocate a
    // dedicated stderr handle so the SystemTagList machinery can
    // close each independently when the child exits without double-
    // closing one of them.
    struct FileHandle * fh_in  = (struct FileHandle *) AllocMem(sizeof(*fh_in),  MEMF_PUBLIC | MEMF_CLEAR);
    struct FileHandle * fh_out = (struct FileHandle *) AllocMem(sizeof(*fh_out), MEMF_PUBLIC | MEMF_CLEAR);
    struct FileHandle * fh_err = (struct FileHandle *) AllocMem(sizeof(*fh_err), MEMF_PUBLIC | MEMF_CLEAR);
    if (fh_in == NULL || fh_out == NULL || fh_err == NULL) {
        __throw_simple_exception("RunningProcess: AllocMem(fh) failed", "in startNative", &__result);
        goto __exit;
    }
    // fh_Port is mis-named in dosextens.h — it's a *boolean*
    // "true if this is an interactive handle". The wiki summary
    // says any non-NULL flips it to "yes, terminal", but if a
    // child's libc treats it as a real MsgPort and dereferences
    // it the resulting address-error trap (error #80000006) takes
    // the whole spawn down. Setting it to our handler's MsgPort
    // gives it *both* truthiness (non-NULL) AND a valid pointer to
    // dereference — paranoid but cheap.
    // fh_Buf/fh_Pos/fh_End deliberately left at 0 so DOS doesn't
    // satisfy Read() from a buffer without ever hitting our handler.
    fh_in->fh_Type = d->handler_port;
    fh_in->fh_Port = d->handler_port;  // interactive=TRUE + valid pointer
    fh_in->fh_Arg1 = (LONG) d->state;
    fh_out->fh_Type = d->handler_port;
    fh_out->fh_Port = d->handler_port;
    fh_out->fh_Arg1 = (LONG) d->state;
    fh_err->fh_Type = d->handler_port;
    fh_err->fh_Port = d->handler_port;
    fh_err->fh_Arg1 = (LONG) d->state;
    d->child_in_bptr  = MKBADDR(fh_in);
    d->child_out_bptr = MKBADDR(fh_out);
    d->child_err_bptr = MKBADDR(fh_err);
    // Count these as opens so child_exited fires when the child
    // closes them all. ACTION_END decrements; when count hits 0
    // we know the child has released all ends.
    d->state->open_handle_count = 3;
    d->state->any_handle_was_open = TRUE;

    // === pr_ConsoleTask: NOT swapping the IDE's value ===
    //
    // Earlier versions of this file swapped the IDE process's
    // pr_ConsoleTask to our handler port for the duration of
    // SystemTagList, so the spawned child would inherit our
    // handler as its console task. That approach is too risky:
    // SystemTagList yields internally (Wait inside CreateNewProc
    // and friends), so other tasks can observe the IDE in the
    // swapped state. AmigaOS Shell windows opened during that
    // window inherited our handler port, and any later access
    // (e.g. clicking the Shell) crashed dos.library when the
    // handler was gone.
    //
    // Instead we rely on NP_ConsoleTask in the SystemTagList tag
    // list below. If the System() wrapper drops it, the child
    // falls back to inheriting the IDE's actual console task —
    // which means SSH/password opens of "*" won't reach our
    // handler. The bebbossh password prompt may then still fail,
    // but at least the IDE / Shell stay stable.

    // === Optionally swap CWD for the spawn ===
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

    // === Spawn the child via SystemTagList ===
    //
    // NP_ConsoleTask = our handler port so the child's opens of
    // "*" / "CONSOLE:" (the traditional /dev/tty equivalent that
    // SSH-style password prompts use) land at us too. Without
    // this, SSH opens "*" to read the password without echo, the
    // open fails (no real console attached), and bebbossh falls
    // back to an empty password — which matches the "3 password
    // denied without typing anything" symptom exactly.
    //
    // SYS_Error wired so stderr writes (error messages, password
    // prompts that go via stderr on some Unix ports) also reach
    // the panel instead of disappearing into the parent's NIL:.
    string_holder * cmd_holder = (string_holder *) (command + 1);
    STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;
    // SYS_FilterTags would be the MorphOS way to ensure NP_* tags
    // pass through to CreateNewProc, but the symbol isn't in the
    // AmigaOS NDK. On AmigaOS, System() doesn't filter NP_* tags
    // by default — they're propagated. So we just include
    // NP_ConsoleTask and trust it'll make it to the child's
    // pr_ConsoleTask, enabling bebbossh's Open("*") / Open("CONSOLE:")
    // password-prompt path to reach our handler.
    struct TagItem run_tags[] = {
        { SYS_Input,       (ULONG) d->child_in_bptr },
        { SYS_Output,      (ULONG) d->child_out_bptr },
        { SYS_Error,       (ULONG) d->child_err_bptr },
        { SYS_Asynch,      TRUE },
        { SYS_UserShell,   TRUE },
        { NP_ConsoleTask,  (ULONG) d->handler_port },
        { TAG_DONE,        0 },
    };
    // Banner: lands in the CLI panel before any child output, so the
    // user has a positive signal that the new handler code ran and
    // knows where the log went.
    {
        char banner[160]; int p = 0;
        rp_log_append_str(banner, &p, "[amStudio-tty] handler started, log=");
        if (g_log_path[0] != 0) {
            rp_log_append_str(banner, &p, g_log_path);
        } else {
            rp_log_append_str(banner, &p, "(open failed)");
        }
        banner[p++] = '\n';
        Forbid();
        rp_ring_push(&d->state->stdout_buf, (const UBYTE *) banner, (ULONG) p);
        Permit();
    }

    rp_log_event("calling SystemTagList", 0);
    LONG status = SystemTagList(cmd_strptr, run_tags);
    rp_log_event("SystemTagList returned", status);

    // Restore CWD.
    if (d->has_old_cwd) {
        BPTR new_lock = CurrentDir(d->old_cwd_lock);
        if (new_lock != 0) UnLock(new_lock);
        d->has_old_cwd = FALSE;
        d->old_cwd_lock = 0;
    }

    if (status == -1) {
        __throw_simple_exception("RunningProcess: SystemTagList async spawn failed", "in startNative", &__result);
        goto __exit;
    }
    d->child_spawned = TRUE;

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

    running_process_data * d = rp_data(this);
    if (d == NULL || d->state == NULL) goto __exit;

    UBYTE buf[2048];
    Forbid();
    ULONG n = rp_ring_pop(&d->state->stdout_buf, buf, sizeof(buf) - 1);
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
    string_holder * h = (string_holder *) (text + 1);
    if (h == NULL || h->string_value == NULL) goto __exit;
    LONG len = 0;
    while (h->string_value[len] != 0) len++;
    if (len <= 0) goto __exit;

    // Forbid around the push + deferred-read fulfilment so the
    // handler can't race us. If there's a Read packet parked in
    // d->state->deferred_read_pkt, we reply to it directly from
    // this side — the AmLang task has the state pointer and the
    // packet's reply port, which is all we need. Previously we
    // PutMsg'd an ACTION_FLUSH wake packet to the handler, but the
    // packet was stack-allocated in this function: by the time the
    // handler GetMsg'd it, the stack frame was gone and the handler
    // read garbage. The result was the deferred Read never got
    // fulfilled, the child's internal timeout fired, and SSH-style
    // children gave up with an empty password.
    rp_log_event("writeInput bytes", len);
    Forbid();
    rp_ring_push(&d->state->stdin_buf, (const UBYTE *) h->string_value, (ULONG) len);
    if (d->state->deferred_read_pkt != NULL && d->state->stdin_buf.count > 0) {
        rp_log_event("writeInput fulfilling deferred read, stdin_buf.count", d->state->stdin_buf.count);
        struct DosPacket * pkt = d->state->deferred_read_pkt;
        UBYTE * dst = (UBYTE *) d->state->deferred_read_arg_buf;
        LONG    n   = d->state->deferred_read_arg_len;
        LONG    popped = (LONG) rp_ring_pop(&d->state->stdin_buf, dst, (ULONG) n);
        d->state->deferred_read_pkt = NULL;
        d->state->deferred_read_msg = NULL;
        // Reply to the child on behalf of the handler. The dp_Port
        // gets set to the handler's port so the child's next packet
        // goes back to the handler (standard convention).
        struct Message * msg = pkt->dp_Link;
        pkt->dp_Res1 = popped;
        pkt->dp_Res2 = 0;
        struct MsgPort * reply_port = pkt->dp_Port;
        pkt->dp_Port = d->handler_port;
        PutMsg(reply_port, msg);
    }
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
        // Alive while: child hasn't issued ACTION_END on both, OR
        // there's still output to drain.
        Forbid();
        BOOL exited = d->state->child_exited;
        ULONG queued = d->state->stdout_buf.count;
        Permit();
        if (!exited || queued > 0) {
            alive = TRUE;
        }
    }
    __result.return_value.value.bool_value = alive ? true : false;
    if (this != NULL) __decrease_reference_count(this);
    return __result;
}

function_result Am_Lang_RunningProcess_close_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    if (this != NULL) __increase_reference_count(this);
    rp_close_internal(rp_data(this));
    if (this != NULL) __decrease_reference_count(this);
    return __result;
}
