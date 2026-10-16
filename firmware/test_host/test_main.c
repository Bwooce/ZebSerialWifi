/*
 * Host tests for zpl_status, urlcodec and netcfg.
 *
 * ~HS fixtures are typed from the field layout documented for ~HS in the
 * Zebra ZPL II Programming Guide:
 *   string 1: aaa,b,c,dddd,eee,f,g,h,iii,j,k,l   (12 fields)
 *   string 2: mmm,n,o,p,q,r,s,t,uuuuuuuu,v,www   (11 fields)
 *   string 3: xxxx,y                             (2 fields)
 * each framed as <STX>...<ETX><CR><LF>.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "zpl_status.h"
#include "urlcodec.h"
#include "netcfg.h"

static int s_failures;
static int s_checks;

#define CHECK(cond)                                                       \
    do {                                                                  \
        s_checks++;                                                       \
        if (!(cond)) {                                                    \
            s_failures++;                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
        }                                                                 \
    } while (0)

#define S1_OK "\x02" "030,0,0,1245,000,0,0,0,000,0,0,0" "\x03\r\n"
#define S2_OK "\x02" "001,0,0,0,1,2,6,0,00000000,1,000" "\x03\r\n"
#define S3_OK "\x02" "1234,0" "\x03\r\n"

static bool parse_str(const char *s, zpl_hs_t *out)
{
    return zpl_hs_parse((const uint8_t *)s, strlen(s), out);
}

static void test_hs_ok(void)
{
    zpl_hs_t hs;
    memset(&hs, 0xA5, sizeof(hs));
    CHECK(parse_str(S1_OK S2_OK S3_OK, &hs));
    CHECK(!hs.paper_out);
    CHECK(!hs.pause);
    CHECK(!hs.head_up);
    CHECK(!hs.ribbon_out);
    CHECK(!hs.buffer_full);
    CHECK(!hs.corrupt_ram);
    CHECK(!hs.under_temp);
    CHECK(!hs.over_temp);
    CHECK(hs.formats_in_buffer == 0);

    /* paper out, paused, 3 formats waiting, head up, ribbon out */
    const char *flags = "\x02" "030,1,1,1245,003,0,0,0,000,0,0,1" "\x03\r\n"
                        "\x02" "001,0,1,1,1,2,6,0,00000000,1,000" "\x03\r\n" S3_OK;
    CHECK(parse_str(flags, &hs));
    CHECK(hs.paper_out);
    CHECK(hs.pause);
    CHECK(hs.formats_in_buffer == 3);
    CHECK(hs.head_up);
    CHECK(hs.ribbon_out);
    CHECK(hs.over_temp);
    CHECK(!hs.under_temp);
}

/*
 * A real ~HS reply from a GX430t (firmware V56.17.16Z) set to 9600 8N1
 * XON/XOFF, captured on the bench on 2026-10-16. aaa = 030: bit a7 (128)
 * clear means XON/XOFF, per the ZPL II guide's ~HS field table.
 */
static void test_hs_handshake_bit(void)
{
    zpl_hs_t hs;
    const char *gx430t = "\x02" "030,0,0,1938,000,0,0,0,000,0,0,0" "\x03\r\n"
                         "\x02" "000,0,0,0,0,2,6,0,00000000,1,000" "\x03\r\n"
                         "\x02" "1234,0" "\x03\r\n";
    CHECK(parse_str(gx430t, &hs));
    CHECK(hs.comm_settings == 30);
    CHECK(!hs.handshake_dtr);
    /* the same settings with a7 set: 30 + 128 = 158, DTR handshake */
    const char *dtr = "\x02" "158,0,0,1938,000,0,0,0,000,0,0,0" "\x03\r\n"
                      "\x02" "000,0,0,0,0,2,6,0,00000000,1,000" "\x03\r\n"
                      "\x02" "1234,0" "\x03\r\n";
    CHECK(parse_str(dtr, &hs));
    CHECK(hs.comm_settings == 158);
    CHECK(hs.handshake_dtr);
}

static void test_hs_with_leading_garbage(void)
{
    zpl_hs_t hs;
    /* line noise and a stray STX before the real reply, as seen after a baud change */
    const char *s = "\xff\xfe\x13\x11xyz\x02" "junk" S1_OK S2_OK S3_OK;
    CHECK(parse_str(s, &hs));
    /* no CR/LF between frames is also accepted */
    const char *tight = "\x02" "030,0,0,1245,000,0,0,0,000,0,0,0" "\x03"
                        "\x02" "001,0,0,0,1,2,6,0,00000000,1,000" "\x03"
                        "\x02" "1234,0" "\x03";
    CHECK(parse_str(tight, &hs));
}

static void test_hs_rejects_short_frames(void)
{
    zpl_hs_t hs;
    /* wrong-baud garbage that happens to contain STX/ETX */
    CHECK(!parse_str("\x02" "a,b" "\x03" "\x02" "c" "\x03" "\x02" "d,e" "\x03", &hs));
    /* string 1 with 11 fields */
    CHECK(!parse_str("\x02" "030,0,0,1245,000,0,0,0,000,0,0" "\x03\r\n" S2_OK S3_OK, &hs));
    /* non-digit content in a flag field */
    CHECK(!parse_str("\x02" "030,X,0,1245,000,0,0,0,000,0,0,0" "\x03\r\n" S2_OK S3_OK, &hs));
    /* empty input */
    CHECK(!zpl_hs_parse((const uint8_t *)"", 0, &hs));
}

