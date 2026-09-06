#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <gio/gio.h>
#include <string.h>

#define HOME_URL "https://www.google.com/"
#define SEARCH_URL "https://www.google.com/search?q="

typedef struct {
    GtkWidget *window;
    GtkWidget *header;
    GtkWidget *back;
    GtkWidget *forward;
    GtkWidget *reload;
    GtkWidget *stop;
    GtkWidget *address;
    GtkWidget *new_tab;
    GtkWidget *tabs;
    GtkWidget *content;
    GPtrArray *views;
} NexoApp;

typedef struct {
    NexoApp *app;
    GtkWidget *page;
    GtkWidget *tab_button;
    GtkWidget *label;
    GtkWidget *close;
    WebKitWebView *view;
} NexoTab;

static NexoTab *current_tab(NexoApp *app) {
    guint index = gtk_stack_get_visible_child_name(GTK_STACK(app->content))
        ? GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(app->content), "nexo-current")) : 0;
    if (index >= app->views->len) index = 0;
    return app->views->len ? g_ptr_array_index(app->views, index) : NULL;
}

static guint tab_index(NexoApp *app, NexoTab *tab) {
    for (guint i = 0; i < app->views->len; i++)
        if (g_ptr_array_index(app->views, i) == tab) return i;
    return 0;
}

static void update_nav(NexoApp *app) {
    NexoTab *tab = current_tab(app);
    if (!tab) return;
    gtk_widget_set_sensitive(app->back, webkit_web_view_can_go_back(tab->view));
    gtk_widget_set_sensitive(app->forward, webkit_web_view_can_go_forward(tab->view));
    gboolean loading = webkit_web_view_is_loading(tab->view);
    gtk_widget_set_visible(app->stop, loading);
    gtk_widget_set_visible(app->reload, !loading);
}

static gchar *display_address(const gchar *uri) {
    if (!uri || !*uri) return g_strdup("");
    if (g_str_has_prefix(uri, "https://")) return g_strdup(uri + 8);
    if (g_str_has_prefix(uri, "http://")) return g_strdup(uri + 7);
    return g_strdup(uri);
}

static gchar *url_from_text(const gchar *text) {
    gchar *trim = g_strstrip(g_strdup(text ? text : ""));
    if (!*trim) { g_free(trim); return g_strdup(HOME_URL); }
    if (g_uri_parse(trim, G_URI_FLAGS_NONE, NULL)) return trim;
    gchar *escaped = g_uri_escape_string(trim, NULL, FALSE);
    gchar *url = g_strconcat(SEARCH_URL, escaped, NULL);
    g_free(escaped); g_free(trim);
    return url;
}

static void load_address(NexoApp *app) {
    NexoTab *tab = current_tab(app);
    if (!tab) return;
    const gchar *text = gtk_editable_get_text(GTK_EDITABLE(app->address));
    gchar *url = url_from_text(text);
    webkit_web_view_load_uri(tab->view, url);
    g_free(url);
}

static void on_address_activate(GtkEntry *entry, gpointer data) { (void)entry; load_address(data); }
static void on_back(GtkButton *b, gpointer data) { (void)b; NexoTab *t=current_tab(data); if(t) webkit_web_view_go_back(t->view); }
static void on_forward(GtkButton *b, gpointer data) { (void)b; NexoTab *t=current_tab(data); if(t) webkit_web_view_go_forward(t->view); }
static void on_reload(GtkButton *b, gpointer data) { (void)b; NexoTab *t=current_tab(data); if(t) webkit_web_view_reload(t->view); }
static void on_stop(GtkButton *b, gpointer data) { (void)b; NexoTab *t=current_tab(data); if(t) webkit_web_view_stop_loading(t->view); }

