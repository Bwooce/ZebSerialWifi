# ota-updates Specification

## Purpose
Firmware updates over the network: the bridge takes a new app image on `POST /update`, writes it to the other of
its two OTA slots, restarts into it, and keeps it only if it reaches the network; otherwise the bootloader goes back
to the previous image. Implemented in `firmware/main/ota.c` with the handlers in `firmware/main/web.c`.

## Requirements

### Requirement: Two OTA app slots with bootloader rollback
The bridge's flash SHALL hold two app slots, `ota_0` at 0x20000 and `ota_1` at 0x200000, each 0x1E0000 bytes, plus an
`otadata` partition, and the bootloader SHALL be built with app rollback enabled. Anti-rollback (version-based
refusal) is not enabled.

**Source**: `firmware/partitions.csv`; `firmware/sdkconfig.defaults` `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`; `firmware/sdkconfig` `# CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK is not set`

#### Scenario: Partition layout
- **WHEN** the firmware is built with `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"`
- **THEN** the partition table SHALL list `ota_0` and `ota_1` of size 0x1E0000 each
- **AND** `nvs` SHALL stay at offset 0x9000, size 0x6000

### Requirement: Update upload endpoint
The bridge SHALL accept a firmware update as the raw body of `POST /update` (no multipart form). On success it SHALL
reply `200` with `text/plain` "Updated. Restarting into the new firmware; this page reloads in 20 s." and restart about
1.5 s later.

**Source**: `firmware/main/web.c` `update_post()`, `restart_soon()`, route `"/update"`

#### Scenario: Upload from a terminal
- **WHEN** a client sends `curl --data-binary @build/zebserial.bin http://<host>.local/update` with a valid image
- **THEN** the bridge SHALL write the image to the other slot, set it as the boot partition and reply 200
- **AND** it SHALL restart about 1.5 s after the reply

### Requirement: Image header checks
Before writing anything, the bridge MUST read the image header, first segment header and app description and refuse
the upload with 400 if the image magic is not `ESP_IMAGE_HEADER_MAGIC`, the chip ID is not `CONFIG_IDF_FIRMWARE_CHIP_ID`,
the app description magic is not `ESP_APP_DESC_MAGIC_WORD`, or the project name differs from the running image's
(`zebserial`).

**Source**: `firmware/main/ota.c` `check_head()`, `OTA_HEAD`; `firmware/CMakeLists.txt` `project(zebserial)`

#### Scenario: ELF or merged image uploaded
- **WHEN** the body does not start with the app image magic
- **THEN** the bridge SHALL reply 400 "Not an ESP32 app image. Upload build/zebserial.bin, not the ELF or a merged image."
- **AND** the other slot SHALL not be erased or written

#### Scenario: Image for another chip
- **WHEN** the image header's chip ID is not the ESP32-C6's
- **THEN** the bridge SHALL reply 400 "This image is for a different ESP32 chip."

#### Scenario: Image from another project
- **WHEN** the app description's project name is not the running image's project name
- **THEN** the bridge SHALL reply 400 "This image is from a different project, not ZebSerialWifi."

### Requirement: Upload size limits
The bridge MUST refuse with 400 "The upload is the wrong size for a firmware image." any upload whose Content-Length
is smaller than the header block it checks (`OTA_HEAD`) or larger than the other slot's partition size (0x1E0000
bytes in the current table).

**Source**: `firmware/main/ota.c` `ota_receive()`, `OTA_HEAD`, `next->size`

#### Scenario: Upload larger than a slot
- **WHEN** the Content-Length exceeds the size of the partition `esp_ota_get_next_update_partition()` returns
- **THEN** the bridge SHALL reply 400 and write nothing

### Requirement: Whole-image validation before switching
The bridge SHALL write the body to the other slot in 4096-byte pieces with sequential writes and SHALL make it the
boot partition only after `esp_ota_end()` accepts the whole image (segments and SHA-256). A damaged or short image
MUST leave the running image as the boot image.