static void test_hs_needs_three_frames(void)
{
    zpl_hs_t hs;
    CHECK(!parse_str(S1_OK S2_OK, &hs));
    /* truncated third frame (no ETX) */
    CHECK(!parse_str(S1_OK S2_OK "\x02" "1234,0", &hs));
}

static void test_sgd_value(void)
{
    char v[32];
    const char *r = "\"xon/xoff\"";
    CHECK(zpl_sgd_value((const uint8_t *)r, strlen(r), v, sizeof(v)));
    CHECK(strcmp(v, "xon/xoff") == 0);

    const char *noisy = "\r\n\"115200\"\r\n";
    CHECK(zpl_sgd_value((const uint8_t *)noisy, strlen(noisy), v, sizeof(v)));
    CHECK(strcmp(v, "115200") == 0);

    const char *unterminated = "\"abc";
    CHECK(!zpl_sgd_value((const uint8_t *)unterminated, strlen(unterminated), v, sizeof(v)));

    /* value longer than output buffer is rejected, not truncated */
    char small[4];
    CHECK(!zpl_sgd_value((const uint8_t *)r, strlen(r), small, sizeof(small)));

    /* loopback: our own command echoed back is not a reply */
    const char *echo = "! U1 getvar \"device.languages\"\r\n";
    CHECK(!zpl_sgd_value((const uint8_t *)echo, strlen(echo), v, sizeof(v)));
    const char *echo_partial = "! U1 getvar \"comm.handshake\"";
    CHECK(!zpl_sgd_value((const uint8_t *)echo_partial, strlen(echo_partial), v, sizeof(v)));

    /* echo followed by a real reply: the reply wins */
    const char *echo_reply = "! U1 getvar \"comm.handshake\"\r\n\"xon/xoff\"";
    CHECK(zpl_sgd_value((const uint8_t *)echo_reply, strlen(echo_reply), v, sizeof(v)));
    CHECK(strcmp(v, "xon/xoff") == 0);
}

static void test_url_decode(void)
{
    char out[32];
    CHECK(url_decode("a+b%26c%25", out, sizeof(out)) == 6);
    CHECK(strcmp(out, "a b&c%") == 0);
    CHECK(url_decode("%7e%7E", out, sizeof(out)) == 2);
    CHECK(strcmp(out, "~~") == 0);
    /* malformed escapes are passed through literally */
    CHECK(url_decode("100%", out, sizeof(out)) == 4);
    CHECK(strcmp(out, "100%") == 0);
    CHECK(url_decode("%zz", out, sizeof(out)) == 3);
    CHECK(strcmp(out, "%zz") == 0);
    /* output truncation keeps NUL termination */
    char tiny[4];
    url_decode("abcdef", tiny, sizeof(tiny));
    CHECK(strcmp(tiny, "abc") == 0);
}

static void test_form_get(void)
{
    char v[65];
    const char *body = "ssid=My+Net%21&pass=p%26ss+w%3Drd&x=1";
    CHECK(form_get(body, "ssid", v, sizeof(v)));
    CHECK(strcmp(v, "My Net!") == 0);
    CHECK(form_get(body, "pass", v, sizeof(v)));
    CHECK(strcmp(v, "p&ss w=rd") == 0);
    CHECK(form_get(body, "x", v, sizeof(v)));
    CHECK(strcmp(v, "1") == 0);
    CHECK(!form_get(body, "missing", v, sizeof(v)));
    /* key must match a whole name, not a suffix */
    CHECK(!form_get("xssid=1", "ssid", v, sizeof(v)));
    /* empty value is present */
    CHECK(form_get("pass=&ssid=a", "pass", v, sizeof(v)));
    CHECK(strcmp(v, "") == 0);
}

static void test_baud_candidates(void)
{
    uint32_t c[6];
    size_t n;
    zpl_baud_candidates(9600, c, &n);
    CHECK(n == 5);
    CHECK(c[0] == 9600 && c[1] == 115200 && c[2] == 19200 && c[3] == 38400 && c[4] == 57600);

    zpl_baud_candidates(0, c, &n);
    CHECK(n == 5);
    CHECK(c[0] == 115200 && c[1] == 9600 && c[4] == 57600);

    zpl_baud_candidates(12345, c, &n);
    CHECK(n == 5);
    CHECK(c[0] == 115200);

    zpl_baud_candidates(115200, c, &n);
    CHECK(n == 5);
    CHECK(c[0] == 115200 && c[1] == 9600);
}


static void test_sc_command(void)
{
    char out[64];
    CHECK(zpl_sc_command(out, sizeof(out), 115200) > 0);
    CHECK(strcmp(out, "^XA^SC115200,8,N,1,X,N^JUS^XZ") == 0);
    CHECK(zpl_sc_command(out, sizeof(out), 9600) > 0);
    CHECK(strcmp(out, "^XA^SC9600,8,N,1,X,N^JUS^XZ") == 0);
    char tiny[8];
    CHECK(zpl_sc_command(tiny, sizeof(tiny), 9600) == 0);
}

/*
 * ^HH sends the configuration label as text. The line layout used here
 * (value, dot leader, label) is the configuration-label layout; the exact
 * GK420t label text is still to be confirmed against a real printer.
 */
