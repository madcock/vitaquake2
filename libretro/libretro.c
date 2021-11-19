/*
Copyright (C) 2017 Felipe Izzo
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifdef HAVE_OPENGL
#include <glsym/rglgen_private_headers.h>
#include <glsym/glsym.h>
#endif

#include <ctype.h>
#include <libretro.h>
#include <retro_dirent.h>
#include <features/features_cpu.h>
#include <file/file_path.h>

#include "libretro_core_options.h"

#include "../client/client.h"
#include "../client/qmenu.h"

#ifdef HAVE_OPENGL
#include "../ref_gl/gl_local.h"

#include <glsm/glsm.h>
#endif

qboolean gl_set = false;
bool is_soft_render = false;

unsigned	sys_frame_time;
uint64_t rumble_tick;
void *tex_buffer = NULL;

float libretro_gamma = 1.0f;
#ifdef HAVE_OPENGL
extern cvar_t *gl_shadows;
static bool libretro_shared_context = false;
static bool enable_opengl = true;
float libretro_gl_modulate = 4.0f;
#else
static const bool enable_opengl = false;
#endif
extern cvar_t *sw_texfilt;

refimport_t ri;

/* TODO/FIXME - should become float for better accuracy */
int      framerate    = 60;
unsigned framerate_ms = 16;

float *gVertexBuffer;
float *gColorBuffer;
float *gTexCoordBuffer;

