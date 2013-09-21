/*
 * ACE loader
 *
 * Loads/Dumps an ACE animation to/from an ImageSet structure.
 * ACE files are used by DSA/ROA 2+3
 *
 * Authors: Henne_NWH <henne@nachtwindheim.de>
 *          Hendrik <hermes9@web.de>
 * License: GPLv3
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <packer.h>
#include <format.h>

int sanitycheck_ace(const char *buf, size_t len) {
    // TODO
    return 1;
}
static ImageSet* do_ass(ImageSet* img, struct ace_header *ace, const char *buf, size_t len)
{
    struct ass_header ass;
    uint32_t i, datalen;

    if (len < 6) {
	fprintf(stderr, "Buffer to small for ASS Header\n");
	return NULL;
    }
	
    ass.celwidth = get_sint16(buf);
    ass.celheight = get_sint16(buf + 2);
    ass.amount = buf[4];
    ass.playmode = buf[5];

    img->globalWidth  = get_sint16(buf);
    img->globalHeight = get_sint16(buf + 2);
    img->frameCount   = buf[4];
    img->frames       = (AnimFrame**)malloc(img->frameCount * sizeof(AnimFrame*));
    for (int i = 0; i<img->frameCount; i++) {
	img->frames[i] = (AnimFrame*)malloc(sizeof(AnimFrame));
    }

    printf("\t%03dx%03d\tAmount: %03u\tMode: %03u\n",
	   ass.celwidth, ass.celheight, ass.amount, ass.playmode);

    datalen = 6;
    for (i = 0; i < img->frameCount; i++) {
	struct cel_header cel;

	cel.size = get_sint32(buf + datalen);
	cel.xoffset = get_sint16(buf + datalen + 4);
	cel.yoffset = get_sint16(buf + datalen + 6);
	cel.width = get_sint16(buf + datalen + 8);
	cel.height = get_sint16(buf + datalen + 10);
	cel.compression = buf[datalen + 12];
	cel.action = buf[datalen + 13];

/*		printf("\t\tSize: %08x\tCompression: %x\n",
		cel.size, cel.compression);
*/
	datalen += 14 + cel.size;
    }

    if (datalen + 2 > len) {
	fprintf(stderr, "ASS File is to small for a palette\n");
	return NULL;
    }

    if (len != datalen + 256 * 3) {
	fprintf(stderr, "ASS File has not the expected size\n");
	return NULL;
    }
    img->globalPalette = (Color*)(buf + datalen);

    datalen = 6;
    for (i = 0; i < ass.amount; i++) {
	struct cel_header cel;
	char fname[100];
	AnimFrame* frame = img->frames[i];

	cel.size = get_sint32(buf + datalen);
	cel.xoffset = get_sint16(buf + datalen + 4);
	cel.yoffset = get_sint16(buf + datalen + 6);
	cel.width = get_sint16(buf + datalen + 8);
	cel.height = get_sint16(buf + datalen + 10);
	cel.compression = buf[datalen + 12];
	cel.action = buf[datalen + 13];
	frame->x0     = get_sint16(buf + datalen + 4);
	frame->y0     = get_sint16(buf + datalen + 6);
	frame->width  = get_sint16(buf + datalen + 8);
	frame->height = get_sint16(buf + datalen + 10);
	frame->delay  = ace->speed * 50;
	frame->comment= "";

	frame->pixels = malloc(frame->width * frame->height);
	if (frame->pixels == NULL) {
	    fprintf(stderr, "Failed to allocate seqs\n");
	    return NULL;
	}

	switch (cel.compression) {
	case 0x32:	/* PP20 */
	    ppdepack(buf + datalen + 14, frame->pixels,
		     cel.size, frame->width * frame->height);
	    break;
	case 0x1:	/* RLE */
	    un_rle(buf + datalen + 14, frame->pixels, cel.size);
	    break;
	case 0x2:	/* RLE */
	    un_rl(buf + datalen + 14, frame->pixels, cel.size);
	    break;
	default:
	    fprintf(stdout, "Unknown ACE Compression %x\n",
		    cel.compression);
	}

	datalen += 14 + cel.size;
    }
	
    return img;
}

