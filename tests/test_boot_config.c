#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../boot/core/config.h"

static const char valid_config[] =
    "# JoshBootloader v1\n"
    "version=1\n"
    "default=josh\n"
    "timeout=3\n"
    "entry=josh\n"
    "name=Josh OS\n"
    "kernel=/boot/josh/kernel.elf\n"
    "previous_kernel=/boot/josh/krnlprev.elf\n"
    "recovery_kernel=/boot/josh/recovery.elf\n"
    "cmdline=quiet\n"
    "flags=development\n";

static josh_boot_config_status_t parse_text(const char *text, josh_boot_config_t *config) {
    return josh_boot_config_parse(text, strlen(text), config);
}

static void test_valid(void) {
    josh_boot_config_t config;
    assert(parse_text(valid_config, &config) == JOSH_CONFIG_OK);
    assert(config.version == 1);
    assert(config.timeout_seconds == 3);
    assert(strcmp(config.default_entry, "josh") == 0);
    assert(strcmp(config.entry_id, "josh") == 0);
    assert(strcmp(config.display_name, "Josh OS") == 0);
    assert(strcmp(config.kernel_path, "/boot/josh/kernel.elf") == 0);
    assert(strcmp(config.previous_kernel_path, "/boot/josh/krnlprev.elf") == 0);
    assert(strcmp(config.recovery_kernel_path, "/boot/josh/recovery.elf") == 0);
    assert(strcmp(config.command_line, "quiet") == 0);
    assert(config.flags == JOSH_BOOT_CONFIG_FLAG_DEVELOPMENT);
}

static void test_optional_fields(void) {
    const char text[] =
        "version=1\n"
        "default=josh\n"
        "timeout=0\n"
        "entry=josh\n"
        "name=Josh OS\n"
        "kernel=/BOOT/JOSH/KERNEL.ELF\n";
    josh_boot_config_t config;
    assert(parse_text(text, &config) == JOSH_CONFIG_OK);
    assert(config.previous_kernel_path[0] == '\0');
    assert(config.recovery_kernel_path[0] == '\0');
    assert(config.command_line[0] == '\0');
    assert(config.flags == 0);
}

static void test_rejections(void) {
    josh_boot_config_t config;

    assert(parse_text(
        "version=2\ndefault=josh\ntimeout=3\nentry=josh\nname=Josh OS\nkernel=/boot/josh/kernel.elf\n",
        &config) == JOSH_CONFIG_UNSUPPORTED_VERSION);

    assert(parse_text(
        "version=1\nversion=1\ndefault=josh\ntimeout=3\nentry=josh\nname=Josh OS\nkernel=/boot/josh/kernel.elf\n",
        &config) == JOSH_CONFIG_DUPLICATE_KEY);

    assert(parse_text(
        "version=1\ndefault=josh\ntimeout=3\nentry=josh\nname=Josh OS\nkernel=/boot/josh/kernel.elf\nmagic=yes\n",
        &config) == JOSH_CONFIG_UNKNOWN_KEY);

    assert(parse_text(
        "version=1\ndefault=josh\ntimeout=31\nentry=josh\nname=Josh OS\nkernel=/boot/josh/kernel.elf\n",
        &config) == JOSH_CONFIG_INVALID_VALUE);

    assert(parse_text(
        "version=1\ndefault=other\ntimeout=3\nentry=josh\nname=Josh OS\nkernel=/boot/josh/kernel.elf\n",
        &config) == JOSH_CONFIG_INVALID_VALUE);

    assert(parse_text(
        "version=1\ndefault=josh\ntimeout=3\nentry=josh\nname=Josh OS\nkernel=boot/josh/kernel.elf\n",
        &config) == JOSH_CONFIG_INVALID_VALUE);

    assert(parse_text(
        "version=1\ndefault=josh\ntimeout=3\nentry=josh\nname=Josh OS\nkernel=/boot/josh/kernel.elf\nmodule=/boot/josh/initrd\n",
        &config) == JOSH_CONFIG_UNSUPPORTED_FEATURE);

    assert(parse_text(
        "version=1\ndefault=josh\ntimeout=3\nentry=josh\nname=Josh OS\n",
        &config) == JOSH_CONFIG_MISSING_REQUIRED);

    assert(parse_text(
        "version=1\ndefault=josh\ntimeout=3\nentry=josh\nname=Josh OS\nkernel=/boot/josh/kernel.elf\nflags=recovery,development\n",
        &config) == JOSH_CONFIG_OK);
    assert(config.flags == (JOSH_BOOT_CONFIG_FLAG_RECOVERY | JOSH_BOOT_CONFIG_FLAG_DEVELOPMENT));

    assert(parse_text(
        "version=1\ndefault=josh\ntimeout=3\nentry=josh\nname=Josh OS\nkernel=/boot/josh/kernel.elf\nflags=unknown\n",
        &config) == JOSH_CONFIG_INVALID_VALUE);
}

static void test_size_limit(void) {
    char oversized[JOSH_BOOT_CONFIG_MAX_BYTES + 2u];
    memset(oversized, 'x', sizeof(oversized));
    josh_boot_config_t config;
    assert(josh_boot_config_parse(oversized, sizeof(oversized), &config) == JOSH_CONFIG_TOO_LARGE);
}

int main(void) {
    test_valid();
    test_optional_fields();
    test_rejections();
    test_size_limit();
    for (int status = JOSH_CONFIG_OK; status <= JOSH_CONFIG_UNSUPPORTED_FEATURE; ++status) {
        assert(josh_boot_config_status_string((josh_boot_config_status_t)status) != NULL);
    }
    assert(josh_boot_config_status_string((josh_boot_config_status_t)999) != NULL);
    puts("boot config tests passed");
    return 0;
}