#ifdef HAVE_OPENGL
void ( APIENTRY * qglBlendFunc )(GLenum sfactor, GLenum dfactor);
void ( APIENTRY * qglTexImage2D )(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglTexParameteri )(GLenum target, GLenum pname, GLint param);
void ( APIENTRY * qglBindFramebuffer )(GLenum target, GLuint framebuffer);
void ( APIENTRY * qglGenerateMipmap )(GLenum target);
void ( APIENTRY * qglTexSubImage2D )(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglDepthMask )(GLboolean flag);
void ( APIENTRY * qglPushMatrix )(void);
void ( APIENTRY * qglRotatef )(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglTranslatef )(GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglDepthRange )(GLclampd zNear, GLclampd zFar);
void ( APIENTRY * qglClear )(GLbitfield mask);
void ( APIENTRY * qglEnable )(GLenum cap);
void ( APIENTRY * qglDisable )(GLenum cap);
void ( APIENTRY * qglPopMatrix )(void);
void ( APIENTRY * qglGetFloatv )(GLenum pname, GLfloat *params);
void ( APIENTRY * qglOrtho )(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void ( APIENTRY * qglFrustum )(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void ( APIENTRY * qglLoadMatrixf )(const GLfloat *m);
void ( APIENTRY * qglLoadIdentity )(void);
void ( APIENTRY * qglMatrixMode )(GLenum mode);
void ( APIENTRY * qglBindTexture )(GLenum target, GLuint texture);
void ( APIENTRY * qglReadPixels )(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
void ( APIENTRY * qglPolygonMode )(GLenum face, GLenum mode);
void ( APIENTRY * qglVertexPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglColorPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglTexCoordPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglDrawElements )(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void ( APIENTRY * qglClearColor )(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void ( APIENTRY * qglCullFace )(GLenum mode);
void ( APIENTRY * qglViewport )(GLint x, GLint y, GLsizei width, GLsizei height);
void ( APIENTRY * qglDeleteTextures )(GLsizei n, const GLuint *textures);
void ( APIENTRY * qglClearStencil )(GLint s);
void ( APIENTRY * qglColor4f )(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void ( APIENTRY * qglScissor )(GLint x, GLint y, GLsizei width, GLsizei height);
void ( APIENTRY * qglEnableClientState )(GLenum array);
void ( APIENTRY * qglDisableClientState )(GLenum array);
void ( APIENTRY * qglStencilFunc )(GLenum func, GLint ref, GLuint mask);
void ( APIENTRY * qglStencilOp )(GLenum fail, GLenum zfail, GLenum zpass);
void ( APIENTRY * qglScalef )(GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglDepthFunc )(GLenum func);
void ( APIENTRY * qglTexEnvi )(GLenum target, GLenum pname, GLint param);
void ( APIENTRY * qglAlphaFunc )(GLenum func,  GLclampf ref);

#define GL_FUNCS_NUM 44

typedef struct api_entry{
	void *ptr;
} api_entry;

api_entry funcs[GL_FUNCS_NUM];
#endif

char g_rom_dir[1024] = {0};
char g_save_dir[1024] = {0};

#ifdef HAVE_OPENGL
extern struct retro_hw_render_callback hw_render;

#define MAX_INDICES 4096
uint16_t* indices = NULL;

float *gVertexBufferPtr = NULL;
float *gColorBufferPtr = NULL;
float *gTexCoordBufferPtr = NULL;
#endif

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
retro_environment_t environ_cb;
static retro_input_poll_t poll_cb;
static retro_input_state_t input_cb;
static struct retro_rumble_interface rumble;
static bool libretro_supports_bitmasks = false;

static void audio_callback(void);

#define SAMPLE_RATE  48000
#define BUFFER_SIZE 	2048

/* System analog stick range is -0x8000 to 0x8000 */
#define ANALOG_RANGE 0x8000
/* Default deadzone: 15% */
static int analog_deadzone = (int)(0.15f * ANALOG_RANGE);

typedef struct {
	char *keyname;
	int keynum;
	char *command;
} input_bind_t;

static struct retro_input_descriptor input_desc[] = {
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Open Inventory" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Menu Up" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Menu Down" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Use Inventory Item" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "Menu Cancel" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "Menu Select" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "Next Weapon" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "Run" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Crouch / Descend" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,     "Jump / Climb" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,     "Attack" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3,     "Drop Inventory Item" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Show / Hide Help Computer" },
	{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Show Menu" },
	{ 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_X, "Strafe" },
	{ 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_Y, "Move" },
	{ 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "Horizontal Turn" },
	{ 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Vertical Look" },
	{ 0 },
};

/* Note: K_AUX1 and K_AUX4 are reserved for use
 * in the menu - but we still have to bind them
 * to *something*, otherwise the keys will be
 * ignored (great design there...) */
static input_bind_t input_binds[] = {
	[RETRO_DEVICE_ID_JOYPAD_B]      = { "AUX4",       K_AUX4,       "" }, /* Menu Cancel */
	[RETRO_DEVICE_ID_JOYPAD_Y]      = { "AUX2",       K_AUX2,       "weapnext" },
	[RETRO_DEVICE_ID_JOYPAD_SELECT] = { "SELECT",     K_ENTER,      "help" },
	[RETRO_DEVICE_ID_JOYPAD_START]  = { 0 },
	[RETRO_DEVICE_ID_JOYPAD_UP]     = { "UPARROW",    K_UPARROW,    "invprev" },
	[RETRO_DEVICE_ID_JOYPAD_DOWN]   = { "DOWNARROW",  K_DOWNARROW,  "invnext" },
	[RETRO_DEVICE_ID_JOYPAD_LEFT]   = { "LEFTARROW",  K_LEFTARROW,  "inven" },
	[RETRO_DEVICE_ID_JOYPAD_RIGHT]  = { "RIGHTARROW", K_RIGHTARROW, "invuse" },
	[RETRO_DEVICE_ID_JOYPAD_A]      = { "AUX1",       K_AUX1,       "" }, /* Menu Select */
	[RETRO_DEVICE_ID_JOYPAD_X]      = { 0 },
	[RETRO_DEVICE_ID_JOYPAD_L]      = { "AUX3",       K_AUX3,       "+speed" },
	[RETRO_DEVICE_ID_JOYPAD_R]      = { "AUX5",       K_AUX5,       "+movedown" },
	[RETRO_DEVICE_ID_JOYPAD_L2]     = { "AUX6",       K_AUX6,       "+moveup" },
	[RETRO_DEVICE_ID_JOYPAD_R2]     = { "AUX7",       K_AUX7,       "+attack" },
	[RETRO_DEVICE_ID_JOYPAD_L3]     = { 0 },
	[RETRO_DEVICE_ID_JOYPAD_R3]     = { "AUX8",       K_AUX8,       "invdrop" },
};
#define INPUT_BINDS_LEN (sizeof(input_binds) / sizeof(input_binds[0]))

static bool context_needs_reinit = true;

#ifdef HAVE_OPENGL
void GL_DrawPolygon(GLenum prim, int num)
{
	qglDrawElements(prim, num, GL_UNSIGNED_SHORT, indices);
}

void glVertexAttribPointerMapped(int id, void* ptr)
{
   switch (id)
   {
      case 0: /* Vertex */
         qglVertexPointer(3, GL_FLOAT, 0, ptr);
         break;
      case 1: /* TexCoord */
         qglTexCoordPointer(2, GL_FLOAT, 0, ptr);
         break;
      case 2: /* Color */
         qglColorPointer(4, GL_FLOAT, 0, ptr);
         break;
   }
}

static bool initialize_gl()
{
	funcs[0].ptr  = qglTexImage2D         = hw_render.get_proc_address ("glTexImage2D");
	funcs[1].ptr  = qglTexSubImage2D      = hw_render.get_proc_address ("glTexSubImage2D");
	funcs[2].ptr  = qglTexParameteri      = hw_render.get_proc_address ("glTexParameteri");
	funcs[3].ptr  = qglBindFramebuffer    = hw_render.get_proc_address ("glBindFramebuffer");
	funcs[4].ptr  = qglGenerateMipmap     = hw_render.get_proc_address ("glGenerateMipmap");
	funcs[5].ptr  = qglBlendFunc          = hw_render.get_proc_address ("glBlendFunc");
	funcs[6].ptr  = qglTexSubImage2D      = hw_render.get_proc_address ("glTexSubImage2D");
	funcs[7].ptr  = qglDepthMask          = hw_render.get_proc_address ("glDepthMask");
	funcs[8].ptr  = qglPushMatrix         = hw_render.get_proc_address ("glPushMatrix");
	funcs[9].ptr  = qglRotatef            = hw_render.get_proc_address ("glRotatef");
	funcs[10].ptr = qglTranslatef         = hw_render.get_proc_address ("glTranslatef");
	funcs[11].ptr = qglDepthRange         = hw_render.get_proc_address ("glDepthRange");
	funcs[12].ptr = qglClear              = hw_render.get_proc_address ("glClear");
	funcs[13].ptr = qglCullFace           = hw_render.get_proc_address ("glCullFace");
	funcs[14].ptr = qglClearColor         = hw_render.get_proc_address ("glClearColor");
	funcs[15].ptr = qglEnable             = hw_render.get_proc_address ("glEnable");
	funcs[16].ptr = qglDisable            = hw_render.get_proc_address ("glDisable");
	funcs[17].ptr = qglEnableClientState  = hw_render.get_proc_address ("glEnableClientState");
	funcs[18].ptr = qglDisableClientState = hw_render.get_proc_address ("glDisableClientState");
	funcs[19].ptr = qglPopMatrix          = hw_render.get_proc_address ("glPopMatrix");
	funcs[20].ptr = qglGetFloatv          = hw_render.get_proc_address ("glGetFloatv");
	funcs[21].ptr = qglOrtho              = hw_render.get_proc_address ("glOrtho");
	funcs[22].ptr = qglFrustum            = hw_render.get_proc_address ("glFrustum");
	funcs[23].ptr = qglLoadMatrixf        = hw_render.get_proc_address ("glLoadMatrixf");
	funcs[24].ptr = qglLoadIdentity       = hw_render.get_proc_address ("glLoadIdentity");
	funcs[25].ptr = qglMatrixMode         = hw_render.get_proc_address ("glMatrixMode");
	funcs[26].ptr = qglBindTexture        = hw_render.get_proc_address ("glBindTexture");
	funcs[27].ptr = qglReadPixels         = hw_render.get_proc_address ("glReadPixels");
	funcs[28].ptr = qglPolygonMode        = hw_render.get_proc_address ("glPolygonMode");
	funcs[29].ptr = qglVertexPointer      = hw_render.get_proc_address ("glVertexPointer");
	funcs[30].ptr = qglTexCoordPointer    = hw_render.get_proc_address ("glTexCoordPointer");
	funcs[31].ptr = qglColorPointer       = hw_render.get_proc_address ("glColorPointer");
	funcs[32].ptr = qglDrawElements       = hw_render.get_proc_address ("glDrawElements");
	funcs[33].ptr = qglViewport           = hw_render.get_proc_address ("glViewport");
	funcs[34].ptr = qglDeleteTextures     = hw_render.get_proc_address ("glDeleteTextures");
	funcs[35].ptr = qglClearStencil       = hw_render.get_proc_address ("glClearStencil");
	funcs[36].ptr = qglColor4f            = hw_render.get_proc_address ("glColor4f");
	funcs[37].ptr = qglScissor            = hw_render.get_proc_address ("glScissor");
	funcs[38].ptr = qglStencilFunc        = hw_render.get_proc_address ("glStencilFunc");
	funcs[39].ptr = qglStencilOp          = hw_render.get_proc_address ("glStencilOp");
	funcs[40].ptr = qglScalef             = hw_render.get_proc_address ("glScalef");
	funcs[41].ptr = qglDepthFunc          = hw_render.get_proc_address ("glDepthFunc");
	funcs[42].ptr = qglTexEnvi            = hw_render.get_proc_address ("glTexEnvi");
	funcs[43].ptr = qglAlphaFunc          = hw_render.get_proc_address ("glAlphaFunc");
	
	if (log_cb) {
		int i;
		for (i = 0; i < GL_FUNCS_NUM; i++) {
			if (!funcs[i].ptr) log_cb(RETRO_LOG_ERROR, "vitaQuakeII: cannot get GL function #%d symbol.\n", i);
		}
	}
	
	return true;
}

int GLimp_Init( void *hinstance, void *wndproc )
{
   return 0;
}

void GLimp_AppActivate( qboolean active )
{
}

void GLimp_BeginFrame( float camera_separation )
{
	gVertexBuffer = gVertexBufferPtr;
	gColorBuffer = gColorBufferPtr;
	gTexCoordBuffer = gTexCoordBufferPtr;
	qglEnableClientState(GL_VERTEX_ARRAY);
	qglAlphaFunc(GL_GREATER,  0.666);
}

void GLimp_EndFrame (void)
{
}

qboolean GLimp_InitGL (void)
{
   int i;
	indices = (uint16_t*)malloc(sizeof(uint16_t)*MAX_INDICES);
	for (i=0;i<MAX_INDICES;i++){
		indices[i] = i;
	}
	gVertexBufferPtr = (float*)malloc(0x400000);
	gColorBufferPtr = (float*)malloc(0x200000);
	gTexCoordBufferPtr = (float*)malloc(0x200000);
	gl_set = true;
	return true;
}

void GLimp_DeinitGL (void)
{
	if (indices)
		free(indices);
	indices = NULL;

	if (gVertexBufferPtr)
		free(gVertexBufferPtr);
	gVertexBufferPtr = NULL;

	if (gColorBufferPtr)
		free(gColorBufferPtr);
	gColorBufferPtr = NULL;

	if (gTexCoordBufferPtr)
		free(gTexCoordBufferPtr);
	gTexCoordBufferPtr = NULL;

	gl_set = false;
}
#endif

static void context_destroy(void) 
{
	context_needs_reinit = true;
}

extern void restore_textures(void);
bool first_reset = true;

static void context_reset(void)
{
	if (!context_needs_reinit)
		return;
#ifdef HAVE_OPENGL
	glsm_ctl(GLSM_CTL_STATE_CONTEXT_RESET, NULL);

   if (!libretro_shared_context)
      if (!glsm_ctl(GLSM_CTL_STATE_SETUP, NULL))
         return;
	
	if (!is_soft_render) {
		initialize_gl();
		if (!first_reset)
			restore_textures();
		first_reset = false;
	}
#endif
	context_needs_reinit = false;
}

#ifdef HAVE_OPENGL
static bool context_framebuffer_lock(void *data)
{
    return false;
}

bool initialize_opengl(void)
{
   glsm_ctx_params_t params = {0};

   params.context_type     = RETRO_HW_CONTEXT_OPENGL;
   params.context_reset    = context_reset;
   params.context_destroy  = context_destroy;
   params.environ_cb       = environ_cb;
   params.stencil          = true;
   params.framebuffer_lock = context_framebuffer_lock;

   if (!glsm_ctl(GLSM_CTL_STATE_CONTEXT_INIT, &params))
   {
      log_cb(RETRO_LOG_ERROR, "Could not setup glsm.\n");
      return false;
   }

   if (environ_cb(RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT, NULL))
      libretro_shared_context = true;
   else
      libretro_shared_context = false;

   return true;
}

void destroy_opengl(void)
{
   if (!glsm_ctl(GLSM_CTL_STATE_CONTEXT_DESTROY, NULL))
   {
      log_cb(RETRO_LOG_ERROR, "Could not destroy glsm context.\n");
   }

   libretro_shared_context = false;
}
#endif

/* sys.c */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "errno.h"

int	curtime;
char cmd_line[256];
	
static byte	*membase    = NULL;
static int  hunkmaxsize = 0;
static int  cursize     = 0;

bool shutdown_core = false;

netadr_t	net_local_adr;

extern uint64_t rumble_tick;
int scr_width = 960, scr_height = 544;
void *GetGameAPI (void *import);
qboolean	NET_CompareAdr (netadr_t a, netadr_t b)
{
	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port)
		return true;
	return false;
}

void NET_Init (void)
{
	
}

void NET_Sleep(int msec)
{
}

#define	LOOPBACK	0x7f000001

#define	MAX_LOOPBACK	4

typedef struct
{
	byte	data[MAX_MSGLEN];
	int		datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t	msgs[MAX_LOOPBACK];
	int			get, send;
} loopback_t;

loopback_t	loopbacks[2];

qboolean	NET_GetLoopPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return false;

	i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	memcpy (net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	*net_from = net_local_adr;
	return true;

}


void NET_SendLoopPacket (netsrc_t sock, int length, void *data, netadr_t to)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock^1];

	i = loop->send & (MAX_LOOPBACK-1);
	loop->send++;

	memcpy (loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

qboolean	NET_CompareBaseAdr (netadr_t a, netadr_t b)
{
	if (a.type != b.type)
		return false;

	if (a.type == NA_LOOPBACK)
		return true;

	if (a.type == NA_IP)
	{
		if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3])
			return true;
		return false;
	}

	if (a.type == NA_IPX)
	{
		if ((memcmp(a.ipx, b.ipx, 10) == 0))
			return true;
		return false;
	}
	return false;
}

char	*NET_AdrToString (netadr_t a)
{
	static	char	s[64];
#if 0
	Com_sprintf (s, sizeof(s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));
#endif

	return s;
}

qboolean	NET_StringToAdr (char *s, netadr_t *a)
{
	memset (a, 0, sizeof(*a));
	a->type = NA_LOOPBACK;
	return true;
}

void NET_SendPacket (netsrc_t sock, int length, void *data, netadr_t to)
{
	if ( to.type == NA_LOOPBACK )
	{
		NET_SendLoopPacket (sock, length, data, to);
		return;
	}
}

void	NET_Config (qboolean multiplayer)
{
}

qboolean	NET_IsLocalAddress (netadr_t adr)
{
	return NET_CompareAdr (adr, net_local_adr);
}

qboolean	NET_GetPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	if (NET_GetLoopPacket (sock, net_from, net_message))
		return true;
	
	return false;
}

void GLimp_Shutdown( void )
{

}

void CDAudio_Play(int track, qboolean looping)
{
}


void CDAudio_Stop(void)
{
}


void CDAudio_Resume(void)
{
}


void CDAudio_Update(void)
{
}


int CDAudio_Init(void)
{
	return 0;
}


void CDAudio_Shutdown(void)
{
}

static void extract_directory(char *buf, const char *path, size_t size)
{
   char *base = NULL;

   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
    {
       buf[0] = '.';
       buf[1] = '\0';
    }
}

int y = 20;
void LOG_FILE(const char *format, ...){
        va_list arg;
	int done;
	va_start(arg, format);
	char msg[512];
	done = vsnprintf(msg, 500, format, arg);
	va_end(arg);
	if (log_cb)
		log_cb(RETRO_LOG_INFO, "LOG2FILE: %s", msg);
	else
		fprintf(stderr, "LOG2FILE: %s\n", msg);
}

void Sys_Error (char *error, ...)
{
	char str[512] = { 0 };
	va_list		argptr;

	va_start (argptr,error);
	vsnprintf (str,512, error, argptr);
	va_end (argptr);
	LOG_FILE("Sys_Error: %s", str);
	Sys_Quit();
}

void Sys_Quit (void)
{
	LOG_FILE("Sys_Quit called");
	environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
	shutdown_core = true;
}

void Sys_UnloadGame (void)
{

} 

void *Sys_GetGameAPI (void *parms)
{
	return GetGameAPI (parms);
}


char *Sys_ConsoleInput (void)
{
	return NULL;
}

void Sys_ConsoleOutput (char *string)
{
	LOG_FILE("%s",string);
}

void utf2ascii(char* dst, uint16_t* src){
	if(!src || !dst)return;
	while(*src)*(dst++)=(*(src++))&0xFF;
	*dst=0x00;
}

void Sys_DefaultConfig(void)
{
   char buf[100];
   unsigned i;

	Cbuf_AddText("unbindall\n");

   for (i = 0; i < INPUT_BINDS_LEN; i++)
   {
		if (!input_binds[i].keyname)
			continue;

      snprintf(buf, sizeof(buf),
				"bind %s \"%s\"\n",
				input_binds[i].keyname,
				input_binds[i].command);

      Cbuf_AddText (buf);
   }

	Cbuf_AddText ("lookstrafe \"1.000000\"\n");
	Cbuf_AddText ("lookspring \"0.000000\"\n");
}

extern menufield_s s_maxclients_field;
char *targetKeyboard;
void Sys_SetKeys(uint32_t keys, uint32_t state){
	Key_Event(keys, state, Sys_Milliseconds());
}

#define SET_BOUND_KEY(ret, id) Sys_SetKeys(input_binds[id].keynum, (ret & (1 << id)) ? 1 : 0)

void Sys_SendKeyEvents (void)
{
	int16_t ret = 0;

	if (!poll_cb)
		return;

	poll_cb();

	if (!input_cb)
		return;

	if (libretro_supports_bitmasks)
		ret = input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
	else
	{
		unsigned i;
		for (i = RETRO_DEVICE_ID_JOYPAD_B; i <= RETRO_DEVICE_ID_JOYPAD_R3; i++)
		{
			if (input_cb(0, RETRO_DEVICE_JOYPAD, 0, i))
				ret |= (1 << i);
		}
	}

	/* Open Inventory */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_LEFT);

	/* Inventory Previous */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_UP);

	/* Inventory Next */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_DOWN);

	/* Use Inventory Item */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_RIGHT);

	/* Next Weapon */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_Y);

	/* Run */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_L);

	/* Crouch / Descend */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_R);

	/* Jump / Climb */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_L2);

	/* Attack */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_R2);

	/* Drop Inventory Item */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_R3);

	/* Show / Hide Help Computer */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_SELECT);

	/* Note: Menu open/cancel/select keys are
	 * arbitrarily hard-coded
	 * > Show Menu: K_ESCAPE
	 * > Cancel: K_AUX4
	 * > Select: K_AUX1 */

	/* Show Menu */
	Sys_SetKeys(K_ESCAPE, (ret & (1 << RETRO_DEVICE_ID_JOYPAD_START)) ? 1 : 0);

	/* Menu Cancel */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_B);

	/* Menu Select */
	SET_BOUND_KEY(ret, RETRO_DEVICE_ID_JOYPAD_A);

	sys_frame_time = Sys_Milliseconds();
}


