/*
 * Copyright (c) 2013 Tiago Vignatti
 * Copyright (c) 2013-2014 Collabora Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>

#include "desktop-shell-client-protocol.h"
#include "shell-helper-client-protocol.h"

#include "maynard-resources.h"

#include "app-icon.h"
#include "clock.h"
#include "favorites.h"
#include "launcher.h"
#include "panel.h"
#include "vertical-clock.h"

extern char **environ; /* defined by libc */

#define DEFAULT_BACKGROUND "/usr/share/wallpapers/berry.jpg"

struct element {
	GtkWidget *window;
	GdkPixbuf *pixbuf;
	struct wl_surface *surface;
};

struct desktop {
	struct wl_display *display;
	struct wl_registry *registry;
	struct desktop_shell *shell;
	struct wl_output *output;
	struct shell_helper *helper;

	struct wl_seat *seat;
	struct wl_pointer *pointer;

	GdkDisplay *gdk_display;

	struct element *background;
	struct element *panel;
	struct element *curtain;
	struct element *launcher_grid;
	struct element *clock;

	guint initial_panel_timeout_id;
	guint hide_panel_idle_id;

	gboolean grid_visible;
	gboolean system_visible;
	gboolean volume_visible;
	gboolean pointer_out_of_panel;
};

static gboolean panel_window_enter_cb(GtkWidget *widget, GdkEventCrossing *event, struct desktop *desktop);
static gboolean panel_window_leave_cb(GtkWidget *widget, GdkEventCrossing *event, struct desktop *desktop);

static gboolean connect_enter_leave_signals(gpointer data)
{
	struct desktop *desktop = data;
	GList *l;

	g_signal_connect(desktop->panel->window, "enter-notify-event",
	                 G_CALLBACK(panel_window_enter_cb), desktop);
	g_signal_connect(desktop->panel->window, "leave-notify-event",
	                 G_CALLBACK(panel_window_leave_cb), desktop);

	g_signal_connect(desktop->clock->window, "enter-notify-event",
	                 G_CALLBACK(panel_window_enter_cb), desktop);
	g_signal_connect(desktop->clock->window, "leave-notify-event",
	                 G_CALLBACK(panel_window_leave_cb), desktop);

	g_signal_connect(desktop->launcher_grid->window, "enter-notify-event",
	                 G_CALLBACK(panel_window_enter_cb), desktop);
	g_signal_connect(desktop->launcher_grid->window, "leave-notify-event",
	                 G_CALLBACK(panel_window_leave_cb), desktop);

	return G_SOURCE_REMOVE;
}

static void
desktop_shell_configure(void *data,
                        struct desktop_shell *desktop_shell,
                        uint32_t edges,
                        struct wl_surface *surface,
                        int32_t width, int32_t height)
{
	struct desktop *desktop = data;
	int window_height;
	int grid_width, grid_height;

	//printf("desktop_shell_configure\n");
	gtk_widget_set_size_request(desktop->background->window, width, height);

	/* TODO: make this height a little nicer */
	window_height = height * MAYNARD_PANEL_HEIGHT_RATIO;
	gtk_window_resize(GTK_WINDOW(desktop->panel->window),
	                   MAYNARD_PANEL_WIDTH, window_height);

	maynard_launcher_calculate(MAYNARD_LAUNCHER (desktop->launcher_grid->window),
	                            &grid_width, &grid_height, NULL);
	gtk_widget_set_size_request(desktop->launcher_grid->window,
	                             grid_width, grid_height);

	shell_helper_move_surface(desktop->helper, desktop->panel->surface,
	                           0, (height - window_height) / 2);

	gtk_window_resize(GTK_WINDOW(desktop->clock->window),
	                   MAYNARD_CLOCK_WIDTH, MAYNARD_CLOCK_HEIGHT);

	shell_helper_move_surface(desktop->helper, desktop->clock->surface,
	                           MAYNARD_PANEL_WIDTH, (height - window_height) / 2);

	shell_helper_move_surface(desktop->helper,
	                           desktop->launcher_grid->surface,
	                           - grid_width,
	                           ((height - window_height) / 2) + MAYNARD_CLOCK_HEIGHT);

	desktop_shell_desktop_ready(desktop->shell);

	/* TODO: why does the panel signal leave on drawing for
	 * startup? we don't want to have to have this silly
	 * timeout. */
	g_timeout_add_seconds(1, connect_enter_leave_signals, desktop);
}

