// Host unit tests for the portable PicoRTK modules.
// Build: g++ -std=c++17 -Wall -I../firmware/PicoRTK test_main.cpp ../firmware/PicoRTK/{nmea,rtcm,selector,ntrip_proto}.cpp -o t && ./t
#include "nmea.h"
#include "ntrip_proto.h"
#include "ring.h"
#include "rtcm.h"
#include "selector.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int fails = 0, checks = 0;
#define CHECK(c)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        checks++;                                                                                                      \
        if (!(c))                                                                                                      \
        {                                                                                                              \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);                                                       \
            fails++;                                                                                                   \
        }                                                                                                              \
    } while (0)

static std::string withChecksum(const char *body)
{
    char out[200];
    nmeaFinish(body, out, sizeof(out));
    return out;
}

static void testNmea()
{
    // Known sentence from the NMEA reference (checksum 47)
    const char *ref = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    CHECK(nmeaChecksumOk(ref, strlen(ref)));
    std::string bad = ref;
    bad[10] = '9';
    CHECK(!nmeaChecksumOk(bad.c_str(), bad.size()));

    // Assembler across noise, binary and a damaged sentence
    NmeaAssembler a;
    std::string gga = withChecksum("$GNGGA,101530.00,3758.1234567,N,02343.7654321,E,4,32,0.5,120.123,M,35.1,M,1.0,0042");
    std::string stream = std::string("\xD3\x00\x13garbage") + gga + "$GNRMC,broken*00\r\n" + gga;
    int got = 0;
    for (unsigned char c : stream)
        if (a.feed(c))
        {
            got++;
            CHECK(std::string(a.sentence()) == gga);
        }
    CHECK(got == 2);
    CHECK(a.bad() >= 1);

    GgaInfo g;
    CHECK(nmeaIsGga(gga.c_str(), gga.size()));
    CHECK(nmeaParseGga(gga.c_str(), gga.size(), g));
    CHECK(g.quality == 4);
    CHECK(g.sats == 32);
    CHECK(g.age > 0.99f && g.age < 1.01f);
    CHECK(g.station == 42);
    CHECK(g.lat > 37.96872 && g.lat < 37.96873);
    CHECK(g.lon > 23.72942 && g.lon < 23.72943);
    CHECK(strcmp(g.utc, "101530.00") == 0);

    std::string noFix = withChecksum("$GNGGA,101531.00,,,,,0,00,99.99,,,,,,");
    CHECK(nmeaParseGga(noFix.c_str(), noFix.size(), g));
    CHECK(g.quality == 0 && g.age < 0 && g.station == -1);

    // Bench GGA: Athens, valid checksum, parses back
    char bench[128];
    size_t bl = nmeaMakeGga(37.968723, 23.729420, 120.5f, 10 * 3600 + 15 * 60 + 30, bench, sizeof(bench));
    CHECK(bl > 40 && nmeaChecksumOk(bench, bl));
    GgaInfo b;
    CHECK(nmeaParseGga(bench, bl, b) && b.quality == 1);
    CHECK(b.lat > 37.96872 && b.lat < 37.96873 && b.lon > 23.72941 && b.lon < 23.72943);
    CHECK(strncmp(b.utc, "101530", 6) == 0);
    bl = nmeaMakeGga(-33.5, -70.25, 0, 0, bench, sizeof(bench));
    CHECK(nmeaParseGga(bench, bl, b) && b.lat < -33.49 && b.lon < -70.24);

    // nmeaFinish keeps an existing checksum
    char out[64];
    nmeaFinish("$PQTMVERNO*58", out, sizeof(out));
    CHECK(strcmp(out, "$PQTMVERNO*58\r\n") == 0);
}