**Source**: `firmware/main/ota.c` `ota_receive()`, `OTA_BUF`, `esp_ota_begin(... OTA_WITH_SEQUENTIAL_WRITES ...)`, `esp_ota_end()`, `esp_ota_set_boot_partition()`

#### Scenario: Image fails validation
- **WHEN** `esp_ota_end()` returns `ESP_ERR_OTA_VALIDATE_FAILED`
- **THEN** the bridge SHALL reply 400 "The image is damaged or incomplete."
- **AND** the boot partition SHALL be unchanged

### Requirement: Upload failure replies
The bridge SHALL reply 500 when the upload stops (a receive error, or more than 5 consecutive receive timeouts) with
"The upload stopped.", or when out of memory, and 400 for every other refusal, including "Another update is already
in progress." and a missing second slot. A failed write after `esp_ota_begin()` SHALL be aborted.

**Source**: `firmware/main/ota.c` `ota_receive()`, `recv_some()`, `OTA_RECV_RETRIES`, `s_busy`, `esp_ota_abort()`; `firmware/main/web.c` `update_post()`

#### Scenario: Second upload during an update
- **WHEN** an upload arrives while another is being written
- **THEN** the bridge SHALL reply 400 "Another update is already in progress."

#### Scenario: No second slot
- **WHEN** the running partition table has no other OTA slot
- **THEN** the bridge SHALL reply 400 "This bridge has no second firmware slot. Flash a build with the OTA partition table over USB once."

#### Scenario: Client goes away mid-upload
- **WHEN** `httpd_req_recv()` fails, or times out more than 5 times in a row
- **THEN** the bridge SHALL abort the OTA write and reply 500 "The upload stopped."

### Requirement: Confirmation once on the network
While the running image is in the `ESP_OTA_IMG_PENDING_VERIFY` state, the bridge SHALL mark it valid at the first
status-loop poll (every 100 ms) at which uptime since boot is at least 15 s and the station state is
`WIFI_ST_CONNECTED` (it has an IPv4 address). The connection need not have lasted 15 s.

**Source**: `firmware/main/ota.c` `ota_poll()`, `OTA_CONFIRM_AFTER_S`; `firmware/main/main.c` status task, `STATUS_TICK_MS`; `firmware/main/wifi_prov.c` `IP_EVENT_STA_GOT_IP`

#### Scenario: New firmware joins Wi-Fi
- **WHEN** a new image boots, gets an IPv4 address, and the uptime reaches 15 s while still connected
- **THEN** the bridge SHALL call `esp_ota_mark_app_valid_cancel_rollback()` and log "new firmware confirmed: it reached the network"

### Requirement: Rollback when not confirmed
If a pending image has not been confirmed by 180 s of uptime, the bridge SHALL mark it invalid and reboot into the
previous image. A restart of a pending image before confirmation, such as a crash, SHALL also lead the bootloader
to boot the previous image.

**Source**: `firmware/main/ota.c` `ota_poll()`, `OTA_CONFIRM_DEADLINE_S`, `esp_ota_mark_app_invalid_rollback_and_reboot()`; `firmware/main/ota.h` header comment; `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`

#### Scenario: New firmware never reaches the network
- **WHEN** a pending image has uptime of 180 s without having been confirmed
- **THEN** the bridge SHALL log "new firmware did not reach the network in 180 s: going back to the previous one"
- **AND** it SHALL reboot into the previous image

#### Scenario: New firmware crashes early
- **WHEN** a pending image restarts before it is confirmed
- **THEN** the bootloader SHALL boot the previous image

### Requirement: Other slot's version only when valid
The bridge SHALL report the other slot's version only if that slot has an app description, its OTA state is neither
`ESP_OTA_IMG_INVALID` nor `ESP_OTA_IMG_ABORTED`, and `esp_image_verify()` accepts the whole image. The verify result
SHALL be cached until the slot is next written.

**Source**: `firmware/main/ota.c` `ota_get_status()`, `other_ok()`, `s_other_ok`

#### Scenario: Refused upload left a header in the other slot
- **WHEN** an earlier upload began writing the other slot but did not finish
- **THEN** the other slot's version SHALL be reported as empty

