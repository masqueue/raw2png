#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <byteswap.h>

#define MAX_PATH_LEN 256

#define DBG 1

extern unsigned long crc(unsigned char *buf, int len);

typedef struct FileBufferObject
{
	size_t size;
	unsigned char *buffer;
} FileBuffer;

typedef struct YuvNv12FormatObject
{
	int dimen_x;
	int dimen_y;
	int num_of_pixels;
	unsigned char *y;
	unsigned char *u;
	unsigned char *v;
} YuvNv12Format;

typedef struct RgbFormatObject
{
	int dimen_x;
	int dimen_y;
	int num_of_pixels;
	unsigned char *buffer;
} RgbFormat;

void converter(char *, char *);
void print_help(); /* print messages */

int main(int argc, char *argv[])
{
	if (argc != 3) {
		print_help();
	} else {
		converter(argv[1], argv[2]);
	}

	return 0;
}

FileBuffer *read_raw(char *raw_path)
{
	size_t raw_file_size;
	FILE *input_fd = fopen(raw_path, "rb");

	FileBuffer *fb = (FileBuffer *) malloc(sizeof(FileBuffer));

	if (input_fd == NULL || fb == NULL) {
		/* TODO: something goes wrong, impl error handling here */
		return NULL;
	}
	/* determine file size */
	fseek(input_fd, 0L, SEEK_END);
	raw_file_size = ftell(input_fd);
	rewind(input_fd);

	if (raw_file_size == 0) {
		printf("Raw file has empty content\n");
	}

	if (DBG) printf("RAW file size is %ld\n", raw_file_size);

	/* allocate memory space for raw */
	fb->buffer = (unsigned char *)malloc(raw_file_size);
	fb->size = raw_file_size;

	if (fb->buffer) {
		fread(fb->buffer, 1, raw_file_size, input_fd);
	}
	fclose(input_fd);

	return fb;
}

YuvNv12Format *parse_raw(FileBuffer *raw_buffer)
{
	int uv_index;
	unsigned char *uv_plane;
	YuvNv12Format *yuv_obj = (YuvNv12Format *) malloc(sizeof(YuvNv12Format));
	if (yuv_obj == NULL) {
		/* TODO: error handling here */
		return yuv_obj;
	}
	/* TODO: find a method to detect resolution */
	/* ignore image size detection, assume image size is 640x480 */
	yuv_obj->dimen_x = 640;
	yuv_obj->dimen_y = 480;

	yuv_obj->num_of_pixels = yuv_obj->dimen_x * yuv_obj->dimen_y;

	/* Y plane has one byte per pixel */
	yuv_obj->y = (unsigned char *) malloc(yuv_obj->num_of_pixels);
	memcpy(yuv_obj->y, raw_buffer->buffer, yuv_obj->num_of_pixels);

	/* 2x2 pixels share one CbCr byte */
	yuv_obj->u = (unsigned char *) malloc(yuv_obj->num_of_pixels/4);
	yuv_obj->v = (unsigned char *) malloc(yuv_obj->num_of_pixels/4);

	uv_plane = raw_buffer->buffer + yuv_obj->num_of_pixels;
	for (uv_index = 0; uv_index < yuv_obj->num_of_pixels/4; uv_index++) {
		yuv_obj->u[uv_index] = uv_plane[uv_index] & 0xf;
		yuv_obj->v[uv_index] = uv_plane[uv_index] >> 4;
	}

	/* free raw_buffer here */
	free(raw_buffer->buffer);
	free(raw_buffer);

	return yuv_obj;
}

