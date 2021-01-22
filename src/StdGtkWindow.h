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

/* GTK+ version of StdWindow */

#pragma once

#include <StdWindow.h>

#include <gtk/gtk.h>

class CStdGtkWindow : public CStdWindow
{
public:
	CStdGtkWindow();
	virtual ~CStdGtkWindow();

	virtual void Clear() override;

	virtual CStdWindow *Init(CStdApp *pApp, const char *Title, CStdWindow *pParent = nullptr, bool HideCursor = true) override;

	GtkWidget *window;

protected:
	// InitGUI should either return a widget which is used as a
	// render target or return what the base class returns, in which
	// case the whole window is used as render target.
	virtual GtkWidget *InitGUI();

private:
	static void OnDestroyStatic(GtkWidget *widget, gpointer data);
	static GdkFilterReturn OnFilter(GdkXEvent *xevent, GdkEvent *event, gpointer user_data);
	static gboolean OnUpdateKeyMask(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

	gulong handlerDestroy;
};
