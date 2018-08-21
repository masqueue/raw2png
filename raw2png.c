#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PATH_LEN 256

#define DBG 1

typedef struct FileBufferObject
{
    size_t size;
    void *buffer;
} FileBuffer;

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

	fb->buffer = malloc(raw_file_size);
	fb->size = raw_file_size;

	if (fb->buffer) {
		fread(fb->buffer, 1, raw_file_size, input_fd);
	}
	fclose(input_fd);

	return fb;
}

FileBuffer *transform_raw_to_png(FileBuffer *raw_buf_ptr)
{
	/* TODO: transform raw to png */
	FileBuffer *png_buf_ptr = (FileBuffer *) malloc(sizeof(FileBuffer));
	if(raw_buf_ptr && png_buf_ptr) {
		png_buf_ptr->size = raw_buf_ptr->size;
		png_buf_ptr->buffer = malloc(png_buf_ptr->size);
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

	/* load raw into memory */
	raw_buf_ptr = read_raw(in_raw_path);

	png_buf_ptr = transform_raw_to_png(raw_buf_ptr);

	write_png(out_png_path, png_buf_ptr);
}

void print_help()
{
	printf("Usage: raw2png [raw file] [png file]\n");
}
