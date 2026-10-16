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
    test_hostname();
    test_ipv4_parse();
    test_static_valid();
    test_hs_multi_digit_flags();
    test_json_escape();
    test_origin_matches_host();
    printf("%d checks, %d failures\n", s_checks, s_failures);
    return s_failures ? 1 : 0;
}
