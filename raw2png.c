#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <byteswap.h>
#include <zlib.h>

#define DBG 1

unsigned long calc_chunk_crc(unsigned char *type, unsigned char *data, int data_len);

typedef struct FileBufferObject
{
	size_t size;
	unsigned char *buffer;
} FileBuffer;

typedef struct Yuv420pBufferObject
{
	int dimen_x;
	int dimen_y;
	int num_of_pixels;
	unsigned char *y;
#if 0
	unsigned char *u;
	unsigned char *v;
#endif
} Yuv420pBuffer;

typedef struct RgbBufferObject
{
	int dimen_x;
	int dimen_y;
	int num_of_pixels;
	unsigned char *buffer;
	long size;
	int is_grayscale;
} RgbBuffer;

typedef struct ChunkObject
{
	unsigned char length[4];
	unsigned char type[4];
	unsigned char *data;
	unsigned char crc[4];
	struct ChunkObject *next;
} Chunk;

typedef struct PngObjectType
{
	Chunk *chunk_list;
	size_t length;
} PngObject;

void converter(char *in, char *out, int w, int h);
void print_help(); /* print messages */

int main(int argc, char *argv[])
{
	char opt;
	int width = 0, height = 0;
	char *input_file_name = NULL, *output_file_name = NULL, *resolution = NULL;

	while ((opt = getopt(argc, argv, "d:i:o:")) != -1) {
		switch (opt) {
			case 'd': /* resolution */
				if (optarg && (strchr(optarg, '*') || strchr(optarg, 'x'))) {
					resolution = strdup(optarg);
					if (DBG) printf("Image resolution = %s\n", resolution);
					width = atoi(strsep(&resolution, "x*"));
					height = atoi(strsep(&resolution, "x*"));
					free(resolution);
				} else {
					fprintf(stderr, "Please provide correct resolution\n");
				}
				break;
			case 'i': /* input file name */
				if (optarg) {
					if (DBG) printf("Input file name: %s\n", optarg);
					input_file_name = strdup(optarg);
				} else {
					fprintf(stderr, "Please provide input file path\n");
				}
				break;
			case 'o': /* output file name */
				if (optarg) {
					if (DBG) printf("Output file name: %s\n", optarg);
					output_file_name = strdup(optarg);
				} else {
					fprintf(stderr, "Please provide output file path\n");
				}
				break;
			default:
				break;
		}
	}

	if (input_file_name == NULL || output_file_name == NULL || width == 0 || height == 0) {
		if (input_file_name != NULL)
			free(input_file_name);
		if (output_file_name != NULL)
			free(output_file_name);
		if (resolution != NULL)
			free(resolution);

		print_help();
	}


	converter(input_file_name, output_file_name, width, height);

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

	//if (DBG) printf("RAW file size is %ld\n", raw_file_size);

	/* allocate memory space for raw */
	fb->buffer = (unsigned char *)malloc(raw_file_size);
	fb->size = raw_file_size;

	if (fb->buffer) {
		fread(fb->buffer, 1, raw_file_size, input_fd);
	}
	fclose(input_fd);

	return fb;
}

