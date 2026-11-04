// parent_fgets — focused libnix-fgets line-termination diagnostic.
// Same custom DOS handler scaffold as parent.c (same FH alloc, same
// CLI patches, same packet dispatch), but spawns ./child_fgets and
// pushes a small canned line into the input ring.
//
// Variant selected by argv[1]:
//   "lf"   -> push "hello" + 0x0A
//   "cr"   -> push "hello" + 0x0D
//   "crlf" -> push "hello" + 0x0D 0x0A
// Default if no arg -> "lf".
//
// Output goes to stdout; the launcher redirects it into the test
// log. Every ACTION_WRITE the child does is echoed inline so we can
// see what fgets actually produced (or didn't).

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

static BPTR g_log = 0;

static void w(const char * s) { if (g_log) Write(g_log, (APTR) s, (LONG) strlen(s)); }
static void w_long(LONG v) {
    char buf[16]; int i = 0; BOOL neg = FALSE;
    if (v < 0) { neg = TRUE; v = -v; }
    if (v == 0) buf[i++] = '0';
    while (v > 0) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    if (neg) buf[i++] = '-';
    for (int a = 0, b = i - 1; a < b; a++, b--) { char t = buf[a]; buf[a] = buf[b]; buf[b] = t; }
    if (g_log) Write(g_log, buf, i);
}
static void w_hex(ULONG v) {
    char buf[12]; int i = 0;
    static const char hex[] = "0123456789abcdef";
    if (v == 0) buf[i++] = '0';
    while (v > 0) { buf[i++] = hex[v & 0xF]; v >>= 4; }
    for (int a = 0, b = i - 1; a < b; a++, b--) { char t = buf[a]; buf[a] = buf[b]; buf[b] = t; }
    if (g_log) Write(g_log, buf, i);
}

#define RING_SIZE (8 * 1024)
typedef struct {
    UBYTE  data[RING_SIZE];
    ULONG  head, tail, count;
    BOOL   writer_closed;
} ring_t;

static ULONG ring_push(ring_t * r, const UBYTE * src, ULONG n) {
    ULONG p = 0;
    while (p < n && r->count < RING_SIZE) {
        r->data[r->head] = src[p];
        r->head = (r->head + 1) % RING_SIZE;
        r->count++; p++;
    }
    return p;
}
static ULONG ring_pop(ring_t * r, UBYTE * dst, ULONG n) {
    ULONG p = 0;
    while (p < n && r->count > 0) {
        dst[p] = r->data[r->tail];
        r->tail = (r->tail + 1) % RING_SIZE;
        r->count--; p++;
    }
    return p;
}

typedef struct {
    ring_t in;
    ring_t out;
    volatile BOOL die;
    volatile BOOL child_exited;
    struct DosPacket * deferred_read_pkt;
    int  open_count;
} state_t;

static state_t g_state;

static void pkt_reply(struct DosPacket * pkt, LONG res1, LONG res2) {
    struct Message * msg = pkt->dp_Link;
    pkt->dp_Res1 = res1; pkt->dp_Res2 = res2;
    struct MsgPort * reply_port = pkt->dp_Port;
    struct Process * self = (struct Process *) FindTask(NULL);
    pkt->dp_Port = &self->pr_MsgPort;
    msg->mn_Node.ln_Type = NT_REPLYMSG;
    PutMsg(reply_port, msg);
}

static void fulfil_deferred(state_t * st) {
    if (st->deferred_read_pkt == NULL) return;
    if (st->in.count == 0 && !st->in.writer_closed && !st->die) return;
    struct DosPacket * pkt = st->deferred_read_pkt;
    UBYTE * dst = (UBYTE *) pkt->dp_Arg2;
    LONG    n   = pkt->dp_Arg3;
    LONG popped = (LONG) ring_pop(&st->in, dst, (ULONG) n);
    st->deferred_read_pkt = NULL;
    w("[hnd] fulfilling deferred READ with "); w_long(popped); w(" bytes\n");
    pkt_reply(pkt, popped, 0);
}