static void test_hh_handshake(void)
{
    char v[24];
    const char *hh = "\x02"
                     "  PRINTER CONFIGURATION\r\n"
                     "  9600.............BAUD\r\n"
                     "  8 BITS...........DATA BITS\r\n"
                     "  XON/XOFF.........HOST HANDSHAKE\r\n"
                     "  NONE.............PROTOCOL\r\n"
                     "\x03";
    CHECK(zpl_hh_handshake((const uint8_t *)hh, strlen(hh), v, sizeof(v)));
    CHECK(strcmp(v, "XON/XOFF") == 0);
    CHECK(zpl_handshake_is_xon(v));

    const char *dtr = "  DTR & XON/XOFF...HOST HANDSHAKE\r\n";
    CHECK(zpl_hh_handshake((const uint8_t *)dtr, strlen(dtr), v, sizeof(v)));
    CHECK(strcmp(v, "DTR & XON/XOFF") == 0);
    CHECK(zpl_handshake_is_xon(v));

    const char *hw = "  DTR/DSR..........HOST HANDSHAKE\r\n";
    CHECK(zpl_hh_handshake((const uint8_t *)hw, strlen(hw), v, sizeof(v)));
    CHECK(!zpl_handshake_is_xon(v));

    const char *none = "  9600.............BAUD\r\n";
    CHECK(!zpl_hh_handshake((const uint8_t *)none, strlen(none), v, sizeof(v)));

    CHECK(zpl_handshake_is_xon("xon/xoff"));
    CHECK(!zpl_handshake_is_xon("rts/cts"));
    CHECK(!zpl_handshake_is_xon(""));
}


/*
 * A real ^HH reply from the bench GX430t (300 dpi, firmware V56.17.16Z),
 * captured 2026-10-16: STX, then CR LF lines of two spaces, the value
 * left-justified in a 20-character field, and the name. Expected values
 * below are typed from that file, not from the parser.
 */
static size_t load_fixture(const char *name, uint8_t *buf, size_t cap)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", FIXTURE_DIR, name);
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        printf("FAIL cannot open %s\n", path);
        s_failures++;
        return 0;
    }
    size_t n = fread(buf, 1, cap, f);
    fclose(f);
    return n;
}

static uint8_t s_hh_fixture[4096];
static size_t s_hh_fixture_len;

static void test_hh_handshake_fixture(void)
{
    char v[24];
    CHECK(s_hh_fixture_len == 2522);
    CHECK(zpl_hh_handshake(s_hh_fixture, s_hh_fixture_len, v, sizeof(v)));
    CHECK(strcmp(v, "XON/XOFF") == 0);
}

static void check_pair(const zpl_hh_t *hh, const char *name, const char *value)
{
    const char *v = zpl_hh_get(hh, name);
    s_checks++;
    if (v == NULL || strcmp(v, value) != 0) {
        s_failures++;
        printf("FAIL ^HH %s: expected \"%s\", got %s%s%s\n", name, value, v ? "\"" : "", v ? v : "nothing",
               v ? "\"" : "");
    }
}

/* Every expected value here is copied from the text of fixtures/gx430t_hh.bin. */
static void test_hh_parse_fixture(void)
{
    static zpl_hh_t hh;
    CHECK(zpl_hh_parse(s_hh_fixture, s_hh_fixture_len, &hh));
    CHECK(hh.count == 60);  /* 60 configuration lines between STX and ETX */
    CHECK(strcmp(hh.pair[0].name, "DARKNESS") == 0);  /* first line, after the STX */
    CHECK(strcmp(hh.pair[hh.count - 1].name, "EARLY WARNING") == 0);
    check_pair(&hh, "DARKNESS", "15.0");
    check_pair(&hh, "PRINT SPEED", "4 IPS");
    check_pair(&hh, "TEAR OFF", "+000");
    check_pair(&hh, "PRINT MODE", "TEAR OFF");
    check_pair(&hh, "MEDIA TYPE", "GAP/NOTCH");
    check_pair(&hh, "PRINT METHOD", "DIRECT-THERMAL");
    check_pair(&hh, "PRINT WIDTH", "1200");
    check_pair(&hh, "LABEL LENGTH", "1938");
    check_pair(&hh, "MAXIMUM LENGTH", "39.0IN   975MM");   /* runs of spaces inside a value */
    check_pair(&hh, "USB COMM.", "NOT CONNECTED");
    check_pair(&hh, "BAUD", "115200");
    check_pair(&hh, "HOST HANDSHAKE", "XON/XOFF");
    check_pair(&hh, "SER COMM. MODE", "AUTO - DTE");
    check_pair(&hh, "CONTROL CHAR", "<~>  7EH");
    check_pair(&hh, "ZPL MODE", "ZPL II");
    check_pair(&hh, "MEDIA POWER UP", "NO MOTION");
    check_pair(&hh, "LEFT POSITION", "+0000");
    check_pair(&hh, "MODES DISABLED", "...");               /* a value that is only dots */
    check_pair(&hh, "RESOLUTION", "1280 12/MM FULL");
    check_pair(&hh, "FIRMWARE", "V56.17.16Z <-");
    check_pair(&hh, "RAM", "2104k R:");                      /* "2104k............R:": dot leader inside */
    check_pair(&hh, "ONBOARD FLASH", "1536k E:");
    check_pair(&hh, "LAST CLEANED", "7,509,549 IN");
    check_pair(&hh, "SERIAL NUMBER", "32J133802399");
    check_pair(&hh, "EARLY WARNING", "MAINT. OFF");
    CHECK(zpl_hh_get(&hh, "NO SUCH LINE") == NULL);
}

