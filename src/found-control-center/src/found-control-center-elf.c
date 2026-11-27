#include <elf.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

typedef struct {
    gsize file_offset;
    guint64 address;
    gsize count;
    gsize entry_size;
} PanelTable;

typedef gboolean (*Patch)(guint8 *elf, const char *resources, GError **error);

typedef struct {
    const char *path;
    const char *original_sha256;
    const char *patched_sha256;
    Patch patch;
} Executable;

static gboolean
check_sha256(const char *path, const guint8 *data, gsize size,
             const char *expected, GError **error)
{
    g_autofree char *actual = g_compute_checksum_for_data(G_CHECKSUM_SHA256, data, size);
    if (g_str_equal(actual, expected))
        return TRUE;

    g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                "%s: SHA-256 %s, expected %s", path, actual, expected);
    return FALSE;
}

static void
replace_brand(guint8 *elf, const gsize *offsets, gsize count)
{
    for (gsize i = 0; i < count; i++)
        memcpy(elf + offsets[i], "vos", 3);
}

static gboolean
replace_ui(guint8 *elf, const char *resources, const char *name,
           gsize offset, gsize slot_size, gboolean pad, GError **error)
{
    g_autofree char *path = g_build_filename(resources, name, NULL);
    g_autofree char *data = NULL;
    gsize size;
    if (!g_file_get_contents(path, &data, &size, error))
        return FALSE;

    if (pad) {
        /* Preserve the resource's stored length and terminal newline. */
        while (size > 0 && (data[size - 1] == '\r' || data[size - 1] == '\n'))
            size--;
    }
    if ((pad && size >= slot_size) || (!pad && size != slot_size)) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "%s: data does not fit the %zu-byte resource slot", path, slot_size);
        return FALSE;
    }

    memcpy(elf + offset, data, size);
    if (pad) {
        memset(elf + offset + size, ' ', slot_size - size - 1);
        elf[offset + slot_size - 1] = '\n';
    }
    return TRUE;
}

static void
remove_contact_panel(guint8 *elf, const PanelTable *tables, gsize table_count,
                     gsize rela_offset, gsize rela_size, gsize relacount_offset)
{
    for (gsize i = 0; i < table_count; i++) {
        const PanelTable *table = &tables[i];
        guint8 *entry = elf + table->file_offset;
        gsize remaining = (table->count - 1) * table->entry_size;
        memmove(entry, entry + table->entry_size, remaining);
        memset(entry + remaining, 0, table->entry_size);
    }

    /* The panel slots use R_X86_64_RELATIVE. Move their relocation targets
     * with the slots and discard the relocations for the removed entry. */
    guint8 *start = elf + rela_offset;
    gsize written = 0;
    guint64 removed = 0;
    for (gsize read = 0; read < rela_size; read += sizeof(Elf64_Rela)) {
        Elf64_Rela relocation;
        memcpy(&relocation, start + read, sizeof(relocation));
        guint64 address = GUINT64_FROM_LE(relocation.r_offset);
        gboolean discard = FALSE;
        for (gsize i = 0; i < table_count; i++) {
            const PanelTable *table = &tables[i];
            guint64 end = table->address + table->count * table->entry_size;
            if (address < table->address || address >= end)
                continue;

            discard = address < table->address + table->entry_size;
            relocation.r_offset = GUINT64_TO_LE(address - table->entry_size);
            break;
        }

        if (discard) {
            removed++;
            continue;
        }

        memcpy(start + written, &relocation, sizeof(relocation));
        written += sizeof(relocation);
    }

    /* Keep DT_RELASZ fixed and update DT_RELACOUNT. The trailing records
     * become R_X86_64_NONE because the section still occupies the same bytes. */
    memset(start + written, 0, rela_size - written);
    guint64 relative_count;
    memcpy(&relative_count, elf + relacount_offset, sizeof(relative_count));
    relative_count = GUINT64_TO_LE(GUINT64_FROM_LE(relative_count) - removed);
    memcpy(elf + relacount_offset, &relative_count, sizeof(relative_count));
}