static void handler_entry(void) {
    struct Process * self = (struct Process *) FindTask(NULL);
    state_t * st = (state_t *) self->pr_ExitData;
    struct MsgPort * port = &self->pr_MsgPort;
    w("[hnd] started\n");
    BOOL running = TRUE;
    while (running) {
        WaitPort(port);
        struct Message * msg;
        while ((msg = GetMsg(port)) != NULL) {
            if (msg->mn_Node.ln_Type == NT_REPLYMSG) {
                // Self-PutMsg from a prior pkt_reply — ignore.
                continue;
            }
            struct DosPacket * pkt = (struct DosPacket *) msg->mn_Node.ln_Name;
            LONG type = pkt->dp_Type;
            if (type != ACTION_DIE) {
                w("[hnd] RX type="); w_long(type); w("\n");
            }
            Forbid();
            if (st->die) {
                pkt_reply(pkt, DOSTRUE, 0);
                running = FALSE;
                Permit();
                continue;
            }
            switch (type) {
                case ACTION_FINDINPUT:
                case ACTION_FINDOUTPUT: {
                    struct FileHandle * fh = (struct FileHandle *) BADDR(pkt->dp_Arg1);
                    fh->fh_Type = port;
                    fh->fh_Arg1 = (LONG) st;
                    st->open_count++;
                    w("[hnd]   FIND ok, open_count="); w_long(st->open_count); w("\n");
                    pkt_reply(pkt, DOSTRUE, 0);
                    break;
                }
                case ACTION_END:
                    if (st->open_count > 0) st->open_count--;
                    w("[hnd]   END, open_count="); w_long(st->open_count); w("\n");
                    pkt_reply(pkt, DOSTRUE, 0);
                    break;
                case ACTION_DIE:
                    pkt_reply(pkt, DOSTRUE, 0);
                    break;
                case ACTION_WRITE: {
                    UBYTE * src = (UBYTE *) pkt->dp_Arg2;
                    LONG    n   = pkt->dp_Arg3;
                    w("[hnd]   WRITE n="); w_long(n); w(" payload=[");
                    if (g_log) Write(g_log, src, n);
                    w("]\n");
                    ring_push(&st->out, src, (ULONG) n);
                    pkt_reply(pkt, n, 0);
                    break;
                }
                case ACTION_READ: {
                    UBYTE * dst = (UBYTE *) pkt->dp_Arg2;
                    LONG    n   = pkt->dp_Arg3;
                    w("[hnd]   READ req="); w_long(n); w(" in.count="); w_long(st->in.count); w("\n");
                    if (st->in.count > 0) {
                        LONG popped = (LONG) ring_pop(&st->in, dst, (ULONG) n);
                        w("[hnd]     served "); w_long(popped); w("\n");
                        pkt_reply(pkt, popped, 0);
                    } else if (st->in.writer_closed) {
                        pkt_reply(pkt, 0, 0);
                    } else {
                        w("[hnd]     DEFERRED\n");
                        if (st->deferred_read_pkt != NULL) {
                            pkt_reply(st->deferred_read_pkt, 0, 0);
                        }
                        st->deferred_read_pkt = pkt;
                    }
                    break;
                }
                case ACTION_WAIT_CHAR:
                    pkt_reply(pkt, DOSTRUE, 0);
                    break;
                case ACTION_SCREEN_MODE:
                case ACTION_CHANGE_SIGNAL:
                    pkt_reply(pkt, DOSTRUE, 0);
                    break;
                case ACTION_DISK_INFO:
                case ACTION_SAME_LOCK:
                case ACTION_FH_FROM_LOCK:
                case ACTION_SEEK:
                    pkt_reply(pkt, -1, ERROR_ACTION_NOT_KNOWN);
                    break;
                case 1034:  /* ACTION_EXAMINE_FH */
                    pkt_reply(pkt, DOSFALSE, 0);
                    break;
                default:
                    w("[hnd]   unhandled, ack TRUE\n");
                    pkt_reply(pkt, DOSTRUE, 0);
                    break;
            }
            Permit();
        }
    }
    w("[hnd] exiting\n");
}

static void __saveds child_exit_cb(
    register LONG status __asm("d0"),
    register LONG data   __asm("d1"))
{
    w("[exit] status="); w_long(status); w(" data=0x"); w_hex((ULONG) data); w("\n");
    state_t * st = (state_t *) data;
    if (st == NULL) return;
    Forbid();
    st->child_exited = TRUE;
    st->in.writer_closed = TRUE;
    fulfil_deferred(st);
    Permit();
}

