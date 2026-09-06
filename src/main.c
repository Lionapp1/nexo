#include <gtk/gtk.h>
#include <webkit/webkit.h>

#define HOME_URL "https://www.google.com/"
#define SEARCH_URL "https://www.google.com/search?q="

typedef struct _NexoApp NexoApp;
typedef struct _NexoTab NexoTab;

struct _NexoApp {
    GtkWidget *window, *address, *back, *forward, *reload, *stop, *tabs, *stack;
    GPtrArray *pages;
    guint next_tab_id;
};

struct _NexoTab {
    NexoApp *app;
    WebKitWebView *view;
    GtkWidget *page, *button, *label, *close;
    gchar *stack_name;
    guint id;
};

static NexoTab *current(NexoApp *a) {
    if (!a || !a->pages || !a->pages->len) return NULL;
    guint i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(a->stack), "current-index"));
    if (i >= a->pages->len) i = 0;
    return g_ptr_array_index(a->pages, i);
}

static guint index_of(NexoApp *a, NexoTab *t) {
    for (guint i = 0; i < a->pages->len; i++) {
        if (g_ptr_array_index(a->pages, i) == t) return i;
    }
    return 0;
}

static void nav_state(NexoApp *a) {
    NexoTab *t = current(a);
    if (!t) return;
    gtk_widget_set_sensitive(a->back, webkit_web_view_can_go_back(t->view));
    gtk_widget_set_sensitive(a->forward, webkit_web_view_can_go_forward(t->view));
    gboolean loading = webkit_web_view_is_loading(t->view);
    gtk_widget_set_visible(a->stop, loading);
    gtk_widget_set_visible(a->reload, !loading);
}

static gchar *shown_uri(const gchar *u) {
    if (!u) return g_strdup("");
    if (g_str_has_prefix(u, "https://")) return g_strdup(u + 8);
    if (g_str_has_prefix(u, "http://")) return g_strdup(u + 7);
    return g_strdup(u);
}

static gchar *make_url(const gchar *s) {
    gchar *t = g_strstrip(g_strdup(s ? s : ""));
    if (!*t) {
        g_free(t);
        return g_strdup(HOME_URL);
    }
    if (g_str_has_prefix(t, "http://") ||
        g_str_has_prefix(t, "https://") ||
        g_str_has_prefix(t, "file://") ||
        g_str_has_prefix(t, "about:")) {
        return t;
    }
    if (!strchr(t, ' ') && strchr(t, '.')) {
        gchar *r = g_strconcat("https://", t, NULL);
        g_free(t);
        return r;
    }
    gchar *q = g_uri_escape_string(t, NULL, FALSE);
    gchar *r = g_strconcat(SEARCH_URL, q, NULL);
    g_free(q);
    g_free(t);
    return r;
}

static void go_address(NexoApp *a) {
    NexoTab *t = current(a);
    if (!t) return;
    gchar *u = make_url(gtk_editable_get_text(GTK_EDITABLE(a->address)));
    webkit_web_view_load_uri(t->view, u);
    g_free(u);
}

static void address_activate(GtkEntry *e, gpointer d) {
    (void)e;
    go_address(d);
}

static void back_cb(GtkButton *b, gpointer d) {
    (void)b;
    NexoTab *t = current(d);
    if (t) webkit_web_view_go_back(t->view);
}

static void forward_cb(GtkButton *b, gpointer d) {
    (void)b;
    NexoTab *t = current(d);
    if (t) webkit_web_view_go_forward(t->view);
}

static void reload_cb(GtkButton *b, gpointer d) {
    (void)b;
    NexoTab *t = current(d);
    if (t) webkit_web_view_reload(t->view);
}

static void stop_cb(GtkButton *b, gpointer d) {
    (void)b;
    NexoTab *t = current(d);
    if (t) webkit_web_view_stop_loading(t->view);
}

static void home_cb(GtkButton *b, gpointer d) {
    (void)b;
    NexoTab *t = current(d);
    if (t) webkit_web_view_load_uri(t->view, HOME_URL);
}

static void select_tab(NexoApp *a, guint i) {
    if (i >= a->pages->len) return;
    NexoTab *t = g_ptr_array_index(a->pages, i);
    gtk_stack_set_visible_child_name(GTK_STACK(a->stack), t->stack_name);
    g_object_set_data(G_OBJECT(a->stack), "current-index", GUINT_TO_POINTER(i));
    gchar *u = shown_uri(webkit_web_view_get_uri(t->view));
    gtk_editable_set_text(GTK_EDITABLE(a->address), u);
    g_free(u);
    nav_state(a);
}

static void tab_click(GtkButton *b, gpointer d) {
    NexoTab *t = g_object_get_data(G_OBJECT(b), "nexo-tab");
    select_tab(d, index_of(d, t));
}

static void title_changed(GObject *o, GParamSpec *p, gpointer d) {
    (void)o;
    (void)p;
    NexoTab *t = d;
    const gchar *s = webkit_web_view_get_title(t->view);
    gtk_label_set_text(GTK_LABEL(t->label), (s && *s) ? s : "Yeni Sekme");
}

