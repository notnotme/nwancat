/*
	!!M - nwancat tinytro
	---------------------
	              01/2012


	A tous les soi-disant sceners qui s'amusent à thumb down
	juste "pour le fun" ou casser les couilles aux autres tout simplement
	alors qu'ils ne sortent jamais rien :
		
	Dédicace à ma chatte MAYA: Miaou, ma vieille :)
*/
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <malloc.h>
#include <wiiuse/wpad.h>
#include <ogc/tpl.h>
#include <ogc/lwp_watchdog.h>
#include <asndlib.h>
#include <mp3player.h>

// makefile generated headers
#include "nyannyannyan_mp3.h"
#include "textures_tpl.h"
#include "textures.h"

// GX stuff
#define DEFAULT_FIFO_SIZE	(256*1024)
static void *gp_fifo = NULL;
static void *xfb[2] = {NULL, NULL};
static GXRModeObj *rmode = NULL;
static u32	wichFb;
static f32 yscale;
static u32 xfbHeight;
static Mtx44 perspective;
static Mtx GXmodelView2D;
GXTexObj spriteSheetTexture;
static GXColor background = {0x00, 0x00, 0x00, 0xff};

// usefull
#define SPRITESHEET_WIDTH 512
#define BKG_WIDTH 64
#define BKG_STEP 7
#define BKG_SIZE 480
#define NYAN_WIDTH 55
#define NYAN_HEIGHT 22
#define NYAN_STEP 6
u64 startTime;
u64 bkgTimeCounter;
u64 nyanTimeCounter;
u64 delta;
int currentBkgStep;
int currentNyanStep;

// nyanise the system
void nyan()
{
	// subsystem
	VIDEO_Init();
	WPAD_Init();
	ASND_Init();
	MP3Player_Init();

	// video setup
	wichFb = 0;
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb[wichFb]);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	wichFb ^= 1;

	// setup the fifo and then init gx
	gp_fifo = memalign(32, DEFAULT_FIFO_SIZE);
	memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);
 	GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);
 	// other gx setup
	GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight==2*rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	if (rmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(xfb[wichFb], GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);
	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);
	GX_SetCopyClear(background, 0x00ffffff);

	// empty the vertex descriptor
	GX_InvVtxCache();
	GX_InvalidateTexAll();
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	// tev is like shadow for me (i don't really understand these Chans, and TevOps-nyan-thing things)
	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_InvalidateTexAll();

	// Load the spriteSheet
	TPLFile spriteTPL;
	TPL_OpenTPLFromMemory(&spriteTPL, (void *)textures_tpl, textures_tpl_size);
	TPL_GetTexture(&spriteTPL, spritesheet, &spriteSheetTexture);
	// no filtering plz
	GX_InitTexObjLOD(&spriteSheetTexture, GX_NEAR, GX_NEAR,	0.0f, 0.0f,	0.0f, GX_FALSE,	GX_FALSE, GX_ANISO_1);
	GX_LoadTexObj(&spriteSheetTexture, GX_TEXMAP0); // Load texture in slot 0 into gx

	// Setup the view
	GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	guOrtho(perspective, 0, 479, 0, 639, 0, 300);
	GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

	startTime = ticks_to_millisecs(gettime());
	bkgTimeCounter = startTime;
	nyanTimeCounter = startTime;
	currentBkgStep = 0;
	currentNyanStep = 0;
}

// draw the nyanground
inline void drawBkgSprite(f32 x, int imageIndex)
{
	static const f32 step = 1.0f / (SPRITESHEET_WIDTH / BKG_WIDTH);
	f32 offset = imageIndex*step;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);			// Draw A Quad
		GX_Position2f32(x, 0);					// Top Left
		GX_TexCoord2f32(offset, 0.0f);

		GX_Position2f32(x+BKG_SIZE, 0);			// Top Right
		GX_TexCoord2f32(offset+step, 0.0f);

		GX_Position2f32(x+BKG_SIZE, BKG_SIZE);	// Bottom Right
		GX_TexCoord2f32(offset+step, step);

		GX_Position2f32(x, BKG_SIZE);			// Bottom Left
		GX_TexCoord2f32(offset, step);
	GX_End();	
}

// nyan the draw
inline void drawNyan(f32 x, f32 y, int imageIndex)
{
	static const f32 _offset = 1.0f / (SPRITESHEET_WIDTH / BKG_WIDTH); // bkg offset
	static const f32 stepw = 1.0f / (SPRITESHEET_WIDTH / NYAN_WIDTH);
	static const f32 steph = 1.0f / (SPRITESHEET_WIDTH / NYAN_HEIGHT);
	f32 offset = imageIndex*stepw;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);			// Draw A Quad
		GX_Position2f32(x, y);					// Top Left
		GX_TexCoord2f32(offset, _offset);

		GX_Position2f32(x+(NYAN_WIDTH*8), y);			// Top Right
		GX_TexCoord2f32(offset+stepw, _offset);

		GX_Position2f32(x+(NYAN_WIDTH*8), y+(NYAN_HEIGHT*8));	// Bottom Right
		GX_TexCoord2f32(offset+stepw, _offset+steph);

		GX_Position2f32(x, y+(NYAN_HEIGHT*8));			// Bottom Left
		GX_TexCoord2f32(offset, _offset+steph);
	GX_End();	
}

// rulez nyan scene
int main(int argc, char **argv)
{
	bool nyaning = true;

	// self explain
	nyan();
	MP3Player_PlayBuffer(nyannyannyan_mp3, nyannyannyan_mp3_size, NULL);

	VIDEO_SetBlack(FALSE);
	while(nyaning)
	{
		u64 cticks = ticks_to_millisecs(gettime());
		// Loop the sound if ended
		if(!MP3Player_IsPlaying()) MP3Player_PlayBuffer(nyannyannyan_mp3, nyannyannyan_mp3_size, NULL);
		//Check wiimote input
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME) nyaning = false;
		// blackscreen until 3,8s like the youtube video (not extreme precison ;))
		if(cticks < startTime+3900) continue;
		// bkg frame counter (tick each 100ms)
		if(cticks > bkgTimeCounter+100)
		{
			bkgTimeCounter = cticks;
			currentBkgStep = (currentBkgStep+1) % BKG_STEP;
		}
		// nyan frame counter (tick each 60ms)
		if(cticks > nyanTimeCounter+60)
		{
			nyanTimeCounter = cticks;
			currentNyanStep = (currentNyanStep+1) % NYAN_STEP;
		}
		// Set the 2d matrix
		guMtxIdentity(GXmodelView2D);
		GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);

		// nyan
		f32 move =  delta % BKG_SIZE;
		f32 x = -move;
		while(x < rmode->fbWidth)
		{
			drawBkgSprite(x, currentBkgStep);
			x += BKG_SIZE;
		}

		// nyan nyan
		drawNyan(-5, 240 - ((NYAN_HEIGHT*8)/2), currentNyanStep);

		// Copy & switch fb
		GX_DrawDone();
		GX_CopyDisp(xfb[wichFb], GX_TRUE);
		VIDEO_SetNextFramebuffer(xfb[wichFb]);
		VIDEO_Flush();
		VIDEO_WaitVSync();
		wichFb ^= 1;
		delta += 8;
	}

	return 0;
}
