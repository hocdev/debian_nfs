#include "shares.h"

#include <glib/gi18n-lib.h>
#include <string.h>
#include <time.h>

static GHashTable *path_share_info_hash = NULL;
static GHashTable *share_name_share_info_hash = NULL;

static gboolean debug_error_on_refresh = FALSE;
static gboolean debug_error_on_add = FALSE;
static gboolean debug_error_on_modify = FALSE;
static gboolean debug_error_on_remove = FALSE;

static int refresh_countdown = 0;
static time_t last_refresh_time = 0;

static void ensure_hashes(void) {
    if (path_share_info_hash == NULL) {
        g_assert(share_name_share_info_hash == NULL);
        path_share_info_hash = g_hash_table_new(g_str_hash, g_str_equal);
        share_name_share_info_hash = g_hash_table_new(g_str_hash, g_str_equal);
    } else {
        g_assert(share_name_share_info_hash != NULL);
    }
}

static ShareInfo *copy_share_info(const ShareInfo *info) {
    if (info == NULL)
        return NULL;
    ShareInfo *copy = g_new(ShareInfo, 1);
    copy->path = g_strdup(info->path);
    copy->share_name = g_strdup(info->share_name);
    copy->comment = g_strdup(info->comment);
    copy->is_writable = info->is_writable;
    copy->guest_ok = info->guest_ok;
    return copy;
}

static gboolean remove_from_path_hash_cb(gpointer key, gpointer value, gpointer user_data) {
    (void)key;
    (void)user_data;
    shares_free_share_info((ShareInfo *)value);
    return TRUE;
}

static gboolean remove_from_name_hash_cb(gpointer key, gpointer value, gpointer user_data) {
    (void)key;
    (void)value;
    (void)user_data;
    return TRUE;
}

static char *get_string_from_key_file(GKeyFile *kf, const char *group, const char *key) {
    GError *err = NULL;
    if (!g_key_file_has_key(kf, group, key, &err)) {
        g_assert(!g_error_matches(err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND));
        if (err) g_error_free(err);
        return NULL;
    }
    char *val = g_key_file_get_string(kf, group, key, &err);
    if (val != NULL)
        return val;
    g_assert(!g_error_matches(err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_NOT_FOUND) &&
             !g_error_matches(err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND));
    if (err) g_error_free(err);
    return NULL;
}

static gboolean net_usershare_run(int argc, const char **argv, GKeyFile **ret_kf, GError **error) {
    if (ret_kf != NULL)
        *ret_kf = NULL;

    char **full_argv = g_new(char *, argc + 3);
    full_argv[0] = "net";
    full_argv[1] = "usershare";
    for (int i = 0; i < argc; i++) {
        g_assert(argv[i] != NULL);
        full_argv[i + 2] = (char *)argv[i];
    }
    full_argv[argc + 2] = NULL;

    char *stdout_buf = NULL;
    char *stderr_buf = NULL;
    int exit_status = 0;
    GError *spawn_err = NULL;

    gboolean ok = g_spawn_sync(NULL, full_argv, NULL, G_SPAWN_SEARCH_PATH,
                               NULL, NULL, &stdout_buf, &stderr_buf, &exit_status, &spawn_err);
    if (!ok) {
        g_propagate_error(error, spawn_err);
        g_free(full_argv);
        g_free(stdout_buf);
        g_free(stderr_buf);
        return FALSE;
    }

    gboolean ret = FALSE;
    if (WIFEXITED(exit_status)) {
        int code = WEXITSTATUS(exit_status);
        if (code == 0) {
            ret = TRUE;
            if (ret_kf != NULL) {
                if (!g_utf8_validate(stdout_buf, -1, NULL)) {
                    g_set_error(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "%s",
                                g_dgettext("nemo-share", "the output of 'net usershare' is not in valid UTF-8 encoding"));
                    ret = FALSE;
                } else {
                    GKeyFile *kf = g_key_file_new();
                    GError *kf_err = NULL;
                    if (!g_key_file_load_from_data(kf, stdout_buf, -1, G_KEY_FILE_NONE, &kf_err)) {
                        g_propagate_error(error, kf_err);
                        g_key_file_free(kf);
                        ret = FALSE;
                    } else {
                        *ret_kf = kf;
                    }
                }
            }
        } else {
            char *err_utf8 = stderr_buf ? g_locale_to_utf8(stderr_buf, -1, NULL, NULL, NULL) : NULL;
            char *msg;
            if (err_utf8 == NULL || *err_utf8 == '\0')
                msg = g_strdup_printf(g_dgettext("nemo-share", "'net usershare' returned error %d"), code);
            else
                msg = g_strdup_printf(g_dgettext("nemo-share", "'net usershare' returned error %d: %s"), code, err_utf8);
            g_free(err_utf8);
            g_set_error(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "%s", msg);
            g_free(msg);
        }
    } else if (WIFSIGNALED(exit_status)) {
        g_set_error(error, shares_error_quark(), SHARES_ERROR_FAILED,
                    g_dgettext("nemo-share", "%s %s %s returned with signal %d"),
                    full_argv[0], full_argv[1], full_argv[2], WTERMSIG(exit_status));
    } else {
        g_set_error(error, shares_error_quark(), SHARES_ERROR_FAILED,
                    g_dgettext("nemo-share", "%s %s %s failed for an unknown reason"),
                    full_argv[0], full_argv[1], full_argv[2]);
    }

    g_free(full_argv);
    g_free(stdout_buf);
    g_free(stderr_buf);
    return ret;
}