/* transform yuv format into rbg 24bit format */
RgbFormat *transform_yuv_to_rgb(YuvNv12Format *yuv_obj)
{
	int x, y, uv_plane_x, uv_plane_y, u_prime, v_prime, yuv_y;
	unsigned char *write_ptr;
	RgbFormat *rgb_obj = (RgbFormat *) malloc(sizeof(RgbFormat));
	rgb_obj->num_of_pixels = yuv_obj->num_of_pixels;
	rgb_obj->dimen_x = yuv_obj->dimen_x;
	rgb_obj->dimen_y = yuv_obj->dimen_y;
	rgb_obj->buffer = (unsigned char *) malloc(yuv_obj->num_of_pixels*3);
	write_ptr = rgb_obj->buffer;

	/* calculate RGB value for each pixel and write to rgb_obj->buffer */
	/* R = Y + 1.402 * (V - 128) */
	/* G = Y - 0.344 * (U - 128) - 0.714 * (V - 128) */
	/* B = Y + 1.722 * (U - 128) */
	/* formula is found on StackOverflow */
	u_prime = (int)(yuv_obj->u[0]) - 128;
	v_prime = (int)(yuv_obj->v[0]) - 128;
	for (y = 0, uv_plane_y = 0; y < yuv_obj->dimen_y; y++) {
		for (x = 0, uv_plane_y = 0; x < yuv_obj->dimen_x; x++) {
			yuv_y = yuv_obj->y[y * yuv_obj->dimen_x + x];
			*write_ptr = yuv_y + 1.402 * v_prime;
			*(write_ptr + 1) = yuv_y - 0.344 * u_prime - 0.714 * v_prime;
			*(write_ptr + 2) = y + 1.722 * u_prime;
			//if (DBG) printf("(%d, %d) R: %u, G: %u, B: %u\n", x, y, *write_ptr, *(write_ptr + 1), *(write_ptr + 2));
			write_ptr += 3;
			if (x % 2) {
				/* every move on uv plane updates u_prime and v_prime */
				uv_plane_x++;
				u_prime = (int)(yuv_obj->u[((yuv_obj->dimen_x)/2) * uv_plane_y + uv_plane_x]) - 128;
				v_prime = (int)(yuv_obj->v[((yuv_obj->dimen_x)/2) * uv_plane_y + uv_plane_x]) - 128;
			}
		}
		if (y % 2) {
			/* every move on uv plane updates u_prime and v_prime */
			uv_plane_y++;
			u_prime = (int)(yuv_obj->u[((yuv_obj->dimen_x)/2) * uv_plane_y + uv_plane_x]) - 128;
			v_prime = (int)(yuv_obj->v[((yuv_obj->dimen_x)/2) * uv_plane_y + uv_plane_x]) - 128;
		}
	}
	free(yuv_obj->y);
	free(yuv_obj->u);
	free(yuv_obj->v);
	free(yuv_obj);

	return rgb_obj;
}