static std::vector<uint8_t> makeRtcm(uint16_t type, uint16_t station, size_t extra)
{
    std::vector<uint8_t> payload(2 + 1 + extra, 0);
    payload[0] = (uint8_t)(type >> 4);
    payload[1] = (uint8_t)((type & 0x0F) << 4) | (uint8_t)((station >> 8) & 0x0F);
    payload[2] = (uint8_t)(station & 0xFF);
    for (size_t i = 0; i < extra; i++)
        payload[3 + i] = (uint8_t)(i * 7);
    std::vector<uint8_t> f = {0xD3, (uint8_t)(payload.size() >> 8), (uint8_t)(payload.size() & 0xFF)};
    f.insert(f.end(), payload.begin(), payload.end());
    uint32_t crc = rtcmCrc24q(f.data(), f.size());
    f.push_back((uint8_t)(crc >> 16));
    f.push_back((uint8_t)(crc >> 8));
    f.push_back((uint8_t)crc);
    return f;
}

static void testRtcm()
{
    const char *v = "123456789";
    CHECK(rtcmCrc24q((const uint8_t *)v, 9) == 0xCDE703);

    RtcmFramer r;
    auto f1005 = makeRtcm(1005, 2003, 16);
    auto f1077 = makeRtcm(1077, 2003, 300);
    std::vector<uint8_t> s = {'I', 'C', 'Y', 0xD3, 0xFF}; // junk incl. a false preamble
    s.insert(s.end(), f1005.begin(), f1005.end());
    auto broken = f1077;
    broken[50] ^= 0x55;
    s.insert(s.end(), broken.begin(), broken.end());
    s.insert(s.end(), f1077.begin(), f1077.end());
    std::vector<uint16_t> types;
    for (uint8_t b : s)
        if (r.feed(b))
        {
            types.push_back(r.messageType());
            CHECK(r.frameLength() == (r.messageType() == 1005 ? f1005.size() : f1077.size()));
        }
    CHECK(types.size() == 2);
    CHECK(types.size() == 2 && types[0] == 1005 && types[1] == 1077);
    CHECK(r.stationId() == 2003);
    CHECK(r.crcErrors() == 1);
}

static void testSelector()
{
    SourceSelector s;
    bool v[4] = {true, true, false, true}; // own base, VRS, (empty), EUREF
    s.setSources(4, v);
    CHECK(s.active() == 0 && s.validCount() == 3);

    auto d = s.fail(false, 0);
    CHECK(d.switched && d.source == 1 && d.delayMs == 2000 && !d.backoff);
    d = s.fail(false, 0);
    CHECK(d.source == 3 && !d.backoff); // skips the empty slot
    d = s.fail(false, 0);
    CHECK(d.backoff && d.source == 0 && d.delayMs == 15000);
    // Second full cycle backs off longer
    s.fail(false, 0);
    s.fail(false, 0);
    d = s.fail(false, 0);
    CHECK(d.backoff && d.delayMs == 30000);

    // A good connection resets the back-off; fail-back after the interval
    d = s.fail(false, 0);
    CHECK(d.source == 1);
    s.connected(1000);
    CHECK(!s.failbackDue(1000 + 599000));
    CHECK(s.failbackDue(1000 + 600000));
    s.reset(false);
    CHECK(s.active() == 0);

    // Hard failure disables the primary; no fail-back to it
    d = s.fail(true, 0);
    CHECK(d.source == 1 && s.disabled(0));
    s.connected(0);
    CHECK(!s.failbackDue(10000000));
    d = s.fail(true, 0);
    CHECK(d.source == 3);
    d = s.fail(false, 0);
    CHECK(d.backoff && d.source == 3); // only one left
    d = s.fail(true, 0);
    CHECK(d.stopped);
    s.reset(true);
    CHECK(s.usableCount() == 3 && s.active() == 0);

    // Failover off: only the first configured source is used
    s.setFailover(false);
    s.reset(true);
    CHECK(s.validCount() == 1);
    d = s.fail(false, 0);
    CHECK(d.backoff && d.source == 0);

    // Single source with the primary slot empty
    SourceSelector t;
    bool v2[3] = {false, true, false};
    t.setSources(3, v2);
    CHECK(t.active() == 1);
}