static void test_hh_parse_edges(void)
{
    static zpl_hh_t hh;
    /* older dot-leader layout */
    const char *dots = "\x02  9600.............BAUD\r\n  DTR & XON/XOFF...HOST HANDSHAKE\r\n\x03";
    CHECK(zpl_hh_parse((const uint8_t *)dots, strlen(dots), &hh));
    CHECK(hh.count == 2);
    check_pair(&hh, "BAUD", "9600");
    check_pair(&hh, "HOST HANDSHAKE", "DTR & XON/XOFF");
    /* a truncated dump still gives the lines that arrived, without the cut-off one */
    const char *cut = "\x02  15.0                DARKNESS\r\n  4 IPS               PRI";
    CHECK(zpl_hh_parse((const uint8_t *)cut, strlen(cut), &hh));
    CHECK(hh.count == 1);
    /* wrong-baud noise and text without a separator are not configuration lines */
    const char *noise = "\xf0\x8e\x02\x7f  15.0                DARK\xffNESS\r\n  PRINTER CONFIGURATION\r\n";
    CHECK(!zpl_hh_parse((const uint8_t *)noise, strlen(noise), &hh));
    CHECK(!zpl_hh_parse((const uint8_t *)"", 0, &hh));
    /* more lines than fit: the first ZPL_HH_MAX_PAIRS are kept */
    static char many[ZPL_HH_MAX_PAIRS * 2 * 40];
    many[0] = '\0';
    for (int i = 0; i < ZPL_HH_MAX_PAIRS * 2; i++) {
        char line[40];
        snprintf(line, sizeof(line), "  %-20dNAME%d\r\n", i, i);
        strcat(many, line);
    }
    CHECK(zpl_hh_parse((const uint8_t *)many, strlen(many), &hh));
    CHECK(hh.count == ZPL_HH_MAX_PAIRS);
    check_pair(&hh, "NAME79", "79");
    /* over-long value and name are truncated, still terminated */
    const char *lng = "  ABCDEFGHIJKLMNOPQRST..........................VERY LONG NAME THAT DOES NOT FIT AT ALL\r\n";
    CHECK(zpl_hh_parse((const uint8_t *)lng, strlen(lng), &hh));
    CHECK(strlen(hh.pair[0].name) == ZPL_HH_NAME_LEN - 1);
    CHECK(strcmp(hh.pair[0].value, "ABCDEFGHIJKLMNOPQRST") == 0);
}

static void cfg_from_fixture(zpl_printer_cfg_t *cfg)
{
    static zpl_hh_t hh;
    zpl_hh_parse(s_hh_fixture, s_hh_fixture_len, &hh);
    zpl_cfg_from_hh(&hh, cfg);
}

static void test_cfg_from_fixture(void)
{
    zpl_printer_cfg_t c;
    cfg_from_fixture(&c);
    CHECK(c.dpmm == 12);           /* "1280 12/MM FULL" */
    CHECK(c.head_dots == 1280);
    CHECK(c.known == (1u << ZPL_CFG_COUNT) - 1);
    CHECK(c.v[ZPL_CFG_DARKNESS] == 15);       /* "15.0" */
    CHECK(c.v[ZPL_CFG_SPEED] == 4);           /* "4 IPS" */
    CHECK(c.v[ZPL_CFG_METHOD] == 'D');        /* "DIRECT-THERMAL" */
    CHECK(c.v[ZPL_CFG_MODE] == 'T');          /* "TEAR OFF" */
    CHECK(c.v[ZPL_CFG_MEDIA] == 'Y');         /* "GAP/NOTCH" */
    CHECK(c.v[ZPL_CFG_SENSOR] == 'A');        /* "AUTO" */
    CHECK(c.v[ZPL_CFG_ZPL_MODE] == 2);        /* "ZPL II" */
    CHECK(c.v[ZPL_CFG_WIDTH] == 1200);
    CHECK(c.v[ZPL_CFG_LENGTH] == 1938);
    /* 39.0 in at 12 dots/mm: 39.0 x 304.8 dots/in (the guide's ^LL formula) = 11887.2 */
    CHECK(c.v[ZPL_CFG_MAX_LENGTH] == 11887);
    CHECK(c.v[ZPL_CFG_TEAR_OFF] == 0);        /* "+000" */
    CHECK(c.v[ZPL_CFG_LABEL_TOP] == 0);       /* "+000" */
    CHECK(c.v[ZPL_CFG_LEFT_POS] == 0);        /* "+0000" */
    CHECK(c.v[ZPL_CFG_POWER_UP] == 'N');      /* "NO MOTION" */
    CHECK(c.v[ZPL_CFG_HEAD_CLOSE] == 'F');    /* "FEED" */
    CHECK(c.v[ZPL_CFG_BACKFEED] == 'N');      /* "DEFAULT" */
    CHECK(c.v[ZPL_CFG_EARLY_WARNING] == 'D'); /* "MAINT. OFF" */

    /* unknown spellings leave the field unknown rather than guessed */
    static zpl_hh_t hh;
    const char *odd = "  SOMETHING NEW       PRINT METHOD\r\n  9 IPS               PRINT SPEED\r\n"
                      "  15.0                DARKNESS\r\n";
    zpl_hh_parse((const uint8_t *)odd, strlen(odd), &hh);
    zpl_cfg_from_hh(&hh, &c);
    CHECK(c.known == (1u << ZPL_CFG_DARKNESS));   /* 9 ips is beyond ^PR's 2..6 without a resolution */
    CHECK(c.dpmm == 0);
}