static void
desktop_shell_prepare_lock_surface(void *data, struct desktop_shell *desktop_shell)
{
	desktop_shell_unlock(desktop_shell);
}

static void desktop_shell_grab_cursor(void *data, struct desktop_shell *desktop_shell, uint32_t cursor)
{
	/*struct desktop *desktop = data;

	switch (cursor) {
	case DESKTOP_SHELL_CURSOR_NONE:
		desktop->grab_cursor = CURSOR_BLANK;
		break;
	case DESKTOP_SHELL_CURSOR_BUSY:
		desktop->grab_cursor = CURSOR_WATCH;
		break;
	case DESKTOP_SHELL_CURSOR_MOVE:
		desktop->grab_cursor = CURSOR_DRAGGING;
		break;
	case DESKTOP_SHELL_CURSOR_RESIZE_TOP:
		desktop->grab_cursor = CURSOR_TOP;
		break;
	case DESKTOP_SHELL_CURSOR_RESIZE_BOTTOM:
		desktop->grab_cursor = CURSOR_BOTTOM;
		break;
	case DESKTOP_SHELL_CURSOR_RESIZE_LEFT:
		desktop->grab_cursor = CURSOR_LEFT;
		break;
	case DESKTOP_SHELL_CURSOR_RESIZE_RIGHT:
		desktop->grab_cursor = CURSOR_RIGHT;
		break;
	case DESKTOP_SHELL_CURSOR_RESIZE_TOP_LEFT:
		desktop->grab_cursor = CURSOR_TOP_LEFT;
		break;
	case DESKTOP_SHELL_CURSOR_RESIZE_TOP_RIGHT:
		desktop->grab_cursor = CURSOR_TOP_RIGHT;
		break;
	case DESKTOP_SHELL_CURSOR_RESIZE_BOTTOM_LEFT:
		desktop->grab_cursor = CURSOR_BOTTOM_LEFT;
		break;
	case DESKTOP_SHELL_CURSOR_RESIZE_BOTTOM_RIGHT:
		desktop->grab_cursor = CURSOR_BOTTOM_RIGHT;
		break;
	case DESKTOP_SHELL_CURSOR_ARROW:
	default:
		desktop->grab_cursor = CURSOR_LEFT_PTR;
	}*/
}

static void
desktop_shell_map(void *data, struct desktop_shell *desktop_shell, unsigned int id, const char *name)
{
	/* receive request from the compositor, map the window to the taskbar */

	/*	struct desktop *desktop = data;
		struct output *output;

		wl_list_for_each(output, &desktop->outputs, link) {
			if (output->taskbar && output->taskbar->painted) {
				taskbar_add_button (output->taskbar, id, name);
				window_schedule_resize(output->taskbar->window, 1000, 32);
			}
		}*/
	//printf("desktop_shell_map [%s]\n", name);
	g_message("desktop_shell_map [%s]\n", name);
}

static void
desktop_shell_unmap(void *data, struct desktop_shell *desktop_shell, unsigned int id, const char *name)
{
	/* receive request from the compositor, unmap the window from the taskbar */

	/*	struct desktop *desktop = data;
		struct output *output;

		struct taskbar_button *button;
		struct taskbar_button *tmp;

		wl_list_for_each(output, &desktop->outputs, link) {
			if (output->taskbar && output->taskbar->painted) {
				wl_list_for_each_safe(button, tmp, &output->taskbar->button_list, link) {
					if (button->id == id) {
						taskbar_destroy_button (button);
						window_schedule_resize(output->taskbar->window, 1000, 32);
					}
				}

			}
		}*/
	//printf("desktop_shell_unmap [%s]\n", name);
}

