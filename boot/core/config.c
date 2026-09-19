#include "config.h"

#define FIELD_VERSION  (1u << 0)
#define FIELD_DEFAULT  (1u << 1)
#define FIELD_TIMEOUT  (1u << 2)
#define FIELD_ENTRY    (1u << 3)
#define FIELD_NAME     (1u << 4)
#define FIELD_KERNEL   (1u << 5)
#define FIELD_CMDLINE  (1u << 6)
#define FIELD_FLAGS    (1u << 7)
#define FIELD_PREVIOUS_KERNEL (1u << 8)
#define FIELD_RECOVERY_KERNEL (1u << 9)

#define REQUIRED_FIELDS \
    (FIELD_VERSION | FIELD_DEFAULT | FIELD_TIMEOUT | FIELD_ENTRY | FIELD_NAME | FIELD_KERNEL)

static void zero_bytes(void *pointer, size_t length) {
    unsigned char *p = (unsigned char *)pointer;
    while (length--) *p++ = 0;
}

static int span_equal(const char *text, size_t length, const char *literal) {
    size_t i = 0;
    while (literal[i]) {
        if (i >= length || text[i] != literal[i]) return 0;
        ++i;
    }
    return i == length;
}

static int copy_value(
    char *destination,
    size_t capacity,
    const char *value,
    size_t length
) {
    if (!destination || !value || capacity == 0 || length >= capacity) return 0;
    for (size_t i = 0; i < length; ++i) destination[i] = value[i];
    destination[length] = '\0';
    return 1;
}

static int entry_id_valid(const char *value, size_t length) {
    if (!value || length == 0 || length > JOSH_BOOT_CONFIG_ENTRY_ID_MAX) return 0;
    for (size_t i = 0; i < length; ++i) {
        char c = value[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_') {
            continue;
        }
        return 0;
    }
    return 1;
}

static int display_name_valid(const char *value, size_t length) {
    if (!value || length == 0 || length > JOSH_BOOT_CONFIG_NAME_MAX) return 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)value[i];
        if (c < 0x20u || c > 0x7eu) return 0;
    }
    return 1;
}

static int path_valid(const char *value, size_t length) {
    if (!value || length < 2 || length > JOSH_BOOT_CONFIG_PATH_MAX || value[0] != '/') {
        return 0;
    }
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)value[i];
        if (c < 0x21u || c > 0x7eu || c == '\\') return 0;
    }
    return 1;
}

static int command_line_valid(const char *value, size_t length) {
    if (!value || length > JOSH_BOOT_CONFIG_CMDLINE_MAX) return 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)value[i];
        if (c < 0x20u || c > 0x7eu) return 0;
    }
    return 1;
}

static int parse_u32(const char *value, size_t length, uint32_t *out) {
    if (!value || !out || length == 0) return 0;
    uint32_t result = 0;
    for (size_t i = 0; i < length; ++i) {
        if (value[i] < '0' || value[i] > '9') return 0;
        uint32_t digit = (uint32_t)(value[i] - '0');
        if (result > (UINT32_MAX - digit) / 10u) return 0;
        result = result * 10u + digit;
    }
    *out = result;
    return 1;
}

static int parse_flags(const char *value, size_t length, uint32_t *flags) {
    if (!value || !flags) return 0;
    *flags = 0;
    if (length == 0 || span_equal(value, length, "none")) return 1;

    size_t start = 0;
    while (start < length) {
        size_t end = start;
        while (end < length && value[end] != ',') ++end;
        if (end == start) return 0;

        if (span_equal(value + start, end - start, "recovery")) {
            if ((*flags & JOSH_BOOT_CONFIG_FLAG_RECOVERY) != 0) return 0;
            *flags |= JOSH_BOOT_CONFIG_FLAG_RECOVERY;
        } else if (span_equal(value + start, end - start, "development")) {
            if ((*flags & JOSH_BOOT_CONFIG_FLAG_DEVELOPMENT) != 0) return 0;
            *flags |= JOSH_BOOT_CONFIG_FLAG_DEVELOPMENT;
        } else {
            return 0;
        }

        if (end == length) break;
        start = end + 1u;
        if (start >= length) return 0;
    }
    return 1;
}

