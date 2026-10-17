#pragma once
/*
 * Pure parsers for Zebra printer replies. No ESP-IDF dependencies so the
 * host tests in test_host/ can build them.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Decoded ~HS host status. Field names follow the ZPL II guide. */
typedef struct {
    bool paper_out;
    bool pause;
    bool buffer_full;
    bool corrupt_ram;
    bool under_temp;
    bool over_temp;
    bool head_up;
    bool ribbon_out;
    bool partial_format;  /* string 1 field h: a ^XA format has been started and not ended */
    int formats_in_buffer;
    int comm_settings;   /* string 1 field aaa: eight bits as a decimal number */
    bool handshake_dtr;  /* aaa bit a7: 0 = XON/XOFF, 1 = DTR */
} zpl_hs_t;

/*
 * Find the three STX...ETX frames of a ~HS reply anywhere in buf and decode
 * them. Returns false unless the frames have the documented field counts
 * (12, 11, 2) and the decoded fields are numeric.
 */
bool zpl_hs_parse(const uint8_t *buf, size_t len, zpl_hs_t *out);

/*
 * Copy the first "..." quoted value of an SGD getvar reply into out. Lines
 * starting with '!' (an echo of the command itself) are skipped.
 */
bool zpl_sgd_value(const uint8_t *buf, size_t len, char *out, size_t out_len);

/*
 * Model name from a ~HI reply ("<STX>GX430t-300dpi,V56.17.16Z,12,2104KB<ETX>"),
 * the first field with any "-<n>dpi" suffix removed. Returns false unless a
 * complete STX...ETX frame with a printable, non-empty first field is found.
 */
bool zpl_hi_model(const uint8_t *buf, size_t len, char *out, size_t out_len);

#define ZPL_BAUD_MAX_CANDIDATES 6

/*
 * Baud rates to probe, in order: saved (if a standard rate), then 115200,
 * 9600, 19200, 38400, 57600, without duplicates.
 */
void zpl_baud_candidates(uint32_t saved, uint32_t out[ZPL_BAUD_MAX_CANDIDATES], size_t *n);

/*
 * Build the ZPL that sets the serial port to baud, 8N1, XON/XOFF, no Zebra
 * protocol, and saves it (^JUS). Returns the length, or 0 if out is too small.
 */
size_t zpl_sc_command(char *out, size_t out_len, uint32_t baud);

/*
 * Find the handshake line in ^HH (configuration dump) output and copy its
 * value, e.g. "XON/XOFF" from "  XON/XOFF            HOST HANDSHAKE" or
 * the dot-leader form "  XON/XOFF.........HOST HANDSHAKE".
 */
bool zpl_hh_handshake(const uint8_t *buf, size_t len, char *out, size_t out_len);

/*
 * True if a handshake value (SGD or ^HH form) is XON/XOFF alone. A combined
 * mode such as the G-series "DTR & XON/XOFF" is false: it also needs the
 * DTR/DSR lines, which a three-wire adapter does not carry.
 */
bool zpl_handshake_is_xon(const char *value);

/*
 * The printer's handshake from what detection has: the SGD comm.handshake
 * reply (sgd NULL if there was none; "?" means the firmware lacks the
 * variable), then the ~HS a7 bit (hs_valid false if no ~HS answered). False
 * when neither tells, and the caller reads ^HH.
 */
bool zpl_pick_handshake(const char *sgd, bool hs_valid, bool hs_dtr, char *out, size_t out_len);

typedef enum {
    ZPL_LANG_UNKNOWN,   /* no reply, "?" or empty */
    ZPL_LANG_ZPL,       /* zpl, epl_zpl, hybrid_xml_zpl, ... */
    ZPL_LANG_OTHER,     /* epl, line_print, cpcl, ...: ZPL would be ignored or printed as text */
} zpl_lang_t;

/* Classify an SGD device.languages value. */
zpl_lang_t zpl_language_kind(const char *value);

/*
 * True when an SGD value means a setting is on, such as power.sleep.enable or
 * power.energy_star.enable on a printer that can sleep. "?" (a printer
 * without the setting), "off", empty and NULL are all false.
 */
bool zpl_sgd_is_on(const char *value);

/*
 * Next baud to raise the printer to: the highest standard rate up to
 * target that is above current and below cap (the lowest rate known to
 * fail; 0 if none). 0 when there is nothing to try.
 */
uint32_t zpl_next_raise(uint32_t target, uint32_t current, uint32_t cap);

/*
 * ^HH (configuration label as text) lines. The GX430t sends two spaces, the
 * value left-justified in a 20-character field, then the name, e.g.
 * "  15.0                DARKNESS"; older firmware puts a dot leader
 * between value and name instead. Long values are truncated.
 */
/* the GX430t sends 60 lines; ZT, ZD and Xi4 dumps add network, RFID and Link-OS lines */
#define ZPL_HH_MAX_PAIRS 128
#define ZPL_HH_NAME_LEN 24
#define ZPL_HH_VALUE_LEN 28

typedef struct {
    char name[ZPL_HH_NAME_LEN];
    char value[ZPL_HH_VALUE_LEN];
} zpl_hh_pair_t;

