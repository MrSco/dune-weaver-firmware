#include <system_error>
#include <cstdint>

bool sd_init_slot(uint32_t freq_hz, int cs_pin, int cd_pin = -1, int wp_pin = -1);
void sd_unmount();
void sd_deinit_slot();

std::error_code sd_mount(int max_files = 1);

// DW Mini fork: "OTA-slot pattern cache".
//
// Boards without an SD card (the Dune Weaver Mini's bare ESP32) can serve /sd
// from a LittleFS filesystem placed in the SPARE OTA app slot - the one
// esp_ota_get_next_update_partition() names.  That slot is only needed while a
// firmware update is being written, so between updates it holds cached
// patterns.  Everything that uses the "sd" mount (playlists, uploads, /sd/
// fetches, $SD/Run) works unchanged.  The host owns the real pattern library
// and re-uploads whatever the cache loses.
//
// Enabled by `sdcard: ota_cache: true` with no cs_pin.
void sd_set_ota_cache(bool enable);
bool sd_is_ota_cache();

// Unmount the cache and refuse to remount it until reboot.  Must be called
// before anything writes the spare OTA slot (a firmware update), since that
// overwrites the cache filesystem.
void sd_cache_release();

// Total/used bytes of the cache filesystem; false if not mounted.
bool sd_cache_stats(uint64_t& total, uint64_t& used);