static void set_current(NexoApp *app, guint index) {
    if (!app->views->len || index >= app->views->len) return;
    NexoTab *tab = g_ptr_array_index(app->views, index);
    gchar *name = g_strdup_printf("tab-%u", index);
    gtk_stack_set_visible_child_name(GTK_STACK(app->content), name);
    g_object_set_data(G_OBJECT(app->content), "nexo-current", GUINT_TO_POINTER(index));
    gtk_editable_set_text(GTK_EDITABLE(app->address), webkit_web_view_get_uri(tab->view) ?: "");
    g_free(name);
    update_nav(app);
}

static void on_tab_clicked(GtkButton *button, gpointer data) {
    NexoApp *app = data;
    NexoTab *tab = g_object_get_data(G_OBJECT(button), "nexo-tab");
    set_current(app, tab_index(app, tab));
}

static void on_tab_close(GtkButton *button, gpointer data) {
    NexoApp *app = data;
    NexoTab *tab = g_object_get_data(G_OBJECT(button), "nexo-tab");
    guint index = tab_index(app, tab);
    if (app->views->len <= 1) { webkit_web_view_load_uri(tab->view, HOME_URL); return; }
    gchar *name = g_strdup_printf("tab-%u", index);
    gtk_stack_remove(GTK_STACK(app->content), tab->page);
    gtk_box_remove(GTK_BOX(app->tabs), tab->tab_button);
    g_ptr_array_remove_index(app->views, index);
    g_free(name);
    for (guint i=0;i<app->views->len;i++) {
        NexoTab *t=g_ptr_array_index(app->views,i);
        gchar *new_name=g_strdup_printf("tab-%u",i);
        gtk_stack_set_child_name(GTK_STACK(app->content), t->page, new_name);
        g_free(new_name);
    }
    set_current(app, index >= app->views->len ? app->views->len-1 : index);
    g_free(tab);
}

static void update_title(NexoTab *tab) {
    const gchar *title = webkit_web_view_get_title(tab->view);
    if (!title || !*title) title = "Yeni Sekme";
    gchar *short_title = g_utf8_substring(title, 0, MIN((gint)g_utf8_strlen(title, -1), 24));
    gtk_label_set_text(GTK_LABEL(tab->label), short_title);
    g_free(short_title);
}

static void on_title(GObject *object, GParamSpec *pspec, gpointer data) { (void)pspec; (void)object; update_title(data); }
static void on_uri(GObject *object, GParamSpec *pspec, gpointer data) {
    (void)pspec;
    NexoTab *tab=data; NexoApp *app=tab->app;
    if (current_tab(app) == tab) {
        gchar *shown=display_address(webkit_web_view_get_uri(tab->view));
        gtk_editable_set_text(GTK_EDITABLE(app->address), shown);
        gtk_editable_set_position(GTK_EDITABLE(app->address), -1);
        g_free(shown);
    }
}
static void on_load_changed(WebKitWebView *view, WebKitLoadEvent event, gpointer data) {
    NexoTab *tab=data; NexoApp *app=tab->app;
    if (event == WEBKIT_LOAD_FINISHED || event == WEBKIT_LOAD_COMMITTED) on_uri(G_OBJECT(view), NULL, tab);
    if (event == WEBKIT_LOAD_FINISHED || event == WEBKIT_LOAD_STARTED) update_nav(app);
}

static void on_new_tab(GtkButton *button, gpointer data);
static gboolean on_decide_policy(WebKitWebView *view, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer data) {
    NexoApp *app=data;
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        WebKitNavigationPolicyDecision *nav = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(nav);
        guint mods = webkit_navigation_action_get_modifiers(action);
        WebKitURIRequest *request = webkit_navigation_action_get_request(action);
        if ((mods & GDK_CONTROL_MASK) && webkit_navigation_action_get_navigation_type(action) == WEBKIT_NAVIGATION_TYPE_LINK_ACTIVATED) {
            const gchar *uri=webkit_uri_request_get_uri(request);
            on_new_tab(NULL, app);
            NexoTab *tab=current_tab(app); if(tab) webkit_web_view_load_uri(tab->view, uri);
            webkit_policy_decision_ignore(decision); return TRUE;
        }
    }
    if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        WebKitNavigationPolicyDecision *nav = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(nav);
        WebKitURIRequest *request = webkit_navigation_action_get_request(action);
        on_new_tab(NULL, app);
        NexoTab *tab=current_tab(app); if(tab) webkit_web_view_load_uri(tab->view, webkit_uri_request_get_uri(request));
        webkit_policy_decision_ignore(decision); return TRUE;
    }
    return FALSE;
}

