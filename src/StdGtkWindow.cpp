/*
 * LegacyClonk
 *
 * Copyright (c) RedWolf Design
 * Copyright (c) 2006, Clonk-Karl
 * Copyright (c) 2017-2019, The LegacyClonk Team and contributors
 *
 * Distributed under the terms of the ISC license; see accompanying file
 * "COPYING" for details.
 *
 * "Clonk" is a registered trademark of Matthes Bender, used with permission.
 * See accompanying file "TRADEMARK" for details.
 *
 * To redistribute this file separately, substitute the full license texts
 * for the above references.
 */

/* A wrapper class to OS dependent event and window interfaces, GTK+ version */

#include <StdGtkWindow.h>

#include "res/lc.xpm"
#include <X11/Xlib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

/* CStdGtkWindow */

CStdGtkWindow::CStdGtkWindow() :
	CStdWindow(), window(nullptr) {}

CStdGtkWindow::~CStdGtkWindow()
{
	Clear();
}

CStdWindow *CStdGtkWindow::Init(CStdApp *pApp, const char *Title, CStdWindow *pParent, bool HideCursor)
{
	Active = true;
	dpy = pApp->dpy;

	if (!FindInfo()) return 0;

	assert(!window);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	// Override gtk's default to match name/class of the XLib windows
	gtk_window_set_wmclass(GTK_WINDOW(window), STD_PRODUCT, STD_PRODUCT);

	handlerDestroy = g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(OnDestroyStatic), this);
	g_signal_connect(G_OBJECT(window), "key-press-event",   G_CALLBACK(OnUpdateKeyMask), pApp);
	g_signal_connect(G_OBJECT(window), "key-release-event", G_CALLBACK(OnUpdateKeyMask), pApp);

	GtkWidget *render_widget = InitGUI();

	gtk_widget_set_visual(render_widget, gdk_x11_screen_lookup_visual(gtk_window_get_screen(GTK_WINDOW(window)), static_cast<XVisualInfo *>(Info)->visualid));

	gtk_widget_show_all(window);

	GdkPixbuf *icon = gdk_pixbuf_new_from_xpm_data(c4x_xpm);
	gtk_window_set_icon(GTK_WINDOW(window), icon);
	g_object_unref(icon);

	gtk_window_set_title(GTK_WINDOW(window), Title);

	auto *gdkWindow = gtk_widget_get_window(window);

	// Wait until window is mapped to get the window's XID
	gtk_widget_show_now(window);
	wnd = gdk_x11_window_get_xid(gdkWindow);
	gdk_window_add_filter(gdkWindow, OnFilter, this);

	XWMHints *wm_hint = XGetWMHints(dpy, wnd);
	if (!wm_hint) wm_hint = XAllocWMHints();
	Hints = wm_hint;

	if (GTK_IS_LAYOUT(render_widget))
		renderwnd = gdk_x11_window_get_xid(gtk_layout_get_bin_window(GTK_LAYOUT(render_widget)));
	else
		renderwnd = gdk_x11_window_get_xid(gtk_widget_get_window(render_widget));

	if (pParent) XSetTransientForHint(dpy, wnd, pParent->wnd);

	if (HideCursor)
	{
		gdk_window_set_cursor(gdkWindow, nullptr);
	}

	// Make sure the window is shown and ready to be rendered into,
	// this avoids an async X error.
	gdk_display_flush(gdk_display_get_default());

	return this;
}

void CStdGtkWindow::Clear()
{
	if (window != nullptr)
	{
		g_signal_handler_disconnect(window, handlerDestroy);
		gtk_widget_destroy(window);
		handlerDestroy = 0;
	}

	// Avoid that the base class tries to free these
	wnd = renderwnd = 0;

	window = nullptr;
	Active = false;

	// We must free it here since we do not call CStdWindow::Clear()
	if (Info)
	{
		XFree(Info);
		Info = 0;
	}
}

void CStdGtkWindow::OnDestroyStatic(GtkWidget *widget, gpointer data)
{
	CStdGtkWindow *wnd = static_cast<CStdGtkWindow *>(data);

	g_signal_handler_disconnect(wnd->window, wnd->handlerDestroy);
	wnd->handlerDestroy = 0;
	wnd->window = nullptr;
	wnd->Active = false;
	wnd->wnd = wnd->renderwnd = 0;

	wnd->Close();
}

GdkFilterReturn CStdGtkWindow::OnFilter(GdkXEvent *xevent, GdkEvent *event, gpointer user_data)
{
	// Handle raw X message, then let GTK+ process it
	static_cast<CStdGtkWindow *>(user_data)->HandleMessage(*reinterpret_cast<XEvent *>(xevent));
	return GDK_FILTER_CONTINUE;
}

gboolean CStdGtkWindow::OnUpdateKeyMask(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	// Update mask so that Application.IsShiftDown,
	// Application.IsControlDown etc. work.
	unsigned int mask = 0;
	if (event->state & GDK_SHIFT_MASK)   mask |= MK_SHIFT;
	if (event->state & GDK_CONTROL_MASK) mask |= MK_CONTROL;
	if (event->state & GDK_MOD1_MASK)    mask |= (1 << 3);

	// For keypress/relases, event->state contains the state _before_
	// the event, but we need to store the current state.
	if (event->keyval == GDK_KEY_Shift_L   || event->keyval == GDK_KEY_Shift_R)   mask ^= MK_SHIFT;
	if (event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R) mask ^= MK_CONTROL;
	if (event->keyval == GDK_KEY_Alt_L     || event->keyval == GDK_KEY_Alt_R)     mask ^= (1 << 3);

	static_cast<CStdApp *>(user_data)->KeyMask = mask;
	return FALSE;
}

GtkWidget *CStdGtkWindow::InitGUI()
{
	return window;
}