static const struct desktop_shell_listener shell_listener = {
	desktop_shell_configure,
	desktop_shell_prepare_lock_surface,
	desktop_shell_grab_cursor,
	/*desktop_shell_map,
	desktop_shell_unmap*/
};

static void launcher_grid_toggle(GtkWidget *widget, struct desktop *desktop)
{
	if (desktop->grid_visible) {
		shell_helper_slide_surface_back(desktop->helper,
		                                 desktop->launcher_grid->surface);

		shell_helper_curtain(desktop->helper, desktop->curtain->surface, 0);
	} else {
		int width;

		gtk_widget_get_size_request(desktop->launcher_grid->window,
		                             &width, NULL);

		shell_helper_slide_surface(desktop->helper,
		                            desktop->launcher_grid->surface,
		                            width + MAYNARD_PANEL_WIDTH, 0);

		shell_helper_curtain(desktop->helper, desktop->curtain->surface, 1);
	}

	desktop->grid_visible = !desktop->grid_visible;
}

static void launcher_grid_create(struct desktop *desktop)
{
	struct element *launcher_grid;
	GdkWindow *gdk_window;

	launcher_grid = malloc(sizeof *launcher_grid);
	memset(launcher_grid, 0, sizeof *launcher_grid);

	launcher_grid->window = maynard_launcher_new (desktop->background->window);
	gdk_window = gtk_widget_get_window(launcher_grid->window);
	launcher_grid->surface = gdk_wayland_window_get_wl_surface(gdk_window);

	gdk_wayland_window_set_use_custom_surface(gdk_window);
	shell_helper_add_surface_to_layer(desktop->helper, launcher_grid->surface, desktop->panel->surface);

	g_signal_connect(launcher_grid->window, "app-launched",
	                  G_CALLBACK(launcher_grid_toggle), desktop);

	gtk_widget_show_all(launcher_grid->window);

	desktop->launcher_grid = launcher_grid;
}

static void
volume_changed_cb(MaynardClock *clock, gdouble value, const gchar *icon_name, struct desktop *desktop)
{
	maynard_panel_set_volume_icon_name(MAYNARD_PANEL(desktop->panel->window), icon_name);
}

static GtkWidget *clock_create(struct desktop *desktop)
{
	struct element *clock;
	GdkWindow *gdk_window;

	clock = malloc(sizeof *clock);
	memset(clock, 0, sizeof *clock);

	clock->window = maynard_clock_new();

	g_signal_connect(clock->window, "volume-changed", G_CALLBACK(volume_changed_cb), desktop);

	gdk_window = gtk_widget_get_window(clock->window);
	clock->surface = gdk_wayland_window_get_wl_surface(gdk_window);

	gdk_wayland_window_set_use_custom_surface(gdk_window);
	shell_helper_add_surface_to_layer(desktop->helper, clock->surface, desktop->panel->surface);

	gtk_widget_show_all(clock->window);

	desktop->clock = clock;
}

static void button_toggled_cb(struct desktop *desktop, gboolean *visible, gboolean *not_visible)
{
	*visible = !*visible;
	*not_visible = FALSE;

	if (desktop->system_visible) {
		maynard_clock_show_section(MAYNARD_CLOCK(desktop->clock->window),
		                            MAYNARD_CLOCK_SECTION_SYSTEM);
		maynard_panel_show_previous(MAYNARD_PANEL(desktop->panel->window),
		                             MAYNARD_PANEL_BUTTON_SYSTEM);
	} else if (desktop->volume_visible) {
		maynard_clock_show_section(MAYNARD_CLOCK(desktop->clock->window),
		                            MAYNARD_CLOCK_SECTION_VOLUME);
		maynard_panel_show_previous(MAYNARD_PANEL(desktop->panel->window),
		                             MAYNARD_PANEL_BUTTON_VOLUME);
	} else {
		maynard_clock_show_section(MAYNARD_CLOCK(desktop->clock->window),
		                            MAYNARD_CLOCK_SECTION_CLOCK);
		maynard_panel_show_previous(MAYNARD_PANEL(desktop->panel->window),
		                             MAYNARD_PANEL_BUTTON_NONE);
	}
}