static void uri_changed(GObject *o, GParamSpec *p, gpointer d) {
    (void)o;
    (void)p;
    NexoTab *t = d;
    if (current(t->app) == t) {
        gchar *u = shown_uri(webkit_web_view_get_uri(t->view));
        gtk_editable_set_text(GTK_EDITABLE(t->app->address), u);
        g_free(u);
    }
}

static void load_changed(WebKitWebView *v, WebKitLoadEvent e, gpointer d) {
    NexoTab *t = d;
    if (e == WEBKIT_LOAD_STARTED || e == WEBKIT_LOAD_COMMITTED || e == WEBKIT_LOAD_FINISHED) {
        if (current(t->app) == t) uri_changed(G_OBJECT(v), NULL, t);
        nav_state(t->app);
    }
}

static void close_tab(GtkButton *b, gpointer d);
static void new_tab(NexoApp *a, const gchar *u);

static gboolean policy(WebKitWebView *v, WebKitPolicyDecision *d,
                       WebKitPolicyDecisionType type, gpointer data) {
    (void)v;
    NexoApp *a = data;
    if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION &&
        type != WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        return FALSE;
    }

    WebKitNavigationPolicyDecision *n = WEBKIT_NAVIGATION_POLICY_DECISION(d);
    WebKitNavigationAction *x = webkit_navigation_policy_decision_get_navigation_action(n);
    WebKitURIRequest *r = webkit_navigation_action_get_request(x);
    const gchar *uri = webkit_uri_request_get_uri(r);

    if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        new_tab(a, uri);
        webkit_policy_decision_ignore(d);
        return TRUE;
    }

    guint modifiers = webkit_navigation_action_get_modifiers(x);
    WebKitNavigationType navigation_type = webkit_navigation_action_get_navigation_type(x);
    if ((modifiers & GDK_CONTROL_MASK) &&
        navigation_type == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED) {
        new_tab(a, uri);
        webkit_policy_decision_ignore(d);
        return TRUE;
    }
    return FALSE;
}

static void css(void) {
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_string(p,
        "headerbar { padding: 6px 10px; min-height: 52px; }"
        ".nav { min-width: 38px; min-height: 38px; padding: 0; }"
        ".address { min-height: 40px; border-radius: 20px; padding: 0 16px; }"
        ".tabs { padding: 4px 8px 0; }"
        ".tab-button { min-height: 36px; padding: 3px 7px; }"
        ".tab-button label { max-width: 190px; }");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(p),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);
}