static void testNtrip()
{
    char b64[64];
    base64Encode((const uint8_t *)"user:pass", 9, b64, sizeof(b64));
    CHECK(strcmp(b64, "dXNlcjpwYXNz") == 0);

    char req[512];
    CHECK(ntripBuildRequest(req, sizeof(req), "gr.nrtk.eu", 7801, "MSM_VRS", "user", "pass", false, "PicoRTK/0.1") > 0);
    CHECK(strstr(req, "GET /MSM_VRS HTTP/1.0\r\n") != nullptr);
    CHECK(strstr(req, "Authorization: Basic dXNlcjpwYXNz\r\n") != nullptr);
    CHECK(strstr(req, "\r\n\r\n") != nullptr);
    ntripBuildRequest(req, sizeof(req), "caster", 2101, "M", "", "", true, "X");
    CHECK(strstr(req, "Ntrip-Version: Ntrip/2.0") && !strstr(req, "Authorization"));

    NtripResponse r;
    size_t off;
    const char *icy = "ICY 200 OK\r\n\xD3\x00";
    r.reset();
    CHECK(r.feed((const uint8_t *)icy, 4, &off) == NtripReply::Incomplete);
    CHECK(r.feed((const uint8_t *)icy + 4, 10, &off) == NtripReply::Ok);
    CHECK(off == 8); // stream starts after "ICY 200 OK\r\n"

    const char *v2ok = "HTTP/1.1 200 OK\r\nContent-Type: gnss/data\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nabcde\r\n";
    r.reset();
    CHECK(r.feed((const uint8_t *)v2ok, strlen(v2ok), &off) == NtripReply::Ok && r.chunked());
    ChunkDecoder cd;
    cd.reset();
    uint8_t out[64];
    size_t n = cd.decode((const uint8_t *)v2ok + off, strlen(v2ok) - off, out);
    CHECK(n == 5 && memcmp(out, "abcde", 5) == 0 && !cd.error());
    // Chunks split across reads
    const char *split[] = {"3", "\r\nxyz", "\r", "\n2\r\n1", "2\r\n"};
    size_t total = 0;
    for (auto p : split)
        total += cd.decode((const uint8_t *)p, strlen(p), out + total);
    CHECK(total == 5 && memcmp(out, "xyz12", 5) == 0);

    const char *st = "SOURCETABLE 200 OK\r\nServer: x\r\n";
    r.reset();
    CHECK(r.feed((const uint8_t *)st, strlen(st), &off) == NtripReply::Sourcetable);
    const char *st2 = "HTTP/1.1 200 OK\r\nContent-Type: gnss/sourcetable\r\n\r\nSTR;...";
    r.reset();
    CHECK(r.feed((const uint8_t *)st2, strlen(st2), &off) == NtripReply::Sourcetable);
    const char *u = "HTTP/1.0 401 Unauthorized\r\nWWW-Authenticate: Basic\r\n\r\n";
    r.reset();
    CHECK(r.feed((const uint8_t *)u, strlen(u), &off) == NtripReply::Unauthorized);
    const char *busy = "HTTP/1.1 503 Service Unavailable\r\n\r\n";
    r.reset();
    CHECK(r.feed((const uint8_t *)busy, strlen(busy), &off) == NtripReply::SoftError);
    const char *banned = "ERROR - Banned\r\n";
    r.reset();
    CHECK(r.feed((const uint8_t *)banned, strlen(banned), &off) == NtripReply::SoftError);
}

static void testRing()
{
    SpscRing<16> ring;
    uint8_t buf[32];
    CHECK(ring.write((const uint8_t *)"0123456789", 10));
    CHECK(!ring.write((const uint8_t *)"0123456789", 10)); // all or nothing
    CHECK(ring.dropped() == 1);
    CHECK(ring.read(buf, 4) == 4 && memcmp(buf, "0123", 4) == 0);
    CHECK(ring.write((const uint8_t *)"abcdefghij", 10)); // wraps around
    size_t n = ring.read(buf, sizeof(buf));
    CHECK(n == 16 && memcmp(buf, "456789abcdefghij", 16) == 0);
    CHECK(ring.available() == 0);
}

int main()
{
    testNmea();
    testRtcm();
    testSelector();
    testNtrip();
    testRing();
    printf("%d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