static void system_toggled_cb(GtkWidget *widget, struct desktop *desktop)
{
	button_toggled_cb(desktop, &desktop->system_visible, &desktop->volume_visible);
}

static void volume_toggled_cb(GtkWidget *widget, struct desktop *desktop)
{
	button_toggled_cb(desktop, &desktop->volume_visible, &desktop->system_visible);
}

static gboolean
panel_window_enter_cb(GtkWidget *widget, GdkEventCrossing *event, struct desktop *desktop)
{
	if (desktop->initial_panel_timeout_id > 0) {
		g_source_remove(desktop->initial_panel_timeout_id);
		desktop->initial_panel_timeout_id = 0;
	}

	if (desktop->hide_panel_idle_id > 0) {
		g_source_remove(desktop->hide_panel_idle_id);
		desktop->hide_panel_idle_id = 0;
		return TRUE;
	}

	if (desktop->pointer_out_of_panel) {
		desktop->pointer_out_of_panel = FALSE;
		return TRUE;
	}

	shell_helper_slide_surface_back(desktop->helper, desktop->panel->surface);
	shell_helper_slide_surface_back(desktop->helper, desktop->clock->surface);

	maynard_panel_set_expand(MAYNARD_PANEL(desktop->panel->window), TRUE);

	return FALSE;
}

static gboolean leave_panel_idle_cb(gpointer data)
{
	struct desktop *desktop = data;
	gint width;

	desktop->hide_panel_idle_id = 0;

	gtk_window_get_size(GTK_WINDOW(desktop->clock->window), &width, NULL);

	shell_helper_slide_surface(desktop->helper,
	                            desktop->panel->surface,
	                            MAYNARD_VERTICAL_CLOCK_WIDTH - MAYNARD_PANEL_WIDTH, 0);
	shell_helper_slide_surface(desktop->helper,
	                            desktop->clock->surface,
	                            MAYNARD_VERTICAL_CLOCK_WIDTH - MAYNARD_PANEL_WIDTH - width, 0);

	maynard_panel_set_expand(MAYNARD_PANEL(desktop->panel->window), FALSE);

	maynard_clock_show_section(MAYNARD_CLOCK(desktop->clock->window), MAYNARD_CLOCK_SECTION_CLOCK);
	maynard_panel_show_previous(MAYNARD_PANEL(desktop->panel->window), MAYNARD_PANEL_BUTTON_NONE);
	desktop->system_visible = FALSE;
	desktop->volume_visible = FALSE;
	desktop->pointer_out_of_panel = FALSE;

	return G_SOURCE_REMOVE;
}

static gboolean panel_window_leave_cb(GtkWidget *widget, GdkEventCrossing *event, struct desktop *desktop)
{
	if (desktop->initial_panel_timeout_id > 0) {
		g_source_remove(desktop->initial_panel_timeout_id);
		desktop->initial_panel_timeout_id = 0;
	}

	if (desktop->hide_panel_idle_id > 0) {
		return TRUE;
	}

	if (desktop->grid_visible) {
		desktop->pointer_out_of_panel = TRUE;
		return TRUE;
	}

	desktop->hide_panel_idle_id = g_idle_add(leave_panel_idle_cb, desktop);

	return FALSE;
}

