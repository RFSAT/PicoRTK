// nmea.cpp - see nmea.h
#include "nmea.h"
#include <cstdlib>
#include <cstring>

static int hexVal(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

uint8_t nmeaChecksum(const char *body, size_t len)
{
    uint8_t c = 0;
    for (size_t i = 0; i < len; i++)
        c ^= (uint8_t)body[i];
    return c;
}

bool nmeaChecksumOk(const char *s, size_t len)
{
    while (len && (s[len - 1] == '\r' || s[len - 1] == '\n'))
        len--;
    if (len < 4 || s[0] != '$' || s[len - 3] != '*')
        return false;
    int h = hexVal(s[len - 2]), l = hexVal(s[len - 1]);
    if (h < 0 || l < 0)
        return false;
    return nmeaChecksum(s + 1, len - 4) == (uint8_t)(h * 16 + l);
}

size_t nmeaFinish(const char *in, char *out, size_t outSize)
{
    size_t len = strlen(in);
    while (len && (in[len - 1] == '\r' || in[len - 1] == '\n' || in[len - 1] == ' '))
        len--;
    if (len + 6 > outSize || len == 0)
        return 0;
    memcpy(out, in, len);
    bool hasStar = (len >= 3 && in[len - 3] == '*');
    if (!hasStar && in[0] == '$')
    {
        static const char hx[] = "0123456789ABCDEF";
        uint8_t c = nmeaChecksum(in + 1, len - 1);
        out[len++] = '*';
        out[len++] = hx[c >> 4];
        out[len++] = hx[c & 15];
    }
    out[len++] = '\r';
    out[len++] = '\n';
    out[len] = 0;
    return len;
}

bool NmeaAssembler::feed(uint8_t c)
{
    if (c == '$')
    {
        _in = true;
        _len = 0;
    }
    if (!_in)
        return false;
    if (c == '\n' || c == '\r')
    {
        _in = false;
        if (_len < 4)
            return false;
        _buf[_len] = 0;
        if (nmeaChecksumOk(_buf, _len))
        {
            memcpy(_out, _buf, _len);
            _out[_len++] = '\r';
            _out[_len++] = '\n';
            _out[_len] = 0;
            _outLen = _len;
            _good++;
            return true;
        }
        _bad++;
        return false;
    }
    if (c < 0x20 || c > 0x7e || _len >= NMEA_MAX)
    {
        // Binary data or an over-long line: drop it
        _in = false;
        _bad++;
        return false;
    }
    _buf[_len++] = (char)c;
    return false;
}

bool nmeaIsGga(const char *s, size_t len)
{
    return len > 7 && s[0] == '$' && s[3] == 'G' && s[4] == 'G' && s[5] == 'A' && s[6] == ',';
}

// Copy field n (0 = sentence id) into buf; returns its length or -1 when missing
static int field(const char *s, size_t len, int n, char *buf, size_t bufSize)
{
    int idx = 0;
    size_t start = 0;
    for (size_t i = 0; i <= len; i++)
    {
        char c = (i < len) ? s[i] : ',';
        if (c == ',' || c == '*' || c == '\r' || c == '\n')
        {
            if (idx == n)
            {
                size_t l = i - start;
                if (l >= bufSize)
                    l = bufSize - 1;
                memcpy(buf, s + start, l);
                buf[l] = 0;
                return (int)l;
            }
            idx++;
            start = i + 1;
            if (c == '*')
                break;
        }
    }
    return -1;
}

static double nmeaDeg(const char *v, const char *hemi, int degDigits)
{
    if (!v[0])
        return 0;
    char d[4] = {0};
    memcpy(d, v, degDigits);
    double deg = atof(d) + atof(v + degDigits) / 60.0;
    if (hemi[0] == 'S' || hemi[0] == 'W')
        deg = -deg;
    return deg;
}

bool nmeaParseGga(const char *s, size_t len, GgaInfo &g)
{
    if (!nmeaIsGga(s, len))
        return false;
    char f[24], h[4];
    g = GgaInfo();
    if (field(s, len, 1, f, sizeof(f)) >= 0)
        strncpy(g.utc, f, sizeof(g.utc) - 1);
    if (field(s, len, 2, f, sizeof(f)) > 4 && field(s, len, 3, h, sizeof(h)) >= 0)
        g.lat = nmeaDeg(f, h, 2);
    if (field(s, len, 4, f, sizeof(f)) > 5 && field(s, len, 5, h, sizeof(h)) >= 0)
        g.lon = nmeaDeg(f, h, 3);
    if (field(s, len, 6, f, sizeof(f)) > 0)
        g.quality = atoi(f);
    if (field(s, len, 7, f, sizeof(f)) > 0)
        g.sats = atoi(f);
    if (field(s, len, 8, f, sizeof(f)) > 0)
        g.hdop = (float)atof(f);
    if (field(s, len, 9, f, sizeof(f)) > 0)
        g.alt = (float)atof(f);
    if (field(s, len, 13, f, sizeof(f)) > 0)
        g.age = (float)atof(f);
    if (field(s, len, 14, f, sizeof(f)) > 0)
        g.station = atoi(f);
    g.valid = true;
    return true;
}