/* A form submitted without edits must send nothing: format each value, parse it back, compare. */
static void test_cfg_round_trip(void)
{
    zpl_printer_cfg_t cur;
    cfg_from_fixture(&cur);
    uint32_t changed = 0;
    for (int f = 0; f < ZPL_CFG_COUNT; f++) {
        char text[16];
        int v = -12345;
        zpl_cfg_format((zpl_cfg_field_t)f, cur.v[f], text, sizeof(text));
        CHECK(zpl_cfg_parse(&cur, (zpl_cfg_field_t)f, text, &v));
        if (v != cur.v[f]) {
            changed |= 1u << f;
        }
    }
    CHECK(changed == 0);
    char out[64];
    CHECK(zpl_cfg_commands(&cur, 0, out, sizeof(out)) == 0);
    CHECK(out[0] == '\0');
}

static void test_cfg_parse(void)
{
    zpl_printer_cfg_t c;
    cfg_from_fixture(&c);
    int v;
    CHECK(zpl_cfg_parse(&c, ZPL_CFG_DARKNESS, "30", &v) && v == 30);
    CHECK(zpl_cfg_parse(&c, ZPL_CFG_DARKNESS, "0", &v) && v == 0);
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_DARKNESS, "31", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_DARKNESS, "-1", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_DARKNESS, "15.0", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_DARKNESS, " 15", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_DARKNESS, "15x", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_DARKNESS, "", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_DARKNESS, "1e1", &v));
    CHECK(zpl_cfg_parse(&c, ZPL_CFG_SPEED, "2", &v) && v == 2);
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_SPEED, "1", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_SPEED, "5", &v));    /* GX430t (12 dots/mm): 4 ips at most */
    CHECK(zpl_cfg_parse(&c, ZPL_CFG_TEAR_OFF, "-120", &v) && v == -120);
    CHECK(zpl_cfg_parse(&c, ZPL_CFG_TEAR_OFF, "+120", &v) && v == 120);
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_TEAR_OFF, "121", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_LABEL_TOP, "-121", &v));
    CHECK(zpl_cfg_parse(&c, ZPL_CFG_LEFT_POS, "-9999", &v) && v == -9999);
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_LEFT_POS, "10000", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_LEFT_POS, "99999999999", &v));
    CHECK(zpl_cfg_parse(&c, ZPL_CFG_WIDTH, "1280", &v) && v == 1280);
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_WIDTH, "1281", &v));   /* printhead is 1280 dots */
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_WIDTH, "1", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_LENGTH, "0", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_LENGTH, "32001", &v));
    CHECK(zpl_cfg_parse(&c, ZPL_CFG_MAX_LENGTH, "600", &v) && v == 600);  /* 300 dpi x 2 */
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_MAX_LENGTH, "599", &v));
    CHECK(zpl_cfg_parse(&c, ZPL_CFG_METHOD, "T", &v) && v == 'T');
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_METHOD, "X", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_METHOD, "t", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_METHOD, "TT", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_MODE, "A", &v));      /* applicator: not offered */
    CHECK(zpl_cfg_parse(&c, ZPL_CFG_MODE, "D", &v) && v == 'D');
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_ZPL_MODE, "3", &v));
    CHECK(zpl_cfg_parse(&c, ZPL_CFG_BACKFEED, "60", &v) && v == 60);
    CHECK(zpl_cfg_parse(&c, ZPL_CFG_BACKFEED, "O", &v) && v == 'O');
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_BACKFEED, "55", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_BACKFEED, "100", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_BACKFEED, "0", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_EARLY_WARNING, "Y", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_COUNT, "1", &v));
}