static void replace_shares_from_key_file(GKeyFile *kf) {
    gsize num_groups = 0;
    char **groups = g_key_file_get_groups(kf, &num_groups);
    for (gsize i = 0; i < num_groups; i++) {
        const char *group = groups[i];
        g_assert(group != NULL);
        ensure_hashes();
        ShareInfo *old = g_hash_table_lookup(share_name_share_info_hash, group);
        if (old != NULL) {
            g_hash_table_remove(path_share_info_hash, old->path);
            g_hash_table_remove(share_name_share_info_hash, old->share_name);
            shares_free_share_info(old);
        }

        char *path = get_string_from_key_file(kf, group, "path");
        if (path == NULL)
            continue;

        old = g_hash_table_lookup(path_share_info_hash, path);
        if (old != NULL) {
            g_hash_table_remove(path_share_info_hash, old->path);
            g_hash_table_remove(share_name_share_info_hash, old->share_name);
            shares_free_share_info(old);
        }

        char *comment = get_string_from_key_file(kf, group, "comment");
        char *acl = get_string_from_key_file(kf, group, "usershare_acl");
        gboolean is_writable = FALSE;
        if (acl != NULL) {
            if (strstr(acl, "Everyone:R") == NULL && strstr(acl, "Everyone:F") != NULL)
                is_writable = TRUE;
            g_free(acl);
        }

        char *guest_str = get_string_from_key_file(kf, group, "guest_ok");
        gboolean guest_ok = FALSE;
        if (guest_str != NULL) {
            if (guest_str[0] == 'y' && guest_str[1] == '\0')
                guest_ok = TRUE;
            g_free(guest_str);
        }

        ShareInfo *info = g_new(ShareInfo, 1);
        info->path = path;
        info->share_name = g_strdup(group);
        info->comment = comment;
        info->is_writable = is_writable;
        info->guest_ok = guest_ok;

        ensure_hashes();
        g_hash_table_insert(path_share_info_hash, info->path, info);
        g_hash_table_insert(share_name_share_info_hash, info->share_name, info);
    }
    g_strfreev(groups);
}

static gboolean refresh_shares(GError **error) {
    ensure_hashes();
    g_hash_table_foreach_remove(path_share_info_hash, remove_from_path_hash_cb, NULL);
    g_hash_table_foreach_remove(share_name_share_info_hash, remove_from_name_hash_cb, NULL);

    if (debug_error_on_refresh) {
        g_set_error(error, shares_error_quark(), SHARES_ERROR_FAILED, "%s",
                    g_dgettext("nemo-share", "Failed"));
        return FALSE;
    }

    const char *argv[] = { "info" };
    GKeyFile *kf = NULL;
    if (!net_usershare_run(1, argv, &kf, error))
        return FALSE;
    g_assert(kf != NULL);
    replace_shares_from_key_file(kf);
    g_key_file_free(kf);
    return TRUE;
}

static gboolean refresh_if_needed(GError **error) {
    if (refresh_countdown == 0) {
        refresh_countdown = 100;
        time_t now = time(NULL);
        if (now - last_refresh_time >= 11) {
            gboolean ok = refresh_shares(error);
            last_refresh_time = now;
            return ok;
        }
        last_refresh_time = now;
    } else {
        refresh_countdown--;
    }
    return TRUE;
}