static NexoTab *create_tab(NexoApp *app, const gchar *url) {
    NexoTab *tab=g_new0(NexoTab,1); tab->app=app;
    tab->view=WEBKIT_WEB_VIEW(webkit_web_view_new());
    WebKitSettings *settings=webkit_web_view_get_settings(tab->view);
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_enable_media(settings, TRUE);
    webkit_settings_set_enable_webaudio(settings, TRUE);
    webkit_settings_set_enable_webgl(settings, TRUE);
    webkit_settings_set_enable_smooth_scrolling(settings, TRUE);
    webkit_settings_set_enable_developer_extras(settings, TRUE);
    webkit_settings_set_enable_dns_prefetching(settings, TRUE);

    GtkWidget *scroller=gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), GTK_WIDGET(tab->view));
    tab->page=scroller;

    gchar *name=g_strdup_printf("tab-%u", app->views->len);
    gtk_stack_add_named(GTK_STACK(app->content), tab->page, name); g_free(name);

    tab->tab_button=gtk_button_new();
    gtk_widget_add_css_class(tab->tab_button,"flat");
    GtkWidget *tab_box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,4);
    tab->label=gtk_label_new("Yeni Sekme"); gtk_label_set_ellipsize(GTK_LABEL(tab->label),PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(tab->label,TRUE);
    tab->close=gtk_button_new_from_icon_name("window-close-symbolic"); gtk_widget_add_css_class(tab->close,"flat"); gtk_widget_set_tooltip_text(tab->close,"Sekmeyi kapat");
    gtk_box_append(GTK_BOX(tab_box),tab->label); gtk_box_append(GTK_BOX(tab_box),tab->close); gtk_button_set_child(GTK_BUTTON(tab->tab_button),tab_box);
    gtk_box_append(GTK_BOX(app->tabs),tab->tab_button);
    g_object_set_data(G_OBJECT(tab->tab_button),"nexo-tab",tab); g_object_set_data(G_OBJECT(tab->close),"nexo-tab",tab);
    g_signal_connect(tab->tab_button,"clicked",G_CALLBACK(on_tab_clicked),app);
    g_signal_connect(tab->close,"clicked",G_CALLBACK(on_tab_close),app);
    g_signal_connect(tab->view,"notify::title",G_CALLBACK(on_title),tab);
    g_signal_connect(tab->view,"notify::uri",G_CALLBACK(on_uri),tab);
    g_signal_connect(tab->view,"load-changed",G_CALLBACK(on_load_changed),tab);
    g_signal_connect(tab->view,"decide-policy",G_CALLBACK(on_decide_policy),app);
    g_ptr_array_add(app->views,tab);
    webkit_web_view_load_uri(tab->view,url ? url : HOME_URL);
    return tab;
}

static void on_new_tab(GtkButton *button, gpointer data) {
    (void)button; NexoApp *app=data;
    create_tab(app, HOME_URL);
    set_current(app, app->views->len-1);
}

static void apply_css(void) {
    GtkCssProvider *css=gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "headerbar { min-height: 54px; padding: 6px 10px; }"
        ".nav-button { min-width: 36px; min-height: 36px; padding: 0; }"
        ".address { min-height: 38px; border-radius: 20px; padding: 0 14px; }"
        ".tabs { padding: 3px 8px 0 8px; }"
        ".tab-strip button { min-height: 34px; padding: 4px 9px; }"
        ".tab-strip label { max-width: 180px; }",
        -1);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