static void test_cfg_commands(void)
{
    zpl_printer_cfg_t c;
    cfg_from_fixture(&c);
    char out[256];
    c.v[ZPL_CFG_DARKNESS] = 20;
    CHECK(zpl_cfg_commands(&c, 1u << ZPL_CFG_DARKNESS, out, sizeof(out)) > 0);
    CHECK(strcmp(out, "~SD20^XA^JUS^XZ") == 0);
    c.v[ZPL_CFG_DARKNESS] = 5;
    zpl_cfg_commands(&c, 1u << ZPL_CFG_DARKNESS, out, sizeof(out));
    CHECK(strcmp(out, "~SD05^XA^JUS^XZ") == 0);

    c.v[ZPL_CFG_TEAR_OFF] = -5;
    zpl_cfg_commands(&c, 1u << ZPL_CFG_TEAR_OFF, out, sizeof(out));
    CHECK(strcmp(out, "~TA-05^XA^JUS^XZ") == 0);
    c.v[ZPL_CFG_TEAR_OFF] = 7;
    zpl_cfg_commands(&c, 1u << ZPL_CFG_TEAR_OFF, out, sizeof(out));
    CHECK(strcmp(out, "~TA007^XA^JUS^XZ") == 0);
    c.v[ZPL_CFG_TEAR_OFF] = -120;
    zpl_cfg_commands(&c, 1u << ZPL_CFG_TEAR_OFF, out, sizeof(out));
    CHECK(strcmp(out, "~TA-120^XA^JUS^XZ") == 0);

    c.v[ZPL_CFG_BACKFEED] = 'A';
    zpl_cfg_commands(&c, 1u << ZPL_CFG_BACKFEED, out, sizeof(out));
    CHECK(strcmp(out, "~JSA^XA^JUS^XZ") == 0);
    c.v[ZPL_CFG_BACKFEED] = 60;
    zpl_cfg_commands(&c, 1u << ZPL_CFG_BACKFEED, out, sizeof(out));
    CHECK(strcmp(out, "~JS60^XA^JUS^XZ") == 0);

    /* one changed ^MF parameter sends both */
    c.v[ZPL_CFG_POWER_UP] = 'C';
    zpl_cfg_commands(&c, 1u << ZPL_CFG_POWER_UP, out, sizeof(out));
    CHECK(strcmp(out, "^XA^MFC,F^XZ^XA^JUS^XZ") == 0);

    c.v[ZPL_CFG_SPEED] = 3;
    c.v[ZPL_CFG_METHOD] = 'T';
    c.v[ZPL_CFG_MODE] = 'P';
    c.v[ZPL_CFG_MEDIA] = 'M';
    c.v[ZPL_CFG_SENSOR] = 'R';
    c.v[ZPL_CFG_WIDTH] = 812;
    c.v[ZPL_CFG_LENGTH] = 1218;
    c.v[ZPL_CFG_MAX_LENGTH] = 3000;
    c.v[ZPL_CFG_LABEL_TOP] = -10;
    c.v[ZPL_CFG_LEFT_POS] = 25;
    c.v[ZPL_CFG_EARLY_WARNING] = 'E';
    c.v[ZPL_CFG_ZPL_MODE] = 1;
    uint32_t all = (1u << ZPL_CFG_COUNT) - 1;
    CHECK(zpl_cfg_commands(&c, all, out, sizeof(out)) == strlen(out));
    CHECK(strcmp(out, "~SD05~TA-120~JS60"
                      "^XA^PR3^MTT^MMP^MNM^JSR^PW812^LL1218^ML3000^LT-10^LS25^MFC,F^JH,,,,,E^SZ1^XZ"
                      "^XA^JUS^XZ") == 0);

    /* out of range or unknown fields refuse the whole change */
    cfg_from_fixture(&c);
    c.v[ZPL_CFG_DARKNESS] = 31;
    CHECK(zpl_cfg_commands(&c, 1u << ZPL_CFG_DARKNESS, out, sizeof(out)) == 0);
    CHECK(out[0] == '\0');
    cfg_from_fixture(&c);
    c.v[ZPL_CFG_LEFT_POS] = -10000;
    CHECK(zpl_cfg_commands(&c, 1u << ZPL_CFG_LEFT_POS, out, sizeof(out)) == 0);
    cfg_from_fixture(&c);
    c.v[ZPL_CFG_MODE] = 'X';
    CHECK(zpl_cfg_commands(&c, 1u << ZPL_CFG_MODE, out, sizeof(out)) == 0);
    cfg_from_fixture(&c);
    c.known &= ~(1u << ZPL_CFG_HEAD_CLOSE);
    CHECK(zpl_cfg_commands(&c, 1u << ZPL_CFG_POWER_UP, out, sizeof(out)) == 0);
    CHECK(zpl_cfg_commands(&c, 1u << ZPL_CFG_HEAD_CLOSE, out, sizeof(out)) == 0);
    CHECK(zpl_cfg_commands(&c, 1u << ZPL_CFG_COUNT, out, sizeof(out)) == 0);
    /* too small for the result: nothing, not a truncated command */
    cfg_from_fixture(&c);
    char small[12];
    CHECK(zpl_cfg_commands(&c, 1u << ZPL_CFG_DARKNESS, small, sizeof(small)) == 0);
    CHECK(small[0] == '\0');
}

/*
 * Every option the settings page offers, and both ends of every number
 * range: format, parse back, build a command. The option lists mirror
 * s_printer_ui in web.c.
 */