static void new_tab(NexoApp *a, const gchar *u) {
    NexoTab *t = g_new0(NexoTab, 1);
    t->app = a;
    t->id = a->next_tab_id++;
    t->view = WEBKIT_WEB_VIEW(webkit_web_view_new());

    WebKitSettings *s = webkit_web_view_get_settings(t->view);
    webkit_settings_set_enable_javascript(s, TRUE);
    webkit_settings_set_enable_webgl(s, TRUE);
    webkit_settings_set_enable_webaudio(s, TRUE);
    webkit_settings_set_enable_media(s, TRUE);
    webkit_settings_set_enable_smooth_scrolling(s, TRUE);
    webkit_settings_set_enable_page_cache(s, TRUE);
    webkit_settings_set_enable_site_specific_quirks(s, TRUE);
    webkit_settings_set_enable_media_stream(s, TRUE);

    t->page = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(t->page), GTK_WIDGET(t->view));
    t->stack_name = g_strdup_printf("tab-%u", t->id);
    gtk_stack_add_named(GTK_STACK(a->stack), t->page, t->stack_name);

    t->button = gtk_button_new();
    gtk_widget_add_css_class(t->button, "tab-button");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    t->label = gtk_label_new("Yeni Sekme");
    gtk_label_set_ellipsize(GTK_LABEL(t->label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_size_request(t->label, 120, -1);
    t->close = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(t->close, "flat");
    gtk_box_append(GTK_BOX(box), t->label);
    gtk_box_append(GTK_BOX(box), t->close);
    gtk_button_set_child(GTK_BUTTON(t->button), box);
    gtk_box_append(GTK_BOX(a->tabs), t->button);

    g_object_set_data(G_OBJECT(t->button), "nexo-tab", t);
    g_object_set_data(G_OBJECT(t->close), "nexo-tab", t);
    g_signal_connect(t->button, "clicked", G_CALLBACK(tab_click), a);
    g_signal_connect(t->close, "clicked", G_CALLBACK(close_tab), a);
    g_signal_connect(t->view, "notify::title", G_CALLBACK(title_changed), t);
    g_signal_connect(t->view, "notify::uri", G_CALLBACK(uri_changed), t);
    g_signal_connect(t->view, "load-changed", G_CALLBACK(load_changed), t);
    g_signal_connect(t->view, "decide-policy", G_CALLBACK(policy), a);

    g_ptr_array_add(a->pages, t);
    webkit_web_view_load_uri(t->view, u ? u : HOME_URL);
    select_tab(a, a->pages->len - 1);
}

static void new_tab_cb(GtkButton *b, gpointer d) {
    (void)b;
    new_tab(d, HOME_URL);
}

static void close_tab(GtkButton *b, gpointer d) {
    NexoApp *a = d;
    NexoTab *t = g_object_get_data(G_OBJECT(b), "nexo-tab");
    guint i = index_of(a, t);

    if (a->pages->len == 1) {
        webkit_web_view_load_uri(t->view, HOME_URL);
        return;
    }

    gtk_stack_remove(GTK_STACK(a->stack), t->page);
    gtk_box_remove(GTK_BOX(a->tabs), t->button);
    g_ptr_array_remove_index(a->pages, i);
    g_free(t->stack_name);
    g_free(t);

    select_tab(a, i < a->pages->len ? i : a->pages->len - 1);
}

static void activate(GtkApplication *g, gpointer u) {
    (void)u;
    NexoApp *a = g_new0(NexoApp, 1);
    a->pages = g_ptr_array_new();
    a->next_tab_id = 1;

    a->window = gtk_application_window_new(g);
    gtk_window_set_title(GTK_WINDOW(a->window), "Nexo Browser");
    gtk_window_set_default_size(GTK_WINDOW(a->window), 1280, 800);
    css();

    GtkWidget *hb = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(a->window), hb);

    GtkWidget *nav = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    a->back = gtk_button_new_from_icon_name("go-previous-symbolic");
    a->forward = gtk_button_new_from_icon_name("go-next-symbolic");
    a->reload = gtk_button_new_from_icon_name("view-refresh-symbolic");
    a->stop = gtk_button_new_from_icon_name("process-stop-symbolic");
    GtkWidget *home = gtk_button_new_from_icon_name("go-home-symbolic");
    GtkWidget *plus = gtk_button_new_from_icon_name("tab-new-symbolic");
    GtkWidget *buttons[] = { a->back, a->forward, a->reload, a->stop, home, plus };

    for (guint i = 0; i < G_N_ELEMENTS(buttons); i++) {
        gtk_widget_add_css_class(buttons[i], "nav");
        gtk_widget_set_focus_on_click(buttons[i], FALSE);
    }

    gtk_widget_set_visible(a->stop, FALSE);
    gtk_widget_set_tooltip_text(a->back, "Geri");
    gtk_widget_set_tooltip_text(a->forward, "İleri");
    gtk_widget_set_tooltip_text(a->reload, "Yenile");
    gtk_widget_set_tooltip_text(a->stop, "Durdur");
    gtk_widget_set_tooltip_text(home, "Ana sayfa");
    gtk_widget_set_tooltip_text(plus, "Yeni sekme");

    gtk_box_append(GTK_BOX(nav), a->back);
    gtk_box_append(GTK_BOX(nav), a->forward);
    gtk_box_append(GTK_BOX(nav), a->reload);
    gtk_box_append(GTK_BOX(nav), a->stop);
    gtk_box_append(GTK_BOX(nav), home);

    a->address = gtk_search_entry_new();
    gtk_widget_add_css_class(a->address, "address");
    gtk_widget_set_hexpand(a->address, TRUE);
    gtk_widget_set_tooltip_text(a->address, "Adres veya arama");
    gtk_box_append(GTK_BOX(nav), a->address);
    gtk_box_append(GTK_BOX(nav), plus);
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(hb), nav);

    g_signal_connect(a->back, "clicked", G_CALLBACK(back_cb), a);
    g_signal_connect(a->forward, "clicked", G_CALLBACK(forward_cb), a);
    g_signal_connect(a->reload, "clicked", G_CALLBACK(reload_cb), a);
    g_signal_connect(a->stop, "clicked", G_CALLBACK(stop_cb), a);
    g_signal_connect(home, "clicked", G_CALLBACK(home_cb), a);
    g_signal_connect(plus, "clicked", G_CALLBACK(new_tab_cb), a);
    g_signal_connect(a->address, "activate", G_CALLBACK(address_activate), a);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    a->tabs = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_widget_add_css_class(a->tabs, "tabs");
    a->stack = gtk_stack_new();
    gtk_widget_set_vexpand(a->stack, TRUE);
    gtk_box_append(GTK_BOX(root), a->tabs);
    gtk_box_append(GTK_BOX(root), a->stack);
    gtk_window_set_child(GTK_WINDOW(a->window), root);

    new_tab(a, HOME_URL);
    gtk_window_present(GTK_WINDOW(a->window));
}

int main(int argc, char **argv) {
    GtkApplication *a = gtk_application_new("com.lionapp1.Nexo", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(a, "activate", G_CALLBACK(activate), NULL);
    int r = g_application_run(G_APPLICATION(a), argc, argv);
    g_object_unref(a);
    return r;
}
