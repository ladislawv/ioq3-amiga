#ifndef IOQ3_MINIGL_LIBRARY_COMPAT_H
#define IOQ3_MINIGL_LIBRARY_COMPAT_H

/*
 * MiniGL shared-library SDK interface, for the minigl.library build of ioq3.
 *
 * <proto/minigl.h> pulls in the dispatch wrappers, which redefine every
 * gl*()/mgl*() name to route through minigl.library once MiniGLOpen() has
 * succeeded. It must therefore be included AFTER <mgl/gl.h>, whose own macros
 * call the entry points directly and would otherwise win.
 *
 * Modelled on glquakelib/minigl_library_compat.h, which has been the shape of
 * a library client here since 2026-08. The static build includes none of this:
 * it compiles the driver into the executable and calls the entry points
 * directly, which is why every reference here sits behind USE_MINIGL_LIBRARY.
 */
#include <proto/minigl.h>

/* The direct-call macros in <mgl/gl.h> name the current context explicitly.
 * A library client has no such global of its own -- the context lives in the
 * dispatch table. */
#ifndef mini_CurrentContext
#define mini_CurrentContext (*MiniGLDispatch->currentContext)
#endif

#endif /* IOQ3_MINIGL_LIBRARY_COMPAT_H */
