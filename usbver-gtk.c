/* usbver_gtk3.c - libusb 监听 U 盘 + GTK3 滚动日志
 * gcc -O2 -o usbver_gtk3 usbver_gtk3.c $(pkg-config --cflags --libs gtk+-3.0 libusb-1.0)
 */
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <libusb.h>
#include <gtk/gtk.h>

static GtkWidget *tv, *hint;
static GtkTextBuffer *buf;
static GtkTextTag *hi, *normal;
static int first = 1;

static int is_ms(libusb_device *dev) {
    struct libusb_config_descriptor *c;
    if (libusb_get_active_config_descriptor(dev, &c) < 0) return 0;
    int f = 0;
    for (int i = 0; i < c->bNumInterfaces && !f; i++)
        for (int j = 0; j < c->interface[i].num_altsetting && !f; j++)
            f = c->interface[i].altsetting[j].bInterfaceClass == 0x08;
    libusb_free_config_descriptor(c);
    return f;
}

static const char *spd(int s) {
    static const char *n[] = {
        [LIBUSB_SPEED_LOW]        = "USB 1.0      (1.5Mbps)",
        [LIBUSB_SPEED_FULL]       = "USB 1.1      (12Mbps)",
        [LIBUSB_SPEED_HIGH]       = "USB 2.0      (480Mbps)",
        [LIBUSB_SPEED_SUPER]      = "USB 3.2 Gen1 (5Gbps)",
        [LIBUSB_SPEED_SUPER_PLUS] = "USB 3.2 Gen2 (10Gbps)",
    };
    return (s >= 0 && s < (int)G_N_ELEMENTS(n) && n[s]) ? n[s] : "未知";
}

static gboolean upd(gpointer d) {
    char *s = d;
    GtkTextIter a, b;

    if (first) {                       /* 首条：清空等待提示 */
        first = 0;
        gtk_widget_hide(hint);
        gtk_text_buffer_set_text(buf, "", -1);
    } else {                           /* 后续：旧行去高亮、统一颜色 */
        gtk_text_buffer_get_bounds(buf, &a, &b);
        gtk_text_buffer_remove_tag(buf, hi, &a, &b);
        gtk_text_buffer_apply_tag(buf, normal, &a, &b);
    }

    gtk_text_buffer_get_end_iter(buf, &b);
    gtk_text_buffer_insert_with_tags(buf, &b, s, -1, hi, NULL);
    gtk_text_buffer_insert(buf, &b, "\n", -1);
    gtk_text_buffer_place_cursor(buf, &b);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(tv),
        gtk_text_buffer_get_insert(buf));
    g_free(s);
    return G_SOURCE_REMOVE;
}

static int on_arrive(libusb_context *c, libusb_device *dev,
                     libusb_hotplug_event e, void *u) {
    if (!is_ms(dev)) return 0;
    struct libusb_device_descriptor d;
    if (libusb_get_device_descriptor(dev, &d) < 0) return 0;
    char t[32], line[256];
    time_t now = time(NULL);
    strftime(t, sizeof t, "%m-%d %H:%M:%S", localtime(&now));
    snprintf(line, sizeof line, "[%s] 新优盘: %s  bus %u addr %u  %04x:%04x",
             t, spd(libusb_get_device_speed(dev)),
             libusb_get_bus_number(dev), libusb_get_device_address(dev),
             d.idVendor, d.idProduct);
    g_idle_add(upd, g_strdup(line));
    return 0;
}

static gpointer usb_thread(gpointer d) {
    struct timeval t = {1, 0};
    while (1) libusb_handle_events_timeout(d, &t);
    return NULL;
}

static void activate(GtkApplication *app, gpointer d) {
    GtkWidget *w = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(w), "USB 监听");
    gtk_window_set_default_size(GTK_WINDOW(w), 900, 360);

    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv), 8);
    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));

    hi = gtk_text_buffer_create_tag(buf, "hi",
        "weight", PANGO_WEIGHT_BOLD, "foreground", "#0a7d22", NULL);
    normal = gtk_text_buffer_create_tag(buf, "normal",
        "weight", PANGO_WEIGHT_NORMAL, "foreground", "#333333", NULL);

    gtk_container_add(GTK_CONTAINER(sw), tv);

    GtkWidget *ov = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(ov), sw);

    hint = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(hint),
                         "<span size='large'>等待 U 盘插入 ...</span>");
    gtk_widget_set_halign(hint, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(hint, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(hint, TRUE);
    gtk_widget_set_vexpand(hint, TRUE);
    gtk_overlay_add_overlay(GTK_OVERLAY(ov), hint);

    gtk_container_add(GTK_CONTAINER(w), ov);
    gtk_widget_show_all(w);
}

int main(int argc, char **argv) {
    if (geteuid() != 0) {
        fprintf(stderr, "请用 root 身份运行\n");
        return 1;
    }
    libusb_context *ctx;
    libusb_hotplug_callback_handle h;
    if (libusb_init(&ctx) < 0) return 1;
    if (libusb_hotplug_register_callback(ctx, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
            LIBUSB_HOTPLUG_NO_FLAGS, LIBUSB_HOTPLUG_MATCH_ANY,
            LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
            on_arrive, NULL, &h) != LIBUSB_SUCCESS) return 1;
    g_thread_new("usb", usb_thread, ctx);

    GtkApplication *app = gtk_application_new("org.x.usbver",
                                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int st = g_application_run(G_APPLICATION(app), argc, argv);
    libusb_hotplug_deregister_callback(ctx, h);
    libusb_exit(ctx);
    g_object_unref(app);
    return st;
}