Yuv420pBuffer *parse_raw(FileBuffer *raw_buffer, int width, int height)
{
	int uv_index, buffer_size;
	unsigned char *uv_plane;
	Yuv420pBuffer *yuv_obj = (Yuv420pBuffer *) malloc(sizeof(Yuv420pBuffer));
	if (yuv_obj == NULL) {
		/* TODO: error handling here */
		return yuv_obj;
	}
	/* TODO: find a method to detect resolution */
	/* ignore image size detection, assume image size is 640x480 */
	yuv_obj->dimen_x = width;
	yuv_obj->dimen_y = height;
	yuv_obj->num_of_pixels = yuv_obj->dimen_x * yuv_obj->dimen_y;
	buffer_size = yuv_obj->num_of_pixels;

	if (raw_buffer->size < yuv_obj->num_of_pixels) {
		/* raw buffer should be greater than or equal to number of pixels */
		fprintf(stderr, "Warning: raw file size is smaller than number of pixel\n");
		buffer_size = raw_buffer->size;
	}

	/* Y plane has one byte per pixel */
	yuv_obj->y = (unsigned char *) malloc(yuv_obj->num_of_pixels);
	memcpy(yuv_obj->y, raw_buffer->buffer, buffer_size);
#if 0
	/* 2x2 pixels share one CbCr byte */
	yuv_obj->u = (unsigned char *) malloc(yuv_obj->num_of_pixels/4);
	yuv_obj->v = (unsigned char *) malloc(yuv_obj->num_of_pixels/4);

	uv_plane = raw_buffer->buffer + yuv_obj->num_of_pixels;
	for (uv_index = 0; uv_index < yuv_obj->num_of_pixels/4; uv_index++) {
		yuv_obj->u[uv_index] = uv_plane[uv_index] & 0xf;
		yuv_obj->v[uv_index] = uv_plane[uv_index] >> 4;
	}
#endif
	/* free raw_buffer here */
	free(raw_buffer->buffer);
	free(raw_buffer);

	return yuv_obj;
}

/* transform yuv format into rgb 24bit format */
RgbBuffer *transform_yuv_to_rgb(Yuv420pBuffer *yuv_obj)
{
	int x, y, uv_plane_x, uv_plane_y, u_prime, v_prime, yuv_y;
	unsigned char *write_ptr;
	RgbBuffer *rgb_obj = (RgbBuffer *) malloc(sizeof(RgbBuffer));
	rgb_obj->num_of_pixels = yuv_obj->num_of_pixels;
	rgb_obj->dimen_x = yuv_obj->dimen_x;
	rgb_obj->dimen_y = yuv_obj->dimen_y;
	/* TODO: hardcoded grayscale config */
	rgb_obj->is_grayscale = 1;
	/* every line contains a filter type byte */
	if (rgb_obj->is_grayscale) {
		rgb_obj->size = yuv_obj->num_of_pixels + yuv_obj->dimen_y;
	} else {
		rgb_obj->size = yuv_obj->num_of_pixels * 3 + yuv_obj->dimen_y;
	}
	rgb_obj->buffer = (unsigned char *) malloc(rgb_obj->size);
	write_ptr = rgb_obj->buffer;

	/* calculate RGB value for each pixel and write to rgb_obj->buffer */
	/* R = Y + 1.402 * (V - 128) */
	/* G = Y - 0.344 * (U - 128) - 0.714 * (V - 128) */
	/* B = Y + 1.722 * (U - 128) */
	/* formula is found on StackOverflow */
#if 0
	u_prime = (int)(yuv_obj->u[0]) - 128;
	v_prime = (int)(yuv_obj->v[0]) - 128;
#endif
	for (y = 0, uv_plane_y = 0; y < yuv_obj->dimen_y; y++) {
		*write_ptr = 0; // filter type byte for each line
		write_ptr++;
		for (x = 0, uv_plane_y = 0; x < yuv_obj->dimen_x; x++) {
			yuv_y = yuv_obj->y[y * yuv_obj->dimen_x + x];
			if (rgb_obj->is_grayscale) {
				memcpy(write_ptr, &yuv_y, 1);
				write_ptr++;
			} else {
#if 0
				*(write_ptr + 0) = yuv_y + 1.402 * v_prime;
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
#endif
			}
		}
#if 0
		if (rgb_obj->is_grayscale != 1) {
			if (y % 2) {
				/* every move on uv plane updates u_prime and v_prime */
				uv_plane_y++;
				u_prime = (int)(yuv_obj->u[((yuv_obj->dimen_x)/2) * uv_plane_y + uv_plane_x]) - 128;
				v_prime = (int)(yuv_obj->v[((yuv_obj->dimen_x)/2) * uv_plane_y + uv_plane_x]) - 128;
			}
		}
#endif
	}
	free(yuv_obj->y);
#if 0
	free(yuv_obj->u);
	free(yuv_obj->v);
#endif
	free(yuv_obj);

	return rgb_obj;
}