static void test_cfg_all_options(void)
{
    static const struct {
        zpl_cfg_field_t f;
        const char *values;
    } opts[] = {
        {ZPL_CFG_SPEED, "2 3 4"},
        {ZPL_CFG_METHOD, "D T"},
        {ZPL_CFG_MODE, "T P R C D"},
        {ZPL_CFG_MEDIA, "Y M N"},
        {ZPL_CFG_SENSOR, "A R T"},
        {ZPL_CFG_POWER_UP, "F C L N"},
        {ZPL_CFG_HEAD_CLOSE, "F C L N"},
        {ZPL_CFG_BACKFEED, "N A B O 10 20 30 40 50 60 70 80 90"},
        {ZPL_CFG_EARLY_WARNING, "D E"},
        {ZPL_CFG_ZPL_MODE, "2 1"},
    };
    zpl_printer_cfg_t c;
    cfg_from_fixture(&c);
    for (size_t i = 0; i < sizeof(opts) / sizeof(opts[0]); i++) {
        char list[64];
        strcpy(list, opts[i].values);
        for (char *tok = strtok(list, " "); tok != NULL; tok = strtok(NULL, " ")) {
            int v;
            char text[16];
            char out[128];
            bool ok = zpl_cfg_parse(&c, opts[i].f, tok, &v);
            zpl_cfg_format(opts[i].f, v, text, sizeof(text));
            zpl_printer_cfg_t want = c;
            want.v[opts[i].f] = v;
            size_t n = ok ? zpl_cfg_commands(&want, 1u << opts[i].f, out, sizeof(out)) : 0;
            s_checks++;
            if (!ok || strcmp(text, tok) != 0 || n == 0 || strstr(out, tok) == NULL) {
                s_failures++;
                printf("FAIL option %s of field %d: parse %d, format \"%s\", command \"%s\"\n", tok,
                       (int)opts[i].f, ok, text, n ? out : "");
            }
        }
    }
    static const zpl_cfg_field_t ints[] = {ZPL_CFG_DARKNESS, ZPL_CFG_WIDTH, ZPL_CFG_LENGTH, ZPL_CFG_MAX_LENGTH,
                                           ZPL_CFG_TEAR_OFF, ZPL_CFG_LABEL_TOP, ZPL_CFG_LEFT_POS};
    for (size_t i = 0; i < sizeof(ints) / sizeof(ints[0]); i++) {
        int min;
        int max;
        zpl_cfg_range(&c, ints[i], &min, &max);
        int ends[2] = {min, max};
        for (int k = 0; k < 2; k++) {
            char text[16];
            char out[128];
            int v;
            zpl_cfg_format(ints[i], ends[k], text, sizeof(text));
            zpl_printer_cfg_t want = c;
            want.v[ints[i]] = ends[k];
            CHECK(zpl_cfg_parse(&c, ints[i], text, &v) && v == ends[k]);
            CHECK(zpl_cfg_commands(&want, 1u << ints[i], out, sizeof(out)) > 0);
            CHECK(!zpl_cfg_value_valid(&c, ints[i], ends[k] + (k ? 1 : -1)));
        }
    }
    /* percentages that are also letter codes ('F' = 70, 'P' = 80, 'Z' = 90) stay numbers */
    char out[64];
    c.v[ZPL_CFG_BACKFEED] = 70;
    zpl_cfg_commands(&c, 1u << ZPL_CFG_BACKFEED, out, sizeof(out));
    CHECK(strcmp(out, "~JS70^XA^JUS^XZ") == 0);
    c.v[ZPL_CFG_BACKFEED] = 90;
    zpl_cfg_commands(&c, 1u << ZPL_CFG_BACKFEED, out, sizeof(out));
    CHECK(strcmp(out, "~JS90^XA^JUS^XZ") == 0);
    int v;
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_BACKFEED, "F", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_BACKFEED, "P", &v));
    CHECK(!zpl_cfg_parse(&c, ZPL_CFG_BACKFEED, "Z", &v));
}

static void test_action_commands(void)
{
    CHECK(strcmp(zpl_action_command("calibrate"), "~JC") == 0);
    CHECK(strcmp(zpl_action_command("config_label"), "~WC") == 0);
    CHECK(strcmp(zpl_action_command("feed"), "~PH") == 0);
    CHECK(strcmp(zpl_action_command("reset_counter1"), "~RO1") == 0);
    CHECK(strcmp(zpl_action_command("reset_counter2"), "~RO2") == 0);
    CHECK(strcmp(zpl_action_command("reset_head_cleaned"), "~ROC") == 0);
    CHECK(strcmp(zpl_action_command("reload_saved"), "^XA^JUR^XZ") == 0);
    CHECK(strcmp(zpl_action_command("factory_defaults"), "^XA^JUF^JUS^XZ") == 0);
    CHECK(zpl_action_command("format_flash") == NULL);
    CHECK(zpl_action_command("") == NULL);
}

static void test_hostname(void)
{
    CHECK(netcfg_hostname_valid("zebserial"));
    CHECK(netcfg_hostname_valid("zeb-2"));
    CHECK(netcfg_hostname_valid("a"));
    CHECK(!netcfg_hostname_valid(""));
    CHECK(!netcfg_hostname_valid("-zeb"));
    CHECK(!netcfg_hostname_valid("zeb-"));
    CHECK(!netcfg_hostname_valid("Zeb"));
    CHECK(!netcfg_hostname_valid("zeb serial"));
    CHECK(!netcfg_hostname_valid("zeb.local"));
    CHECK(netcfg_hostname_valid("abcdefghijklmnopqrstuvwxyz012345"));   /* 32 */
    CHECK(!netcfg_hostname_valid("abcdefghijklmnopqrstuvwxyz0123456")); /* 33 */
}

static void test_ipv4_parse(void)
{
    uint32_t v;
    CHECK(netcfg_ipv4_parse("192.168.1.20", &v) && v == 0xC0A80114u);
    CHECK(netcfg_ipv4_parse("0.0.0.0", &v) && v == 0);
    CHECK(netcfg_ipv4_parse("255.255.255.0", &v) && v == 0xFFFFFF00u);
    CHECK(!netcfg_ipv4_parse("256.1.1.1", &v));
    CHECK(!netcfg_ipv4_parse("1.2.3", &v));
    CHECK(!netcfg_ipv4_parse("1.2.3.4.5", &v));
    CHECK(!netcfg_ipv4_parse("1..3.4", &v));
    CHECK(!netcfg_ipv4_parse(" 1.2.3.4", &v));
    CHECK(!netcfg_ipv4_parse("1.2.3.4 ", &v));
    CHECK(!netcfg_ipv4_parse("01.2.3.4", &v));   /* leading zeros are ambiguous (octal) */
    CHECK(!netcfg_ipv4_parse("", &v));
}