static gboolean
patch_control_center(guint8 *elf, const char *resources, GError **error)
{
    static const PanelTable tables[] = {
        { 0x4af0c8, 0x4b00c8, 3, 8 },
        { 0x4b81b8, 0x4b91b8, 20, 24 },
    };
    static const gsize brand_offsets[] = {
        0x15cdd2, 0x15cf03, 0x15d568, 0x15d57e, 0x15d593, 0x15d6ad,
        0x161653, 0x161739, 0x16184b, 0x16185e, 0x1628a9, 0x1685e4,
        0x17005b, 0x17006c, 0x17605d, 0x176077, 0x176087, 0x17655c, 0x1765c6,
    };

    /* The sidebar loops compare against a constant. The panel registry
     * keeps its own count in .data. */
    elf[0x5976f] = 39;
    elf[0x597a7] = 39;
    elf[0x4b8060] = 28;
    memset(elf + 0x153a59, 0, sizeof("info-contact") - 1);
    remove_contact_panel(elf, tables, G_N_ELEMENTS(tables), 0x34558, 0x95d0, 0x4b33a0);

    /* Point g_strconcat at the empty string after its original separator. */
    elf[0x895b9]++;
    replace_brand(elf, brand_offsets, G_N_ELEMENTS(brand_offsets));

    char *branded_string = (char *) elf + 0x17654b;
    gsize old_length = strlen(branded_string);
    gsize old_prefix = sizeof("nfschina") - 1;
    gsize new_prefix = sizeof("VOSViet") - 1;
    memmove(branded_string + new_prefix, branded_string + old_prefix,
            old_length - old_prefix + 1);
    memcpy(branded_string, "VOSViet", new_prefix);
    branded_string[old_length] = '\0';

    return replace_ui(elf, resources, "cc-info-contact-panel.ui",
                      0x291d28, 15726, TRUE, error) &&
           replace_ui(elf, resources, "online-accounts.ui",
                      0x3040b0, 14493, FALSE, error);
}

static gboolean
patch_search_provider(guint8 *elf, const char *resources G_GNUC_UNUSED,
                      GError **error G_GNUC_UNUSED)
{
    static const PanelTable tables[] = { { 0x16108, 0x17108, 20, 8 } };
    static const gsize brand_offsets[] = { 0xb958, 0xb9b4 };

    elf[0x16048] = 28;
    memset(elf + 0xb621, 0, sizeof("info-contact") - 1);
    remove_contact_panel(elf, tables, G_N_ELEMENTS(tables), 0x2950, 0xb28, 0x15960);
    replace_brand(elf, brand_offsets, G_N_ELEMENTS(brand_offsets));
    return TRUE;
}

static gboolean
rebuild(const char *original, const char *output, const char *resources, GError **error)
{
    static const Executable executables[] = {
        {
            "usr/bin/found-control-center",
            "ea8f8332488e31be7cebf52e698073a8d0c2081f4154fd99059872e592f66a90",
            "d65ab34d0a404ec482d8fffd6303355f8d5480b486a2b08667540aa01e23409f",
            patch_control_center,
        },
        {
            "usr/libexec/gnome-control-center-search-provider",
            "16707e8d090baed2bdf3b72b804001301f3d20ec46ca7ac128205e3850de267b",
            "b693f02923353f0018f8c7353c3048a3adc8394c3e3e8e57995045c38e061ff0",
            patch_search_provider,
        },
    };
    g_autoptr(GPtrArray) images = g_ptr_array_new_with_free_func(g_free);
    gsize sizes[G_N_ELEMENTS(executables)];

    /* Validate both inputs and results before writing either executable. */
    for (gsize i = 0; i < G_N_ELEMENTS(executables); i++) {
        const Executable *executable = &executables[i];
        g_autofree char *source = g_build_filename(original, executable->path, NULL);
        g_autofree char *target = g_build_filename(output, executable->path, NULL);
        g_autofree char *data = NULL;
        GStatBuf source_stat, target_stat;
        if (g_stat(source, &source_stat) == 0 && g_stat(target, &target_stat) == 0 &&
            source_stat.st_dev == target_stat.st_dev && source_stat.st_ino == target_stat.st_ino) {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                        "%s: use a separate output directory", target);
            return FALSE;
        }
        if (!g_file_get_contents(source, &data, &sizes[i], error) ||
            !check_sha256(source, (guint8 *) data, sizes[i], executable->original_sha256, error) ||
            !executable->patch((guint8 *) data, resources, error) ||
            !check_sha256(target, (guint8 *) data, sizes[i], executable->patched_sha256, error))
            return FALSE;

        g_ptr_array_add(images, g_steal_pointer(&data));
    }

    for (gsize i = 0; i < G_N_ELEMENTS(executables); i++) {
        g_autofree char *target = g_build_filename(output, executables[i].path, NULL);
        g_autofree char *directory = g_path_get_dirname(target);
        if (g_mkdir_with_parents(directory, 0755) != 0) {
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                        "%s: %s", directory, g_strerror(errno));
            return FALSE;
        }
        if (!g_file_set_contents_full(target, g_ptr_array_index(images, i), sizes[i],
                                      G_FILE_SET_CONTENTS_CONSISTENT, 0755, error))
            return FALSE;
        if (g_chmod(target, 0755) != 0) {
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                        "%s: %s", target, g_strerror(errno));
            return FALSE;
        }
    }
    return TRUE;
}

int
main(int argc, char **argv)
{
    if (argc != 4) {
        g_printerr("Usage: %s ORIGINAL_ROOT OUTPUT_ROOT RESOURCE_DIR\n", argv[0]);
        return 2;
    }

    g_autoptr(GError) error = NULL;
    if (!rebuild(argv[1], argv[2], argv[3], &error)) {
        g_printerr("%s\n", error->message);
        return 1;
    }
    return 0;
}
