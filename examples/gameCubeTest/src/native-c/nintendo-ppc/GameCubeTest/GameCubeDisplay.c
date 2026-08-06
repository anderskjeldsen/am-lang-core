#include <libc/core.h>
#include <libc/core_inline_functions.h>
#include <gccore.h>
#include <malloc.h>
#include <string.h>

#define FIFO_SIZE (256 * 1024)

static void *frameBuffer[2] = { NULL, NULL };
static void *fifoBuffer = NULL;
static GXRModeObj *screenMode = NULL;

function_result GameCubeTest_GameCubeDisplay__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	__increase_reference_count(this);
	return __result;
}

function_result GameCubeTest_GameCubeDisplay__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	(void) this;
	return __result;
}

function_result GameCubeTest_GameCubeDisplay__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	(void) this;
	return __result;
}

function_result GameCubeTest_GameCubeDisplay_renderTriangle_0()
{
	function_result __result = { .has_return_value = false };

	VIDEO_Init();
	screenMode = VIDEO_GetPreferredMode(NULL);
	if (screenMode == NULL) {
		goto __wait_forever;
	}

	// Double-buffered external framebuffers (canonical libogc pattern).
	frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(screenMode));
	frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(screenMode));
	if (frameBuffer[0] == NULL || frameBuffer[1] == NULL) {
		goto __wait_forever;
	}

	fifoBuffer = MEM_K0_TO_K1(memalign(32, FIFO_SIZE));
	if (fifoBuffer == NULL) {
		goto __wait_forever;
	}
	memset(fifoBuffer, 0, FIFO_SIZE);

	VIDEO_Configure(screenMode);
	VIDEO_SetNextFramebuffer(frameBuffer[0]);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (screenMode->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}

	// --- GX setup -------------------------------------------------------
	GXColor background = { 0x10, 0x10, 0x30, 0xFF };
	GX_Init(fifoBuffer, FIFO_SIZE);
	GX_SetCopyClear(background, 0x00FFFFFF);

	GX_SetViewport(0.0f, 0.0f, (f32) screenMode->fbWidth, (f32) screenMode->efbHeight, 0.0f, 1.0f);
	GX_SetDispCopyYScale((f32) screenMode->xfbHeight / (f32) screenMode->efbHeight);
	GX_SetScissor(0, 0, screenMode->fbWidth, screenMode->efbHeight);
	GX_SetDispCopySrc(0, 0, screenMode->fbWidth, screenMode->efbHeight);
	GX_SetDispCopyDst(screenMode->fbWidth, screenMode->xfbHeight);
	GX_SetCopyFilter(screenMode->aa, screenMode->sample_pattern, GX_TRUE, screenMode->vfilter);
	GX_SetFieldMode(screenMode->field_rendering,
		((screenMode->viHeight == 2 * screenMode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetDispCopyGamma(GX_GM_1_0);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);

	// Orthographic projection: (0,0) top-left .. (fbWidth,efbHeight).
	Mtx44 projection;
	guOrtho(projection, 0, screenMode->efbHeight, 0, screenMode->fbWidth, 0, 1);
	GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);

	Mtx modelView;
	guMtxIdentity(modelView);
	GX_LoadPosMtxImm(modelView, GX_PNMTX0);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetNumChans(1);
	GX_SetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
	GX_SetNumTexGens(0);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetNumTevStages(1);

	// --- Main render loop (never returns) -------------------------------
	// Everything — draw, finish, copy, flip — happens here on the main
	// thread. No work is done from the retrace interrupt, so the GP FIFO
	// is never touched concurrently. A GameCube program must not return
	// from its entry point, hence the infinite loop.
	unsigned int fbi = 0;
	unsigned int frame = 0;
	for (;;) {
		// A centered orange quad so it is unmistakably visible, on a
		// pulsing background that proves the loop is live.
		unsigned char pulse = (unsigned char)((frame & 0x7F) < 0x40
			? (frame & 0x7F) * 2
			: (0x7F - (frame & 0x7F)) * 2);
		GXColor bg = { pulse, 0x10, (unsigned char)(0x40 + pulse / 2), 0xFF };
		GX_SetCopyClear(bg, 0x00FFFFFF);

		s16 x0 = (s16)(screenMode->fbWidth / 4);
		s16 x1 = (s16)(screenMode->fbWidth * 3 / 4);
		s16 y0 = (s16)(screenMode->efbHeight / 4);
		s16 y1 = (s16)(screenMode->efbHeight * 3 / 4);

		GX_SetViewport(0.0f, 0.0f, (f32) screenMode->fbWidth, (f32) screenMode->efbHeight, 0.0f, 1.0f);

		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
			GX_Position3s16(x0, y0, 0); GX_Color4u8(0xFF, 0x80, 0x00, 0xFF);
			GX_Position3s16(x1, y0, 0); GX_Color4u8(0xFF, 0x80, 0x00, 0xFF);
			GX_Position3s16(x1, y1, 0); GX_Color4u8(0xFF, 0x80, 0x00, 0xFF);
			GX_Position3s16(x0, y1, 0); GX_Color4u8(0xFF, 0x80, 0x00, 0xFF);
		GX_End();

		// Finish drawing, copy the EFB to the current framebuffer, and
		// flip it in on the next retrace.
		GX_DrawDone();
		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
		GX_SetColorUpdate(GX_TRUE);
		GX_CopyDisp(frameBuffer[fbi], GX_TRUE);
		GX_Flush();

		VIDEO_SetNextFramebuffer(frameBuffer[fbi]);
		VIDEO_Flush();
		VIDEO_WaitVSync();

		fbi ^= 1;
		++frame;
	}

__wait_forever:
	// Reached only if video/framebuffer/FIFO allocation failed above.
	for (;;) {
		VIDEO_WaitVSync();
	}

	return __result;
}