void Sys_AppActivate (void)
{
}

void Sys_CopyProtect (void)
{
}

char *Sys_GetClipboardData( void )
{
	return NULL;
}

void *Hunk_Begin (int maxsize)
{
	/* reserve a huge chunk of memory, but don't commit any yet */
	hunkmaxsize = maxsize;
	cursize     = 0;
	membase     = malloc(hunkmaxsize);

	if (!membase)
		Sys_Error("unable to allocate %d bytes", hunkmaxsize);
	else
		memset (membase, 0, hunkmaxsize);

	return (void*)membase;
}

void *Hunk_Alloc (int size)
{
	byte *buf;

	/* round to cacheline */
	size = (size+31)&~31;

	if (cursize + size > hunkmaxsize)
		Sys_Error("Hunk_Alloc overflow");

	buf = membase + cursize;
	cursize += size;

	return buf;
}

int Hunk_End (void)
{
	/* We would prefer to shrink the allocated memory
	 * buffer to 'cursize' bytes here, but there exists
	 * no robust cross-platform method for doing this
	 * given that pointers to arbitrary locations in
	 * the buffer are stored and used throughout the
	 * codebase...
	 * (i.e. realloc() would invalidate these pointers,
	 * and break everything)
	 * Attempts were made to allocate hunks dynamically,
	 * storing them in an RBUF array - but the codebase
	 * plays such dirty tricks with the returned pointers
	 * that this turned out to be impractical (it would
	 * have required a major rewrite of the renderers...) */
	return cursize;
}