static ImageSet* do_seq(ImageSet* img, struct ace_header *ace, const char *buf, size_t len) {
    uint32_t datalen = 0, framecounter = 0, i, j;
    struct seq_header * seqs;
    AnimFrame* frame;
    
    if (len < ace->sequences*sizeof(struct seq_header)) {
	fprintf(stderr, "Buffer to small for SEQ Headers\n");
	return NULL;
    }

    seqs=malloc(ace->sequences*sizeof(struct seq_header));
    if (!seqs) {
	fprintf(stderr, "Failed to allocate seqs\n");
	return NULL;
    }
    
    img->globalPalette = (Color*)(buf + len - 768);
    // Folgende Werte werden während des Durchlaufens der Sequenzen bestimmt.
    img->globalWidth   = 0;
    img->globalHeight  = 0;
    img->frameCount = 0;
    
    // Erstmaliges Durchlaufen der Sequenzen, um Header-Gesamtgröße zu bestimmen
    for (i = 0; i < ace->sequences; i++) {
	struct seq_header *seq=&seqs[i];
	
	seq->offset = get_sint32(buf + datalen);
	seq->label = get_sint16(buf + datalen + 4);
	seq->celwidth = get_sint16(buf + datalen + 6);
	seq->celheight = get_sint16(buf + datalen + 8);
	seq->hotspotx = get_sint16(buf + datalen + 10);
	seq->hotspoty = get_sint16(buf + datalen + 12);
	seq->amount = buf[datalen + 14];
	seq->playmode = buf[datalen + 15];
/*
	printf("\tSequence: %d\tImages: %d\tOffset: %x\n", i,
	       seq->amount, seq->offset);
*/
	datalen += sizeof(*seq);
	img->frameCount += seq->amount;
    }
    
    img->frames = (AnimFrame**)malloc(img->frameCount * sizeof(AnimFrame*));
    for (int i = 0; i<img->frameCount; i++) {
	img->frames[i] = (AnimFrame*)malloc(sizeof(AnimFrame));
    }
  
    for (i=0; i < ace->sequences; i++) {
	uint16_t pos = 0;

	for (j=0; j < seqs[i].amount; j++) {
	    struct cel_header cel;
	    char fname[16];

	    frame = img->frames[framecounter++];

	    cel.size = get_uint32(buf+datalen);
	    cel.xoffset = get_sint16(buf+datalen + 4);
	    cel.yoffset = get_sint16(buf+datalen + 6);
	    cel.width = get_uint16(buf+datalen + 8);
	    cel.height = get_uint16(buf+datalen + 10);
	    cel.compression = *(uint8_t*)(buf+datalen + 12);
	    cel.action = *(uint8_t*)(buf+datalen + 13);
	    frame->x0     = get_sint16(buf + datalen + 4);
	    frame->y0     = get_sint16(buf + datalen + 6);
	    frame->width  = get_uint16(buf + datalen + 8);
	    frame->height = get_uint16(buf + datalen + 10);
	    frame->delay  = ace->speed * 50;
	    frame->comment= (char*)malloc(7*sizeof(char));
	    sprintf(frame->comment, "%d", seqs[i].label);
	    // TODO: playmode, hotspotx, hotspoty, action
	    
	    if ((frame->width  + frame->x0)  > img->globalWidth)
		img->globalWidth   = frame->width  + frame->x0;
	    if ((frame->height + frame->y0)  > img->globalHeight)
		img->globalHeight  = frame->height + frame->y0;
	    
	    /*
	      printf("Sequence: %d\tCel: %d\n", i, j);
	      printf("\tSize: %d\tCompression: %02x\n", cel.size, cel.compression);
	    */

	    switch (cel.compression) {
	    case 0x1:
		frame->pixels = malloc(cel.width * cel.height);
		if (!frame->pixels) {
		    fprintf(stderr,
			    "Failed to allocate seqs\n");
		    return NULL;
		}
		un_rle(buf+seqs[i].offset+6+pos, frame->pixels, cel.size);
		break;
	    case 0x2:
		frame->pixels = malloc(cel.width * cel.height);
		if (!frame->pixels) {
		    fprintf(stderr,
			    "Failed to allocate seqs\n");
		    return NULL;
		}
		un_rl(buf+seqs[i].offset+6+pos, frame->pixels, cel.size);
		break;
	    case 0x32:
		frame->pixels = malloc(cel.width * cel.height);
		if (!frame->pixels) {
		    fprintf(stderr,
			    "Failed to allocate seqs\n");
		    return NULL;
		}
		ppdepack(buf+seqs[i].offset+6+pos, frame->pixels, cel.size, cel.width * cel.height);
		break;
	    default:
		printf("Compression 0x%x is not supportet yet\n", cel.compression);
	    }
	    pos += cel.size + 14;
	    datalen += cel.size + 14;
	}
    }

    if (len < datalen) {
	fprintf(stderr, "ACE-SEQ file is to small for CEL Headers\n");
	return NULL;
    }
    datalen += 768;
    if (datalen != len)
	printf("len: %ld datalen %d\n", len, datalen);
    free(seqs);
    
    return img;
}

ImageSet* process_ace(const char *buf, size_t len)
{
    struct ace_header ace;
    ImageSet* img;

    if (!buf) {
	fprintf(stderr, "%s() got NULL Ptr\n", __func__);
	return NULL;
    }

    if (len < 8) {
	fprintf(stderr, "Buffer is too small for ACE header\n");
	return NULL;
    }

    strncpy(ace.label, buf, 4);
    ace.version = get_sint16(buf + 4);
    ace.sequences = buf[6];
    ace.speed = buf[7];

    if (strncmp(ace.label, "ACE\0", 4)) {
	fprintf(stderr, "No ACE Signature\n");
	return NULL;
    }

    if (ace.version != 1) {
	fprintf(stderr, "ACE Version is not 1\n");
	return NULL;
    }

    if (ace.sequences > 250) {
	fprintf(stderr, "ACE sequences %u is not a valid value\n",
		ace.sequences);
	return NULL;
    }
    if (ace.speed > 99) {
	fprintf(stderr, "ACE speed %u is not a valid value\n",
		ace.speed);
	return NULL;
    }

    printf("Sequences: %03u\tSpeed: %02u\n", ace.sequences, ace.speed);

    img = (ImageSet*)malloc(sizeof(ImageSet));

    if (ace.sequences == 1) {
	do_ass(img, &ace, buf + 8, len - 8);
    } else {
	do_seq(img, &ace, buf + 8, len - 8);
    }

    return img;
}

int dump_ace(ImageSet* img, char* prefix) {
    // TODO
    return 1;
}