static uint32_t field_from_key(const char *key, size_t key_length) {
    static const struct {
        const char *name;
        uint32_t field;
    } fields[] = {
        {"version", FIELD_VERSION},
        {"default", FIELD_DEFAULT},
        {"timeout", FIELD_TIMEOUT},
        {"entry", FIELD_ENTRY},
        {"name", FIELD_NAME},
        {"kernel", FIELD_KERNEL},
        {"previous_kernel", FIELD_PREVIOUS_KERNEL},
        {"recovery_kernel", FIELD_RECOVERY_KERNEL},
        {"cmdline", FIELD_CMDLINE},
        {"flags", FIELD_FLAGS},
    };
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        if (span_equal(key, key_length, fields[i].name)) return fields[i].field;
    }
    return 0;
}

static int unsupported_key(const char *key, size_t key_length) {
    return span_equal(key, key_length, "module") ||
           span_equal(key, key_length, "initrd");
}

static josh_boot_config_status_t set_version(
    josh_boot_config_t *config, const char *value, size_t value_length
) {
    uint32_t version = 0;
    if (!parse_u32(value, value_length, &version)) return JOSH_CONFIG_INVALID_VALUE;
    if (version != JOSH_BOOT_CONFIG_VERSION) return JOSH_CONFIG_UNSUPPORTED_VERSION;
    config->version = version;
    return JOSH_CONFIG_OK;
}

static josh_boot_config_status_t set_timeout(
    josh_boot_config_t *config, const char *value, size_t value_length
) {
    uint32_t timeout = 0;
    if (!parse_u32(value, value_length, &timeout) || timeout > 30u) {
        return JOSH_CONFIG_INVALID_VALUE;
    }
    config->timeout_seconds = timeout;
    return JOSH_CONFIG_OK;
}

static josh_boot_config_status_t set_entry_value(
    char *destination, size_t capacity, const char *value, size_t value_length
) {
    if (!entry_id_valid(value, value_length) ||
        !copy_value(destination, capacity, value, value_length)) {
        return JOSH_CONFIG_INVALID_VALUE;
    }
    return JOSH_CONFIG_OK;
}

static josh_boot_config_status_t set_name(
    josh_boot_config_t *config, const char *value, size_t value_length
) {
    if (!display_name_valid(value, value_length) ||
        !copy_value(config->display_name, sizeof(config->display_name),
                    value, value_length)) {
        return JOSH_CONFIG_INVALID_VALUE;
    }
    return JOSH_CONFIG_OK;
}

static josh_boot_config_status_t set_path(
    josh_boot_config_t *config, uint32_t field,
    const char *value, size_t value_length
) {
    char *destination = config->kernel_path;
    size_t capacity = sizeof(config->kernel_path);
    if (field == FIELD_PREVIOUS_KERNEL) {
        destination = config->previous_kernel_path;
        capacity = sizeof(config->previous_kernel_path);
    } else if (field == FIELD_RECOVERY_KERNEL) {
        destination = config->recovery_kernel_path;
        capacity = sizeof(config->recovery_kernel_path);
    }
    if (!path_valid(value, value_length) ||
        !copy_value(destination, capacity, value, value_length)) {
        return JOSH_CONFIG_INVALID_VALUE;
    }
    return JOSH_CONFIG_OK;
}

