#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <byteswap.h>
#include <zlib.h>

#define MAX_PATH_LEN 256
#define CHUNK_SIZE_WO_DATA sizeof(Chunk) - 2 * sizeof(void *)

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
	unsigned char *u;
	unsigned char *v;
} Yuv420pBuffer;

typedef struct RgbBufferObject
{
	int dimen_x;
	int dimen_y;
	int num_of_pixels;
	unsigned char *buffer;
	long size;
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

Yuv420pBuffer *parse_raw(FileBuffer *raw_buffer)
{
	int uv_index;
	unsigned char *uv_plane;
	Yuv420pBuffer *yuv_obj = (Yuv420pBuffer *) malloc(sizeof(Yuv420pBuffer));
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

/* transform yuv format into rgb 24bit format */
RgbBuffer *transform_yuv_to_rgb(Yuv420pBuffer *yuv_obj)
{
	int x, y, uv_plane_x, uv_plane_y, u_prime, v_prime, yuv_y;
	unsigned char *write_ptr;
	RgbBuffer *rgb_obj = (RgbBuffer *) malloc(sizeof(RgbBuffer));
	rgb_obj->num_of_pixels = yuv_obj->num_of_pixels;
	rgb_obj->dimen_x = yuv_obj->dimen_x;
	rgb_obj->dimen_y = yuv_obj->dimen_y;
	/* every line contains a filter type byte */
	rgb_obj->size = yuv_obj->num_of_pixels * 3 + yuv_obj->dimen_y;
	rgb_obj->buffer = (unsigned char *) malloc(rgb_obj->size);
	write_ptr = rgb_obj->buffer;

	/* calculate RGB value for each pixel and write to rgb_obj->buffer */
	/* R = Y + 1.402 * (V - 128) */
	/* G = Y - 0.344 * (U - 128) - 0.714 * (V - 128) */
	/* B = Y + 1.722 * (U - 128) */
	/* formula is found on StackOverflow */
	u_prime = (int)(yuv_obj->u[0]) - 128;
	v_prime = (int)(yuv_obj->v[0]) - 128;
	for (y = 0, uv_plane_y = 0; y < yuv_obj->dimen_y; y++) {
		*write_ptr = 0; // filter type byte for each line
		write_ptr += 1;
		for (x = 0, uv_plane_y = 0; x < yuv_obj->dimen_x; x++) {
			yuv_y = yuv_obj->y[y * yuv_obj->dimen_x + x];
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
			0x02, /* color type rgb triple */
			0x00, /* compressed method */
			0x00, /* filter method */
			0x00 /* interface method */ };
	unsigned char ihdr_chunk_crc_test[] = {
			0x49, 0x48, 0x44, 0x52, /* chunk type IHDR */
			0x00, 0x00, 0x02, 0x80, /* width 640 */
			0x00, 0x00, 0x01, 0xE0, /* height 480 */
			0x08, /* bit depth */
			0x02, /* color type rgb triple */
			0x00, /* compressed method */
			0x00, /* filter method */
			0x00 /* interface method */ };
	long chunk_crc;
	unsigned int reversed_bytes;

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
	memcpy(ihdr->data, ihdr_chunk_data, sizeof(ihdr_chunk_data));

	chunk_crc = calc_chunk_crc(ihdr_chunk_type, ihdr_chunk_data, sizeof(ihdr_chunk_data));

	reversed_bytes = __bswap_32(chunk_crc);
	memcpy(ihdr->crc, &reversed_bytes, sizeof(ihdr->crc));

	png->length += CHUNK_SIZE_WO_DATA; /* exclude 2 pointers in struct */
	png->length += sizeof(ihdr_chunk_data); /* include size of data */;
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
	} else if (rgb_buf_len != 0) {
		printf("not being comsumed rgb buffer size: %ld\n", rgb_buf_len);
	} else if (DBG) {
		printf("allocated compress buffer size: %ld\n", compress_buf_len);
	}

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

	png->length += CHUNK_SIZE_WO_DATA; /* exclude 2 pointers in struct */
	png->length += compress_buf_len; /* include size of idat data */;
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

	png->length += CHUNK_SIZE_WO_DATA; /* exclude pointer to data */
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
		if (DBG) printf("chunk data length = %d\n", data_len);
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

void converter(char *in_raw_path, char *out_png_path)
{
	FileBuffer *raw_buffer;
	Yuv420pBuffer *yuv_obj;
	RgbBuffer *rgb_obj;
	PngObject *png;

	/* load raw into memory */
	raw_buffer = read_raw(in_raw_path);

	yuv_obj = parse_raw(raw_buffer);

	rgb_obj = transform_yuv_to_rgb(yuv_obj);

	png = build_png_chunks(rgb_obj);

	write_png_to_file(out_png_path, png);
}

void print_help()
{
	printf("Usage: raw2png [raw file] [png file]\n");
}