void Hunk_Free (void *base)
{
	if (base == membase)
		membase = NULL;

	if (base)
		free(base);
}

int Sys_Milliseconds (void)
{
	static uint64_t	base;

	uint64_t time = cpu_features_get_time_usec() / 1000;
	
	if (!base)
	{
		base = time;
	}

	curtime = (int)(time - base);
	
	return curtime;
}

void Sys_Mkdir (char *path)
{
	path_mkdir(path);
}

void Sys_MkdirRecursive(char *path) {
        char tmp[256];
        char *p = NULL;
        size_t len;
 
        snprintf(tmp, sizeof(tmp),"%s",path);
        len = strlen(tmp);
        if(tmp[len - 1] == '/')
                tmp[len - 1] = 0;
        for(p = tmp + 1; *p; p++)
                if(*p == '/') {
                        *p = 0;
                        Sys_Mkdir(tmp);
                        *p = '/';
                }
        Sys_Mkdir(tmp);
}

static	char	findbase[MAX_OSPATH];
static	char	findpath[MAX_OSPATH * 2];
static	char	findpattern[MAX_OSPATH];
static	RDIR	*fdir = NULL;

/* Forward declarations */
static int glob_match(char *pattern, char *text);

/* Like glob_match, but match PATTERN against any final segment of TEXT.  */
static int glob_match_after_star(char *pattern, char *text)
{
	register char *p = pattern, *t = text;
	register char c, c1;

	while ((c = *p++) == '?' || c == '*')
		if (c == '?' && *t++ == '\0')
			return 0;

	if (c == '\0')
		return 1;

	if (c == '\\')
		c1 = *p;
	else
		c1 = c;

	while (1) {
		if ((c == '[' || *t == c1) && glob_match(p - 1, t))
			return 1;
		if (*t++ == '\0')
			return 0;
	}
}

/* Match the pattern PATTERN against the string TEXT;
   return 1 if it matches, 0 otherwise.
   A match means the entire string TEXT is used up in matching.
   In the pattern string, `*' matches any sequence of characters,
   `?' matches any character, [SET] matches any character in the specified set,
   [!SET] matches any character not in the specified set.
   A set is composed of characters or ranges; a range looks like
   character hyphen character (as in 0-9 or A-Z).
   [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
   Any other character in the pattern must be matched exactly.
   To suppress the special syntactic significance of any of `[]*?!-\',
   and match the character exactly, precede it with a `\'.
*/
static int glob_match(char *pattern, char *text)
{
	register char *p = pattern, *t = text;
	register char c;

	while ((c = *p++) != '\0')
		switch (c) {
		case '?':
			if (*t == '\0')
				return 0;
			else
				++t;
			break;

		case '\\':
			if (*p++ != *t++)
				return 0;
			break;

		case '*':
			return glob_match_after_star(p, t);

		case '[':
			{
				register char c1 = *t++;
				int invert;

				if (!c1)
					return (0);

				invert = ((*p == '!') || (*p == '^'));
				if (invert)
					p++;

				c = *p++;
				while (1) {
					register char cstart = c, cend = c;

					if (c == '\\') {
						cstart = *p++;
						cend = cstart;
					}
					if (c == '\0')
						return 0;

					c = *p++;
					if (c == '-' && *p != ']') {
						cend = *p++;
						if (cend == '\\')
							cend = *p++;
						if (cend == '\0')
							return 0;
						c = *p++;
					}
					if (c1 >= cstart && c1 <= cend)
						goto match;
					if (c == ']')
						break;
				}
				if (!invert)
					return 0;
				break;

			  match:
				/* Skip the rest of the [...] construct that already matched.  */
				while (c != ']') {
					if (c == '\0')
						return 0;
					c = *p++;
					if (c == '\0')
						return 0;
					else if (c == '\\')
						++p;
				}
				if (invert)
					return 0;
				break;
			}

		default:
			if (c != *t++)
				return 0;
		}

	return *t == '\0';
}

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canhave)
{
	char *p;

	if (fdir != NULL)
		Sys_Error ("Sys_BeginFind without close");

	COM_FilePath (path, findbase);
	strcpy(findbase, path);

	if ((p = strrchr(findbase, '/')) != NULL) {
		*p = 0;
		strcpy(findpattern, p + 1);
	} else
		strcpy(findpattern, "*");

	if (strcmp(findpattern, "*.*") == 0)
		strcpy(findpattern, "*");
	
	fdir = retro_opendir(findbase);
	if (fdir == NULL)
		return NULL;
	while ((retro_readdir(fdir)) > 0)
   {
      if (!*findpattern || 
            glob_match(findpattern, retro_dirent_get_name(fdir)))
      {
            sprintf (findpath, "%s/%s", findbase, retro_dirent_get_name(fdir));
            return findpath;
      }
   }
	return NULL;
}

char *Sys_FindNext (unsigned musthave, unsigned canhave)
{
	if (fdir == NULL)
		return NULL;
	while ((retro_readdir(fdir)) > 0)
   {
      if (!*findpattern || glob_match(findpattern, retro_dirent_get_name(fdir)))
      {
            sprintf (findpath, "%s/%s", findbase, retro_dirent_get_name(fdir));
            return findpath;
      }
   }
	return NULL;
}

void Sys_FindClose (void)
{
	if (fdir != NULL)
		retro_closedir(fdir);
		
	fdir = NULL;
}

void	Sys_Init (void)
{
}

extern void IN_StopRumble();

static int invert_y_axis = 1;