int main(int argc, char ** argv) {
    g_log = Output();
    w("[parent_fgets] starting, argc="); w_long((LONG) argc); w("\n");

    // Pick terminator from argv[1].
    const UBYTE * payload;
    ULONG payload_len;
    if (argc >= 2 && argv[1] != NULL && strcmp(argv[1], "cr") == 0) {
        payload = (const UBYTE *) "hello\r";
        payload_len = 6;
        w("[parent_fgets] variant=CR (0x0d)\n");
    } else if (argc >= 2 && argv[1] != NULL && strcmp(argv[1], "crlf") == 0) {
        payload = (const UBYTE *) "hello\r\n";
        payload_len = 7;
        w("[parent_fgets] variant=CRLF (0x0d 0x0a)\n");
    } else {
        payload = (const UBYTE *) "hello\n";
        payload_len = 6;
        w("[parent_fgets] variant=LF (0x0a)\n");
    }

    // Spawn handler.
    Forbid();
    struct Process * hp = CreateNewProcTags(
        NP_Entry,     (ULONG) handler_entry,
        NP_Name,      (ULONG) "FGetsHandler",
        NP_StackSize, 8192,
        TAG_DONE);
    if (hp != NULL) hp->pr_ExitData = (LONG) &g_state;
    Permit();
    if (hp == NULL) { w("[parent_fgets] handler spawn failed\n"); return 1; }
    struct MsgPort * handler_port = &hp->pr_MsgPort;
    w("[parent_fgets] handler_port=0x"); w_hex((ULONG) handler_port); w("\n");

    struct FileHandle * fh_in  = (struct FileHandle *) AllocDosObject(DOS_FILEHANDLE, NULL);
    struct FileHandle * fh_out = (struct FileHandle *) AllocDosObject(DOS_FILEHANDLE, NULL);
    struct FileHandle * fh_err = (struct FileHandle *) AllocDosObject(DOS_FILEHANDLE, NULL);
    fh_in->fh_Type  = handler_port; fh_in->fh_Port  = handler_port; fh_in->fh_Arg1  = (LONG) &g_state;
    fh_out->fh_Type = handler_port; fh_out->fh_Port = handler_port; fh_out->fh_Arg1 = (LONG) &g_state;
    fh_err->fh_Type = handler_port; fh_err->fh_Port = handler_port; fh_err->fh_Arg1 = (LONG) &g_state;
    BPTR fh_in_b  = MKBADDR(fh_in);
    BPTR fh_out_b = MKBADDR(fh_out);
    BPTR fh_err_b = MKBADDR(fh_err);
    g_state.open_count = 3;

    w("[parent_fgets] LoadSeg DH1:child_fgets\n");
    BPTR seg = LoadSeg((CONST_STRPTR) "DH1:child_fgets");
    if (seg == 0) {
        w("[parent_fgets] LoadSeg failed, IoErr="); w_long(IoErr()); w("\n");
        return 1;
    }

    Forbid();
    struct Process * child = CreateNewProcTags(
        NP_Seglist,     (ULONG) seg,
        NP_FreeSeglist, TRUE,
        NP_Cli,         TRUE,
        NP_Input,       (ULONG) fh_in_b,
        NP_Output,      (ULONG) fh_out_b,
        NP_Error,       (ULONG) fh_err_b,
        NP_ConsoleTask, (ULONG) handler_port,
        NP_Name,        (ULONG) "child_fgets",
        NP_StackSize,   32768,
        NP_Arguments,   (ULONG) "\n",
        NP_ExitCode,    (ULONG) child_exit_cb,
        NP_ExitData,    (LONG)  &g_state,
        TAG_DONE);
    if (child != NULL) {
        child->pr_CES = fh_err_b;
        struct CommandLineInterface * cli =
            (struct CommandLineInterface *) BADDR(child->pr_CLI);
        if (cli != NULL) {
            cli->cli_StandardInput  = fh_in_b;
            cli->cli_CurrentInput   = fh_in_b;
            cli->cli_StandardOutput = fh_out_b;
            cli->cli_CurrentOutput  = fh_out_b;
        }
    }
    Permit();
    if (child == NULL) {
        w("[parent_fgets] CreateNewProc(child) failed\n");
        UnLoadSeg(seg);
        return 1;
    }
    w("[parent_fgets] child spawned\n");

    // Give the child ~1 second to call fgets and issue ACTION_READ
    // (which the handler defers). Then push the payload + fulfil.
    Delay(50);
    w("[parent_fgets] pushing payload len="); w_long((LONG) payload_len); w("\n");
    Forbid();
    ring_push(&g_state.in, payload, payload_len);
    fulfil_deferred(&g_state);
    Permit();

    // Wait up to 10 seconds for the child to finish.
    int ticks = 0;
    while (!g_state.child_exited && ticks < 50) {
        Delay(10);   // 200ms
        ticks++;
    }
    w("[parent_fgets] loop done, child_exited="); w_long(g_state.child_exited);
    w(" after "); w_long(ticks); w(" ticks (200ms each)\n");

    // Shut handler down via ACTION_DIE.
    Forbid(); g_state.die = TRUE; Permit();
    struct MsgPort * reply = CreateMsgPort();
    struct StandardPacket * sp = AllocMem(sizeof(*sp), MEMF_PUBLIC | MEMF_CLEAR);
    if (sp != NULL && reply != NULL) {
        sp->sp_Msg.mn_Node.ln_Type = NT_MESSAGE;
        sp->sp_Msg.mn_Node.ln_Name = (char *) &sp->sp_Pkt;
        sp->sp_Msg.mn_ReplyPort    = reply;
        sp->sp_Msg.mn_Length       = sizeof(*sp);
        sp->sp_Pkt.dp_Link         = &sp->sp_Msg;
        sp->sp_Pkt.dp_Port         = reply;
        sp->sp_Pkt.dp_Type         = ACTION_DIE;
        PutMsg(handler_port, &sp->sp_Msg);
        WaitPort(reply);
        GetMsg(reply);
        FreeMem(sp, sizeof(*sp));
    }
    if (reply != NULL) DeleteMsgPort(reply);
    w("[parent_fgets] done\n");
    return 0;
}