Chunk *get_last_chunk(Chunk *list)
{
	Chunk *current_chunk = list;
	while(current_chunk != current_chunk->next) {
		current_chunk = current_chunk->next;
	}
	return current_chunk;
}

Chunk *remove_first_chunk(PngObject *png)
{
	Chunk *holder = png->chunk_list;

	if (holder == NULL) {
		return holder;
	} else if (holder == holder->next) {
		png->chunk_list = NULL;
	} else {
		png->chunk_list = holder->next;
	}
	return holder;
}

void build_png_header(PngObject *png, RgbBuffer *rgb_buffer)
{
	unsigned char ihdr_chunk_data_length[] = {
			0x00, 0x00, 0x00, 0x0D /* num of data bytes */ };
	unsigned char ihdr_chunk_type[] = {
			0x49, 0x48, 0x44, 0x52 /* chunk type IHDR */ };
	unsigned char ihdr_chunk_data[] = {
			0x00, 0x00, 0x00, 0x00, /* width 640 */
			0x00, 0x00, 0x00, 0x00, /* height 480 */
			0x08, /* bit depth */
			0x00, /* color type */
			0x00, /* compressed method */
			0x00, /* filter method */
			0x00 /* interface method */ };
	long chunk_crc;
	unsigned int reversed_bytes;
	unsigned char color_type;

	/* ihdr is the first chunk */
	Chunk *ihdr = (Chunk *) malloc(sizeof(Chunk));
	png->chunk_list = ihdr;
	ihdr->next = ihdr;

	memcpy(ihdr->length, ihdr_chunk_data_length, sizeof(ihdr->length));
	memcpy(ihdr->type, ihdr_chunk_type, sizeof(ihdr->type));

	ihdr->data = (unsigned char *) malloc(sizeof(ihdr_chunk_data));
	reversed_bytes = __bswap_32(rgb_buffer->dimen_x);
	memcpy(ihdr_chunk_data , &reversed_bytes, 4);
	reversed_bytes = __bswap_32(rgb_buffer->dimen_y);
	memcpy(ihdr_chunk_data + 4, &reversed_bytes, 4);
	color_type = rgb_buffer->is_grayscale?0x00:0x02;
	memcpy(ihdr_chunk_data + 9, &color_type, 1);
	memcpy(ihdr->data, ihdr_chunk_data, sizeof(ihdr_chunk_data));

	chunk_crc = calc_chunk_crc(ihdr_chunk_type, ihdr_chunk_data, sizeof(ihdr_chunk_data));

	reversed_bytes = __bswap_32(chunk_crc);
	memcpy(ihdr->crc, &reversed_bytes, sizeof(ihdr->crc));
}

void build_png_data(PngObject *png, RgbBuffer *rgb_buffer)
{
	/* compress rgb raw value and fill compressed stream into idat data */
	Chunk *idat, *last_chunk;
	unsigned char idat_chunk_type[] = {
			0x49, 0x44, 0x41, 0x54 }; /* chunk type IDAT */
	size_t compress_buf_len = compressBound(rgb_buffer->size);
	unsigned int reversed_bytes;
	size_t rgb_buf_len = rgb_buffer->size;
	int c_result;
	long crc;
	unsigned char *compressed_buffer = (unsigned char *) malloc(compress_buf_len);

	c_result = compress2(compressed_buffer, &compress_buf_len, rgb_buffer->buffer, rgb_buf_len, 0);
	if (DBG) printf("actual compress buffer length: %lu\n", compress_buf_len);
	free(rgb_buffer->buffer);
	free(rgb_buffer);

	if (c_result != Z_OK) {
		printf("Failed to compress image data!!\n");
		return;
	}/* else if (DBG) {
		printf("allocated compress buffer size: %ld\n", compress_buf_len);
	}*/

	/* update chunk list */
	idat = (Chunk *) malloc(sizeof(Chunk));
	last_chunk = get_last_chunk(png->chunk_list);
	last_chunk->next = idat;
	idat->next = idat;

	reversed_bytes = __bswap_32(compress_buf_len);
	memcpy(idat->length, &reversed_bytes, sizeof(idat->length));
	memcpy(idat->type, idat_chunk_type, sizeof(idat->type));
	idat->data = compressed_buffer;
	crc = calc_chunk_crc(idat->type, idat->data, compress_buf_len);
	reversed_bytes = __bswap_32(crc);
	memcpy(idat->crc, &reversed_bytes, sizeof(idat->crc));
}