static josh_boot_config_status_t apply_field(
    josh_boot_config_t *config, uint32_t field,
    const char *value, size_t value_length
) {
    switch (field) {
        case FIELD_VERSION:
            return set_version(config, value, value_length);
        case FIELD_TIMEOUT:
            return set_timeout(config, value, value_length);
        case FIELD_DEFAULT:
            return set_entry_value(config->default_entry, sizeof(config->default_entry),
                                   value, value_length);
        case FIELD_ENTRY:
            return set_entry_value(config->entry_id, sizeof(config->entry_id),
                                   value, value_length);
        case FIELD_NAME:
            return set_name(config, value, value_length);
        case FIELD_KERNEL:
        case FIELD_PREVIOUS_KERNEL:
        case FIELD_RECOVERY_KERNEL:
            return set_path(config, field, value, value_length);
        case FIELD_CMDLINE:
            if (!command_line_valid(value, value_length) ||
                !copy_value(config->command_line, sizeof(config->command_line),
                            value, value_length)) {
                return JOSH_CONFIG_INVALID_VALUE;
            }
            return JOSH_CONFIG_OK;
        case FIELD_FLAGS:
            return parse_flags(value, value_length, &config->flags)
                ? JOSH_CONFIG_OK : JOSH_CONFIG_INVALID_VALUE;
        default:
            return JOSH_CONFIG_UNKNOWN_KEY;
    }
}

static josh_boot_config_status_t set_field(
    josh_boot_config_t *config,
    uint32_t *seen,
    const char *key,
    size_t key_length,
    const char *value,
    size_t value_length
) {
    uint32_t field = field_from_key(key, key_length);
    if (field == 0) {
        return unsupported_key(key, key_length)
            ? JOSH_CONFIG_UNSUPPORTED_FEATURE : JOSH_CONFIG_UNKNOWN_KEY;
    }
    if ((*seen & field) != 0) return JOSH_CONFIG_DUPLICATE_KEY;
    *seen |= field;
    return apply_field(config, field, value, value_length);
}

josh_boot_config_status_t josh_boot_config_parse(
    const char *text,
    size_t length,
    josh_boot_config_t *config
) {
    if (!text || !config) return JOSH_CONFIG_INVALID_ARGUMENT;
    if (length == 0 || length > JOSH_BOOT_CONFIG_MAX_BYTES) return JOSH_CONFIG_TOO_LARGE;

    zero_bytes(config, sizeof(*config));
    uint32_t seen = 0;
    size_t position = 0;

    while (position < length) {
        size_t line_start = position;
        while (position < length && text[position] != '\n') ++position;
        size_t line_end = position;
        if (position < length && text[position] == '\n') ++position;
        if (line_end > line_start && text[line_end - 1u] == '\r') --line_end;

        if (line_end == line_start) continue;
        if (text[line_start] == '#') continue;

        size_t equals = line_start;
        while (equals < line_end && text[equals] != '=') ++equals;
        if (equals == line_start || equals == line_end) return JOSH_CONFIG_SYNTAX;

        josh_boot_config_status_t status = set_field(
            config,
            &seen,
            text + line_start,
            equals - line_start,
            text + equals + 1u,
            line_end - equals - 1u
        );
        if (status != JOSH_CONFIG_OK) return status;
    }

    if ((seen & REQUIRED_FIELDS) != REQUIRED_FIELDS) {
        return JOSH_CONFIG_MISSING_REQUIRED;
    }

    size_t default_length = 0;
    while (config->default_entry[default_length]) ++default_length;
    size_t entry_length = 0;
    while (config->entry_id[entry_length]) ++entry_length;
    if (default_length != entry_length ||
        !span_equal(config->default_entry, default_length, config->entry_id)) {
        return JOSH_CONFIG_INVALID_VALUE;
    }

    return JOSH_CONFIG_OK;
}

const char *josh_boot_config_status_string(josh_boot_config_status_t status) {
    switch (status) {
        case JOSH_CONFIG_OK: return "ok";
        case JOSH_CONFIG_INVALID_ARGUMENT: return "invalid argument";
        case JOSH_CONFIG_TOO_LARGE: return "config too large";
        case JOSH_CONFIG_SYNTAX: return "syntax error";
        case JOSH_CONFIG_UNKNOWN_KEY: return "unknown key";
        case JOSH_CONFIG_DUPLICATE_KEY: return "duplicate key";
        case JOSH_CONFIG_MISSING_REQUIRED: return "missing required field";
        case JOSH_CONFIG_UNSUPPORTED_VERSION: return "unsupported version";
        case JOSH_CONFIG_INVALID_VALUE: return "invalid value";
        case JOSH_CONFIG_UNSUPPORTED_FEATURE: return "unsupported feature";
        default: return "unknown config error";
    }
}