### Requirement: Firmware section on the Bridge page
The Bridge page SHALL end with a Firmware section showing the running version and slot label, ", new and not yet
confirmed" while pending, a file input with "Upload and restart" that posts the file to `/update`, a note that new
firmware has 180 s to reach the network, and a `curl` example with the hostname.

**Source**: `firmware/main/web.c` `send_firmware_section()`, `bridge_get_page()`

#### Scenario: Upload from the page
- **WHEN** the user chooses a file and presses "Upload and restart"
- **THEN** the page SHALL send it with `XMLHttpRequest` to `/update`, show upload progress in per cent and then the reply text
- **AND** on a 200 reply it SHALL reload after 20 s

#### Scenario: No file chosen
- **WHEN** the user presses "Upload and restart" with no file
- **THEN** the page SHALL show "Choose the .bin file first." and send nothing

### Requirement: Go back to the other firmware
When the other slot holds a valid image, the Bridge page SHALL offer a "Go back to <version>" button that posts to
`/boot_other` after a confirm prompt. `POST /boot_other` SHALL set that slot as the boot partition and restart about
1.5 s later, or reply 400 "There is no other valid firmware to go back to." if there is none.

**Source**: `firmware/main/web.c` `send_firmware_section()`, `boot_other_post()`; `firmware/main/ota.c` `ota_boot_other()`

#### Scenario: Valid image in the other slot
- **WHEN** the user confirms "Go back to <version>"
- **THEN** the bridge SHALL reply with a page saying "Restarting into the other firmware." and restart into the other slot

#### Scenario: Other slot invalid
- **WHEN** the other slot's version is empty
- **THEN** the Bridge page SHALL show no "Go back to" button
- **AND** a `POST /boot_other` SHALL get 400

### Requirement: OTA state in the status API
`GET /api/status` SHALL include an `ota` object with `running` (slot label), `pending` (true while unconfirmed) and
`other_version` (empty when the other slot has no valid image).

**Source**: `firmware/main/web.c` `status_get()`, JSON key `"ota"`

#### Scenario: Status after an update
- **WHEN** a client reads `/api/status` from a new, unconfirmed image
- **THEN** `ota.pending` SHALL be `true`

### Requirement: Cross-site and setup-network refusal
`POST /update` and `POST /boot_other` MUST be refused with 403 "cross-site request refused" when `same_origin()`
fails (an untrusted Host, or an Origin or Referer that does not name the Host), and MUST redirect with 302 to the
setup page when the request comes in over the setup access point.

**Source**: `firmware/main/web.c` `update_post()`, `boot_other_post()`, `same_origin()`, `host_allowed()`, `cross_site()`, `on_ap()`, `to_setup_page()`

#### Scenario: Upload from another web site
- **WHEN** a browser posts to `/update` with an Origin that does not match the Host header
- **THEN** the bridge SHALL reply 403 and SHALL not read the image

#### Scenario: Upload without Origin or Referer
- **WHEN** `curl` posts to `/update` with an allowed Host and neither header
- **THEN** the bridge SHALL accept the request for the image checks

### Requirement: First flash over USB
The OTA path SHALL NOT change the partition table or bootloader, so the first flash with this two-slot layout MUST
be over USB; an image running from a table without a second slot refuses `/update` uploads.

**Source**: `firmware/main/ota.c` `ota_receive()` (`esp_ota_get_next_update_partition()` returns NULL); `README.md` "Updates over the network (OTA)"

#### Scenario: Bridge still on the single-app layout
- **WHEN** an upload reaches firmware running from a table with no second OTA slot
- **THEN** the bridge SHALL refuse it with 400 and the message telling the user to flash over USB once

## Notes
- The 180 s in the Bridge page note is a literal in `send_firmware_section()`, not `OTA_CONFIRM_DEADLINE_S`.
- Both the 15 s and 180 s limits count from boot (`esp_timer_get_time()`), not from the moment Wi-Fi connects.
- `ota_poll()` decides once per boot: after confirming, rolling back, or finding the image not pending, it does nothing more.