FileBuffer *write_rbg_to_png(RgbFormat *rgb_buffer)
{
	/* TODO: generate png */
	FileBuffer *png_buf_ptr;
	unsigned long crc_tmp, idat_len_r, idat_cnt;
	unsigned char *idat_crc_ptr;
	unsigned char *png_write_buf_ptr;
	unsigned char png_signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
	/* hardcode for now */
	unsigned char png_ihdr_chunk[] = {
			0x00, 0x00, 0x00, 0x0D, /* num of data bytes */
			0x49, 0x48, 0x44, 0x52, /* chunk type IHDR */
			0x00, 0x00, 0x02, 0x80, /* width 640 */
			0x00, 0x00, 0x01, 0xE0, /* height 480 */
			0x08, /* bit depth */
			0x02, /* color type rgb triple */
			0x00, /* compressed method */
			0x00, /* filter method */
			0x00 /* interface method */
			/* append 4 bytes CRC */ };

	unsigned char png_idat_chunk_type[] = {
			0x49, 0x44, 0x41, 0x54 /* chunk type IDAT */ };

	unsigned char png_iend_chunk[] = {
			0x00, 0x00, 0x00, 0x00, /* num of data bytes */
			0x49, 0x45, 0x4E, 0x44, /* chunk type IEND */
			0xAE, 0x42, 0x60, 0x82  /* CRC (hardcoded) */ };

	png_buf_ptr = (FileBuffer *) malloc(sizeof(FileBuffer));
	if(rgb_buffer && png_buf_ptr) {
		/* TODO: calculate how much memory space is required here */
		png_buf_ptr->size = sizeof(png_signature)
				+ sizeof(png_ihdr_chunk)
				+ 4 /* ihdr crc */
				+ (4 /* idat chunk length info */
				+ sizeof(png_idat_chunk_type)
				+ (rgb_buffer->dimen_x * 3)
				+ 4) * rgb_buffer->dimen_y /* idat crc */
				+ sizeof(png_iend_chunk);
		if (DBG) printf("allocating %ld bytes for png_buf_ptr\n", png_buf_ptr->size);
		png_buf_ptr->buffer = (unsigned char *)malloc(png_buf_ptr->size);
		png_write_buf_ptr = png_buf_ptr->buffer;
		if(png_write_buf_ptr) {
			if (DBG) printf("allocated %ld bytes for png_buf_ptr\n", png_buf_ptr->size);
			/* write png signature */
			memcpy(png_write_buf_ptr, png_signature, sizeof(png_signature));
			png_write_buf_ptr += sizeof(png_signature);
			/* write ihdr crc */
			memcpy(png_write_buf_ptr, png_ihdr_chunk, sizeof(png_ihdr_chunk));
			png_write_buf_ptr += sizeof(png_ihdr_chunk);
			/* write ihdr crc */
			crc_tmp = crc(png_ihdr_chunk + 4, 17);
			printf("ihdr crc = %ld\n", crc_tmp);
			crc_tmp = __bswap_32(crc_tmp);
			memcpy(png_write_buf_ptr, &crc_tmp, 4);
			png_write_buf_ptr += 4;


			printf("number of idat pixels %d\n", rgb_buffer->num_of_pixels);
			/* write idat length info */
			idat_len_r = __bswap_32(rgb_buffer->dimen_x * 3);
			for (idat_cnt = 0; idat_cnt < rgb_buffer->dimen_y; idat_cnt++) {
				memcpy(png_write_buf_ptr, &idat_len_r, 4);
				png_write_buf_ptr += 4;
				/* write idat chunk type */
				memcpy(png_write_buf_ptr, png_idat_chunk_type, sizeof(png_idat_chunk_type));
				idat_crc_ptr = png_write_buf_ptr; /* remember idat crc start point */
				png_write_buf_ptr += sizeof(png_idat_chunk_type);
				/* write idat chunk data; use only y for grayscale png */
				memcpy(png_write_buf_ptr, rgb_buffer->buffer + (rgb_buffer->dimen_x * 3 * idat_cnt), rgb_buffer->dimen_x * 3);
				png_write_buf_ptr += (rgb_buffer->dimen_x * 3);
				/* write idat crc */
				crc_tmp = crc(idat_crc_ptr, sizeof(png_idat_chunk_type) + rgb_buffer->dimen_x * 3);
				crc_tmp = __bswap_32(crc_tmp);
				memcpy(png_write_buf_ptr, &crc_tmp, 4);
				png_write_buf_ptr += 4;
			}

			/* write iend chunk */
			memcpy(png_write_buf_ptr, png_iend_chunk, sizeof(png_iend_chunk));
		}
		free(rgb_buffer->buffer);
		free(rgb_buffer);
	}
	return png_buf_ptr;
}

void write_png_to_disk(char *png_path, FileBuffer *png_buf_ptr)
{
	FILE *output_fd;
	if (png_buf_ptr != NULL  && png_buf_ptr->buffer != NULL) {
		output_fd = fopen(png_path, "wb");
		if(output_fd) {
			fwrite(png_buf_ptr->buffer, 1, png_buf_ptr->size, output_fd);
			fclose(output_fd);
		}
		free(png_buf_ptr->buffer);
		free(png_buf_ptr);
	}
}

void converter(char *in_raw_path, char *out_png_path)
{
	FileBuffer *raw_buf_ptr, *png_buf_ptr;
	YuvNv12Format *yuv_obj;
	RgbFormat *rgb_obj;

	/* load raw into memory */
	raw_buf_ptr = read_raw(in_raw_path);

	yuv_obj = parse_raw(raw_buf_ptr);

	rgb_obj = transform_yuv_to_rgb(yuv_obj);

	png_buf_ptr = write_rbg_to_png(rgb_obj);

	write_png_to_disk(out_png_path, png_buf_ptr);
}

void print_help()
{
	printf("Usage: raw2png [raw file] [png file]\n");
}
