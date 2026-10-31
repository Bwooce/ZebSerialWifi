# discovery Specification

## Purpose
How clients find the bridge on the network over mDNS (Bonjour): its hostname, the services and TXT records it
advertises so macOS offers it as a Zebra ZPL printer, and how the advertised model follows the printer found. Code:
`firmware/main/main.c` `mdns_setup()`, `mdns_set_model()` and `status_task()`, using the `espressif/mdns` component.

## Requirements

### Requirement: mDNS hostname
At boot, after Wi-Fi starts, the bridge SHALL start mDNS and answer for `<hostname>.local`, where `<hostname>` is the
saved hostname (default `zebserial`). A hostname change MUST take effect only after a restart.

**Source**: `firmware/main/main.c` `mdns_setup()` `mdns_hostname_set()`, `app_main()`; `firmware/main/settings.c` `settings_get_net()`

#### Scenario: Default hostname
- **WHEN** no hostname is saved
- **THEN** the bridge SHALL answer mDNS queries for `zebserial.local`

### Requirement: Advertised services
The bridge SHALL advertise exactly two services: `_pdl-datastream._tcp` on port `CONFIG_ZEB_TCP_PORT` (default 9100)
with the printer TXT records, and `_http._tcp` on port 80 with no TXT records. It MUST NOT advertise `_printer` (LPD)
or `_ipp`.

**Source**: `firmware/main/main.c` `mdns_setup()` `mdns_service_add()`, `PDL_SERVICE`; `firmware/main/Kconfig.projbuild` `ZEB_TCP_PORT`

#### Scenario: Browsing for raw printers
- **WHEN** a client browses for `_pdl-datastream._tcp`
- **THEN** it SHALL find the bridge on port 9100

### Requirement: Instance name
Both services and the default mDNS instance SHALL be named `Zebra <model> (<hostname>)`, with the model cut to its
first 22 characters, once a model is known (saved or reported). With no model known it MUST be `ZebSerial
(<hostname>)`.

**Source**: `firmware/main/main.c` `mdns_setup()`, `mdns_set_model()` (`"Zebra %.22s (%s)"`, `"ZebSerial (%s)"`), `mdns_instance_name_set()`, `mdns_service_instance_name_set()`

#### Scenario: GX430t known
- **WHEN** the saved model is `GX430t` and the hostname is `zebserial`
- **THEN** the instance name SHALL be `Zebra GX430t (zebserial)`

#### Scenario: Never seen a printer
- **WHEN** no model has been saved and none reported since boot
- **THEN** the instance name SHALL be `ZebSerial (zebserial)`

### Requirement: Printer TXT records
The `_pdl-datastream._tcp` service SHALL carry TXT `txtvers=1`, `qtotal=1`, `ty=Zebra <model>` (or `Zebra ZPL Printer`
with no model), `product=(ZPL Label Printer)`, `usb_MFG=Zebra`, `usb_MDL=<model>` (or `ZPL Printer` with no model)
and `adminurl=http://<hostname>.local/`.

**Source**: `firmware/main/main.c` `mdns_setup()` `txt[]`, `ZPL_PPD_PRODUCT`

#### Scenario: macOS driver match
- **WHEN** macOS reads the TXT records
- **THEN** `product` SHALL be `(ZPL Label Printer)`, the `*Product` of its bundled Zebra ZPL Label Printer driver

#### Scenario: No model yet
- **WHEN** no model is known
- **THEN** `ty` SHALL be `Zebra ZPL Printer` and `usb_MDL` SHALL be `ZPL Printer`

### Requirement: Model follows the printer
Every 100 ms the bridge SHALL compare the model the printer reported (from `~HI`) with the advertised one. When it is
non-empty and different, the bridge MUST update `ty` and `usb_MDL`, rename both services and the default instance,
and save the model in NVS key `model` if it differs from the saved one.

**Source**: `firmware/main/main.c` `status_task()`, `mdns_set_model()`, `STATUS_TICK_MS`; `firmware/main/settings.c` `settings_set_model()`

#### Scenario: Printer replaced
- **WHEN** the printer was a `GX430t` and a `ZD421` now answers `~HI`
- **THEN** the services SHALL be renamed `Zebra ZD421 (<hostname>)` with `ty=Zebra ZD421` and `usb_MDL=ZD421`
- **AND** `ZD421` SHALL be saved as the model

### Requirement: Advertised model kept while the printer is silent
The bridge MUST keep advertising the last known model when the printer is off or not answering, and SHALL advertise
the saved model from boot, before the printer answers.

**Source**: `firmware/main/main.c` `mdns_setup()` `settings_get_model()`, `status_task()` (`p.model[0] != '\0'`)

#### Scenario: Printer off at boot
- **WHEN** the bridge boots with model `GX430t` saved and the printer does not answer
- **THEN** the instance name SHALL be `Zebra GX430t (<hostname>)`

### Requirement: Failed model update
If any mDNS update for a new model fails, the bridge SHALL log one warning, MUST NOT save the model, and SHALL NOT
retry until the printer reports a different model.

**Source**: `firmware/main/main.c` `mdns_set_model()` (`strlcpy(s_mdns_model, ...)` before the updates)

#### Scenario: TXT update error
- **WHEN** `mdns_service_txt_item_set()` fails for model `ZD421`
- **THEN** the bridge SHALL log "mDNS model update failed" once and leave the saved model unchanged