static void activate(GtkApplication *gtk_app, gpointer user_data) {
    (void)user_data;
    NexoApp *app=g_new0(NexoApp,1); app->views=g_ptr_array_new();
    app->window=gtk_application_window_new(gtk_app); gtk_window_set_default_size(GTK_WINDOW(app->window),1280,800); gtk_window_set_title(GTK_WINDOW(app->window),"Nexo Browser");
    apply_css();

    app->header=gtk_header_bar_new(); gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(app->header),TRUE); gtk_window_set_titlebar(GTK_WINDOW(app->window),app->header);
    GtkWidget *nav=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    app->back=gtk_button_new_from_icon_name("go-previous-symbolic"); app->forward=gtk_button_new_from_icon_name("go-next-symbolic"); app->reload=gtk_button_new_from_icon_name("view-refresh-symbolic"); app->stop=gtk_button_new_from_icon_name("process-stop-symbolic");
    GtkWidget *home=gtk_button_new_from_icon_name("go-home-symbolic");
    GtkWidget *newtab=gtk_button_new_from_icon_name("tab-new-symbolic"); app->new_tab=newtab;
    for(GtkWidget *w: (GtkWidget*[]){app->back,app->forward,app->reload,app->stop,home,newtab}) { gtk_widget_add_css_class(w,"nav-button"); gtk_widget_set_focus_on_click(w,FALSE); }
    gtk_widget_set_visible(app->stop,FALSE);
    gtk_widget_set_tooltip_text(app->back,"Geri"); gtk_widget_set_tooltip_text(app->forward,"İleri"); gtk_widget_set_tooltip_text(app->reload,"Yenile"); gtk_widget_set_tooltip_text(home,"Ana sayfa"); gtk_widget_set_tooltip_text(newtab,"Yeni sekme");
    gtk_box_append(GTK_BOX(nav),app->back); gtk_box_append(GTK_BOX(nav),app->forward); gtk_box_append(GTK_BOX(nav),app->reload); gtk_box_append(GTK_BOX(nav),app->stop); gtk_box_append(GTK_BOX(nav),home);
    app->address=GTK_WIDGET(gtk_search_entry_new()); gtk_widget_add_css_class(app->address,"address"); gtk_widget_set_hexpand(app->address,TRUE); gtk_widget_set_tooltip_text(app->address,"Adres veya arama yazın"); gtk_box_append(GTK_BOX(nav),app->address); gtk_box_append(GTK_BOX(nav),newtab);
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(app->header),nav);
    g_signal_connect(app->back,"clicked",G_CALLBACK(on_back),app); g_signal_connect(app->forward,"clicked",G_CALLBACK(on_forward),app); g_signal_connect(app->reload,"clicked",G_CALLBACK(on_reload),app); g_signal_connect(app->stop,"clicked",G_CALLBACK(on_stop),app); g_signal_connect(app->new_tab,"clicked",G_CALLBACK(on_new_tab),app);
    g_signal_connect(home,"clicked",G_CALLBACK(+[](GtkButton*b,gpointer d){(void)b;NexoTab*t=current_tab(d);if(t)webkit_web_view_load_uri(t->view,HOME_URL);}),app);
    g_signal_connect(app->address,"activate",G_CALLBACK(on_address_activate),app);

    GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); app->tabs=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,3); gtk_widget_add_css_class(app->tabs,"tabs"); gtk_widget_add_css_class(app->tabs,"tab-strip");
    app->content=gtk_stack_new(); gtk_widget_set_vexpand(app->content,TRUE); gtk_box_append(GTK_BOX(root),app->tabs); gtk_box_append(GTK_BOX(root),app->content); gtk_window_set_child(GTK_WINDOW(app->window),root);
    create_tab(app,HOME_URL); set_current(app,0); gtk_window_present(GTK_WINDOW(app->window));
}

int main(int argc,char **argv){
    GtkApplication *app=gtk_application_new("com.lionapp1.Nexo",G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app,"activate",G_CALLBACK(activate),NULL);
    int status=g_application_run(G_APPLICATION(app),argc,argv); g_object_unref(app); return status;
}
