#ifndef _SOMETOOLS_H_
#define _SOMETOOLS_H_

// sometools.h
// some additional utilities for JSON and others
// part of Microservices demo: MSBasics

#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdlib>

char *skip(char *p0)
{
    char *p = p0;
    while ((*p != 0) && ((*p == '\r') || (*p == '\n') || (*p == ' ')))
        p++;
    return p;
}

char *c_skip(char *p0, char c)
{
    char *p = p0;
    while ((*p != 0) && ((*p == '\r') || (*p == '\n') || (*p == ' ') || (*p == c)))
        p++;
    return p;
}

char *find_key(const char *start, const char *key)
{
    char *kp0;
    char *kp1;
    char *jp0;
    char *jp1;

    if (*key == '/')
        kp0 = (char *)key + 1;
    else
        kp0 = (char *)key;
    if (*kp0 == 0)
        return NULL;
    kp1 = strchr(kp0, '/');
    if (kp1 == NULL)
        kp1 = kp0 + strlen(kp0);
    jp0 = skip((char *)start);
    if (*jp0 != '"')
        return NULL;
    jp0++;
    jp1 = strchr(jp0, '"');
    if (jp1 == NULL)
        return NULL;
    if (strncmp(kp0, jp0, kp1 - kp0) == 0)
    {
        if ((*kp1 == 0) || ((*kp1 == '/') && (*(kp1 + 1) == 0)))
        {
            jp0 = skip(jp1 + 1);
            if (*jp0 != ':')
                return NULL;
            return (skip(jp0 + 1));
        }
        // next key down
        jp1 = c_skip(jp1 + 1, ':');
        if (*jp1 != '{')
            return NULL;
        return find_key(jp1 + 1, kp1);
    }
    else
    { // next on same level in json
        jp0 = c_skip(jp1 + 1, ':');
        int level = 1;
        while ((*jp0 != ',') && (*jp0 != '{'))
        {
            jp0++;
            if (*jp0 == 0)
                return NULL;
        }
        if (*jp0 == '{')
            level = 1;
        while ((*jp0 != ',') && (level > 0))
        {
            jp0++;
            if (*jp0 == 0)
                return NULL;
            if (*jp0 == '}')
                level--;
            if (*jp0 == '{')
                level++;
        }
        return find_key(c_skip(jp0 + 1, ','), key);
    }
}

// scan json string jstr for key return value
// jstr: String containing json document
// key: string containing key to search (e.g. "/data/attributes/name")
// value: the found value of the key/value pair, can be another json substring or atom
// valmax: number of bytes allocated in value including trailing 0.
// returns:
// -1 invalid json format
// -2 buffer for value too small; valmax contains necessary size
//  0 key not found
//  > 0: length of value without trailing 0
int json_get_object(const char *jstr, const char *key, char *value, int &valmax)
{
    char *pos = strchr((char *)jstr, '{');
    if (pos == NULL)
        return -1;
    pos = find_key((const char *)c_skip(pos, '{'), key);
    if (pos == NULL)
        return 0; // not found
    // copy value
    char *p0 = skip(pos);
    char *p1;
    if (*p0 == 0)
        return -1;
    if (*p0 == '{')
    { // copy until same level '}'
        p1 = p0 + 1;
        int level = 1;
        while (level > 0)
        {
            p1++;
            if (*p1 == '}')
                level--;
            if (*p1 == '{')
                level++;
            if (*p1 == 0)
                return -1;
        }
    }
    else if (*p0 == '"')
    { // copy until next '"'
        p0++;
        p1 = p0;
        if (*p1 == 0)
            return -1;
        while (*p1 != '"')
        {
            p1++;
            if (*p1 == 0)
                return -1;
        }
        p1--;
    }
    else
    {
        p1 = p0 + 1;
        while ((*p1 != ' ') && (*p1 != '}') && (*p1 != '\r') && (*p1 != '\n'))
        {
            p1++;
            if (*p1 == 0)
                return -1;
        }
        p1--;
    }
    if ((p1 - p0 + 1) >= valmax)
    {
        valmax = (p1 - p0 + 1);
        return -2;
    }
    strncpy(value, p0, p1 - p0 + 1);
    value[p1 - p0 + 1] = 0;
    return strlen(value);
}

// check, if json document is complete: every '{' has a '}'
int json_complete(const char *jstr)
{
    int i;
    int level = 0;
    int in_string = 0;
    for (i = 0; i < strlen(jstr); i++)
    {
        if (jstr[i] == '\\')
            i++; // skip escaped characters
        else
        {
            if (jstr[i] == '"')
                in_string = (in_string == 0) ? 1 : 0;
            if (in_string == 0)
            {
                if (jstr[i] == '{')
                    level++;
                else if (jstr[i] == '}')
                    level--;
            }
        }
    }
    return (level == 0);
}

char *timestamp()
{
    static const char wday_name[][4] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char mon_name[][4] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static char result[26];
    time_t _tm = time(NULL);
    struct tm *curtime = localtime(&_tm);
    sprintf(result, "%.3s %.3s%3d %.2d:%.2d:%.2d %d",
            wday_name[curtime->tm_wday],
            mon_name[curtime->tm_mon],
            curtime->tm_mday, curtime->tm_hour,
            curtime->tm_min, curtime->tm_sec,
            1900 + curtime->tm_year);
    return result;
}

const char *b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void b64_encode(const unsigned char *in, size_t len, char *out)
{
    size_t outlen = len;
    if (len % 3 != 0)
        outlen += 3 - (len % 3);
    outlen /= 3;
    outlen *= 4;

    size_t i;
    size_t j;
    size_t v;

    out[outlen] = '\0';
    for (i = 0, j = 0; i < len; i += 3, j += 4)
    {
        v = in[i];
        v = i + 1 < len ? v << 8 | in[i + 1] : v << 8;
        v = i + 2 < len ? v << 8 | in[i + 2] : v << 8;

        out[j] = b64chars[(v >> 18) & 0x3F];
        out[j + 1] = b64chars[(v >> 12) & 0x3F];
        if (i + 1 < len)
        {
            out[j + 2] = b64chars[(v >> 6) & 0x3F];
        }
        else
        {
            out[j + 2] = '=';
        }
        if (i + 2 < len)
        {
            out[j + 3] = b64chars[v & 0x3F];
        }
        else
        {
            out[j + 3] = '=';
        }
    }
}


template <class T>
void PrintBin(T x)
{
    size_t bytes = sizeof(x) * 8;
    for (int i = 0; i < bytes; i++)
    {
        printf("%c", ((x & (1 << (bytes - 1 - i))) != 0) ? '1' : '0');
        if ((i + 1) % 4 == 0)
            printf(" ");
    }
    printf("\n");
}

char *http_header_value(const char *header, const char *key, int valnum, char *result)
{
    char *pos = strstr((char*)header, key);
    if (pos == NULL)
        return NULL;
    pos = c_skip(pos + strlen(key), ':');
    char *pstart, *pend;
    for (int i = 0; i < valnum; i++)
    {
        while (strchr(" ,", *pos) != NULL)
            pos++;
        pstart = pos;
        while (strchr(" ,\r\n", *pos) == NULL)
            pos++;
        pend = pos;
    }
    strncpy(result, pstart, pend - pstart);
    result[pend - pstart] = 0;
    return result;
}

uint32_t gen_rnd32()
{
    uint32_t x = 0;
    srand((unsigned int)clock());
    for (int i = 0; i < 31; i++)
    {
        x = x | (1 << (rand() % 32));
    }
    return x;
}

#endif // _SOMETOOLS_H_