typedef struct {
    int count;
    zpl_hh_pair_t pair[ZPL_HH_MAX_PAIRS];
} zpl_hh_t;

/* Split ^HH output into name/value pairs, in printer order. False if no line parsed. */
bool zpl_hh_parse(const uint8_t *buf, size_t len, zpl_hh_t *out);

/* Value for an exact name, or NULL. */
const char *zpl_hh_get(const zpl_hh_t *hh, const char *name);

/*
 * Printer settings the web page can change. Each maps to one ^HH line and
 * one ZPL command (see zpl_cfg_commands for the command and guide reference).
 */
typedef enum {
    ZPL_CFG_DARKNESS,       /* ~SD, 0..30 */
    ZPL_CFG_SPEED,          /* ^PR, ips */
    ZPL_CFG_METHOD,         /* ^MT, D/T */
    ZPL_CFG_MODE,           /* ^MM, T/P/R/C/D */
    ZPL_CFG_MEDIA,          /* ^MN, Y/M/N */
    ZPL_CFG_SENSOR,         /* ^JS, A/R/T */
    ZPL_CFG_ZPL_MODE,       /* ^SZ, 1/2 */
    ZPL_CFG_WIDTH,          /* ^PW, dots */
    ZPL_CFG_LENGTH,         /* ^LL, dots */
    ZPL_CFG_MAX_LENGTH,     /* ^ML, dots */
    ZPL_CFG_TEAR_OFF,       /* ~TA, -120..120 */
    ZPL_CFG_LABEL_TOP,      /* ^LT, -120..120 */
    ZPL_CFG_LEFT_POS,       /* ^LS, -9999..9999 */
    ZPL_CFG_POWER_UP,       /* ^MF first parameter, F/C/L/N */
    ZPL_CFG_HEAD_CLOSE,     /* ^MF second parameter, F/C/L/N */
    ZPL_CFG_BACKFEED,       /* ~JS, N/A/B/O or 10..90 */
    ZPL_CFG_EARLY_WARNING,  /* ^JH f parameter, E/D */
    ZPL_CFG_COUNT
} zpl_cfg_field_t;

typedef enum {
    ZPL_CFG_INT,            /* signed decimal within a range */
    ZPL_CFG_LETTER,         /* one of `letters` */
    ZPL_CFG_PERCENT_OR_LETTER, /* one of `letters`, or 10..90 in steps of 10 (~JS) */
} zpl_cfg_kind_t;

typedef struct {
    const char *key;        /* form field name */
    const char *hh_name;    /* ^HH line name */
    zpl_cfg_kind_t kind;
    const char *letters;    /* accepted letters, LETTER and PERCENT_OR_LETTER kinds */
} zpl_cfg_desc_t;

const zpl_cfg_desc_t *zpl_cfg_desc(zpl_cfg_field_t f);

/*
 * Settings decoded from ^HH. Letter settings hold the ZPL letter ('D'),
 * percentages the number. For ~JS a value is a letter only if it is one of
 * N, A, B, O (none a multiple of ten); 70 is 70 %, not 'F'.
 */
typedef struct {
    int v[ZPL_CFG_COUNT];
    uint32_t known;         /* bit (1u << field) set when v[field] was read and is in range */
    int dpmm;               /* dots per mm from RESOLUTION, 0 if unknown */
    int head_dots;          /* printhead width in dots from RESOLUTION, 0 if unknown */
} zpl_printer_cfg_t;

/* Decode the editable settings. Text this code does not recognise leaves the field unknown. */
void zpl_cfg_from_hh(const zpl_hh_t *hh, zpl_printer_cfg_t *out);

/* Accepted range of an INT field for this printer (dpmm and head width narrow some). */
void zpl_cfg_range(const zpl_printer_cfg_t *cfg, zpl_cfg_field_t f, int *min, int *max);

bool zpl_cfg_value_valid(const zpl_printer_cfg_t *cfg, zpl_cfg_field_t f, int v);

/* Parse form text strictly (no spaces or trailing junk) and range-check it. */
bool zpl_cfg_parse(const zpl_printer_cfg_t *cfg, zpl_cfg_field_t f, const char *text, int *out);

/* Form text for a value: what zpl_cfg_parse reads back as the same value. */
void zpl_cfg_format(zpl_cfg_field_t f, int v, char *out, size_t out_len);

/*
 * Build the ZPL that applies the fields in `changed` (bits as in `known`)
 * from want, then saves with ^JUS: ~ commands first, each on its own, then
 * one ^XA...^XZ for the ^ commands, then ^XA^JUS^XZ. Returns the length, or
 * 0 if nothing changed, a changed field is unknown or out of range, or out
 * is too small.
 */
size_t zpl_cfg_commands(const zpl_printer_cfg_t *want, uint32_t changed, char *out, size_t out_len);

/*
 * ZPL for a one-shot printer action by name ("calibrate", "config_label",
 * "feed", "reset_counter1", "reset_counter2", "reset_head_cleaned",
 * "reload_saved", "factory_defaults"), or NULL for an unknown name.
 */
const char *zpl_action_command(const char *action);
