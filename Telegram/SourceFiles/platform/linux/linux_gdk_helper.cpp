/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "platform/linux/linux_gdk_helper.h"

#include "base/platform/linux/base_linux_gtk_integration.h"
#include "base/platform/linux/base_linux_gtk_integration_p.h"
#include "platform/linux/linux_gtk_integration_p.h"
#include "platform/linux/linux_wayland_integration.h"

#include <QtGui/QWindow>

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
extern "C" {
#include <gdk/gdkx.h>
} // extern "C"
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

// CentOS 7 seem to be too old for needed definitions,
// so don't include until we link to gtk directly.
#if !defined DESKTOP_APP_DISABLE_WAYLAND_INTEGRATION && defined LINK_TO_GTK
extern "C" {
#include <gdk/gdkwayland.h>
} // extern "C"
#endif // !DESKTOP_APP_DISABLE_WAYLAND_INTEGRATION && LINK_TO_GTK

namespace Platform {
namespace internal {
namespace {

using base::Platform::GtkIntegration;
using namespace Platform::Gtk;

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
// To be able to compile with gtk-3.0 headers as well
#define GdkDrawable GdkWindow

// Gtk 2
using f_gdk_x11_drawable_get_xdisplay = Display*(*)(GdkDrawable*);
f_gdk_x11_drawable_get_xdisplay gdk_x11_drawable_get_xdisplay = nullptr;

using f_gdk_x11_drawable_get_xid = XID(*)(GdkDrawable*);
f_gdk_x11_drawable_get_xid gdk_x11_drawable_get_xid = nullptr;

// Gtk 3
using f_gdk_x11_window_get_type = GType (*)(void);
f_gdk_x11_window_get_type gdk_x11_window_get_type = nullptr;

// To be able to compile with gtk-2.0 headers as well
template <typename Object>
inline bool gdk_is_x11_window_check(Object *obj) {
	return g_type_cit_helper(obj, gdk_x11_window_get_type());
}

using f_gdk_window_get_display = GdkDisplay*(*)(GdkWindow *window);
f_gdk_window_get_display gdk_window_get_display = nullptr;

using f_gdk_x11_display_get_xdisplay = Display*(*)(GdkDisplay *display);
f_gdk_x11_display_get_xdisplay gdk_x11_display_get_xdisplay = nullptr;

using f_gdk_x11_window_get_xid = Window(*)(GdkWindow *window);
f_gdk_x11_window_get_xid gdk_x11_window_get_xid = nullptr;
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

#ifndef DESKTOP_APP_DISABLE_WAYLAND_INTEGRATION
using f_gdk_wayland_window_get_type = GType (*)(void);
f_gdk_wayland_window_get_type gdk_wayland_window_get_type = nullptr;

template <typename Object>
inline bool gdk_is_wayland_window_check(Object *obj) {
	return g_type_cit_helper(obj, gdk_wayland_window_get_type());
}

using f_gdk_wayland_window_set_transient_for_exported = gboolean(*)(GdkWindow *window, char *parent_handle_str);
f_gdk_wayland_window_set_transient_for_exported gdk_wayland_window_set_transient_for_exported = nullptr;
#endif // !DESKTOP_APP_DISABLE_WAYLAND_INTEGRATION

void GdkHelperLoadGtk2(QLibrary &lib) {
#if !defined DESKTOP_APP_DISABLE_X11_INTEGRATION && !defined LINK_TO_GTK
	LOAD_GTK_SYMBOL(lib, gdk_x11_drawable_get_xdisplay);
	LOAD_GTK_SYMBOL(lib, gdk_x11_drawable_get_xid);
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION && !LINK_TO_GTK
}

void GdkHelperLoadGtk3(QLibrary &lib) {
#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
	LOAD_GTK_SYMBOL(lib, gdk_x11_window_get_type);
	LOAD_GTK_SYMBOL(lib, gdk_window_get_display);
	LOAD_GTK_SYMBOL(lib, gdk_x11_display_get_xdisplay);
	LOAD_GTK_SYMBOL(lib, gdk_x11_window_get_xid);
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION
#ifndef DESKTOP_APP_DISABLE_WAYLAND_INTEGRATION
	LOAD_GTK_SYMBOL(lib, gdk_wayland_window_get_type);
	LOAD_GTK_SYMBOL(lib, gdk_wayland_window_set_transient_for_exported);
#endif // !DESKTOP_APP_DISABLE_WAYLAND_INTEGRATION
}

} // namespace

void GdkHelperLoad(QLibrary &lib) {
	if (const auto integration = GtkIntegration::Instance()) {
		if (integration->checkVersion(3, 0, 0)) {
			GdkHelperLoadGtk3(lib);
		} else {
			GdkHelperLoadGtk2(lib);
		}
	}
}

void GdkSetTransientFor(GdkWindow *window, QWindow *parent) {
#ifndef DESKTOP_APP_DISABLE_WAYLAND_INTEGRATION
	if (gdk_wayland_window_get_type != nullptr
		&& gdk_wayland_window_set_transient_for_exported != nullptr
		&& gdk_is_wayland_window_check(window)) {
		if (const auto integration = WaylandIntegration::Instance()) {
			if (const auto handle = integration->nativeHandle(parent)
				; !handle.isEmpty()) {
				auto handleUtf8 = handle.toUtf8();
				gdk_wayland_window_set_transient_for_exported(
					window,
					handleUtf8.data());
				return;
			}
		}
	}
#endif // !DESKTOP_APP_DISABLE_WAYLAND_INTEGRATION

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
	if (gdk_x11_window_get_type != nullptr
		&& gdk_x11_display_get_xdisplay != nullptr
		&& gdk_x11_window_get_xid != nullptr
		&& gdk_window_get_display != nullptr
		&& gdk_is_x11_window_check(window)) {
		XSetTransientForHint(
			gdk_x11_display_get_xdisplay(gdk_window_get_display(window)),
			gdk_x11_window_get_xid(window),
			parent->winId());
		return;
	}
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION

#ifndef DESKTOP_APP_DISABLE_X11_INTEGRATION
	if (gdk_x11_drawable_get_xdisplay != nullptr
		&& gdk_x11_drawable_get_xid != nullptr) {
		XSetTransientForHint(
			gdk_x11_drawable_get_xdisplay(window),
			gdk_x11_drawable_get_xid(window),
			parent->winId());
		return;
	}
#endif // !DESKTOP_APP_DISABLE_X11_INTEGRATION
}

} // namespace internal
} // namespace Platform
