/**************************************************************************
 * Copyright 2012 Intel corporation
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

#include <stdio.h>

#include <X11/Xlib.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int width = 64;
int height = 64;

static void
set_2d_projection (void)
{
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	glOrtho (0, width, height, 0, 0, 1);
	glMatrixMode (GL_MODELVIEW);
}

static void
draw_fullscreen_quad (void)
{
	glBegin (GL_QUADS);
	glVertex2f (0, 0);
	glVertex2f (width, 0);
	glVertex2f (width, height);
	glVertex2f (0, height);
	glEnd ();
}

static void
draw_fullscreen_textured_quad (void)
{
	glBegin (GL_QUADS);
	glTexCoord2f(0, 0); glVertex2f (0, 0);
	glTexCoord2f(1, 0); glVertex2f (width, 0);
	glTexCoord2f(1, 1); glVertex2f (width, height);
	glTexCoord2f(0, 1); glVertex2f (0, height);
	glEnd ();
}

static void
paint_rgb_using_clear (double r, double g, double b)
{
        glClearColor(r, g, b, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
}

static void
paint_rgb_using_glsl (double r, double g, double b)
{
	const char * vs_source =
		"void main()\n"
		"{\n"
		"        gl_Position = ftransform();\n"
		"}\n";
	const char * fs_source =
		"#version 120\n"
		"uniform vec4 color;\n"
		"void main()\n"
		"{\n"
		"        gl_FragColor = color;\n"
		"}\n";

	GLuint vs, fs, program;
	GLint color;

	vs = glCreateShader (GL_VERTEX_SHADER);
	glShaderSource (vs, 1, &vs_source, NULL);
	glCompileShader (vs);

	fs = glCreateShader (GL_FRAGMENT_SHADER);
	glShaderSource (fs, 1, &fs_source, NULL);
	glCompileShader (fs);

	program = glCreateProgram ();
	glAttachShader (program, vs);
	glAttachShader (program, fs);

	glLinkProgram (program);
	glUseProgram (program);

	color = glGetUniformLocation (program, "color");

	glUniform4f (color, r, g, b, 1.0);

	draw_fullscreen_quad ();

	glUseProgram (0);
}

static GLuint
create_rgb_texture (double r, double g, double b)
{
	uint8_t data[3];
	GLuint texture = 0;

	data[0] = (uint8_t) (255.0 * r);
	data[1] = (uint8_t) (255.0 * g);
	data[2] = (uint8_t) (255.0 * b);

	glGenTextures (1, &texture);

	glBindTexture (GL_TEXTURE_2D, texture);

	glTexImage2D (GL_TEXTURE_2D,
		      0, GL_COMPRESSED_RGBA,
		      1, 1, 0,
		      GL_RGB, GL_UNSIGNED_BYTE, data);

	return texture;
}

static void
paint_using_texture (GLuint texture)
{
	if (0) glBindTexture (GL_TEXTURE_2D, texture);

	glEnable (GL_TEXTURE_2D);

	draw_fullscreen_textured_quad ();

	glDisable (GL_TEXTURE_2D);
}

static void
draw (Display *dpy, Window window, int width, int height)
{
#define PASSES 2
	int i;
	GLenum glew_err;
	GLuint texture[PASSES];

	glew_err = glewInit();
	if (glew_err != GLEW_OK)
	{
		fprintf (stderr, "glewInit failed: %s\n",
			 glewGetErrorString(glew_err));
		exit (1);
	}

        glViewport(0, 0, width, height);

	set_2d_projection ();

/* Simply count through some colors, frame by frame. */
#define RGB(frame) (((frame+1)/4) % 2), (((frame+1)/2) % 2), ((frame+1) % 2)

	int frame = 0;
	for (i = 0; i < PASSES; i++) {

		/* Frame: Draw a solid frame using glClear. */
		paint_rgb_using_clear (RGB(frame));
		glXSwapBuffers (dpy, window);
		frame++;

		/* Frame: Draw a solid frame using GLSL. */
		paint_rgb_using_glsl (RGB(frame));
		glXSwapBuffers (dpy, window);
		frame++;

		/* Frame: Draw a solid frame using a texture. */
		texture[i] = create_rgb_texture (RGB(frame));
		paint_using_texture (texture[i]);
		glXSwapBuffers (dpy, window);
		frame++;
	}

        if (0) {
	        /* Draw another frame with a re-used texture. */
	        paint_using_texture (texture[0]);
	        glXSwapBuffers (dpy, window);
	        frame++;
        }
}

static void
handle_events(Display *dpy, Window window, int width, int height)
{
        XEvent xev;
        KeyCode quit_code = XKeysymToKeycode (dpy, XStringToKeysym("Q"));

        XNextEvent (dpy, &xev);

        while (1) {
                XNextEvent (dpy, &xev);
                switch (xev.type) {
                case KeyPress:
                        if (xev.xkey.keycode == quit_code) {
                                return;
                        }
                        break;
                case ConfigureNotify:
                        width = xev.xconfigure.width;
                        height = xev.xconfigure.height;
                        break;
                case Expose:
                        if (xev.xexpose.count == 0) {
                                draw (dpy, window, width, height);
                                return;
                        }
                        break;
                }
        }
}

int
main (void)
{
        Display *dpy;
        Window window;

        dpy = XOpenDisplay (NULL);

        if (dpy == NULL) {
                fprintf(stderr, "Failed to open display %s\n",
                        XDisplayName(NULL));
                return 1;
        }

        int visual_attr[] = {
                GLX_RGBA,
                GLX_RED_SIZE,		8,
                GLX_GREEN_SIZE, 	8,
                GLX_BLUE_SIZE,		8,
                GLX_ALPHA_SIZE, 	8,
                GLX_DOUBLEBUFFER,
                GLX_DEPTH_SIZE,		24,
                GLX_STENCIL_SIZE,	8,
                None
        };

        int screen = DefaultScreen(dpy);

	/* Window and context setup. */
        XVisualInfo *visual_info = glXChooseVisual(dpy, screen, visual_attr);

	Window root = DefaultRootWindow (dpy);

	/* window attributes */
	XSetWindowAttributes attr;
	attr.background_pixel = BlackPixel(dpy, screen);
	attr.border_pixel = BlackPixel(dpy, screen);
	attr.colormap = XCreateColormap(dpy, root, visual_info->visual, AllocNone);
	attr.event_mask = KeyPressMask | StructureNotifyMask | ExposureMask;

	unsigned long mask;
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	window = XCreateWindow(
		dpy, root,
		0, 0, width, height,
		0,
		visual_info->depth,
		InputOutput,
		visual_info->visual,
		mask,
		&attr);

        XMapWindow (dpy, window);

        GLXContext ctx = glXCreateContext(dpy, visual_info, NULL, True);
        glXMakeCurrent(dpy, window, ctx);

        handle_events (dpy, window, width, height);

	/* Cleanup */
        glXDestroyContext (dpy, ctx);

        XDestroyWindow (dpy, window);
        XCloseDisplay (dpy);

        return 0;
}
