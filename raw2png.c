#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PATH_LEN 256

#define DBG 1

typedef struct FileBufferObject
{
	size_t size;
	unsigned char *buffer;
} FileBuffer;

typedef struct YuvNv12FormatObject
{
	int dimen_x;
	int dimen_y;
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
	int number_of_pixels, uv_index;
	unsigned char *uv_plane;
	/* NOTE: ignore image size detection, assume image size is 640x480 */
	YuvNv12Format *yuv_obj = (YuvNv12Format *) malloc(sizeof(YuvNv12Format));
	if (yuv_obj == NULL) {
		/* TODO: error handling here */
		return yuv_obj;
	}
	yuv_obj->dimen_x = 640;
	yuv_obj->dimen_y = 480;
	number_of_pixels = yuv_obj->dimen_x * yuv_obj->dimen_y;

	/* Y plane has one byte per pixel */
	yuv_obj->y = (unsigned char *) malloc(sizeof(char)*number_of_pixels);
	/* 2x2 pixels share one CbCr byte */
	yuv_obj->u = (unsigned char *) malloc(sizeof(char)*number_of_pixels/4);
	yuv_obj->v = (unsigned char *) malloc(sizeof(char)*number_of_pixels/4);

	memcpy(yuv_obj->y, raw_buffer->buffer, number_of_pixels);

	uv_plane = raw_buffer->buffer + number_of_pixels;
	for (uv_index = 0; uv_index < number_of_pixels/4; uv_index++) {
		yuv_obj->u[uv_index] = uv_plane[uv_index] & 0xf;
		yuv_obj->v[uv_index] = uv_plane[uv_index] >> 4;
	}

	/* TODO: free raw_buffer here */
	free(yuv_obj);

	return yuv_obj;
}

FileBuffer *transform_raw_to_png(FileBuffer *raw_buf_ptr)
{
	/* TODO: transform raw to png */
	FileBuffer *png_buf_ptr = (FileBuffer *) malloc(sizeof(FileBuffer));
	if(raw_buf_ptr && png_buf_ptr) {
		png_buf_ptr->size = raw_buf_ptr->size;
		png_buf_ptr->buffer = (unsigned char *)malloc(png_buf_ptr->size);
		if(png_buf_ptr->buffer) {
			memcpy(png_buf_ptr->buffer, raw_buf_ptr->buffer, png_buf_ptr->size);
		}
		free(raw_buf_ptr->buffer);
		free(raw_buf_ptr);
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

	png_buf_ptr = transform_raw_to_png(raw_buf_ptr);

	write_png(out_png_path, png_buf_ptr);
}

void print_help()
{
	printf("Usage: raw2png [raw file] [png file]\n");
}