void build_png_end(PngObject *png)
{
	unsigned char iend_type[] = {0x49, 0x45, 0x4E, 0x44};
	unsigned char iend_crc[]  = {0xAE, 0x42, 0x60, 0x82};

	/* update chunk list */
	Chunk *iend = (Chunk *) malloc(sizeof(Chunk));
	Chunk *last_chunk = get_last_chunk(png->chunk_list);
	last_chunk->next = iend;
	iend->next = iend;

	memset(iend->length, 0, sizeof(iend->length));
	memcpy(iend->type, iend_type, sizeof(iend->type));
	memcpy(iend->crc, iend_crc, sizeof(iend->crc));
	iend->data = NULL;
}

PngObject *build_png_chunks(RgbBuffer *rgb_buffer)
{
	PngObject *png = (PngObject *) malloc(sizeof(PngObject));

	build_png_header(png, rgb_buffer);

	build_png_data(png, rgb_buffer);

	build_png_end(png);

	return png;
}

void write_png_to_file(char *png_path, PngObject *png)
{
	unsigned char png_signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
	Chunk *chunk_holder;
	unsigned int data_len;
	FILE *output_fd;

	if (png == NULL) {
		printf("No PngObject, abort writing png file.\n");
		return;
	}
	output_fd = fopen(png_path, "wb");
	if(output_fd == NULL) {
		printf("Failed to open file to write.\n");
	}
	/* write png signature */
	fwrite(png_signature, 1, sizeof(png_signature), output_fd);
	/* write ihdr chunk */
	chunk_holder = remove_first_chunk(png);
	while (chunk_holder != NULL) {
		fwrite(chunk_holder->length, 1, sizeof(chunk_holder->length), output_fd);
		fwrite(chunk_holder->type, 1, sizeof(chunk_holder->type), output_fd);
		data_len = *(unsigned int *)chunk_holder->length;
		data_len = __bswap_32(data_len);
		//if (DBG) printf("chunk data length = %d\n", data_len);
		if (data_len != 0) {
			fwrite(chunk_holder->data, 1, data_len, output_fd);
			free(chunk_holder->data);
		}
		fwrite(chunk_holder->crc, 1, sizeof(chunk_holder->crc), output_fd);
		free(chunk_holder);
		chunk_holder = remove_first_chunk(png);
	}

	fclose(output_fd);
	/* free memory space allocated in build_png */
}

void converter(char *in_raw_path, char *out_png_path, int width, int height)
{
	FileBuffer *raw_buffer;
	Yuv420pBuffer *yuv_obj;
	RgbBuffer *rgb_obj;
	PngObject *png;

	/* load raw into memory */
	raw_buffer = read_raw(in_raw_path);
	/* parse binary data into yuv nv12 format */
	yuv_obj = parse_raw(raw_buffer, width, height);
	/* transform yuv format into rgb format */
	rgb_obj = transform_yuv_to_rgb(yuv_obj);
	/* build png chunks with rgb data */
	png = build_png_chunks(rgb_obj);
	/* write png chunks into file */
	write_png_to_file(out_png_path, png);

	free(in_raw_path);
	free(out_png_path);
}

void print_help()
{
	fprintf(stderr, "Usage: raw2png -i [raw file] -d [width]x[height] -o [png file]\n");
	exit(EXIT_FAILURE);
}
