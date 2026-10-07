/*
 * Host tests for zpl_status and urlcodec.
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

int main(void)
{
    test_hs_ok();
    test_hs_with_leading_garbage();
    test_hs_rejects_short_frames();
    test_hs_needs_three_frames();
    test_sgd_value();
    test_url_decode();
    test_form_get();
    test_baud_candidates();
    printf("%d checks, %d failures\n", s_checks, s_failures);
    return s_failures ? 1 : 0;
}