/*============================================================================= */
bool initial_resolution_set = false;
static void update_variables(bool startup)
{
	struct retro_variable var;
	struct retro_core_option_display option_display;

   var.key = "vitaquakeii_framerate";
   var.value = NULL;

   if (startup)
   {
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
      {
         if (!strcmp(var.value, "auto"))
         {
            float target_framerate = 0.0f;
            if (!environ_cb(RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE,
                     &target_framerate))
               target_framerate = 60.0f;

            framerate = (unsigned)target_framerate;

				/* Only discrete frame rates are permitted
				 * > 'round' to a legal value */
				if (framerate < 50)
					framerate = 50;
				else if ((framerate > 50) && (framerate < 72))
					framerate = 60;
				else if ((framerate > 60) && (framerate < 75))
					framerate = 72;
				else if ((framerate > 72) && (framerate < 90))
					framerate = 75;
				else if ((framerate > 75) && (framerate < 100))
					framerate = 90;
				else if ((framerate > 90) && (framerate < 119))
					framerate = 100;
				else if ((framerate > 100) && (framerate < 120))
					framerate = 119;
				else if ((framerate > 120) && (framerate < 155))
					framerate = 144;
				else if ((framerate > 144) && (framerate < 160))
					framerate = 155;
				else if ((framerate > 155) && (framerate < 180))
					framerate = 165;
				else if ((framerate > 165) && (framerate < 200))
					framerate = 180;
				else if ((framerate > 180) && (framerate < 240))
					framerate = 200;
				else if ((framerate > 200) && (framerate < 244))
					framerate = 240;
				else if ((framerate > 240) && (framerate < 300))
					framerate = 244;
				else if ((framerate > 244) && (framerate < 360))
					framerate = 300;
				else if (framerate > 360)
					framerate = 360;
         }
         else
            framerate = atoi(var.value);
      }
      else
         framerate    = 60;

      switch (framerate)
      {
         case 50:
            framerate_ms = 20;
            break;
         case 60:
            framerate_ms = 16;
            break;
         case 72:
            framerate_ms = 14;
            break;
         case 75:
            framerate_ms = 13;
            break;
         case 90:
            framerate_ms = 11;
            break;
         case 100:
            framerate_ms = 10;
            break;
         case 119:
         case 120:
            framerate_ms = 8;
            break;
         case 144:
            framerate_ms = 7;
            break;
         case 155:
         case 160:
         case 165:
            framerate_ms = 6;
            break;
         case 180:
         case 200:
            framerate_ms = 5;
            break;
         case 240:
         case 244:
            framerate_ms = 4;
            break;
         case 300:
         case 360:
            framerate_ms = 3;
            break;
         default:
				framerate    = 60;
				framerate_ms = 16;
				break;
      }

      var.key = "vitaquakeii_resolution";
      var.value = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && !initial_resolution_set)
      {
         char *pch;
         char str[100];
         snprintf(str, sizeof(str), "%s", var.value);

         pch = strtok(str, "x");
         if (pch)
            scr_width = strtoul(pch, NULL, 0);
         pch = strtok(NULL, "x");
         if (pch)
            scr_height = strtoul(pch, NULL, 0);

         if (log_cb)
            log_cb(RETRO_LOG_INFO, "Got size: %u x %u.\n", scr_width, scr_height);

         initial_resolution_set = true;
      }

		var.key = "vitaquakeii_gamma";
		var.value = NULL;
		libretro_gamma = 1.0f;

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			/* Invert sense so greater = brighter,
			 * and scale to a range of 0.5 to 1.3 */
			float gamma_correction = atof(var.value);

			if (gamma_correction < 0.2f)
				gamma_correction = 0.2f;
			if (gamma_correction > 1.0f)
				gamma_correction = 1.0f;

			libretro_gamma = (-1.0f * gamma_correction) + 1.5f;
		}

#ifdef HAVE_OPENGL
      var.key = "vitaquakeii_renderer";
      var.value = NULL;

      enable_opengl = !environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || strcmp(var.value, "software") != 0;

		var.key = "vitaquakeii_gl_modulate";
		var.value = NULL;

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			libretro_gl_modulate = atof(var.value);

			if (libretro_gl_modulate > 5.0f)
				libretro_gl_modulate = 5.0f;
			if (libretro_gl_modulate < 1.0f)
				libretro_gl_modulate = 1.0f;
		}

		/* Hide irrelevant options */
		option_display.visible = false;

		if (enable_opengl)
		{
			option_display.key = "vitaquakeii_sw_dithered_filtering";
			environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
		}
		else
		{
			option_display.key = "vitaquakeii_gl_modulate";
			environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

			option_display.key = "vitaquakeii_gl_texture_filtering";
			environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

			option_display.key = "vitaquakeii_gl_shadows";
			environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

			option_display.key = "vitaquakeii_gl_gl_xflip";
			environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
		}
#endif
   }
   
	var.key = "vitaquakeii_invert_y_axis";
	var.value = NULL;

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{
		if (strcmp(var.value, "disabled") == 0)
			invert_y_axis = 1;
		else
			invert_y_axis = -1;
	}

	var.key = "vitaquakeii_analog_deadzone";
	var.value = NULL;
	analog_deadzone = (int)(0.15f * ANALOG_RANGE);

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		analog_deadzone = (int)(atoi(var.value) * 0.01f * ANALOG_RANGE);

	/* We need Qcommon_Init to be executed to be able to set Cvars */
	if (!startup)
	{
		var.key = "vitaquakeii_rumble";
		var.value = NULL;

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			if (strcmp(var.value, "disabled") == 0)
				Cvar_SetValue( "pstv_rumble", 0 );
			else
				Cvar_SetValue( "pstv_rumble", 1 );
		}

		var.key = "vitaquakeii_cl_run";
		var.value = NULL;

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			if (strcmp(var.value, "disabled") == 0)
				Cvar_SetValue( "cl_run", 0 );
			else
				Cvar_SetValue( "cl_run", 1 );
		}

		var.key = "vitaquakeii_mouse_sensitivity";
		var.value = NULL;

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			float mouse_sensitivity = atof(var.value);
			Cvar_SetValue( "sensitivity", mouse_sensitivity );
		}

		var.key = "vitaquakeii_sw_dithered_filtering";
		var.value = NULL;

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && sw_texfilt)
		{
			if (strcmp(var.value, "enabled") == 0)
				Cvar_SetValue( "sw_texfilt", 1 );
			else
				Cvar_SetValue( "sw_texfilt", 0);
		}

#ifdef HAVE_OPENGL
		var.key = "vitaquakeii_gl_texture_filtering";
		var.value = NULL;

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			if (strcmp(var.value, "nearest") == 0)
				Cvar_Set( "gl_texturemode", "GL_NEAREST" );
			else if (strcmp(var.value, "linear") == 0)
				Cvar_Set( "gl_texturemode", "GL_LINEAR" );
			else if (strcmp(var.value, "linear_hq") == 0)
				Cvar_Set( "gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
			else
				Cvar_Set( "gl_texturemode", "GL_NEAREST_MIPMAP_LINEAR" );
		}

		var.key = "vitaquakeii_gl_gl_xflip";
		var.value = NULL;

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			if (strcmp(var.value, "disabled") == 0)
				Cvar_SetValue( "gl_xflip", 0 );
			else
				Cvar_SetValue( "gl_xflip", 1 );
		}
#endif

		var.key = "vitaquakeii_xhair";
		var.value = NULL;

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			if (strcmp(var.value, "disabled") == 0)
				Cvar_SetValue( "crosshair", 0 );
			else
				Cvar_SetValue( "crosshair", 1 );
		}
		
		var.key = "vitaquakeii_fps";
		var.value = NULL;

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			if (strcmp(var.value, "disabled") == 0)
				Cvar_SetValue( "cl_drawfps", 0 );
			else
				Cvar_SetValue( "cl_drawfps", 1 );
		}

#ifdef HAVE_OPENGL
		var.key = "vitaquakeii_gl_shadows";
		var.value = NULL;

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && !is_soft_render)
		{
			if (strcmp(var.value, "disabled") == 0)
				Cvar_SetValue( "gl_shadows", 0 );
			else
				Cvar_SetValue( "gl_shadows", 1 );
		}
#endif

		var.key = "vitaquakeii_hand";
		var.value = NULL;

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			if (strcmp(var.value, "right") == 0)
				Cvar_SetValue( "hand", 0 );
			else if (strcmp(var.value, "left") == 0)
				Cvar_SetValue( "hand", 1 );
			else if (strcmp(var.value, "center") == 0)
				Cvar_SetValue( "hand", 2 );
			else
				Cvar_SetValue( "hand", 3 );
		}
	}
}

void retro_init(void)
{
	shutdown_core = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;
}

void retro_deinit(void)
{
	if (!shutdown_core)
		CL_Quit_f();

#ifdef HAVE_OPENGL
	GLimp_DeinitGL();
#endif

   libretro_supports_bitmasks = false;
}

static void extract_basename(char *buf, const char *path, size_t size)
{
   char *ext        = NULL;
   const char *base = strrchr(path, '/');
   if (!base)
      base = strrchr(path, '\\');
   if (!base)
      base = path;

   if (*base == '\\' || *base == '/')
      base++;

   strncpy(buf, base, size - 1);
   buf[size - 1] = '\0';

   ext = strrchr(buf, '.');
   if (ext)
      *ext = '\0';
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "vitaQuakeII";
   info->library_version  = "v2.3" ;
   info->need_fullpath    = true;
   info->valid_extensions = "pak";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing.fps            = framerate;
   info->timing.sample_rate    = SAMPLE_RATE;

   info->geometry.base_width   = scr_width;
   info->geometry.base_height  = scr_height;
   info->geometry.max_width    = scr_width;
   info->geometry.max_height   = scr_height;
   info->geometry.aspect_ratio = (scr_width * 1.0f) / (scr_height * 1.0f);
}