static gboolean panel_hide_timeout_cb(gpointer data)
{
	struct desktop *desktop = data;

	panel_window_leave_cb(NULL, NULL, desktop);

	return G_SOURCE_REMOVE;
}

static void favorite_launched_cb(MaynardPanel *panel, struct desktop *desktop)
{
	if (desktop->grid_visible) {
		launcher_grid_toggle(desktop->launcher_grid->window, desktop);
	}

	panel_window_leave_cb(NULL, NULL, desktop);
}

static void panel_create(struct desktop *desktop)
{
	struct element *panel;
	GdkWindow *gdk_window;

	panel = malloc(sizeof *panel);
	memset(panel, 0, sizeof *panel);

	panel->window = maynard_panel_new();

	g_signal_connect(panel->window, "app-menu-toggled",
	                  G_CALLBACK(launcher_grid_toggle), desktop);
	g_signal_connect(panel->window, "system-toggled",
	                  G_CALLBACK(system_toggled_cb), desktop);
	g_signal_connect(panel->window, "volume-toggled",
	                  G_CALLBACK(volume_toggled_cb), desktop);
	g_signal_connect(panel->window, "favorite-launched",
	                  G_CALLBACK(favorite_launched_cb), desktop);

	desktop->initial_panel_timeout_id =
	        g_timeout_add_seconds(2, panel_hide_timeout_cb, desktop);

	/* set it up as the panel */
	gdk_window = gtk_widget_get_window(panel->window);
	gdk_wayland_window_set_use_custom_surface(gdk_window);

	panel->surface = gdk_wayland_window_get_wl_surface(gdk_window);
	desktop_shell_set_user_data(desktop->shell, desktop);
	desktop_shell_set_panel(desktop->shell, desktop->output, panel->surface);
	desktop_shell_set_panel_position(desktop->shell, DESKTOP_SHELL_PANEL_POSITION_LEFT);
	shell_helper_set_panel(desktop->helper, panel->surface);

	gtk_widget_show_all(panel->window);

	desktop->panel = panel;
}

/* Expose callback for the drawing area */
static gboolean draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	struct desktop *desktop = data;

	gdk_cairo_set_source_pixbuf(cr, desktop->background->pixbuf, 0, 0);
	cairo_paint(cr);

	return TRUE;
}

/* Destroy handler for the window */
static void destroy_cb(GObject *object, gpointer data)
{
	gtk_main_quit();
}

char *exts = ".jpg.JPG.png.PNG";
int selects(const struct dirent *dir)
{
	int *p;

	char a = strlen(dir->d_name);
	if (a<4) return 0;
	a -= 4;
	p = (int*)&(dir->d_name[a]);

	for (int i=0; i<strlen(exts)/4; i++) {
		if (*p == *((int*)(&exts[i*4]))) return 1;
	}
	return 0;
}
char *getFile(char *dirname, int c)
{
	static char p[256];
	struct dirent **namelist;

	int r = scandir(dirname, &namelist, selects, alphasort);
	if (r==-1) return 0;

	strncpy(p, dirname, 255);
	strncat(p, "/", 255);
	if (c==-1) c = rand()%r;
	for (int i=0; i<r; i++) {
		if (i==c) strncat(p, namelist[i]->d_name, 255);
		free(namelist[i]);
	}
	free(namelist);

	return p;
}
static GdkPixbuf *scale_background(GdkPixbuf *original_pixbuf)
{
	/* Scale original_pixbuf so it mostly fits on the screen.
	 * If the aspect ratio is different than a bit on the right or on the
	 * bottom could be cropped out. */
	GdkScreen *screen = gdk_screen_get_default();
	gint screen_width, screen_height;
	gint original_width, original_height;
	gint final_width, final_height;
	gdouble ratio_horizontal, ratio_vertical, ratio;

	screen_width = gdk_screen_get_width(screen);
	screen_height = gdk_screen_get_height(screen);
	original_width = gdk_pixbuf_get_width(original_pixbuf);
	original_height = gdk_pixbuf_get_height(original_pixbuf);

	ratio_horizontal = (double) screen_width / original_width;
	ratio_vertical = (double) screen_height / original_height;
	ratio = MAX(ratio_horizontal, ratio_vertical);

	final_width = ceil(ratio * original_width);
	final_height = ceil(ratio * original_height);

	return gdk_pixbuf_scale_simple(original_pixbuf, final_width, final_height, GDK_INTERP_BILINEAR);
}

