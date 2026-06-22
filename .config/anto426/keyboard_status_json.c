#include <stdio.h>
#include <stdlib.h>

static void json_escape(FILE *out, const char *text) {
    const unsigned char *p = (const unsigned char *)(text ? text : "");

    fputc('"', out);
    while (*p) {
        switch (*p) {
            case '"':
                fputs("\\\"", out);
                break;
            case '\\':
                fputs("\\\\", out);
                break;
            case '\b':
                fputs("\\b", out);
                break;
            case '\f':
                fputs("\\f", out);
                break;
            case '\n':
                fputs("\\n", out);
                break;
            case '\r':
                fputs("\\r", out);
                break;
            case '\t':
                fputs("\\t", out);
                break;
            default:
                if (*p < 0x20) {
                    fprintf(out, "\\u%04x", *p);
                } else {
                    fputc(*p, out);
                }
                break;
        }
        p++;
    }
    fputc('"', out);
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <text> <tooltip> <class>\n", argv[0]);
        return 2;
    }

    fputs("{\"text\":", stdout);
    json_escape(stdout, argv[1]);
    fputs(",\"tooltip\":", stdout);
    json_escape(stdout, argv[2]);
    fputs(",\"class\":", stdout);
    json_escape(stdout, argv[3]);
    fputs("}\n", stdout);

    return 0;
}
