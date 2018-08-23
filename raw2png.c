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
	/*yuv_obj->u = (unsigned char *) malloc(yuv_obj->num_of_pixels/4);
	yuv_obj->v = (unsigned char *) malloc(yuv_obj->num_of_pixels/4);


	uv_plane = raw_buffer->buffer + yuv_obj->num_of_pixels;
	for (uv_index = 0; uv_index < yuv_obj->num_of_pixels/4; uv_index++) {
		yuv_obj->u[uv_index] = uv_plane[uv_index] & 0xf;
		yuv_obj->v[uv_index] = uv_plane[uv_index] >> 4;
	}*/

	/* free raw_buffer here */
	free(raw_buffer->buffer);
	free(raw_buffer);

	return yuv_obj;
}

FileBuffer *transform_raw_to_png(YuvNv12Format *yuv_buffer)
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
			0x00, /* color type grayscale */
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
	if(yuv_buffer && png_buf_ptr) {
		/* TODO: calculate how much memory space is required here */
		png_buf_ptr->size = sizeof(png_signature)
				+ sizeof(png_ihdr_chunk)
				+ 4 /* ihdr crc */
				+ (4 /* idat chunk length info */
				+ sizeof(png_idat_chunk_type)
				+ yuv_buffer->dimen_x
				+ 4) * yuv_buffer->dimen_y /* idat crc */
				+ sizeof(png_iend_chunk);
		png_buf_ptr->buffer = (unsigned char *)malloc(png_buf_ptr->size);
		png_write_buf_ptr = png_buf_ptr->buffer;
		if(png_write_buf_ptr) {
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


			printf("number of idat pixels %d\n", yuv_buffer->num_of_pixels);
			/* write idat length info */
			idat_len_r = __bswap_32(yuv_buffer->dimen_x);
			for (idat_cnt = 0; idat_cnt < yuv_buffer->dimen_y; idat_cnt++) {
				memcpy(png_write_buf_ptr, &idat_len_r, 4);
				png_write_buf_ptr += 4;
				/* write idat chunk type */
				memcpy(png_write_buf_ptr, png_idat_chunk_type, sizeof(png_idat_chunk_type));
				idat_crc_ptr = png_write_buf_ptr; /* remember idat crc start point */
				png_write_buf_ptr += sizeof(png_idat_chunk_type);
				/* write idat chunk data; use only y for grayscale png */
				memcpy(png_write_buf_ptr, yuv_buffer->y + (yuv_buffer->dimen_x*idat_cnt), yuv_buffer->dimen_x);
				png_write_buf_ptr += yuv_buffer->dimen_x;
				/* write idat crc */
				crc_tmp = crc(idat_crc_ptr, sizeof(png_idat_chunk_type) + yuv_buffer->dimen_x);
				crc_tmp = __bswap_32(crc_tmp);
				memcpy(png_write_buf_ptr, &crc_tmp, 4);
				png_write_buf_ptr += 4;
			}

			/* write iend chunk */
			memcpy(png_write_buf_ptr, png_iend_chunk, sizeof(png_iend_chunk));
		}
		free(yuv_buffer->y);
		/*free(yuv_buffer->u);
		free(yuv_buffer->v);*/
		free(yuv_buffer);
	}
	return png_buf_ptr;
}

void write_png(char *png_path, FileBuffer *png_buf_ptr)
{
	FILE *output_fd;
	if (png_buf_ptr && png_buf_ptr->buffer) {
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

	/* load raw into memory */
	raw_buf_ptr = read_raw(in_raw_path);

	yuv_obj = parse_raw(raw_buf_ptr);

	png_buf_ptr = transform_raw_to_png(yuv_obj);

	write_png(out_png_path, png_buf_ptr);
}

void print_help()
{
	printf("Usage: raw2png [raw file] [png file]\n");
}