static void background_create(struct desktop *desktop)
{
	GdkWindow *gdk_window;
	struct element *background;
	const gchar *filename;
	GdkPixbuf *unscaled_background;

	background = malloc(sizeof *background);
	memset(background, 0, sizeof *background);

	GSettings *settings = g_settings_new("org.berry.maynard");
	srand((unsigned)time(NULL));
	filename = getFile(g_settings_get_string(settings, "background"), -1);
	g_clear_object(&settings);
	//filename = g_getenv("BACKGROUND");
	if (filename == NULL) {
		filename = DEFAULT_BACKGROUND;
	}
	unscaled_background = gdk_pixbuf_new_from_file(filename, NULL);
	if (!unscaled_background) {
		g_message("Could not load background. "
		          "Do you have kdewallpapers installed?");
		exit(EXIT_FAILURE);
	}

	background->pixbuf = scale_background(unscaled_background);
	g_object_unref(unscaled_background);

	background->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	g_signal_connect(background->window, "destroy", G_CALLBACK(destroy_cb), NULL);

	g_signal_connect(background->window, "draw", G_CALLBACK(draw_cb), desktop);

	gtk_window_set_title(GTK_WINDOW(background->window), "maynard");
	gtk_window_set_decorated(GTK_WINDOW(background->window), FALSE);
	gtk_widget_realize(background->window);

	gdk_window = gtk_widget_get_window(background->window);
	gdk_wayland_window_set_use_custom_surface(gdk_window);

	background->surface = gdk_wayland_window_get_wl_surface(gdk_window);
	desktop_shell_set_grab_surface(desktop->shell, background->surface);	// Important!!
	desktop_shell_set_user_data(desktop->shell, desktop);
	desktop_shell_set_background(desktop->shell, desktop->output, background->surface);

	desktop->background = background;

	gtk_widget_show_all(background->window);
}

static void curtain_create(struct desktop *desktop)
{
	GdkWindow *gdk_window;
	struct element *curtain;

	curtain = malloc(sizeof *curtain);
	memset(curtain, 0, sizeof *curtain);

	curtain->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_title(GTK_WINDOW(curtain->window), "maynard");
	gtk_window_set_decorated(GTK_WINDOW(curtain->window), FALSE);
	gtk_widget_set_size_request(curtain->window, 8192, 8192);
	gtk_widget_realize(curtain->window);

	gdk_window = gtk_widget_get_window(curtain->window);
	gdk_wayland_window_set_use_custom_surface(gdk_window);

	curtain->surface = gdk_wayland_window_get_wl_surface(gdk_window);

	desktop->curtain = curtain;

	gtk_widget_show_all(curtain->window);
}

static void css_setup(struct desktop *desktop)
{
	GtkCssProvider *provider;
	GFile *file;
	GError *error = NULL;

	provider = gtk_css_provider_new();

	file = g_file_new_for_uri("resource:///org/berry/maynard/style.css");

	if (!gtk_css_provider_load_from_file(provider, file, &error)) {
		g_warning("Failed to load CSS file: %s", error->message);
		g_clear_error(&error);
		g_object_unref(file);
		return;
	}

	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
	                GTK_STYLE_PROVIDER(provider), 600);

	g_object_unref(file);
}


static void
pointer_handle_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t sx_w, wl_fixed_t sy_w)
{
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface)
{
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t sx_w, wl_fixed_t sy_w)
{
}

