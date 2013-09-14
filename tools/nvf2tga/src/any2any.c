/*
 * NLT image converter
 * Converts the following image formats
 * - Nordlandtrilogie formats (NVF, AIF, ACE, RAW, BOB)
 *   These often contain series of images or animations.
 * - GIF files
 * 
 * NVF files are used by DSA/ROA 2+3
 * Some files of DSA/ROA 1 work, too, but aren't supported.
 *
 * Author: Henne_NWH <henne@nachtwindheim.de>, Hendrik Radke <hermes9@gmx.net>
 * License: GPLv3
 *
 * Compilation: gcc -o any2any any2any.c
 * Usage:	./any2any infile outfile
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>  // toupper()
#include <stdint.h> // int32_t
#include <string.h>

#include <loader.h>
#include <version.h>

char* strtoupper(char* str) {
	int i, len = strlen(str);
	for(i=0; i<len; i++) {
		str[i]=toupper(str[i]);
	}
	return str;
}

int main(int argc, char **argv)
{

	FILE *fd = NULL;
	char *buf, *tmp;
	int32_t flen, readlen;
	ImageSet* img;

	if (argc == 1) {
		printf("%s %s\n", BANNER, VERSION);
		printf("Usage: %s <infile> <outfile>\n", argv[0]);
		exit(1);
	}

	fd = fopen(argv[1], "rb");
	if (!fd) {
		fprintf(stderr, "Cant open file %s\n", argv[1]);
		exit(1);
	}

	fseek(fd, 0, SEEK_END);
	flen = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	/* flen == -1 happens when opening a directory with fopen() under Linux. */
	if (flen < 3) {
		fprintf(stderr, "File %s is to small.\n", argv[1]);
		fclose(fd);
		exit(1);
	}

	buf = calloc(flen, sizeof(char));
	if (!buf) {
		fprintf(stderr, "Not enough memory\n");
		fclose(fd);
		exit(1);
	}

	readlen = fread(buf, sizeof(char), flen, fd);
	fclose(fd);
	if (readlen != flen) {
		fprintf(stderr, "Could not read the whole file.\n");
		free(buf);
		exit(1);
	}
	
	// Determine the input file type
	tmp = strrchr(strtoupper(argv[1]), '.') + 1;
	if        (strcmp(tmp, "NVF") == 0) {
		if (!sanitycheck_nvf(buf, flen)) exit(1);
		img = process_nvf(buf, flen);
	} else if (strcmp(tmp, "RAW") == 0) {
		if (!sanitycheck_raw(buf, flen)) exit(1);
		img = process_raw(buf, flen);
	} else if (strcmp(tmp, "AIF") == 0) {
		if (!sanitycheck_aif(buf, flen)) exit(1);
		img = process_aif(buf, flen);
	} else if (strcmp(tmp, "ACE") == 0) {
		if (!sanitycheck_ace(buf, flen)) exit(1);
		img = process_ace(buf, flen);
	} else if (strcmp(tmp, "BOB") == 0) {
		if (!sanitycheck_bob(buf, flen)) exit(1);
		img = process_bob(buf, flen);
	} else if (strcmp(tmp, "GIF") == 0) {
		if (!sanitycheck_gif(buf, flen)) exit(1);
		img = process_gif(buf, flen);
	} else {
		fprintf(stderr, "Unknown input filetype '%s'\n", tmp);
		exit(1);
	}
	
	if (img == NULL) {
		fprintf(stderr, "Error processing image '%s'\n", tmp);
		exit(1);
	}

	// Determine the output file type
	tmp = strrchr(strtoupper(argv[2]), '.') + 1;
	if        (strcmp(tmp, "NVF") == 0) {
		dump_nvf(img);
	} else if (strcmp(tmp, "RAW") == 0) {
		dump_raw(img);
	} else if (strcmp(tmp, "AIF") == 0) {
		dump_aif(img);
	} else if (strcmp(tmp, "ACE") == 0) {
		dump_ace(img);
	} else if (strcmp(tmp, "BOB") == 0) {
		dump_bob(img);
	} else if (strcmp(tmp, "GIF") == 0) {
		dump_gif(img);
	} else if (strcmp(tmp, "TGA") == 0) {
		dump_tga(img);
	} else {
		fprintf(stderr, "Unknown output filetype '%s'\n", tmp);
		exit(1);
	}
	printf("image dumped\n");
	/*for (int i=0; i<img->frameCount; i++) {
		free(img->frames[i]->pixels);
		free(img->frames[i]);
		}*/
	printf("pixels/frames destroyed\n");
	//free(img);

	exit(0);
}