static gboolean add_share(ShareInfo *info, GError **error) {
    if (debug_error_on_add) {
        g_set_error(error, shares_error_quark(), SHARES_ERROR_FAILED, "%s",
                    g_dgettext("nemo-share", "Failed"));
        return FALSE;
    }
    gboolean supports_guest = FALSE;
    if (!shares_supports_guest_ok(&supports_guest, error))
        return FALSE;

    char *acl = info->is_writable ? "Everyone:F" : g_strdup_printf("Everyone:R,%s:F", g_get_user_name());
    const char *argv[7];
    argv[0] = "add";
    argv[1] = "--long";
    argv[2] = info->share_name;
    argv[3] = info->path;
    argv[4] = info->comment ? info->comment : "";
    argv[5] = acl;
    int argc = 6;
    if (supports_guest) {
        argv[6] = info->guest_ok ? "guest_ok=y" : "guest_ok=n";
        argc = 7;
    }

    GKeyFile *kf = NULL;
    gboolean ok = net_usershare_run(argc, argv, &kf, error);
    if (!info->is_writable)
        g_free(acl);
    if (!ok)
        return FALSE;

    replace_shares_from_key_file(kf);
    if (kf) g_key_file_free(kf);

    ShareInfo *copy = copy_share_info(info);
    ensure_hashes();
    g_hash_table_insert(path_share_info_hash, copy->path, copy);
    g_hash_table_insert(share_name_share_info_hash, copy->share_name, copy);
    return TRUE;
}

static gboolean remove_share(const char *path, GError **error) {
    ensure_hashes();
    ShareInfo *old = g_hash_table_lookup(path_share_info_hash, path);
    if (old == NULL) {
        char *disp = g_filename_display_name(path);
        g_set_error(error, shares_error_quark(), SHARES_ERROR_NONEXISTENT,
                    g_dgettext("nemo-share", "Cannot remove the share for path %s: that path is not shared"),
                    disp);
        g_free(disp);
        return FALSE;
    }

    const char *argv[] = { "delete", old->share_name };
    if (!net_usershare_run(2, argv, NULL, error))
        return FALSE;

    ensure_hashes();
    g_hash_table_remove(path_share_info_hash, old->path);
    g_hash_table_remove(share_name_share_info_hash, old->share_name);
    shares_free_share_info(old);
    return TRUE;
}

GQuark shares_error_quark(void) {
    static GQuark quark = 0;
    if (quark == 0)
        quark = g_quark_from_string("nemo-shares-error-quark");
    return quark;
}

gboolean shares_supports_guest_ok(gboolean *supports_guest_ok_ret, GError **error) {
    *supports_guest_ok_ret = FALSE;
    char *stdout_buf = NULL;
    char *stderr_buf = NULL;
    int exit_status = 0;

    if (!g_spawn_command_line_sync("testparm -s --parameter-name='usershare allow guests'",
                                   &stdout_buf, &stderr_buf, &exit_status, error)) {
        return FALSE;
    }

    gboolean ret = FALSE;
    if (WIFEXITED(exit_status)) {
        int code = WEXITSTATUS(exit_status);
        if (code == 0) {
            ret = TRUE;
            *supports_guest_ok_ret = (stdout_buf && g_ascii_strncasecmp(stdout_buf, "Yes", 3) == 0);
        } else {
            char *err_utf8 = stderr_buf ? g_locale_to_utf8(stderr_buf, -1, NULL, NULL, NULL) : NULL;
            char *msg = (err_utf8 == NULL || *err_utf8 == '\0')
                ? g_strdup_printf(g_dgettext("nemo-share", "Samba's testparm returned error %d"), code)
                : g_strdup_printf(g_dgettext("nemo-share", "Samba's testparm returned error %d: %s"), code, err_utf8);
            g_free(err_utf8);
            g_set_error(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "%s", msg);
            g_free(msg);
        }
    } else if (WIFSIGNALED(exit_status)) {
        g_set_error(error, shares_error_quark(), SHARES_ERROR_FAILED,
                    g_dgettext("nemo-share", "Samba's testparm returned with signal %d"),
                    WTERMSIG(exit_status));
    } else {
        g_set_error(error, shares_error_quark(), SHARES_ERROR_FAILED, "%s",
                    g_dgettext("nemo-share", "Samba's testparm failed for an unknown reason"));
    }

    g_free(stdout_buf);
    g_free(stderr_buf);
    return ret;
}

void shares_free_share_info(ShareInfo *info) {
    g_assert(info != NULL);
    g_free(info->path);
    g_free(info->share_name);
    g_free(info->comment);
    g_free(info);
}