void retro_set_environment(retro_environment_t cb)
{
   bool option_categories_supported;
   struct retro_log_callback log;

   environ_cb = cb;

   libretro_set_core_options(environ_cb,
         &option_categories_supported);

   if(environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;
}

void retro_reset(void)
{
}

void retro_set_rumble_strong(void)
{
   uint16_t strength_strong = 0xffff;
   if (!rumble.set_rumble_state)
      return;

   rumble.set_rumble_state(0, RETRO_RUMBLE_STRONG, strength_strong);
}

void retro_unset_rumble_strong(void)
{
   if (!rumble.set_rumble_state)
      return;

   rumble.set_rumble_state(0, RETRO_RUMBLE_STRONG, 0);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

bool retro_load_game(const struct retro_game_info *info)
{
	int i;
	char path_lower[1024];
	char parent_dir[1024];
#if defined(_WIN32)
	char slash = '\\';
#else
	char slash = '/';
#endif
	bool use_external_savedir = false;
	const char *base_save_dir = NULL;
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;

#if defined(ROGUE)
	const char *core_game_dir = "rogue";
	const char *core_game_error_msg = "Error: Quake II - Ground Zero (rogue) game files required";
#elif defined(XATRIX)
	const char *core_game_dir = "xatrix";
	const char *core_game_error_msg = "Error: Quake II - The Reckoning (xatrix) game files required";
#elif defined(ZAERO)
	const char *core_game_dir = "zaero";
	const char *core_game_error_msg = "Error: Quake II - Zaero (zaero) game files required";
#else
	const char *core_game_dir = "baseq2";
	const char *core_game_error_msg = "Error: Quake II (baseq2) game files required";
#endif

	path_lower[0] = '\0';
	parent_dir[0] = '\0';

	if (!info || !info->path)
		return false;

	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_desc);

	if (environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble))
		log_cb(RETRO_LOG_INFO, "Rumble environment supported.\n");
	else
		log_cb(RETRO_LOG_INFO, "Rumble environment not supported.\n");

	update_variables(true);

	if (enable_opengl && !environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
	{
		if (log_cb)
			log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
		return false;
	}

	if (!enable_opengl
#ifdef HAVE_OPENGL
	    || !initialize_opengl()
#endif
	    )
	{
		if (log_cb)
			log_cb(RETRO_LOG_INFO, "vitaQuakeII: using software renderer.\n");
		
		fmt = RETRO_PIXEL_FORMAT_RGB565;
		if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
		{
			if (log_cb)
				log_cb(RETRO_LOG_INFO, "RGB565 is not supported.\n");
			return false;
		}
		is_soft_render = true;
	}
	else
	{
		if (log_cb)
			log_cb(RETRO_LOG_INFO, "vitaQuakeII: using OpenGL renderer.\n");
	}
	
	sprintf(path_lower, "%s", info->path);
	
	for (i=0; path_lower[i]; ++i)
		path_lower[i] = tolower(path_lower[i]);
	
	extract_directory(g_rom_dir, info->path, sizeof(g_rom_dir));
	
	if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &base_save_dir) && base_save_dir)
	{
		if (strlen(base_save_dir) > 0)
		{
			/* Get game 'name' (i.e. subdirectory) */
			char game_name[1024];
			extract_basename(game_name, g_rom_dir, sizeof(game_name));
			
			/* > Build final save path */
			snprintf(g_save_dir, sizeof(g_save_dir), "%s%c%s", base_save_dir, slash, game_name);
			use_external_savedir = true;
			
			/* > Create save directory, if required */
			if (!path_is_directory(g_save_dir))
				use_external_savedir = path_mkdir(g_save_dir);
		}
	}
	
	/* > Final check: is the save directory the same as the 'rom' directory?
	 *   (i.e. ensure logical behaviour if user has set a bizarre save path...) */
	use_external_savedir = use_external_savedir && (strcmp(g_save_dir, g_rom_dir) != 0);
	
	/* If we are not using an external save directory,
	 * then set g_save_dir to an empty string (rom directory
	 * will be used by default) */
	if (!use_external_savedir)
		g_save_dir[0] = false;
	
	/* Ensure that we have valid content
	 * (different cores are required for base
	 * game + expansions) */
	if (!strstr(path_lower, core_game_dir))
	{
		unsigned msg_interface_version = 0;
		environ_cb(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION,
				&msg_interface_version);

		if (msg_interface_version >= 1)
		{
			struct retro_message_ext msg;

			msg.msg      = core_game_error_msg;
			msg.duration = 3000;
			msg.priority = 3;
			msg.level    = RETRO_LOG_ERROR;
			msg.target   = RETRO_MESSAGE_TARGET_ALL;
			msg.type     = RETRO_MESSAGE_TYPE_NOTIFICATION;
			msg.progress = -1;

			environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);
		}
		else
		{
			struct retro_message msg;

			msg.msg;
			msg.frames;

			environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
		}

		return false;
	}

	/* Quake II base directory is the *parent*
	 * of the game directory */
	extract_directory(parent_dir, g_rom_dir, sizeof(parent_dir));
	strncpy(g_rom_dir, parent_dir, sizeof(g_rom_dir) - 1);

	return true;
}

static void audio_process(void)
{
}

bool first_boot = true;

void retro_run(void)
{
	bool updated = false;
#ifdef HAVE_OPENGL
	if (!is_soft_render) {
      if (!libretro_shared_context)
         glsm_ctl(GLSM_CTL_STATE_BIND, NULL);
		qglBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
		qglEnable(GL_TEXTURE_2D);
	}
#endif
	if (first_boot)
	{
		const char *argv[32];
		const char *empty_string = "";
	
		argv[0] = empty_string;
		int argc = 1;
#if defined(ROGUE) || defined(XATRIX) || defined(ZAERO)
		argc = 4;
		argv[1] = "+set";
		argv[2] = "game";
#if defined(ROGUE)
		argv[3] = "rogue";
#elif defined(XATRIX)
		argv[3] = "xatrix";
#elif defined(ZAERO)
		argv[3] = "zaero";
#endif
#endif
		Qcommon_Init(argc, (char**)argv);
		if (is_soft_render) Cvar_Set( "vid_ref", "soft" );
		update_variables(false);
		first_boot = false;
	}
	
	if (rumble_tick != 0)
		if (cpu_features_get_time_usec() - rumble_tick > 500000)
         IN_StopRumble(); /* 0.5 sec */
	
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
		update_variables(false);

   /* TODO/FIXME - argument should be changed into float for better accuracy of fixed timestep */
	Qcommon_Frame (framerate_ms);

	if (shutdown_core)
		return;

	if (is_soft_render)
		video_cb(tex_buffer, scr_width, scr_height, scr_width << 1);
	else
	{
#ifdef HAVE_OPENGL
      if (!libretro_shared_context)
         glsm_ctl(GLSM_CTL_STATE_UNBIND, NULL);
		video_cb(RETRO_HW_FRAME_BUFFER_VALID, scr_width, scr_height, 0);
#endif
	}
	
	audio_process();
	audio_callback();
}

void retro_unload_game(void)
{
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}


/* snddma.c */
#include "../client/snd_loc.h"

static volatile int sound_initialized = 0;
static int stop_audio = false;

static int16_t audio_buffer[BUFFER_SIZE];
static unsigned audio_buffer_ptr;

static void audio_callback(void)
{
	unsigned read_first, read_second;
	float samples_per_frame = (2 * SAMPLE_RATE) / framerate;
	unsigned read_end       = audio_buffer_ptr + samples_per_frame;

	if (read_end > BUFFER_SIZE)
		read_end = BUFFER_SIZE;

	read_first  = read_end - audio_buffer_ptr;
	read_second = samples_per_frame - read_first;

	audio_batch_cb(audio_buffer + audio_buffer_ptr, read_first / (dma.samplebits / 8));
	audio_buffer_ptr += read_first;
	if (read_second >= 1) {
		audio_batch_cb(audio_buffer, read_second / (dma.samplebits / 8));
		audio_buffer_ptr = read_second;
	}
}

