#ifndef JOSHBOOT_CONFIG_H
#define JOSHBOOT_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define JOSH_BOOT_CONFIG_VERSION 1u
#define JOSH_BOOT_CONFIG_MAX_BYTES 1024u
#define JOSH_BOOT_CONFIG_ENTRY_ID_MAX 15u
#define JOSH_BOOT_CONFIG_NAME_MAX 31u
#define JOSH_BOOT_CONFIG_PATH_MAX 95u
#define JOSH_BOOT_CONFIG_CMDLINE_MAX 127u

#define JOSH_BOOT_CONFIG_FLAG_RECOVERY    (1u << 0)
#define JOSH_BOOT_CONFIG_FLAG_DEVELOPMENT (1u << 1)

typedef enum {
    JOSH_CONFIG_OK = 0,
    JOSH_CONFIG_INVALID_ARGUMENT,
    JOSH_CONFIG_TOO_LARGE,
    JOSH_CONFIG_SYNTAX,
    JOSH_CONFIG_UNKNOWN_KEY,
    JOSH_CONFIG_DUPLICATE_KEY,
    JOSH_CONFIG_MISSING_REQUIRED,
    JOSH_CONFIG_UNSUPPORTED_VERSION,
    JOSH_CONFIG_INVALID_VALUE,
    JOSH_CONFIG_UNSUPPORTED_FEATURE
} josh_boot_config_status_t;

typedef struct {
    uint32_t version;
    uint32_t timeout_seconds;
    uint32_t flags;
    char default_entry[JOSH_BOOT_CONFIG_ENTRY_ID_MAX + 1u];
    char entry_id[JOSH_BOOT_CONFIG_ENTRY_ID_MAX + 1u];
    char display_name[JOSH_BOOT_CONFIG_NAME_MAX + 1u];
    char kernel_path[JOSH_BOOT_CONFIG_PATH_MAX + 1u];
    char command_line[JOSH_BOOT_CONFIG_CMDLINE_MAX + 1u];
} josh_boot_config_t;

josh_boot_config_status_t josh_boot_config_parse(
    const char *text,
    size_t length,
    josh_boot_config_t *config
);

const char *josh_boot_config_status_string(josh_boot_config_status_t status);

#endif
