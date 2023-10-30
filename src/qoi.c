#include "qoi.h"
#include "state.h"
#include <pspdisplay.h>
#include <string.h>

#define QOI_MAGIC 0x66696F71

#define ISOP_INDEX(t) (((t) & 0xC0) == 0x00)
#define ISOP_DIFF(t) (((t) & 0xC0) == 0x40)
#define ISOP_LUMA(t) (((t) & 0xC0) == 0x80)
#define ISOP_RUN(t) (((t) & 0xC0) == 0xC0)

static inline __attribute__((always_inline)) uint32_t bswap32(uint32_t n) {
	uint32_t b;
	b = (n & 0xFF) << 24;
	b |= (n & 0xFF00) << 8;
	b |= (n & 0xFF0000) >> 8;
	b |= (n & 0xFF000000) >> 24;
	return b;
}

static inline __attribute__((always_inline)) uint32_t computeHash(QoiColor* px) {
	return (px->r * 3 + px->g * 5 + px->b * 7 + px->a * 11) % 64;
}

int qoiInitJob(QoiJobDescriptor *desc, void *src, void *dst) {
	QoiHeader *header = (QoiHeader *) src;
	if (header->magic != QOI_MAGIC || header->channels != 4) {
		return -1;
	}
	desc->src = (const uint8_t *) (header + 1);
	desc->dst = (QoiColor *) dst;
	memset(desc->colors, 0, sizeof(desc->colors));
	desc->width = bswap32(header->width);
	desc->height = bswap32(header->height);
	desc->pixelsLeft = desc->width * desc->height;
	desc->pixel = (QoiColor) { .r = 0, .g = 0, .b = 0, .a = 255 };
	return 0;
}

int qoiDecodeJobWhileVBlank(QoiJobDescriptor *d) {
	QoiColor *px = &d->pixel;
	while (d->pixelsLeft > 0 && sceDisplayGetCurrentHcount() < PSP_SCREEN_HEIGHT) {
		uint8_t b = *(d->src++);
		if (ISOP_INDEX(b)) {
			px->c = d->colors[b & 0x3F].c;
		} else if (ISOP_RUN(b)) {
			uint8_t run = (b & 0x3F);
			if (run < 62) {
				d->pixelsLeft -= ++run;
				for (; run > 0; --run) {
					(d->dst++)->c = px->c;
				}
				continue;
			} else {
				int is_rgba = run & 1;
				px->r = *(d->src++);
				px->g = *(d->src++);
				px->b = *(d->src++);
				px->a = is_rgba ? *(d->src++) : px->a;
			}
		} else if (ISOP_LUMA(b)) {
			int8_t dg = (b & 0x3F) - 32;
			b = *(d->src++);
			px->b += (b & 0xF) + dg - 8;			
			px->r += ((b >> 4) & 0xF) + dg - 8;
			px->g += dg;
		} else if (ISOP_DIFF(b)) {
			px->b += (b & 0x3) - 2;
			b >>= 2;
			px->g += (b & 0x3) - 2;
			b >>= 2;
			px->r += (b & 0x3) - 2;
		}
		(d->dst++)->c = px->c;
		int hash = computeHash(px);
		d->colors[hash].c = px->c;
		--d->pixelsLeft;
	}
	return d->pixelsLeft > 0;
}

int qoiDecode(void *src, void *dst, uint32_t *outWidth, uint32_t *outHeight) {
	if (src == NULL) {
		return -1;
	}
	QoiHeader *hdr = src;
	if (hdr->magic != QOI_MAGIC || hdr->channels != 4) {
		return -1;
	}
	int width = bswap32(hdr->width);
	int height = bswap32(hdr->height);
	if (outWidth != NULL) {
		*outWidth = width;
	}
	if (outHeight != NULL) {
		*outHeight = height;
	}
	if (dst == NULL) {
		return -1;
	}
 	uint32_t pixels = width * height;
	QoiColor *out = dst;
	const uint8_t* in = (uint8_t *)(hdr + 1);
	QoiColor colors[64];
	QoiColor px = (QoiColor) { .r = 0, .g = 0, .b = 0, .a = 255 };
	memset(colors, 0, sizeof(colors));
	while (pixels > 0) {
		uint8_t b = *(in++);
		if (ISOP_INDEX(b)) {
			px.c = colors[b & 0x3F].c;
		} else if (ISOP_RUN(b)) {
			uint8_t run = (b & 0x3F);
			if (run < 62) {
				pixels -= ++run;
				for (; run > 0; --run) {
					(out++)->c = px.c;
				}
				continue;
			} else {
				int is_rgba = run & 1;
				px.r = *(in++);
				px.g = *(in++);
				px.b = *(in++);
				px.a = is_rgba ? *(in++) : px.a;
			}
		} else if (ISOP_LUMA(b)) {
			int8_t dg = (b & 0x3F) - 32;
			b = *(in++);
			px.b += (b & 0xF) + dg - 8;			
			px.r += ((b >> 4) & 0xF) + dg - 8;
			px.g += dg;
		} else if (ISOP_DIFF(b)) {
			px.b += (b & 0x3) - 2;
			b >>= 2;
			px.g += (b & 0x3) - 2;
			b >>= 2;
			px.r += (b & 0x3) - 2;
		}
		(out++)->c = px.c;
		int hash = computeHash(&px);
		colors[hash].c = px.c;
		--pixels;
	}
	return 0;
}
