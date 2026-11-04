// Mirror of amStudio's RunningProcess setup, in test-harness form,
// to validate the bridge against real bebbossh hitting a real host
// over amiberry's bsdsocket_emu network.
//
// Spawn: bebbossh anders@172.22.8.177 via LoadSeg + CreateNewProcTags
// Setup applied (same as amStudio):
//   - NP_Cli=TRUE, NP_Input/Output/Error wired to handler FHs
//   - NP_ConsoleTask=handler_port so Open("*") routes to us
//   - NP_ExitCode + register-bound (D0/D1) callback to flip exit flag
//   - V40 workaround: manually set child->pr_CES + cli_StandardInput
//     / cli_CurrentInput / cli_StandardOutput / cli_CurrentOutput
//     while Forbid is held (CreateNewProcTags doesn't populate them
//     on Kickstart 3.1)
// Pre-load input ring with "y\n" so the cert-approval prompt (if any)
// gets answered automatically. Log every packet type + payload.

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

// ===== Shared state =====
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

// ===== Custom handler =====
static void pkt_reply(struct DosPacket * pkt, LONG res1, LONG res2) {
    struct Message * msg = pkt->dp_Link;
    pkt->dp_Res1 = res1; pkt->dp_Res2 = res2;
    struct MsgPort * reply_port = pkt->dp_Port;
    struct Process * self = (struct Process *) FindTask(NULL);
    pkt->dp_Port = &self->pr_MsgPort;
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
            struct DosPacket * pkt = (struct DosPacket *) msg->mn_Node.ln_Name;
            LONG type = pkt->dp_Type;
            w("[hnd] RX type="); w_long(type); w("\n");
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
                    w("[hnd]   advisory DIE, ignoring\n");
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
                    // Always TRUE so FGets doesn't bail with EOF.
                    pkt_reply(pkt, DOSTRUE, 0);
                    break;
                case ACTION_SCREEN_MODE:
                case ACTION_CHANGE_SIGNAL:
                    pkt_reply(pkt, DOSTRUE, 0);
                    break;
                case ACTION_DISK_INFO:
                case ACTION_SAME_LOCK:
                case ACTION_FH_FROM_LOCK:
                case ACTION_SEEK:           /* not seekable */
                    pkt_reply(pkt, -1, ERROR_ACTION_NOT_KNOWN);
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

// NP_ExitCode hook — register-bound (D0/D1).
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
    w("[parent] bebbossh live-network test\n");

    // DO NOT pre-load input. We want to see whether bebbossh
    // blocks indefinitely on its cert-prompt fgets when no one
    // has pushed bytes to the input ring. If it blocks → handler
    // architecture is correct. If it doesn't → something at the
    // dos/libnix layer is auto-answering despite a deferred Read.
    w("[parent] (input ring NOT pre-loaded)\n");

    // Spawn handler.
    Forbid();
    struct Process * hp = CreateNewProcTags(
        NP_Entry,     (ULONG) handler_entry,
        NP_Name,      (ULONG) "TestHandler",
        NP_StackSize, 8192,
        TAG_DONE);
    if (hp != NULL) hp->pr_ExitData = (LONG) &g_state;
    Permit();
    if (hp == NULL) { w("handler spawn failed\n"); return 1; }
    struct MsgPort * handler_port = &hp->pr_MsgPort;
    w("[parent] handler_port=0x"); w_hex((ULONG) handler_port); w("\n");

    // Use AllocDosObject(DOS_FILEHANDLE) — V40 may have private
    // fields beyond the public struct that FPuts touches. Allocating
    // via dos.library gets us the correct size.
    struct FileHandle * fh_in  = (struct FileHandle *) AllocDosObject(DOS_FILEHANDLE, NULL);
    struct FileHandle * fh_out = (struct FileHandle *) AllocDosObject(DOS_FILEHANDLE, NULL);
    struct FileHandle * fh_err = (struct FileHandle *) AllocDosObject(DOS_FILEHANDLE, NULL);
    w("[parent] fh_in=0x");  w_hex((ULONG) fh_in);  w("\n");
    w("[parent] fh_out=0x"); w_hex((ULONG) fh_out); w("\n");
    w("[parent] fh_err=0x"); w_hex((ULONG) fh_err); w("\n");
    fh_in->fh_Type  = handler_port; fh_in->fh_Port  = handler_port; fh_in->fh_Arg1  = (LONG) &g_state;
    fh_out->fh_Type = handler_port; fh_out->fh_Port = handler_port; fh_out->fh_Arg1 = (LONG) &g_state;
    fh_err->fh_Type = handler_port; fh_err->fh_Port = handler_port; fh_err->fh_Arg1 = (LONG) &g_state;
    BPTR fh_in_b  = MKBADDR(fh_in);
    BPTR fh_out_b = MKBADDR(fh_out);
    BPTR fh_err_b = MKBADDR(fh_err);
    g_state.open_count = 3;

    // Lock the bebbossh install dir so we can pass it as NP_HomeDir.
    // PROGDIR: resolves to pr_HomeDir, and bebbossh's OpenLibrary
    // for libcryptossh.library looks in PROGDIR:. Without this the
    // lib load fails (which IS what we want diagnostically for our
    // test env, but we also want to see what happens past it).
    BPTR home = Lock((CONST_STRPTR) "SYS:Apps/bebbossh", ACCESS_READ);
    w("[parent] home lock="); w_long((LONG) home); w("\n");

    // Spawn child4 (libnix printf) to isolate whether libnix's
    // stdio path reaches our handler at all.
    w("[parent] LoadSeg SYS:Apps/bebbossh/bebbossh\n");
    BPTR seg = LoadSeg((CONST_STRPTR) "SYS:Apps/bebbossh/bebbossh");
    if (seg == 0) {
        w("[parent] LoadSeg failed, IoErr="); w_long(IoErr()); w("\n");
        if (home != 0) UnLock(home);
        return 1;
    }

    // DIAGNOSTIC: redirect bebbossh's stdout AND stderr to REAL
    // files (DH1:bb-out.log / bb-err.log). Compare with handler-FH
    // version — if the file gets data but handler-FH doesn't, the
    // issue is in our handler. If file is also empty, bebbossh
    // writes via a non-standard path.
    BPTR real_out = Open((CONST_STRPTR) "DH1:bb-out.log", MODE_NEWFILE);
    BPTR real_err = Open((CONST_STRPTR) "DH1:bb-err.log", MODE_NEWFILE);
    w("[parent] real_out="); w_long((LONG) real_out);
    w(" real_err="); w_long((LONG) real_err); w("\n");

    Forbid();
    struct Process * child = CreateNewProcTags(
        NP_Seglist,     (ULONG) seg,
        NP_FreeSeglist, TRUE,
        NP_Cli,         TRUE,
        NP_Input,       (ULONG) fh_in_b,
        NP_Output,      (ULONG) fh_out_b,           /* handler-FH */
        NP_Error,       (ULONG) fh_err_b,           /* handler-FH */
        NP_ConsoleTask, (ULONG) handler_port,
        NP_HomeDir,     (ULONG) home,
        NP_Arguments,   (ULONG) "anders@172.22.8.177\n",
        NP_Name,        (ULONG) "bebbossh-test",
        NP_StackSize,   65536,
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
            w("[parent] CLI patched, cli=0x"); w_hex((ULONG) cli); w("\n");
        }
    }
    Permit();
    if (child == NULL) {
        w("[parent] CreateNewProc(child) failed\n");
        UnLoadSeg(seg);
        return 1;
    }
    w("[parent] child spawned, child=0x"); w_hex((ULONG) child); w("\n");
    w("[parent] pr_CIS=0x"); w_hex((ULONG) child->pr_CIS); w("\n");
    w("[parent] pr_COS=0x"); w_hex((ULONG) child->pr_COS); w("\n");
    w("[parent] pr_CES=0x"); w_hex((ULONG) child->pr_CES); w("\n");
    w("[parent] pr_ConsoleTask=0x"); w_hex((ULONG) child->pr_ConsoleTask); w("\n");

    // Give bebbossh time to attempt the connection. amiberry's
    // bsdsocket_emu reaches host, so SSH banner exchange + cert
    // prompt + password prompt should fit comfortably in 20s.
    int ticks = 0;
    while (!g_state.child_exited && ticks < 30) {
        Delay(50);   // 1s
        ticks++;
        if ((ticks % 5) == 0) {
            w("[parent] wait tick="); w_long(ticks);
            w(" child_exited="); w_long(g_state.child_exited);
            w(" open_count="); w_long(g_state.open_count); w("\n");
        }
    }
    w("[parent] loop done after "); w_long(ticks); w(" seconds\n");
    w("[parent] final child_exited="); w_long(g_state.child_exited); w("\n");

    // Shut handler down.
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
    // After everything, read the bb-out / bb-err logs (child should
    // have closed them on exit). Echo into our log so we see what
    // bebbossh actually wrote.
    w("[parent] reading bb-out.log\n");
    BPTR ro = Open((CONST_STRPTR) "DH1:bb-out.log", MODE_OLDFILE);
    if (ro != 0) {
        char b[256]; LONG r;
        while ((r = Read(ro, b, sizeof(b))) > 0) Write(g_log, b, r);
        Close(ro);
    }
    w("\n[parent] reading bb-err.log\n");
    BPTR re = Open((CONST_STRPTR) "DH1:bb-err.log", MODE_OLDFILE);
    if (re != 0) {
        char b[256]; LONG r;
        while ((r = Read(re, b, sizeof(b))) > 0) Write(g_log, b, r);
        Close(re);
    }
    w("\n[parent] done\n");
    return 0;
}
