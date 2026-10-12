/*
** mglv3d_log_noop.c -- no-op mglv3d_log() for library-client builds.
**
** ioq3 calls mglv3d_log() from amiga_glimp.c, tr_main.c and tr_mesh.c, but
** that symbol lives in the MiniGLV3D driver, not in minigl.library's
** dispatched surface. Stand-in for MiniGLV3D/demos/mglv3d_log_noop.c, which
** is not part of the client SDK.
*/

void mglv3d_log(const char *msg)
{
	(void)msg;
}