static void test_static_valid(void)
{
    uint32_t ip, mask, gw;
    netcfg_ipv4_parse("192.168.1.20", &ip);
    netcfg_ipv4_parse("255.255.255.0", &mask);
    netcfg_ipv4_parse("192.168.1.1", &gw);
    CHECK(netcfg_static_valid(ip, mask, gw));
    CHECK(netcfg_static_valid(ip, mask, 0));                  /* no gateway */
    CHECK(!netcfg_static_valid(ip, mask, ip));                /* gw == ip */
    CHECK(!netcfg_static_valid(ip, mask, 0xC0A80201u));       /* gw off-subnet */
    CHECK(!netcfg_static_valid(0xC0A80100u, mask, gw));       /* network address */
    CHECK(!netcfg_static_valid(0xC0A801FFu, mask, gw));       /* broadcast */
    CHECK(!netcfg_static_valid(ip, 0xFF00FF00u, gw));         /* non-contiguous mask */
    CHECK(!netcfg_static_valid(ip, 0, gw));
    CHECK(!netcfg_static_valid(ip, 0xFFFFFFFFu, gw));
    CHECK(!netcfg_static_valid(0x7F000001u, mask, gw));       /* 127.0.0.1 loopback */
    CHECK(!netcfg_static_valid(0xE0000001u, mask, gw));       /* 224.0.0.1 multicast */
    CHECK(!netcfg_static_valid(0x00010203u, mask, gw));       /* 0.1.2.3 zero-net */
    CHECK(!netcfg_static_valid(0xC0A80402u, mask, 0xC0A80401u)); /* 192.168.4.2 setup AP subnet */
}

static void test_hs_multi_digit_flags(void)
{
    zpl_hs_t hs;
    /* flags are numeric: "00" is false, "01" is true */
    CHECK(parse_str("\x02" "030,00,01,1245,000,0,0,0,000,0,0,0" "\x03\r\n" S2_OK S3_OK, &hs));
    CHECK(!hs.paper_out);
    CHECK(hs.pause);
    CHECK(parse_str(S1_OK "\x02" "001,0,00,000,1,2,6,0,00000000,1,000" "\x03\r\n" S3_OK, &hs));
    CHECK(!hs.head_up);
    CHECK(!hs.ribbon_out);
}

static void test_json_escape(void)
{
    char out[64];
    CHECK(json_escape("plain", out, sizeof(out)) == 5);
    CHECK(strcmp(out, "plain") == 0);
    json_escape("a\"b\\c", out, sizeof(out));
    CHECK(strcmp(out, "a\\\"b\\\\c") == 0);
    json_escape("t\tn\nr\r\x01\x1f", out, sizeof(out));
    CHECK(strcmp(out, "t\\tn\\nr\\r\\u0001\\u001f") == 0);
    json_escape("caf\xc3\xa9 </x>", out, sizeof(out));
    CHECK(strcmp(out, "caf\xc3\xa9 \\u003c/x\\u003e") == 0);  /* UTF-8 passes; < > never reach markup */
    /* never a partial escape at the end: "ab" + \u0001 needs 9 bytes with the NUL */
    char small[8];
    CHECK(json_escape("ab\x01", small, sizeof(small)) == 2);
    CHECK(strcmp(small, "ab") == 0);
    CHECK(json_escape("a\"", small, 3) == 1);
    CHECK(strcmp(small, "a") == 0);
    CHECK(json_escape("x", small, 1) == 0);
    CHECK(small[0] == '\0');
}

static void test_origin_matches_host(void)
{
    CHECK(url_origin_matches_host("http://192.168.4.1", "192.168.4.1"));
    CHECK(url_origin_matches_host("http://ZebSerial.local", "zebserial.local"));
    CHECK(url_origin_matches_host("http://zebserial.local:8080", "zebserial.local:8080"));
    CHECK(url_origin_matches_host("http://[fe80::1]/settings?x=1", "[fe80::1]"));
    CHECK(url_origin_matches_host("http://10.0.0.5/", "10.0.0.5"));
    CHECK(!url_origin_matches_host("http://evil.example", "zebserial.local"));
    CHECK(!url_origin_matches_host("http://zebserial.local.evil.example/", "zebserial.local"));
    CHECK(!url_origin_matches_host("http://zebserial.local:81", "zebserial.local"));
    CHECK(!url_origin_matches_host("http://zebserial", "zebserial.local"));
    CHECK(!url_origin_matches_host("null", "zebserial.local"));
    CHECK(!url_origin_matches_host("", "zebserial.local"));
    CHECK(!url_origin_matches_host("http://zebserial.local", ""));
}

int main(void)
{
    test_hs_ok();
    test_hs_handshake_bit();
    test_hs_with_leading_garbage();
    test_hs_rejects_short_frames();
    test_hs_needs_three_frames();
    test_sgd_value();
    test_url_decode();
    test_form_get();
    test_baud_candidates();
    test_sc_command();
    test_hh_handshake();
    s_hh_fixture_len = load_fixture("gx430t_hh.bin", s_hh_fixture, sizeof(s_hh_fixture));
    test_hh_handshake_fixture();
    test_hh_parse_fixture();
    test_hh_parse_edges();
    test_cfg_from_fixture();
    test_cfg_round_trip();
    test_cfg_parse();
    test_cfg_commands();
    test_cfg_all_options();
    test_action_commands();
    test_hostname();
    test_ipv4_parse();
    test_static_valid();
    test_hs_multi_digit_flags();
    test_json_escape();
    test_origin_matches_host();
    printf("%d checks, %d failures\n", s_checks, s_failures);
    return s_failures ? 1 : 0;
}
