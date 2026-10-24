// nmea.h - NMEA 0183 sentence assembly, checksum and GGA parsing (portable, unit-tested on the PC)
#pragma once
#include <cstddef>
#include <cstdint>

static const size_t NMEA_MAX = 128; // NMEA limit is 82, Quectel PQTM replies can be longer

// Collects bytes from the receiver and returns complete sentences ("$...*hh\r\n").
// Bytes that are not part of an NMEA sentence (binary, RTCM echoes) are ignored here;
// the raw stream is still forwarded unchanged to the outputs.
class NmeaAssembler
{
  public:
    // Feed one byte. Returns true when a sentence with a valid checksum is ready in sentence().
    bool feed(uint8_t c);
    const char *sentence() const { return _out; }
    size_t length() const { return _outLen; }
    uint32_t good() const { return _good; }
    uint32_t bad() const { return _bad; }

  private:
    char _buf[NMEA_MAX + 3];
    size_t _len = 0;
    bool _in = false;
    char _out[NMEA_MAX + 3];
    size_t _outLen = 0;
    uint32_t _good = 0, _bad = 0;
};

// Validate "$....*hh" (CR/LF optional)
bool nmeaChecksumOk(const char *s, size_t len);
// Compute the XOR checksum of the characters between '$' and '*' (or end)
uint8_t nmeaChecksum(const char *body, size_t len);
// Append "*hh\r\n" to a "$BODY" command if it has no checksum. out must hold len+6.
size_t nmeaFinish(const char *in, char *out, size_t outSize);

struct GgaInfo
{
    bool valid = false;  // parsed a GGA
    int quality = 0;     // 0 none, 1 GNSS, 2 DGNSS, 4 RTK fixed, 5 RTK float, 6 dead reckoning
    int sats = 0;
    float hdop = 0;
    double lat = 0, lon = 0; // degrees, signed
    float alt = 0;           // metres above mean sea level
    float age = -1;          // seconds since last correction, -1 when empty
    int station = -1;        // reference station id, -1 when empty
    char utc[11] = {0};      // hhmmss.ss
};

// Build a GGA for a fixed position (quality 1), e.g. a bench position sent to VRS/nearest casters
// when the receiver has no fix. secOfDay = UTC seconds since midnight. Returns the length.
size_t nmeaMakeGga(double lat, double lon, float alt, uint32_t secOfDay, char *out, size_t outSize);

// True when the sentence is a GGA from any talker ($GPGGA, $GNGGA, ...)
bool nmeaIsGga(const char *s, size_t len);
// Parse a GGA sentence (checksum already verified)
bool nmeaParseGga(const char *s, size_t len, GgaInfo &out);
