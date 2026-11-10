#ifndef __CODEC_H_
#define __CODEC_H_

/* UTF-8 to GB2312/GBK. Returns bytes written; unknown chars become '?' */
int Utf8ToGbk(const char *in, char *out, int out_max);

#endif