gboolean shares_get_path_is_shared(const char *path, gboolean *ret_is_shared, GError **error) {
    g_assert(ret_is_shared != NULL);
    g_assert(error == NULL || *error == NULL);
    if (!refresh_if_needed(error)) {
        *ret_is_shared = FALSE;
        return FALSE;
    }
    ensure_hashes();
    *ret_is_shared = (g_hash_table_lookup(path_share_info_hash, path) != NULL);
    return TRUE;
}

gboolean shares_get_share_info_for_path(const char *path, ShareInfo **ret_share_info, GError **error) {
    g_assert(path != NULL);
    g_assert(ret_share_info != NULL);
    g_assert(error == NULL || *error == NULL);
    if (!refresh_if_needed(error)) {
        *ret_share_info = NULL;
        return FALSE;
    }
    ensure_hashes();
    *ret_share_info = copy_share_info(g_hash_table_lookup(path_share_info_hash, path));
    return TRUE;
}

gboolean shares_get_share_name_exists(const char *share_name, gboolean *ret_exists, GError **error) {
    g_assert(share_name != NULL);
    g_assert(ret_exists != NULL);
    g_assert(error == NULL || *error == NULL);
    if (!refresh_if_needed(error)) {
        *ret_exists = FALSE;
        return FALSE;
    }
    ensure_hashes();
    *ret_exists = (g_hash_table_lookup(share_name_share_info_hash, share_name) != NULL);
    return TRUE;
}

gboolean shares_get_share_info_for_share_name(const char *share_name, ShareInfo **ret_share_info, GError **error) {
    g_assert(share_name != NULL);
    g_assert(ret_share_info != NULL);
    g_assert(error == NULL || *error == NULL);
    if (!refresh_if_needed(error)) {
        *ret_share_info = NULL;
        return FALSE;
    }
    ensure_hashes();
    *ret_share_info = copy_share_info(g_hash_table_lookup(share_name_share_info_hash, share_name));
    return TRUE;
}

gboolean shares_modify_share(const char *old_path, ShareInfo *info, GError **error) {
    g_assert((old_path == NULL && info != NULL) ||
             (old_path != NULL && info == NULL) ||
             (old_path != NULL && info != NULL));
    g_assert(error == NULL || *error == NULL);

    if (!refresh_if_needed(error))
        return FALSE;

    if (old_path == NULL)
        return add_share(info, error);

    if (info == NULL) {
        if (debug_error_on_remove) {
            g_set_error(error, shares_error_quark(), SHARES_ERROR_FAILED, "Failed");
            return FALSE;
        }
        return remove_share(old_path, error);
    }

    ensure_hashes();
    ShareInfo *old = g_hash_table_lookup(path_share_info_hash, old_path);
    if (old == NULL)
        return add_share(info, error);

    if (strcmp(info->path, old->path) != 0) {
        g_set_error(error, shares_error_quark(), SHARES_ERROR_FAILED, "%s",
                    g_dgettext("nemo-share", "Cannot change the path of an existing share; please remove the old share first and add a new one"));
        return FALSE;
    }
    if (debug_error_on_modify) {
        g_set_error(error, shares_error_quark(), SHARES_ERROR_FAILED, "Failed");
        return FALSE;
    }
    if (debug_error_on_remove) {
        g_set_error(error, shares_error_quark(), SHARES_ERROR_FAILED, "Failed");
        return FALSE;
    }
    if (!remove_share(old_path, error))
        return FALSE;
    return add_share(info, error);
}

static void collect_share_info_cb(gpointer key, gpointer value, gpointer user_data) {
    (void)key;
    GSList **list = (GSList **)user_data;
    *list = g_slist_prepend(*list, copy_share_info((const ShareInfo *)value));
}

gboolean shares_get_share_info_list(GSList **ret_info_list, GError **error) {
    g_assert(ret_info_list != NULL);
    g_assert(error == NULL || *error == NULL);
    if (!refresh_if_needed(error)) {
        *ret_info_list = NULL;
        return FALSE;
    }
    *ret_info_list = NULL;
    ensure_hashes();
    g_hash_table_foreach(path_share_info_hash, collect_share_info_cb, ret_info_list);
    return TRUE;
}

void shares_free_share_info_list(GSList *list) {
    for (GSList *l = list; l != NULL; l = l->next)
        shares_free_share_info((ShareInfo *)l->data);
    g_slist_free(list);
}

void shares_set_debug(gboolean error_on_refresh, gboolean error_on_add,
                      gboolean error_on_modify, gboolean error_on_remove) {
    debug_error_on_refresh = error_on_refresh;
    debug_error_on_add = error_on_add;
    debug_error_on_modify = error_on_modify;
    debug_error_on_remove = error_on_remove;
}