static void
pointer_handle_button(void *data,
                      struct wl_pointer *pointer,
                      uint32_t serial,
                      uint32_t time,
                      uint32_t button,
                      uint32_t state_w)
{
	struct desktop *desktop = data;

	//printf("pointer_handle_button\n");
	if (state_w != WL_POINTER_BUTTON_STATE_RELEASED) {
		return;
	}

	if (!desktop->pointer_out_of_panel) {
		return;
	}

	if (desktop->grid_visible) {
		launcher_grid_toggle(desktop->launcher_grid->window, desktop);
	}

	panel_window_leave_cb(NULL, NULL, desktop);
}

static void
pointer_handle_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
	struct desktop *desktop = data;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !desktop->pointer) {
		desktop->pointer = wl_seat_get_pointer(seat);
		wl_pointer_set_user_data(desktop->pointer, desktop);
		wl_pointer_add_listener(desktop->pointer, &pointer_listener, desktop);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && desktop->pointer) {
		wl_pointer_destroy(desktop->pointer);
		desktop->pointer = NULL;
	}

	/* TODO: keyboard and touch */
}

static void seat_handle_name(void *data, struct wl_seat *seat, const char *name)
{
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
	seat_handle_name
};

static void
registry_handle_global(void *data,
                       struct wl_registry *registry,
                       uint32_t name,
                       const char *interface,
                       uint32_t version)
{
	struct desktop *d = data;

	if (!strcmp(interface, "desktop_shell")) {
		d->shell = wl_registry_bind(registry, name,
		                             &desktop_shell_interface, 3);
		desktop_shell_add_listener (d->shell, &shell_listener, d);
	} else if (!strcmp(interface, "wl_output")) {
		/* TODO: create multiple outputs */
		d->output = wl_registry_bind(registry, name, &wl_output_interface, 1);
	} else if (!strcmp(interface, "wl_seat")) {
		d->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
		wl_seat_add_listener (d->seat, &seat_listener, d);
	} else if (!strcmp(interface, "shell_helper")) {
		d->helper = wl_registry_bind(registry, name, &shell_helper_interface, 1);
	}
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

int main(int argc, char *argv[])
{
	struct desktop *desktop;

	gdk_set_allowed_backends("wayland");

	gtk_init(&argc, &argv);

	g_resources_register(maynard_get_resource());

	desktop = malloc(sizeof *desktop);
	desktop->output = NULL;
	desktop->shell = NULL;
	desktop->helper = NULL;
	desktop->seat = NULL;
	desktop->pointer = NULL;

	desktop->gdk_display = gdk_display_get_default();
	desktop->display = gdk_wayland_display_get_wl_display(desktop->gdk_display);
	if (desktop->display == NULL) {
		fprintf(stderr, "failed to get display: %m\n");
		return -1;
	}

	desktop->registry = wl_display_get_registry(desktop->display);
	wl_registry_add_listener(desktop->registry, &registry_listener, desktop);

	/* Wait until we have been notified about the compositor,
	 * shell, and shell helper objects */
	if (!desktop->output || !desktop->shell || !desktop->helper) {
		wl_display_roundtrip(desktop->display);
	}
	if (!desktop->output || !desktop->shell || !desktop->helper) {
		fprintf(stderr, "could not find output, shell or helper modules\n");
		return -1;
	}

	desktop->grid_visible = FALSE;
	desktop->system_visible = FALSE;
	desktop->volume_visible = FALSE;
	desktop->pointer_out_of_panel = FALSE;

	css_setup(desktop);
	background_create(desktop);
	curtain_create(desktop);

	/* panel needs to be first so the clock and launcher grid can
	 * be added to its layer */
	panel_create(desktop);
	clock_create(desktop);
	launcher_grid_create(desktop);

	gtk_main();

	/* TODO cleanup */
	return EXIT_SUCCESS;
}