uint64_t initial_tick;

qboolean SNDDMA_Init(void)
{
   sound_initialized = 0;

   /* Force Quake to use our settings */
   Cvar_SetValue( "s_khz", SAMPLE_RATE );
   Cvar_SetValue( "s_loadas8bit", false );

   /* Fill the audio DMA information block */
   dma.samplebits       = 16;
   dma.speed            = SAMPLE_RATE;
   dma.channels         = 2;
   dma.samples          = BUFFER_SIZE;
   dma.samplepos        = 0;
   dma.submission_chunk = 1;
   dma.buffer           = audio_buffer;

   sound_initialized    = 1;

   initial_tick         = cpu_features_get_time_usec();

   return true;
}

int SNDDMA_GetDMAPos(void)
{
	if(!sound_initialized)
		return 0;
	return dma.samplepos = audio_buffer_ptr;
}

void SNDDMA_Shutdown(void)
{
	if(!sound_initialized)
		return;

	stop_audio = true;

	sound_initialized = 0;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
}

void SNDDMA_BeginPainting(void)
{    
}

/* vid.c */

#define REF_OPENGL  0

cvar_t *vid_ref;
cvar_t *vid_fullscreen;

extern cvar_t *gl_picmip;
cvar_t *gl_mode;
#ifdef HAVE_OPENGL
extern cvar_t *gl_driver;
#endif

extern void M_ForceMenuOff( void );

static menuframework_s  s_opengl_menu;
static menuframework_s *s_current_menu;

static menulist_s       s_mode_list;
static menulist_s       s_ref_list;
static menuslider_s     s_screensize_slider;
static menuaction_s     s_cancel_action;
static menuaction_s     s_defaults_action;
static menulist_s       s_shadows_slider;

viddef_t    viddef;             /* global video state */

refexport_t re;

refexport_t GetRefAPI (refimport_t rimp);
refexport_t SWR_GetRefAPI (refimport_t rimp);


/*
==========================================================================

DIRECT LINK GLUE

==========================================================================
*/

#define MAXPRINTMSG 4096
void VID_Printf (int print_level, char *fmt, ...)
{
   va_list     argptr;
   char        msg[MAXPRINTMSG];

   va_start(argptr,fmt);
   vsprintf(msg,fmt,argptr);
   va_end(argptr);
#ifdef DEBUG
   printf(msg);
#endif
   if (print_level == PRINT_ALL)
      Com_Printf("%s", msg);
   else
      Com_DPrintf("%s", msg);
}

void VID_Error (int err_level, char *fmt, ...)
{
   va_list     argptr;
   char        msg[MAXPRINTMSG];

   va_start (argptr,fmt);
   vsprintf (msg,fmt,argptr);
   va_end (argptr);
#ifdef DEBUG
   printf(msg);
#endif
   Com_Error (err_level, "%s", msg);
}

void VID_NewWindow (int width, int height)
{
   viddef.width = width;
   viddef.height = height;
}

/*
** VID_GetModeInfo
*/
typedef struct vidmode_s
{
    const char *description;
    int         width, height;
    int         mode;
} vidmode_t;

vidmode_t vid_modes[] =
{
   { "Mode 0: 480x272",     480, 272,   0 },
   { "Mode 1: 640x368",     640, 368,   1 },
   { "Mode 2: 720x408",     720, 408,   2 },
   { "Mode 3: 960x544",     960, 544,   3 },
   { "Mode 4: 1280x720",   1280, 720,   4 },
   { "Mode 5: 1920x1080",  1920,1080,   5 },
   { "Mode 6: 2560x1440",  2560,1440,   6 },
   { "Mode 7: 3840x2160",  3840,2160,   7 },
   { "Mode 8: 5120x2880",  5120,2880,   8 },
   { "Mode 9: 7680x4320",  7680,4320,   9 },
   { "Mode 10: 15360x8640",  15360,8640,   10 }
};
#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

qboolean VID_GetModeInfo( int *width, int *height, int mode )
{
   if ( mode < 0 || mode >= VID_NUM_MODES )
      return false;

   *width  = vid_modes[mode].width;
   *height = vid_modes[mode].height;
   printf("VID_GetModeInfo %dx%d mode %d\n",*width,*height,mode);

   return true;
}

static void NullCallback( void *unused )
{
}

static void ResCallback( void *unused )
{
}

static void ScreenSizeCallback( void *s )
{
    menuslider_s *slider = ( menuslider_s * ) s;

    Cvar_SetValue( "viewsize", slider->curvalue * 10 );
}

static void ShadowsCallback( void *unused )
{
	Cvar_SetValue( "gl_shadows", s_shadows_slider.curvalue );
}

static void ResetDefaults( void *unused )
{
	VID_MenuInit();
}

static void ApplyChanges( void *unused )
{
#ifdef HAVE_OPENGL
   if (enable_opengl)
   {
     Cvar_Set( "vid_ref", "gl" );
     Cvar_Set( "gl_driver", "opengl32" );
   }
#endif
   M_ForceMenuOff();
}

static void CancelChanges( void *unused )
{
   extern void M_PopMenu( void );

   M_PopMenu();
}

void    VID_Init (void)
{
   refimport_t _ri;

   viddef.width = scr_width;
   viddef.height = scr_height;

   _ri.Cmd_AddCommand = Cmd_AddCommand;
   _ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
   _ri.Cmd_Argc = Cmd_Argc;
   _ri.Cmd_Argv = Cmd_Argv;
   _ri.Cmd_ExecuteText = Cbuf_ExecuteText;
   _ri.Con_Printf = VID_Printf;
   _ri.Sys_Error = VID_Error;
   _ri.FS_LoadFile = FS_LoadFile;
   _ri.FS_FreeFile = FS_FreeFile;
   _ri.FS_Gamedir = FS_Gamedir;
   _ri.Vid_NewWindow = VID_NewWindow;
   _ri.Cvar_Get = Cvar_Get;
   _ri.Cvar_Set = Cvar_Set;
   _ri.Cvar_SetValue = Cvar_SetValue;
   _ri.Vid_GetModeInfo = VID_GetModeInfo;
   _ri.Vid_MenuInit = VID_MenuInit;

   if (is_soft_render) re = SWR_GetRefAPI(_ri);
#ifdef HAVE_OPENGL
   else re = GetRefAPI(_ri);
#endif
   if (re.api_version != API_VERSION)
      Com_Error (ERR_FATAL, "Re has incompatible api_version");

   /* call the init function */
   if (re.Init (NULL, NULL) == -1)
      Com_Error (ERR_FATAL, "Couldn't start refresh");

   vid_ref = Cvar_Get ("vid_ref", "soft", CVAR_ARCHIVE);
   vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
}

void    VID_Shutdown (void)
{
    if (re.Shutdown)
        re.Shutdown ();
}

void    VID_CheckChanges (void)
{
}

void    VID_MenuInit (void)
{
   static const char *resolutions[] = 
   {
      "480x272",
      "640x368",
      "720x408",
      "960x544",
      0
   };

   static const char *refs[] =
   {
      "openGL",
      0
   };

   static const char *yesno_names[] =
   {
      "no",
      "yes",
      0
   };

#ifdef HAVE_OPENGL
   if ( !gl_driver )
      gl_driver = Cvar_Get( "gl_driver", "opengl32", 0 );
#endif
   if ( !scr_viewsize )
      scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE);

   s_screensize_slider.curvalue = scr_viewsize->value/10;

   switch (viddef.width) {
      case 960:
         s_mode_list.curvalue = 3;
         break;
      case 720:
         s_mode_list.curvalue = 2;
         break;
      case 640:
         s_mode_list.curvalue = 1;
         break;
      default:
         s_mode_list.curvalue = 0;
         break;
   }
   Cvar_SetValue( "gl_mode", s_mode_list.curvalue );

   s_ref_list.curvalue = REF_OPENGL;
   s_opengl_menu.x = viddef.width * 0.50;
   s_opengl_menu.nitems = 0;

   s_ref_list.generic.type = MTYPE_SPINCONTROL;
   s_ref_list.generic.name = "driver";
   s_ref_list.generic.x = 0;
   s_ref_list.generic.y = 0;
   s_ref_list.generic.callback = NullCallback;
   s_ref_list.itemnames = refs;

   s_cancel_action.generic.type = MTYPE_ACTION;
   s_cancel_action.generic.name = "cancel";
   s_cancel_action.generic.x    = 0;
   s_cancel_action.generic.y    = 100;
   s_cancel_action.generic.callback = CancelChanges;

   Menu_AddItem( &s_opengl_menu, ( void * ) &s_ref_list );

   Menu_AddItem( &s_opengl_menu, ( void * ) &s_cancel_action );


   Menu_Center( &s_opengl_menu );

   s_opengl_menu.x -= 8;
}

void    VID_MenuDraw (void)
{
    int w, h;
	float scale = SCR_GetMenuScale();
	
    s_current_menu = &s_opengl_menu;

    /*
    ** draw the banner
    */
    re.DrawGetPicSize( &w, &h, "m_banner_video" );
    re.DrawPic( viddef.width / 2 - (w * scale) / 2, viddef.height /2 - 110 * scale, "m_banner_video", scale );

    /*
    ** move cursor to a reasonable starting position
    */
    Menu_AdjustCursor( s_current_menu, 1 );

    /*
    ** draw the menu
    */
    Menu_Draw( s_current_menu );
}

const char *VID_MenuKey( int k)
{
   menuframework_s *m = s_current_menu;
   static const char *sound = "misc/menu1.wav";

   switch ( k )
   {
      case K_AUX4:
         ApplyChanges( NULL );
         return NULL;
      case K_KP_UPARROW:
      case K_UPARROW:
         m->cursor--;
         Menu_AdjustCursor( m, -1 );
         break;
      case K_KP_DOWNARROW:
      case K_DOWNARROW:
         m->cursor++;
         Menu_AdjustCursor( m, 1 );
         break;
      case K_KP_LEFTARROW:
      case K_LEFTARROW:
         Menu_SlideItem( m, -1 );
         break;
      case K_KP_RIGHTARROW:
      case K_RIGHTARROW:
         Menu_SlideItem( m, 1 );
         break;
      case K_KP_ENTER:
      case K_AUX1:
         if ( !Menu_SelectItem( m ) )
            ApplyChanges( NULL );
         break;
   }

   return sound;
}
#ifdef HAVE_OPENGL
int GLimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	/*int width, height;

	ri.Con_Printf( PRINT_ALL, "Initializing OpenGL display\n");
	ri.Con_Printf (PRINT_ALL, "...setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( &width, &height, mode ) )
	{
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}
*/
	ri.Con_Printf( PRINT_ALL, " %d %d\n", scr_width, scr_height );

	/* destroy the existing window */
	GLimp_Shutdown ();

	*pwidth = scr_width;
	*pheight = scr_height;
	ri.Vid_NewWindow (scr_width, scr_height);

	if (!gl_set) GLimp_InitGL();

	return rserr_ok;
}
#endif
/* input.c */

cvar_t *in_joystick;
cvar_t *leftanalog_sensitivity;
cvar_t *rightanalog_sensitivity;
cvar_t *vert_motioncam_sensitivity;
cvar_t *hor_motioncam_sensitivity;
extern cvar_t *use_gyro;
extern cvar_t *pstv_rumble;
extern cvar_t *gl_xflip;

void IN_Init (void)
{
	in_joystick	= Cvar_Get ("in_joystick", "1",	CVAR_ARCHIVE);
	leftanalog_sensitivity = Cvar_Get ("leftanalog_sensitivity", "2.0", CVAR_ARCHIVE);
	rightanalog_sensitivity = Cvar_Get ("rightanalog_sensitivity", "2.0", CVAR_ARCHIVE);
	vert_motioncam_sensitivity = Cvar_Get ("vert_motioncam_sensitivity", "2.0", CVAR_ARCHIVE);
	hor_motioncam_sensitivity = Cvar_Get ("hor_motioncam_sensitivity", "2.0", CVAR_ARCHIVE);
	use_gyro = Cvar_Get ("use_gyro", "0", CVAR_ARCHIVE);
	pstv_rumble	= Cvar_Get ("pstv_rumble", "1",	CVAR_ARCHIVE);
	
	rumble_tick = cpu_features_get_time_usec();
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
}

void IN_Frame (void)
{
}

void IN_StartRumble (void)
{
	if (!pstv_rumble->value) return;
	
	uint16_t strength_strong = 0xffff;
	if (!rumble.set_rumble_state)
		return;

	rumble.set_rumble_state(0, RETRO_RUMBLE_STRONG, strength_strong);
	rumble_tick = cpu_features_get_time_usec();
}

void IN_StopRumble (void)
{
	if (!rumble.set_rumble_state)
		return;

	rumble.set_rumble_state(0, RETRO_RUMBLE_STRONG, 0);
	rumble_tick = 0;
}

void IN_Move (usercmd_t *cmd)
{
   int lsx, lsy, rsx, rsy;
   float speed;
   
   if ((in_speed.state & 1) ^ (int)cl_run->value)
       speed = 2.0f;
   else
       speed = 1.0f;
   
	/* Left stick move */
	lsx = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
			RETRO_DEVICE_ID_ANALOG_X);
	lsy = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
			RETRO_DEVICE_ID_ANALOG_Y);

	if (lsx > analog_deadzone || lsx < -analog_deadzone)
	{
		float lx_delta;

		if (lsx > analog_deadzone)
			lsx = lsx - analog_deadzone;
		if (lsx < -analog_deadzone)
			lsx = lsx + analog_deadzone;

		lx_delta = (speed * (float)cl_sidespeed->value * (float)lsx) /
				(float)(ANALOG_RANGE - analog_deadzone);

		if (enable_opengl && gl_xflip->value)
			cmd->sidemove -= lx_delta;
		else
			cmd->sidemove += lx_delta;
	}

	if (lsy > analog_deadzone || lsy < -analog_deadzone)
	{
		if (lsy > analog_deadzone)
			lsy = lsy - analog_deadzone;
		if (lsy < -analog_deadzone)
			lsy = lsy + analog_deadzone;

		cmd->forwardmove -= (speed * (float)cl_forwardspeed->value * (float)lsy) /
				(float)(ANALOG_RANGE - analog_deadzone);
	}

	/* Right stick Look */
	rsx = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
				RETRO_DEVICE_ID_ANALOG_X);
	rsy = invert_y_axis * input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
				RETRO_DEVICE_ID_ANALOG_Y);

	if (rsx > analog_deadzone || rsx < -analog_deadzone)
	{
		float rx_delta;

		if (rsx > analog_deadzone)
			rsx = rsx - analog_deadzone;
		if (rsx < -analog_deadzone)
			rsx = rsx + analog_deadzone;

		/* For now we are sharing the sensitivity with the mouse setting */
		rx_delta = ((float)sensitivity->value * (float)rsx /
				(float)(ANALOG_RANGE - analog_deadzone)) /
						((float)framerate / 60.0f);

		if (enable_opengl && gl_xflip->value)
			cl.viewangles[YAW] += rx_delta;
		else
			cl.viewangles[YAW] -= rx_delta;
	}

	if (rsy > analog_deadzone || rsy < -analog_deadzone)
	{
		/* Have to correct for widescreen aspect ratios,
		 * otherwise vertical motion is too fast */
		float aspect_correction = (float)(4 * viddef.height) /
				(float)(3 * viddef.width);

		if (rsy > analog_deadzone)
			rsy = rsy - analog_deadzone;
		if (rsy < -analog_deadzone)
			rsy = rsy + analog_deadzone;

		cl.viewangles[PITCH] -= (aspect_correction * (float)sensitivity->value * (float)rsy /
				(float)(ANALOG_RANGE - analog_deadzone)) /
						((float)framerate / 60.0f);
	}

	if (cl.viewangles[PITCH] > 80)
		cl.viewangles[PITCH] = 80;
	if (cl.viewangles[PITCH] < -70)
		cl.viewangles[PITCH] = -70;
}

void IN_Activate (qboolean active)
{
}

void IN_ActivateMouse (void)
{
}

void IN_DeactivateMouse (void)
{
}

void IN_MouseEvent (int mstate)
{
}
