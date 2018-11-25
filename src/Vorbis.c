#include "Vorbis.h"
#include "ErrorHandler.h"
#include "Platform.h"
#include "Event.h"
#include "Block.h"
#include "ExtMath.h"
#include "Funcs.h"
#include "Errors.h"
#include "Stream.h"

/*########################################################################################################################*
*-------------------------------------------------------Ogg stream--------------------------------------------------------*
*#########################################################################################################################*/
#define OGG_FourCC(a, b, c, d) (((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d)
static ReturnCode Ogg_NextPage(struct Stream* stream) {
	uint8_t header[27];
	struct Stream* source;
	uint32_t sig, size;
	int i, numSegments;
	uint8_t segments[255];
	ReturnCode res;

	/* OGG page format:
	* header[0]  (4) page signature
	* header[4]  (1) page version
	* header[5]  (1) page flags
	* header[6]  (8) granule position
	* header[14] (4) serial number
	* header[18] (4) page sequence number
	* header[22] (4) page checksum
	* header[26] (1) number of segments
	* [number of segments] number of bytes in each segment
	* [sum of bytes in each segment] page data
	*/
	source = stream->Meta.Ogg.Source;
	if ((res = Stream_Read(source, header, sizeof(header)))) return res;

	sig = Stream_GetU32_BE(&header[0]);
	if (sig != OGG_FourCC('O','g','g','S')) return OGG_ERR_INVALID_SIG;
	if (header[4] != 0) return OGG_ERR_VERSION;

	numSegments = header[26];
	size = 0;
	if ((res = Stream_Read(source, segments, numSegments))) return res;
	for (i = 0; i < numSegments; i++) size += segments[i];

	if ((res = Stream_Read(source, stream->Meta.Ogg.Base, size))) return res;
	stream->Meta.Ogg.Cur  = stream->Meta.Ogg.Base;
	stream->Meta.Ogg.Left = size;
	stream->Meta.Ogg.Last = header[5] & 4;
	return 0;
}

static ReturnCode Ogg_Read(struct Stream* stream, uint8_t* data, uint32_t count, uint32_t* modified) {
	ReturnCode res;
	for (;;) {
		if (stream->Meta.Ogg.Left) {
			count = min(count, stream->Meta.Ogg.Left);
			Mem_Copy(data, stream->Meta.Ogg.Cur, count);

			*modified = count;
			stream->Meta.Ogg.Cur  += count;
			stream->Meta.Ogg.Left -= count;
			return 0;
		}

		/* try again with data from next page */
		*modified = 0;
		if (stream->Meta.Ogg.Last) return 0;
		if ((res = Ogg_NextPage(stream))) return res;
	}
}

static ReturnCode Ogg_ReadU8(struct Stream* stream, uint8_t* data) {
	if (!stream->Meta.Ogg.Left) return Stream_DefaultReadU8(stream, data);

	*data = *stream->Meta.Ogg.Cur;
	stream->Meta.Ogg.Cur++; 
	stream->Meta.Ogg.Left--;
	return 0;
}

void Ogg_MakeStream(struct Stream* stream, uint8_t* buffer, struct Stream* source) {
	Stream_Init(stream);
	stream->Read   = Ogg_Read;
	stream->ReadU8 = Ogg_ReadU8;

	stream->Meta.Ogg.Cur  = buffer;
	stream->Meta.Ogg.Base = buffer;
	stream->Meta.Ogg.Left = 0;
	stream->Meta.Ogg.Last = 0;
	stream->Meta.Ogg.Source = source;
}


/*########################################################################################################################*
*------------------------------------------------------Vorbis utils-------------------------------------------------------*
*#########################################################################################################################*/
#define Vorbis_PushByte(ctx, value) ctx->Bits |= (uint32_t)(value) << ctx->NumBits; ctx->NumBits += 8;
#define Vorbis_PeekBits(ctx, bits) (ctx->Bits & ((1UL << (bits)) - 1UL))
#define Vorbis_ConsumeBits(ctx, bits) ctx->Bits >>= (bits); ctx->NumBits -= (bits);
/* Aligns bit buffer to be on a byte boundary */
#define Vorbis_AlignBits(ctx) alignSkip = ctx->NumBits & 7; Vorbis_ConsumeBits(ctx, alignSkip);

/* TODO: Make sure this is inlined */
static uint32_t Vorbis_ReadBits(struct VorbisState* ctx, uint32_t bitsCount) {
	uint8_t portion;
	uint32_t data;
	ReturnCode res;

	while (ctx->NumBits < bitsCount) {
		res = ctx->Source->ReadU8(ctx->Source, &portion);
		if (res) { ErrorHandler_Fail2(res, "Failed to read byte for vorbis"); }
		Vorbis_PushByte(ctx, portion);
	}

	data = Vorbis_PeekBits(ctx, bitsCount); Vorbis_ConsumeBits(ctx, bitsCount);
	return data;
}

static ReturnCode Vorbis_TryReadBits(struct VorbisState* ctx, uint32_t bitsCount, uint32_t* data) {
	uint8_t portion;
	ReturnCode res;

	while (ctx->NumBits < bitsCount) {
		res = ctx->Source->ReadU8(ctx->Source, &portion);
		if (res) return res;
		Vorbis_PushByte(ctx, portion);
	}

	*data = Vorbis_PeekBits(ctx, bitsCount); Vorbis_ConsumeBits(ctx, bitsCount);
	return 0;
}

static uint32_t Vorbis_ReadBit(struct VorbisState* ctx) {
	uint8_t portion;
	uint32_t data;
	ReturnCode res;

	if (!ctx->NumBits) {
		res = ctx->Source->ReadU8(ctx->Source, &portion);
		if (res) { ErrorHandler_Fail2(res, "Failed to read byte for vorbis"); }
		Vorbis_PushByte(ctx, portion);
	}

	data = Vorbis_PeekBits(ctx, 1); Vorbis_ConsumeBits(ctx, 1);
	return data;
}


static int iLog(int x) {
	int bits = 0;
	while (x > 0) { bits++; x >>= 1; }
	return bits;
}

static float float32_unpack(struct VorbisState* ctx) {
	/* ReadBits can't reliably read over 24 bits */
	uint32_t lo = Vorbis_ReadBits(ctx, 16);
	uint32_t hi = Vorbis_ReadBits(ctx, 16);
	uint32_t x = (hi << 16) | lo;

	int32_t mantissa  =  x & 0x1fffff;
	uint32_t exponent = (x & 0x7fe00000) >> 21;
	if (x & 0x80000000UL) mantissa = -mantissa;

	#define LOG_2 0.693147180559945
	/* TODO: Can we make this more accurate? maybe ldexp ?? */
	return (float)(mantissa * Math_Exp(LOG_2 * ((int)exponent - 788))); /* pow(2, x) */
}


/*########################################################################################################################*
*----------------------------------------------------Vorbis codebooks-----------------------------------------------------*
*#########################################################################################################################*/
#define CODEBOOK_SYNC 0x564342
struct Codebook {
	uint32_t Dimensions, Entries, TotalCodewords;
	uint32_t* Codewords;
	uint32_t* Values;
	uint32_t NumCodewords[33]; /* number of codewords of bit length i */
	/* vector quantisation values */
	float MinValue, DeltaValue;
	uint32_t SequenceP, LookupType, LookupValues;
	uint16_t* Multiplicands;
};

static void Codebook_Free(struct Codebook* c) {
	Mem_Free(c->Codewords);
	Mem_Free(c->Values);
	Mem_Free(c->Multiplicands);
}

static uint32_t Codebook_Pow(uint32_t base, uint32_t exp) {
	uint32_t result = 1; /* exponentiation by squaring */
	while (exp) {
		if (exp & 1) result *= base;
		exp >>= 1;
		base *= base;
	}
	return result;
}

static uint32_t Codebook_Lookup1Values(uint32_t entries, uint32_t dimensions) {
	uint32_t i, pow, next;
	/* the greatest integer value for which [value] to the power of [dimensions] is less than or equal to [entries] */
	/* TODO: verify this */
	for (i = 1; ; i++) {
		pow  = Codebook_Pow(i,     dimensions);
		next = Codebook_Pow(i + 1, dimensions);

		if (next < pow)     return i; /* overflow */
		if (pow == entries) return i;
		if (next > entries) return i;
	}
	return 0;
}

static bool Codebook_CalcCodewords(struct Codebook* c, uint8_t* len) {
	/* This is taken from stb_vorbis.c because I gave up trying */
	uint32_t i, depth;
	uint32_t root, codeword;
	uint32_t next_codewords[33] = { 0 };
	int offset;
	int len_offsets[33];

	c->Codewords = Mem_Alloc(c->TotalCodewords, 4, "codewords");
	c->Values    = Mem_Alloc(c->TotalCodewords, 4, "values");

	/* Codeword entries are ordered by length */
	offset = 0;
	for (i = 0; i < Array_Elems(len_offsets); i++) {
		len_offsets[i] = offset;
		offset += c->NumCodewords[i];
	}

	/* add codeword 0 to tree */
	for (i = 0; i < c->Entries; i++) {
		if (!len[i]) continue;
		offset = len_offsets[len[i]];

		c->Codewords[offset] = 0;
		c->Values[offset]    = i;

		len_offsets[len[i]]++;
		break;
	}

	/* set codewords that new nodes can start from */
	for (depth = 1; depth <= len[i]; depth++) {
		next_codewords[depth] = 1U << (32 - depth);
	}

	i++; /* first codeword was already handled */
	for (; i < c->Entries; i++) {
		root = len[i];
		if (!root) continue;
		offset = len_offsets[len[i]];

		/* per spec, find lowest possible value (leftmost) */
		while (root && next_codewords[root] == 0) root--;
		if (root == 0) return false;

		codeword = next_codewords[root];
		next_codewords[root] = 0;

		c->Codewords[offset] = codeword;
		c->Values[offset]    = i;

		for (depth = len[i]; depth > root; depth--) {
			next_codewords[depth] = codeword + (1U << (32 - depth));
		}

		len_offsets[len[i]]++;
	}
	return true;
}

static ReturnCode Codebook_DecodeSetup(struct VorbisState* ctx, struct Codebook* c) {
	uint32_t sync;
	uint8_t* codewordLens;
	int i, entry;
	int sparse, len;
	int runBits, runLen;
	int valueBits;
	uint32_t lookupValues;

	sync = Vorbis_ReadBits(ctx, 24);
	if (sync != CODEBOOK_SYNC) return VORBIS_ERR_CODEBOOK_SYNC;
	c->Dimensions = Vorbis_ReadBits(ctx, 16);
	c->Entries    = Vorbis_ReadBits(ctx, 24);

	codewordLens = Mem_Alloc(c->Entries, 1, "raw codeword lens");
	for (i = 0; i < Array_Elems(c->NumCodewords); i++) {
		c->NumCodewords[i] = 0;
	}

	/* ordered entries flag */
	if (!Vorbis_ReadBit(ctx)) {
		sparse = Vorbis_ReadBit(ctx);
		entry  = 0;
		for (i = 0; i < c->Entries; i++) {
			/* sparse trees may not have all entries */
			if (sparse && !Vorbis_ReadBit(ctx)){
				codewordLens[i] = 0;
				continue; /* unused entry */
			}

			len = Vorbis_ReadBits(ctx, 5) + 1;
			codewordLens[i] = len;
			c->NumCodewords[len]++;
			entry++;
		}
	} else {
		len = Vorbis_ReadBits(ctx, 5) + 1;
		for (entry = 0; entry < c->Entries; entry += runLen) {
			runBits = iLog(c->Entries - entry);
			runLen  = Vorbis_ReadBits(ctx, runBits);

			for (i = entry; i < entry + runLen; i++) {
				codewordLens[i] = len;
			}
			c->NumCodewords[len++] = runLen;
			if (entry > c->Entries) return VORBIS_ERR_CODEBOOK_ENTRY;
		}
		entry = c->Entries;
	}

	c->TotalCodewords = entry;
	Codebook_CalcCodewords(c, codewordLens);
	Mem_Free(codewordLens);

	c->LookupType    = Vorbis_ReadBits(ctx, 4);
	c->Multiplicands = NULL;
	if (c->LookupType == 0) return 0;
	if (c->LookupType > 2)  return VORBIS_ERR_CODEBOOK_LOOKUP;

	c->MinValue   = float32_unpack(ctx);
	c->DeltaValue = float32_unpack(ctx);
	valueBits     = Vorbis_ReadBits(ctx, 4) + 1;
	c->SequenceP  = Vorbis_ReadBit(ctx);

	if (c->LookupType == 1) {
		lookupValues = Codebook_Lookup1Values(c->Entries, c->Dimensions);
	} else {
		lookupValues = c->Entries * c->Dimensions;
	}
	c->LookupValues = lookupValues;

	c->Multiplicands = Mem_Alloc(lookupValues, 2, "multiplicands");
	for (i = 0; i < lookupValues; i++) {
		c->Multiplicands[i] = Vorbis_ReadBits(ctx, valueBits);
	}
	return 0;
}

static uint32_t Codebook_DecodeScalar(struct VorbisState* ctx, struct Codebook* c) {
	uint32_t codeword = 0, shift = 31, depth, i;
	uint32_t* codewords = c->Codewords;
	uint32_t* values    = c->Values;

	/* TODO: This is so massively slow */
	for (depth = 1; depth <= 32; depth++, shift--) {
		codeword |= Vorbis_ReadBit(ctx) << shift;

		for (i = 0; i < c->NumCodewords[depth]; i++) {
			if (codeword != codewords[i]) continue;
			return values[i];
		}

		codewords += c->NumCodewords[depth];
		values    += c->NumCodewords[depth];
	}
	ErrorHandler_Fail("Invalid huffman code");
	return -1;
}

static void Codebook_DecodeVectors(struct VorbisState* ctx, struct Codebook* c, float* v, int step) {
	uint32_t lookupOffset = Codebook_DecodeScalar(ctx, c);
	float last = 0.0f, value;
	uint32_t i, offset;

	if (c->LookupType == 1) {		
		uint32_t indexDivisor = 1;
		for (i = 0; i < c->Dimensions; i++, v += step) {
			offset = (lookupOffset / indexDivisor) % c->LookupValues;
			value  = c->Multiplicands[offset] * c->DeltaValue + c->MinValue + last;

			*v += value;
			if (c->SequenceP) last = value;
			indexDivisor *= c->LookupValues;
		}
	} else if (c->LookupType == 2) {
		offset = lookupOffset * c->Dimensions;
		for (i = 0; i < c->Dimensions; i++, offset++, v += step) {
			value  = c->Multiplicands[offset] * c->DeltaValue + c->MinValue + last;

			*v += value;
			if (c->SequenceP) last = value;
		}
	} else {
		ErrorHandler_Fail("Invalid huffman code");
	}
}

/*########################################################################################################################*
*-----------------------------------------------------Vorbis floors-------------------------------------------------------*
*#########################################################################################################################*/
#define FLOOR_MAX_PARTITIONS 32
#define FLOOR_MAX_CLASSES 16
#define FLOOR_MAX_VALUES (FLOOR_MAX_PARTITIONS * 8 + 2)
struct Floor {
	uint8_t Partitions, Multiplier; int32_t Range, Values;
	uint8_t PartitionClasses[FLOOR_MAX_PARTITIONS];
	uint8_t ClassDimensions[FLOOR_MAX_CLASSES];
	uint8_t ClassSubClasses[FLOOR_MAX_CLASSES];
	uint8_t ClassMasterbooks[FLOOR_MAX_CLASSES];
	int16_t SubclassBooks[FLOOR_MAX_CLASSES][8];
	int16_t  XList[FLOOR_MAX_VALUES];
	uint16_t ListOrder[FLOOR_MAX_VALUES];
	int32_t  YList[VORBIS_MAX_CHANS][FLOOR_MAX_VALUES];
};

/* TODO: Make this thread safe */
static int16_t* tmp_xlist;
static uint16_t* tmp_order;
static void Floor_SortXList(int left, int right) {
	uint16_t* values = tmp_order; uint16_t value;
	int16_t* keys = tmp_xlist;    int16_t key;

	while (left < right) {
		int i = left, j = right;
		int16_t pivot = keys[(i + j) >> 1];

		/* partition the list */
		while (i <= j) {
			while (pivot > keys[i]) i++;
			while (pivot < keys[j]) j--;
			QuickSort_Swap_KV_Maybe();
		}
		/* recurse into the smaller subset */
		QuickSort_Recurse(Floor_SortXList)
	}
}

static ReturnCode Floor_DecodeSetup(struct VorbisState* ctx, struct Floor* f) {
	static int16_t ranges[4] = { 256, 128, 84, 64 };
	int i, j, idx, maxClass;
	int rangeBits, classNum;
	int16_t xlist_sorted[FLOOR_MAX_VALUES];

	f->Partitions = Vorbis_ReadBits(ctx, 5);
	maxClass = -1;
	for (i = 0; i < f->Partitions; i++) {
		f->PartitionClasses[i] = Vorbis_ReadBits(ctx, 4);
		maxClass = max(maxClass, f->PartitionClasses[i]);
	}

	for (i = 0; i <= maxClass; i++) {
		f->ClassDimensions[i] = Vorbis_ReadBits(ctx, 3) + 1;
		f->ClassSubClasses[i] = Vorbis_ReadBits(ctx, 2);

		if (f->ClassSubClasses[i]) {
			f->ClassMasterbooks[i] = Vorbis_ReadBits(ctx, 8);
		}
		for (j = 0; j < (1 << f->ClassSubClasses[i]); j++) {
			f->SubclassBooks[i][j] = (int16_t)Vorbis_ReadBits(ctx, 8) - 1;
		}
	}

	f->Multiplier = Vorbis_ReadBits(ctx, 2) + 1;
	f->Range      = ranges[f->Multiplier - 1];
	rangeBits     = Vorbis_ReadBits(ctx, 4);

	f->XList[0] = 0;
	f->XList[1] = 1 << rangeBits;
	for (i = 0, idx = 2; i < f->Partitions; i++) {
		classNum = f->PartitionClasses[i];

		for (j = 0; j < f->ClassDimensions[classNum]; j++) {
			f->XList[idx++] = Vorbis_ReadBits(ctx, rangeBits);
		}
	}
	f->Values = idx;

	/* sort X list for curve computation later */
	Mem_Copy(xlist_sorted, f->XList, idx * 2);
	for (i = 0; i < idx; i++) { f->ListOrder[i] = i; }

	tmp_xlist = xlist_sorted; 
	tmp_order = f->ListOrder;
	Floor_SortXList(0, idx - 1);
	return 0;
}

static bool Floor_DecodeFrame(struct VorbisState* ctx, struct Floor* f, int ch) {
	int32_t* yList;
	int i, j, idx, rangeBits;
	uint8_t class, cdim, cbits;
	int bookNum;
	uint32_t csub, cval;

	/* does this frame have any energy */
	if (!Vorbis_ReadBit(ctx)) return false;
	yList = f->YList[ch];

	rangeBits = iLog(f->Range - 1);
	yList[0]  = Vorbis_ReadBits(ctx, rangeBits);
	yList[1]  = Vorbis_ReadBits(ctx, rangeBits);

	for (i = 0, idx = 2; i < f->Partitions; i++) {
		class = f->PartitionClasses[i];
		cdim  = f->ClassDimensions[class];
		cbits = f->ClassSubClasses[class];

		csub = (1 << cbits) - 1;
		cval = 0;
		if (cbits) {
			bookNum = f->ClassMasterbooks[class];
			cval = Codebook_DecodeScalar(ctx, &ctx->Codebooks[bookNum]);
		}

		for (j = 0; j < cdim; j++) {
			bookNum = f->SubclassBooks[class][cval & csub];
			cval >>= cbits;

			if (bookNum >= 0) {
				yList[idx + j] = Codebook_DecodeScalar(ctx, &ctx->Codebooks[bookNum]);
			} else {
				yList[idx + j] = 0;
			}
		}
		idx += cdim;
	}
	return true;
}

static int Floor_RenderPoint(int x0, int y0, int x1, int y1, int X) {
	int dy  = y1 - y0, adx = x1 - x0;
	int ady = Math_AbsI(dy);
	int err = ady * (X - x0);
	int off = err / adx;

	if (dy < 0) {
		return y0 - off;
	} else {
		return y0 + off;
	}
}

static float floor1_inverse_dB_table[256];
static void Floor_RenderLine(int x0, int y0, int x1, int y1, float* data) {
	int dy   = y1 - y0, adx = x1 - x0;
	int ady  = Math_AbsI(dy);
	int base = dy / adx, sy;
	int x    = x0, y = y0, err = 0;

	if (dy < 0) {
		sy = base - 1;
	} else {
		sy = base + 1;
	}

	ady = ady - Math_AbsI(base) * adx;
	data[x] *= floor1_inverse_dB_table[y];

	for (x = x0 + 1; x < x1; x++) {
		err = err + ady;
		if (err >= adx) {
			err = err - adx;
			y = y + sy;
		} else {
			y = y + base;
		}
		data[x] *= floor1_inverse_dB_table[y];
	}
}

static int low_neighbor(int16_t* v, int x) {
	int n = 0, i, max = Int32_MinValue;
	for (i = 0; i < x; i++) {
		if (v[i] < v[x] && v[i] > max) { n = i; max = v[i]; }
	}
	return n;
}

static int high_neighbor(int16_t* v, int x) {
	int n = 0, i, min = Int32_MaxValue;
	for (i = 0; i < x; i++) {
		if (v[i] > v[x] && v[i] < min) { n = i; min = v[i]; }
	}
	return n;
}

static void Floor_Synthesis(struct VorbisState* ctx, struct Floor* f, int ch) {
	/* amplitude arrays */
	int32_t YFinal[FLOOR_MAX_VALUES];
	bool Step2[FLOOR_MAX_VALUES];
	int32_t* yList;
	float* data;
	/* amplitude variables */
	int lo_offset, hi_offset, predicted;
	int val, highroom, lowroom, room;
	int i;
	/* curve variables */
	int lx, hx, ly, hy;
	int rawI;
	float value;

	/* amplitude value synthesis */
	yList = f->YList[ch];
	data  = ctx->CurOutput[ch];

	Step2[0]  = true;
	Step2[1]  = true;
	YFinal[0] = yList[0];
	YFinal[1] = yList[1];

	for (i = 2; i < f->Values; i++) {
		lo_offset = low_neighbor(f->XList, i);
		hi_offset = high_neighbor(f->XList, i);
		predicted = Floor_RenderPoint(f->XList[lo_offset], YFinal[lo_offset],
									  f->XList[hi_offset], YFinal[hi_offset], f->XList[i]);

		val      = yList[i];
		highroom = f->Range - predicted;
		lowroom  = predicted;

		if (highroom < lowroom) {
			room = highroom * 2;
		} else {
			room = lowroom * 2;
		}

		if (val) {
			Step2[lo_offset] = true;
			Step2[hi_offset] = true;
			Step2[i] = true;

			if (val >= room) {
				if (highroom > lowroom) {
					YFinal[i] = val - lowroom + predicted;
				} else {
					YFinal[i] = predicted - val + highroom - 1;
				}
			} else {
				if (val & 1) {
					YFinal[i] = predicted - (val + 1) / 2;
				} else {
					YFinal[i] = predicted + val / 2;
				}
			}
		} else {
			Step2[i]  = false;
			YFinal[i] = predicted;
		}
	}

	/* curve synthesis */ 
	lx = 0; ly = YFinal[f->ListOrder[0]] * f->Multiplier; 
	hx = 0; hy = ly;

	for (rawI = 1; rawI < f->Values; rawI++) {
		i = f->ListOrder[rawI];
		if (!Step2[i]) continue;

		hx = f->XList[i]; hy = YFinal[i] * f->Multiplier;
		if (lx < hx) {
			Floor_RenderLine(lx, ly, min(hx, ctx->DataSize), hy, data);
		}
		lx = hx; ly = hy;
	}

	/* fill remainder of floor with a flat line */
	/* TODO: Is this right? should hy be 0, if Step2 is false for all */
	if (hx >= ctx->DataSize) return;
	lx = hx; hx = ctx->DataSize;

	value = floor1_inverse_dB_table[hy];
	for (; lx < hx; lx++) { data[lx] *= value; }
}


/*########################################################################################################################*
*----------------------------------------------------Vorbis residues------------------------------------------------------*
*#########################################################################################################################*/
#define RESIDUE_MAX_CLASSIFICATIONS 65
struct Residue {
	uint8_t  Type, Classifications, Classbook;
	uint32_t Begin, End, PartitionSize;
	uint8_t  Cascade[RESIDUE_MAX_CLASSIFICATIONS];
	int16_t  Books[RESIDUE_MAX_CLASSIFICATIONS][8];
};

static ReturnCode Residue_DecodeSetup(struct VorbisState* ctx, struct Residue* r, int type) {
	int16_t codebook;
	int i, j;

	r->Type  = (uint8_t)type;
	r->Begin = Vorbis_ReadBits(ctx, 24);
	r->End   = Vorbis_ReadBits(ctx, 24);
	r->PartitionSize   = Vorbis_ReadBits(ctx, 24) + 1;
	r->Classifications = Vorbis_ReadBits(ctx, 6)  + 1;
	r->Classbook       = Vorbis_ReadBits(ctx, 8);

	for (i = 0; i < r->Classifications; i++) {
		r->Cascade[i] = Vorbis_ReadBits(ctx, 3);
		if (!Vorbis_ReadBit(ctx)) continue;
		r->Cascade[i] |= Vorbis_ReadBits(ctx, 5) << 3;
	}

	for (i = 0; i < r->Classifications; i++) {
		for (j = 0; j < 8; j++) {
			codebook = -1;

			if (r->Cascade[i] & (1 << j)) {
				codebook = Vorbis_ReadBits(ctx, 8);
			}
			r->Books[i][j] = codebook;
		}
	}
	return 0;
}

static void Residue_DecodeCore(struct VorbisState* ctx, struct Residue* r, uint32_t size, int ch, bool* doNotDecode, float** data) {
	struct Codebook* classbook;
	uint32_t residueBeg, residueEnd;
	uint32_t classwordsPerCodeword;
	uint32_t nToRead, partitionsToRead;
	int pass, i, j, k;

	/* classification variables */
	uint8_t* classifications[VORBIS_MAX_CHANS];
	uint8_t* classifications_raw;
	uint32_t temp;

	/* partition variables */
	struct Codebook* c;
	float* v;
	uint32_t offset;
	uint8_t class;
	int16_t book;

	/* per spec, ensure decoded bounds are actually in size */
	residueBeg = min(r->Begin, size);
	residueEnd = min(r->End,   size);
	classbook = &ctx->Codebooks[r->Classbook];

	classwordsPerCodeword = classbook->Dimensions;
	nToRead = residueEnd - residueBeg;
	partitionsToRead = nToRead / r->PartitionSize;

	/* first half of temp array is used by residue type 2 for storing temp interleaved data */
	classifications_raw = ((uint8_t*)ctx->Temp) + (ctx->DataSize * ctx->Channels * 5);
	for (i = 0; i < ch; i++) {
		/* add a bit of space in case classwordsPerCodeword is > partitionsToRead*/
		classifications[i] = classifications_raw + i * (partitionsToRead + 64);
	}

	if (nToRead == 0) return;
	for (pass = 0; pass < 8; pass++) {
		uint32_t partitionCount = 0;
		while (partitionCount < partitionsToRead) {

			/* read classifications in pass 0 */
			if (pass == 0) {
				for (j = 0; j < ch; j++) {
					if (doNotDecode[j]) continue;

					temp = Codebook_DecodeScalar(ctx, classbook);
					for (i = classwordsPerCodeword - 1; i >= 0; i--) {
						classifications[j][i + partitionCount] = temp % r->Classifications;
						temp /= r->Classifications;
					}
				}
			}

			for (i = 0; i < classwordsPerCodeword && partitionCount < partitionsToRead; i++) {
				for (j = 0; j < ch; j++) {
					if (doNotDecode[j]) continue;

					class = classifications[j][partitionCount];
					book  = r->Books[class][pass];
					if (book < 0) continue;

					offset = residueBeg + partitionCount * r->PartitionSize;
					v = data[j] + offset;
					c = &ctx->Codebooks[book];

					if (r->Type == 0) {
						int step = r->PartitionSize / c->Dimensions;
						for (k = 0; k < step; k++) {
							Codebook_DecodeVectors(ctx, c, v, step); v++;
						}
					} else {
						for (k = 0; k < r->PartitionSize; k += c->Dimensions) {
							Codebook_DecodeVectors(ctx, c, v, 1); v += c->Dimensions;
						}
					}
				}
				partitionCount++;
			}
		}
	}
}

static void Residue_DecodeFrame(struct VorbisState* ctx, struct Residue* r, int ch, bool* doNotDecode, float** data) {
	uint32_t size = ctx->DataSize;
	float* interleaved;
	int i, j;

	if (r->Type == 2) {
		bool decodeAny = false;

		/* type 2 decodes all channel vectors, if at least 1 channel to decode */
		for (i = 0; i < ch; i++) {
			if (!doNotDecode[i]) decodeAny = true;
		}
		if (!decodeAny) return;
		decodeAny = false; /* because DecodeCore expects this to be 'false' for 'do not decode' */

		interleaved = ctx->Temp;
		/* TODO: avoid using ctx->temp and deinterleaving at all */
		/* TODO: avoid setting memory to 0 here */
		Mem_Set(interleaved, 0, ctx->DataSize * ctx->Channels * sizeof(float));
		Residue_DecodeCore(ctx, r, size * ch, 1, &decodeAny, &interleaved);

		/* deinterleave type 2 output */	
		for (i = 0; i < size; i++) {
			for (j = 0; j < ch; j++) {
				data[j][i] = interleaved[i * ch + j];
			}
		}
	} else {
		Residue_DecodeCore(ctx, r, size, ch, doNotDecode, data);
	}
}


/*########################################################################################################################*
*----------------------------------------------------Vorbis mappings------------------------------------------------------*
*#########################################################################################################################*/
#define MAPPING_MAX_COUPLINGS 256
#define MAPPING_MAX_SUBMAPS 15
struct Mapping {
	uint8_t CouplingSteps, Submaps;
	uint8_t Mux[VORBIS_MAX_CHANS];
	uint8_t FloorIdx[MAPPING_MAX_SUBMAPS];
	uint8_t ResidueIdx[MAPPING_MAX_SUBMAPS];
	uint8_t Magnitude[MAPPING_MAX_COUPLINGS];
	uint8_t Angle[MAPPING_MAX_COUPLINGS];
};

static ReturnCode Mapping_DecodeSetup(struct VorbisState* ctx, struct Mapping* m) {
	int i, submaps, reserved;
	int couplingSteps, couplingBits;

	submaps = 1;
	if (Vorbis_ReadBit(ctx)) {
		submaps = Vorbis_ReadBits(ctx, 4) + 1;
	}

	couplingSteps = 0;
	if (Vorbis_ReadBit(ctx)) {
		couplingSteps = Vorbis_ReadBits(ctx, 8) + 1;
		/* TODO: How big can couplingSteps ever really get in practice? */
		couplingBits  = iLog(ctx->Channels - 1);

		for (i = 0; i < couplingSteps; i++) {
			m->Magnitude[i] = Vorbis_ReadBits(ctx, couplingBits);
			m->Angle[i]     = Vorbis_ReadBits(ctx, couplingBits);
			if (m->Magnitude[i] == m->Angle[i]) return VORBIS_ERR_MAPPING_CHANS;
		}
	}

	reserved = Vorbis_ReadBits(ctx, 2);
	if (reserved != 0) return VORBIS_ERR_MAPPING_RESERVED;
	m->Submaps = submaps;
	m->CouplingSteps = couplingSteps;

	if (submaps > 1) {
		for (i = 0; i < ctx->Channels; i++) {
			m->Mux[i] = Vorbis_ReadBits(ctx, 4);
		}
	} else {
		for (i = 0; i < ctx->Channels; i++) {
			m->Mux[i] = 0;
		}
	}

	for (i = 0; i < submaps; i++) {
		Vorbis_ReadBits(ctx, 8); /* time value */
		m->FloorIdx[i]   = Vorbis_ReadBits(ctx, 8);
		m->ResidueIdx[i] = Vorbis_ReadBits(ctx, 8);
	}
	return 0;
}


/*########################################################################################################################*
*------------------------------------------------------imdct impl---------------------------------------------------------*
*#########################################################################################################################*/
#define PI MATH_PI
void imdct_slow(float* in, float* out, int N) {
	double sum;
	int i, k;

	for (i = 0; i < 2 * N; i++) {
		sum = 0;
		for (k = 0; k < N; k++) {
			sum += in[k] * Math_Cos((PI / N) * (i + 0.5 + N * 0.5) * (k + 0.5));
		}
		out[i] = sum;
	}
}

static uint32_t Vorbis_ReverseBits(uint32_t v) {
	v = ((v >> 1) & 0x55555555) | ((v & 0x55555555) << 1);
	v = ((v >> 2) & 0x33333333) | ((v & 0x33333333) << 2);
	v = ((v >> 4) & 0x0F0F0F0F) | ((v & 0x0F0F0F0F) << 4);
	v = ((v >> 8) & 0x00FF00FF) | ((v & 0x00FF00FF) << 8);
	v = (v >> 16) | (v << 16);
	return v;
}

void imdct_init(struct imdct_state* state, int n) {
	int k, k2, n4 = n >> 2, n8 = n >> 3, log2_n;
	float *A = state->A, *B = state->B, *C = state->C;
	uint32_t* reversed;

	log2_n   = Math_Log2(n);
	reversed = state->Reversed;
	state->n = n; state->log2_n = log2_n;

	/* setup twiddle factors */
	for (k = 0, k2 = 0; k < n4; k++, k2 += 2) {
		A[k2]   =  (float)Math_Cos((4*k * PI) / n);
		A[k2+1] = -(float)Math_Sin((4*k * PI) / n);
		B[k2]   =  (float)Math_Cos(((k2+1) * PI) / (2*n));
		B[k2+1] =  (float)Math_Sin(((k2+1) * PI) / (2*n));
	}
	for (k = 0, k2 = 0; k < n8; k++, k2 += 2) {
		C[k2]   =  (float)Math_Cos(((k2+1) * (2*PI)) / n);
		C[k2+1] = -(float)Math_Sin(((k2+1) * (2*PI)) / n);
	}

	for (k = 0; k < n8; k++) {
		reversed[k] = Vorbis_ReverseBits(k) >> (32-log2_n+3);
	}
}

void imdct_calc(float* in, float* out, struct imdct_state* state) {
	int k, k2, k4, k8, n = state->n;
	int n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, n3_4 = n - n4;
	int l, log2_n;
	uint32_t* reversed;
	
	/* Optimised algorithm from "The use of multirate filter banks for coding of high quality digital audio" */
	/* Uses a few fixes for the paper noted at http://www.nothings.org/stb_vorbis/mdct_01.txt */
	float *A = state->A, *B = state->B, *C = state->C;

	float u[VORBIS_MAX_BLOCK_SIZE];
	float w[VORBIS_MAX_BLOCK_SIZE];
	float e_1, e_2, f_1, f_2;
	float g_1, g_2, h_1, h_2;
	float x_1, x_2, y_1, y_2;

	/* spectral coefficients, step 1, step 2 */
	for (k = 0, k2 = 0, k4 = 0; k < n8; k++, k2 += 2, k4 += 4) {
		e_1 = -in[k4+3];   e_2 = -in[k4+1];
		g_1 = e_1 * A[n2-1-k2] + e_2 * A[n2-2-k2];
		g_2 = e_1 * A[n2-2-k2] - e_2 * A[n2-1-k2];

		f_1 = in[n2-4-k4]; f_2 = in[n2-2-k4];
		h_2 = f_1 * A[n4-2-k2] - f_2 * A[n4-1-k2];
		h_1 = f_1 * A[n4-1-k2] + f_2 * A[n4-2-k2];

		w[n2+3+k4] = h_2 + g_2;
		w[n2+1+k4] = h_1 + g_1;

		w[k4+3] = (h_2 - g_2) * A[n2-4-k4] - (h_1 - g_1) * A[n2-3-k4];
		w[k4+1] = (h_1 - g_1) * A[n2-4-k4] + (h_2 - g_2) * A[n2-3-k4];
	}

	/* step 3 */
	log2_n = state->log2_n;
	for (l = 0; l <= log2_n - 4; l++) {
		int k0 = n >> (l+2), k1 = 1 << (l+3);
		int r, r4, rMax = n >> (l+4), s2, s2Max = 1 << (l+2);

		for (r = 0, r4 = 0; r < rMax; r++, r4 += 4) {
			for (s2 = 0; s2 < s2Max; s2 += 2) {
				e_1 = w[n-1-k0*s2-r4];     e_2 = w[n-3-k0*s2-r4];
				f_1 = w[n-1-k0*(s2+1)-r4]; f_2 = w[n-3-k0*(s2+1)-r4];

				u[n-1-k0*s2-r4] = e_1 + f_1;
				u[n-3-k0*s2-r4] = e_2 + f_2;

				u[n-1-k0*(s2+1)-r4] = (e_1 - f_1) * A[r*k1] - (e_2 - f_2) * A[r*k1+1];
				u[n-3-k0*(s2+1)-r4] = (e_2 - f_2) * A[r*k1] + (e_1 - f_1) * A[r*k1+1];
			}
		}

		/* TODO: eliminate this, do w/u in-place */
		/* TODO: dynamically allocate mem for imdct */
		if (l+1 <= log2_n - 4) {
			Mem_Copy(w, u, sizeof(u));
		}
	}

	/* step 4, step 5, step 6, step 7, step 8, output */
	reversed = state->Reversed;
	for (k = 0, k2 = 0, k8 = 0; k < n8; k++, k2 += 2, k8 += 8) {
		uint32_t j = reversed[k], j8 = j << 3;
		e_1 = u[n-j8-1]; e_2 = u[n-j8-3];
		f_1 = u[j8+3];   f_2 = u[j8+1];

		g_1 =  e_1 + f_1 + C[k2+1] * (e_1 - f_1) + C[k2] * (e_2 + f_2);
		h_1 =  e_1 + f_1 - C[k2+1] * (e_1 - f_1) - C[k2] * (e_2 + f_2);
		g_2 =  e_2 - f_2 + C[k2+1] * (e_2 + f_2) - C[k2] * (e_1 - f_1);
		h_2 = -e_2 + f_2 + C[k2+1] * (e_2 + f_2) - C[k2] * (e_1 - f_1);

		x_1 = -0.5f * (g_1 * B[k2]   + g_2 * B[k2+1]);
		x_2 = -0.5f * (g_1 * B[k2+1] - g_2 * B[k2]);
		out[n4-1-k]   = -x_2;
		out[n4+k]     =  x_2;
		out[n3_4-1-k] =  x_1;
		out[n3_4+k]   =  x_1;

		y_1 = -0.5f * (h_1 * B[n2-2-k2] + h_2 * B[n2-1-k2]);
		y_2 = -0.5f * (h_1 * B[n2-1-k2] - h_2 * B[n2-2-k2]);
		out[k]      = -y_2;
		out[n2-1-k] =  y_2;
		out[n2+k]   =  y_1;
		out[n-1-k]  =  y_1;
	}
}


/*########################################################################################################################*
*-----------------------------------------------------Vorbis setup--------------------------------------------------------*
*#########################################################################################################################*/
struct Mode { uint8_t BlockSizeFlag, MappingIdx; };
static ReturnCode Mode_DecodeSetup(struct VorbisState* ctx, struct Mode* m) {
	int windowType, transformType;
	m->BlockSizeFlag = Vorbis_ReadBit(ctx);

	windowType    = Vorbis_ReadBits(ctx, 16);
	if (windowType != 0)    return VORBIS_ERR_MODE_WINDOW;
	transformType = Vorbis_ReadBits(ctx, 16);
	if (transformType != 0) return VORBIS_ERR_MODE_TRANSFORM;

	m->MappingIdx = Vorbis_ReadBits(ctx, 8);
	return 0;
}

static void Vorbis_CalcWindow(struct VorbisWindow* window, int blockSize) {
	int i, n = blockSize / 2;
	float *cur_window, *prev_window;
	double inner;

	window->Cur = window->Prev + n;
	cur_window  = window->Cur;
	prev_window = window->Prev;

	for (i = 0; i < n; i++) {
		inner          = Math_Sin((i + 0.5) / n * (PI/2));
		cur_window[i]  = Math_Sin((PI/2) * inner * inner);
	}
	for (i = 0; i < n; i++) {
		inner          = Math_Sin((i + 0.5) / n * (PI/2) + (PI/2));
		prev_window[i] = Math_Sin((PI/2) * inner * inner);
	}
}

void Vorbis_Free(struct VorbisState* ctx) {
	int i;
	for (i = 0; i < ctx->NumCodebooks; i++) {
		Codebook_Free(&ctx->Codebooks[i]);
	}

	Mem_Free(ctx->Codebooks);
	Mem_Free(ctx->Floors);
	Mem_Free(ctx->Residues);
	Mem_Free(ctx->Mappings);
	Mem_Free(ctx->Modes);
	Mem_Free(ctx->WindowRaw);
	Mem_Free(ctx->Temp);
}

static bool Vorbis_ValidBlockSize(uint32_t size) {
	return size >= 64 && size <= VORBIS_MAX_BLOCK_SIZE && Math_IsPowOf2(size);
}

static ReturnCode Vorbis_CheckHeader(struct VorbisState* ctx, uint8_t type) {
	uint8_t header[7];
	bool OK;
	ReturnCode res;

	if ((res = Stream_Read(ctx->Source, header, sizeof(header)))) return res;
	if (header[0] != type) return VORBIS_ERR_WRONG_HEADER;

	OK = 
		header[1] == 'v' && header[2] == 'o' && header[3] == 'r' &&
		header[4] == 'b' && header[5] == 'i' && header[6] == 's';
	return OK ? 0 : ReturnCode_InvalidArg;
}

static ReturnCode Vorbis_DecodeIdentifier(struct VorbisState* ctx) {
	uint8_t header[23];
	uint32_t version;
	ReturnCode res;

	if ((res = Stream_Read(ctx->Source, header, sizeof(header)))) return res;
	version  = Stream_GetU32_LE(&header[0]);
	if (version != 0) return VORBIS_ERR_VERSION;

	ctx->Channels   = header[4];
	ctx->SampleRate = Stream_GetU32_LE(&header[5]);
	/* (12) bitrate_maximum, nominal, minimum */
	ctx->BlockSizes[0] = 1 << (header[21] & 0xF);
	ctx->BlockSizes[1] = 1 << (header[21] >>  4);

	if (!Vorbis_ValidBlockSize(ctx->BlockSizes[0])) return VORBIS_ERR_BLOCKSIZE;
	if (!Vorbis_ValidBlockSize(ctx->BlockSizes[1])) return VORBIS_ERR_BLOCKSIZE;
	if (ctx->BlockSizes[0] > ctx->BlockSizes[1])    return VORBIS_ERR_BLOCKSIZE;

	if (ctx->Channels == 0 || ctx->Channels > VORBIS_MAX_CHANS) return VORBIS_ERR_CHANS;
	/* check framing flag */
	return (header[22] & 1) ? 0 : VORBIS_ERR_FRAMING;
}

static ReturnCode Vorbis_DecodeComments(struct VorbisState* ctx) {
	uint32_t i, len, comments;
	uint8_t flag;
	ReturnCode res;
	struct Stream* stream = ctx->Source;

	/* vendor name, followed by comments */
	if ((res = Stream_ReadU32_LE(stream, &len)))      return res;
	if ((res = stream->Skip(stream, len)))            return res;
	if ((res = Stream_ReadU32_LE(stream, &comments))) return res;

	for (i = 0; i < comments; i++) {
		/* comments such as artist, year, etc */
		if ((res = Stream_ReadU32_LE(stream, &len))) return res;
		if ((res = stream->Skip(stream, len)))       return res;
	}

	/* check framing flag */
	if ((res = stream->ReadU8(stream, &flag))) return res;
	return (flag & 1) ? 0 : VORBIS_ERR_FRAMING;
}

static ReturnCode Vorbis_DecodeSetup(struct VorbisState* ctx) {
	uint32_t framing, alignSkip;
	int i, count;
	ReturnCode res;

	count = Vorbis_ReadBits(ctx, 8) + 1;
	ctx->Codebooks = Mem_Alloc(count, sizeof(struct Codebook), "vorbis codebooks");
	for (i = 0; i < count; i++) {
		res = Codebook_DecodeSetup(ctx, &ctx->Codebooks[i]);
		if (res) return res;
	}
	ctx->NumCodebooks = count;

	count = Vorbis_ReadBits(ctx, 6) + 1;
	for (i = 0; i < count; i++) {
		int time = Vorbis_ReadBits(ctx, 16);
		if (time != 0) return VORBIS_ERR_TIME_TYPE;
	}

	count = Vorbis_ReadBits(ctx, 6) + 1;
	ctx->Floors = Mem_Alloc(count, sizeof(struct Floor), "vorbis floors");
	for (i = 0; i < count; i++) {
		int floor = Vorbis_ReadBits(ctx, 16);
		if (floor != 1) return VORBIS_ERR_FLOOR_TYPE;
		res = Floor_DecodeSetup(ctx, &ctx->Floors[i]);
		if (res) return res;
	}

	count = Vorbis_ReadBits(ctx, 6) + 1;
	ctx->Residues = Mem_Alloc(count, sizeof(struct Residue), "vorbis residues");
	for (i = 0; i < count; i++) {
		int residue = Vorbis_ReadBits(ctx, 16);
		if (residue > 2) return VORBIS_ERR_FLOOR_TYPE;
		res = Residue_DecodeSetup(ctx, &ctx->Residues[i], residue);
		if (res) return res;
	}

	count = Vorbis_ReadBits(ctx, 6) + 1;
	ctx->Mappings = Mem_Alloc(count, sizeof(struct Mapping), "vorbis mappings");
	for (i = 0; i < count; i++) {
		int mapping = Vorbis_ReadBits(ctx, 16);
		if (mapping != 0) return VORBIS_ERR_MAPPING_TYPE;
		res = Mapping_DecodeSetup(ctx, &ctx->Mappings[i]);
		if (res) return res;
	}

	count = Vorbis_ReadBits(ctx, 6) + 1;
	ctx->Modes = Mem_Alloc(count, sizeof(struct Mode), "vorbis modes");
	for (i = 0; i < count; i++) {
		res = Mode_DecodeSetup(ctx, &ctx->Modes[i]);
		if (res) return res;
	}
	
	ctx->ModeNumBits = iLog(count - 1); /* ilog([vorbis_mode_count]-1) bits */
	framing = Vorbis_ReadBit(ctx);
	Vorbis_AlignBits(ctx);
	/* check framing flag */
	return (framing & 1) ? 0 : VORBIS_ERR_FRAMING;
}

ReturnCode Vorbis_DecodeHeaders(struct VorbisState* ctx) {
	uint32_t count;
	ReturnCode res;
	
	if ((res = Vorbis_CheckHeader(ctx, 1)))   return res;
	if ((res = Vorbis_DecodeIdentifier(ctx))) return res;
	if ((res = Vorbis_CheckHeader(ctx, 3)))   return res;
	if ((res = Vorbis_DecodeComments(ctx)))   return res;
	if ((res = Vorbis_CheckHeader(ctx, 5)))   return res;
	if ((res = Vorbis_DecodeSetup(ctx)))      return res;

	/* window calculations can be pre-computed here */
	count = ctx->BlockSizes[0] + ctx->BlockSizes[1];
	ctx->WindowRaw = Mem_Alloc(count, sizeof(float), "Vorbis windows");
	ctx->Windows[0].Prev = ctx->WindowRaw;
	ctx->Windows[1].Prev = ctx->WindowRaw + ctx->BlockSizes[0];

	Vorbis_CalcWindow(&ctx->Windows[0], ctx->BlockSizes[0]);
	Vorbis_CalcWindow(&ctx->Windows[1], ctx->BlockSizes[1]);

	count = ctx->Channels * ctx->BlockSizes[1];
	ctx->Temp      = Mem_AllocCleared(count * 3, sizeof(float), "Vorbis values");
	ctx->Values[0] = ctx->Temp + count;
	ctx->Values[1] = ctx->Temp + count * 2;

	imdct_init(&ctx->imdct[0], ctx->BlockSizes[0]);
	imdct_init(&ctx->imdct[1], ctx->BlockSizes[1]);
	return 0;
}


/*########################################################################################################################*
*-----------------------------------------------------Vorbis frame--------------------------------------------------------*
*#########################################################################################################################*/
ReturnCode Vorbis_DecodeFrame(struct VorbisState* ctx) {
	/* frame header */
	uint32_t packetType;
	struct Mapping* mapping;
	struct Mode* mode;
	int modeIdx;

	/* floor/residue */
	bool hasFloor[VORBIS_MAX_CHANS];
	bool hasResidue[VORBIS_MAX_CHANS];
	bool doNotDecode[VORBIS_MAX_CHANS];
	float* data[VORBIS_MAX_CHANS];
	int submap, floorIdx;
	int ch, residueIdx;

	/* inverse coupling */
	int magChannel, angChannel;
	float* magValues, m;
	float* angValues, a;

	/* misc variables */
	float* tmp;
	uint32_t alignSkip;
	int i, j; 
	ReturnCode res;
	
	res = Vorbis_TryReadBits(ctx, 1, &packetType);
	if (res) return res;
	if (packetType) return VORBIS_ERR_FRAME_TYPE;

	modeIdx = Vorbis_ReadBits(ctx, ctx->ModeNumBits);
	mode    = &ctx->Modes[modeIdx];
	mapping = &ctx->Mappings[mode->MappingIdx];

	/* decode window shape */
	ctx->CurBlockSize = ctx->BlockSizes[mode->BlockSizeFlag];
	ctx->DataSize     = ctx->CurBlockSize / 2;
	/* long window lapping flags - we don't care about them though */
	if (mode->BlockSizeFlag) { Vorbis_ReadBits(ctx, 2); } /* TODO: do we just SkipBits here */

	/* swap prev and cur outputs around */
	tmp = ctx->Values[1]; ctx->Values[1] = ctx->Values[0]; ctx->Values[0] = tmp;
	Mem_Set(ctx->Values[0], 0, ctx->Channels * ctx->CurBlockSize);

	for (i = 0; i < ctx->Channels; i++) {
		ctx->CurOutput[i]  = ctx->Values[0] + i * ctx->CurBlockSize;
		ctx->PrevOutput[i] = ctx->Values[1] + i * ctx->PrevBlockSize;
	}

	/* decode floor */
	for (i = 0; i < ctx->Channels; i++) {
		submap   = mapping->Mux[i];
		floorIdx = mapping->FloorIdx[submap];
		hasFloor[i]   = Floor_DecodeFrame(ctx, &ctx->Floors[floorIdx], i);
		hasResidue[i] = hasFloor[i];
	}

	/* non-zero vector propogate */
	for (i = 0; i < mapping->CouplingSteps; i++) {
		magChannel = mapping->Magnitude[i];
		angChannel = mapping->Angle[i];

		if (hasResidue[magChannel] || hasResidue[angChannel]) {
			hasResidue[magChannel] = true; hasResidue[angChannel] = true;
		}
	}

	/* decode residue */
	for (i = 0; i < mapping->Submaps; i++) {
		ch = 0;
		/* map residue data to actual channel data */
		for (j = 0; j < ctx->Channels; j++) {
			if (mapping->Mux[j] != i) continue;

			doNotDecode[ch] = !hasResidue[j];
			data[ch] = ctx->CurOutput[j];
			ch++;
		}

		residueIdx = mapping->FloorIdx[i];
		Residue_DecodeFrame(ctx, &ctx->Residues[residueIdx], ch, doNotDecode, data);
	}

	/* inverse coupling */
	for (i = mapping->CouplingSteps - 1; i >= 0; i--) {
		magValues = ctx->CurOutput[mapping->Magnitude[i]];
		angValues = ctx->CurOutput[mapping->Angle[i]];

		for (j = 0; j < ctx->DataSize; j++) {
			m = magValues[j]; a = angValues[j];

			if (m > 0.0f) {
				if (a > 0.0f) { angValues[j] = m - a; }
				else {
					angValues[j] = m;
					magValues[j] = m + a;
				}
			} else {
				if (a > 0.0f) { angValues[j] = m + a; }
				else {
					angValues[j] = m;
					magValues[j] = m - a;
				}
			}
		}
	}

	/* compute dot product of floor and residue, producing audio spectrum vector */
	for (i = 0; i < ctx->Channels; i++) {
		if (!hasFloor[i]) continue;

		submap   = mapping->Mux[i];
		floorIdx = mapping->FloorIdx[submap];
		Floor_Synthesis(ctx, &ctx->Floors[floorIdx], i);
	}

	/* inverse monolithic transform of audio spectrum vector */
	for (i = 0; i < ctx->Channels; i++) {
		tmp = ctx->CurOutput[i];

		if (!hasFloor[i]) {
			/* TODO: Do we actually need to zero data here (residue type 2 maybe) */
			Mem_Set(tmp, 0, ctx->CurBlockSize * sizeof(float));
		} else {
			imdct_calc(tmp, tmp, &ctx->imdct[mode->BlockSizeFlag]);
			/* defer windowing until output */
		}
	}

	/* discard remaining bits at end of packet */
	Vorbis_AlignBits(ctx);
	return 0;
}

int Vorbis_OutputFrame(struct VorbisState* ctx, int16_t* data) {
	struct VorbisWindow window;
	float* prev[VORBIS_MAX_CHANS];
	float*  cur[VORBIS_MAX_CHANS];

	int curQrtr, prevQrtr, overlapQtr;
	int curOffset, prevOffset, overlapSize;
	float sample;
	int i, ch;

	/* first frame decoded has no data */
	if (ctx->PrevBlockSize == 0) {
		ctx->PrevBlockSize = ctx->CurBlockSize;
		return 0;
	}

	/* data returned is from centre of previous block to centre of current block */
	/* data is aligned, such that 3/4 of prev block is aligned to 1/4 of cur block */
	curQrtr   = ctx->CurBlockSize  / 4;
	prevQrtr  = ctx->PrevBlockSize / 4;
	overlapQtr = min(curQrtr, prevQrtr);
	
	/* So for example, consider a short block overlapping with a long block
	   a) we need to chop off 'prev' before its halfway point
	   b) then need to chop off the 'cur' before the halfway point of 'prev'
	             |-   ********|*****                        |-   ********|
	            -| - *        |     ***                     | - *        |
	           - |  #         |        ***          ===>    |  #         |
	          -  | * -        |           ***               | * -        |
	   ******-***|*   -       |              ***            |*   -       |
	*/	
	curOffset  = curQrtr  - overlapQtr;
	prevOffset = prevQrtr - overlapQtr;

	for (i = 0; i < ctx->Channels; i++) {
		prev[i] = ctx->PrevOutput[i] + (prevQrtr * 2);
		cur[i]  = ctx->CurOutput[i];
	}

	/* for long prev and short cur block, there will be non-overlapped data before */
	for (i = 0; i < prevOffset; i++) {
		for (ch = 0; ch < ctx->Channels; ch++) {
			sample = prev[ch][i];
			Math_Clamp(sample, -1.0f, 1.0f);
			*data++ = (int16_t)(sample * 32767);
		}
	}

	/* adjust pointers to start at 0 for overlapping */
	for (i = 0; i < ctx->Channels; i++) {
		prev[i] += prevOffset; cur[i] += curOffset;
	}

	overlapSize = overlapQtr * 2;
	window = ctx->Windows[(overlapQtr * 4) == ctx->BlockSizes[1]];

	/* overlap and add data */
	/* also perform windowing here */
	for (i = 0; i < overlapSize; i++) {
		for (ch = 0; ch < ctx->Channels; ch++) {
			sample = prev[ch][i] * window.Prev[i] + cur[ch][i] * window.Cur[i];
			Math_Clamp(sample, -1.0f, 1.0f);
			*data++ = (int16_t)(sample * 32767);
		}
	}

	/* for long cur and short prev block, there will be non-overlapped data after */
	for (i = 0; i < ctx->Channels; i++) { cur[i] += overlapSize; }
	for (i = 0; i < curOffset; i++) {
		for (ch = 0; ch < ctx->Channels; ch++) {
			sample = cur[ch][i];
			Math_Clamp(sample, -1.0f, 1.0f);
			*data++ = (int16_t)(sample * 32767);
		}
	}

	ctx->PrevBlockSize = ctx->CurBlockSize;
	return (prevQrtr + curQrtr) * ctx->Channels;
}

static float floor1_inverse_dB_table[256] = {
	1.0649863e-07f, 1.1341951e-07f, 1.2079015e-07f, 1.2863978e-07f,
	1.3699951e-07f, 1.4590251e-07f, 1.5538408e-07f, 1.6548181e-07f,
	1.7623575e-07f, 1.8768855e-07f, 1.9988561e-07f, 2.1287530e-07f,
	2.2670913e-07f, 2.4144197e-07f, 2.5713223e-07f, 2.7384213e-07f,
	2.9163793e-07f, 3.1059021e-07f, 3.3077411e-07f, 3.5226968e-07f,
	3.7516214e-07f, 3.9954229e-07f, 4.2550680e-07f, 4.5315863e-07f,
	4.8260743e-07f, 5.1396998e-07f, 5.4737065e-07f, 5.8294187e-07f,
	6.2082472e-07f, 6.6116941e-07f, 7.0413592e-07f, 7.4989464e-07f,
	7.9862701e-07f, 8.5052630e-07f, 9.0579828e-07f, 9.6466216e-07f,
	1.0273513e-06f, 1.0941144e-06f, 1.1652161e-06f, 1.2409384e-06f,
	1.3215816e-06f, 1.4074654e-06f, 1.4989305e-06f, 1.5963394e-06f,
	1.7000785e-06f, 1.8105592e-06f, 1.9282195e-06f, 2.0535261e-06f,
	2.1869758e-06f, 2.3290978e-06f, 2.4804557e-06f, 2.6416497e-06f,
	2.8133190e-06f, 2.9961443e-06f, 3.1908506e-06f, 3.3982101e-06f,
	3.6190449e-06f, 3.8542308e-06f, 4.1047004e-06f, 4.3714470e-06f,
	4.6555282e-06f, 4.9580707e-06f, 5.2802740e-06f, 5.6234160e-06f,
	5.9888572e-06f, 6.3780469e-06f, 6.7925283e-06f, 7.2339451e-06f,
	7.7040476e-06f, 8.2047000e-06f, 8.7378876e-06f, 9.3057248e-06f,
	9.9104632e-06f, 1.0554501e-05f, 1.1240392e-05f, 1.1970856e-05f,
	1.2748789e-05f, 1.3577278e-05f, 1.4459606e-05f, 1.5399272e-05f,
	1.6400004e-05f, 1.7465768e-05f, 1.8600792e-05f, 1.9809576e-05f,
	2.1096914e-05f, 2.2467911e-05f, 2.3928002e-05f, 2.5482978e-05f,
	2.7139006e-05f, 2.8902651e-05f, 3.0780908e-05f, 3.2781225e-05f,
	3.4911534e-05f, 3.7180282e-05f, 3.9596466e-05f, 4.2169667e-05f,
	4.4910090e-05f, 4.7828601e-05f, 5.0936773e-05f, 5.4246931e-05f,
	5.7772202e-05f, 6.1526565e-05f, 6.5524908e-05f, 6.9783085e-05f,
	7.4317983e-05f, 7.9147585e-05f, 8.4291040e-05f, 8.9768747e-05f,
	9.5602426e-05f, 0.00010181521f, 0.00010843174f, 0.00011547824f,
	0.00012298267f, 0.00013097477f, 0.00013948625f, 0.00014855085f,
	0.00015820453f, 0.00016848555f, 0.00017943469f, 0.00019109536f,
	0.00020351382f, 0.00021673929f, 0.00023082423f, 0.00024582449f,
	0.00026179955f, 0.00027881276f, 0.00029693158f, 0.00031622787f,
	0.00033677814f, 0.00035866388f, 0.00038197188f, 0.00040679456f,
	0.00043323036f, 0.00046138411f, 0.00049136745f, 0.00052329927f,
	0.00055730621f, 0.00059352311f, 0.00063209358f, 0.00067317058f,
	0.00071691700f, 0.00076350630f, 0.00081312324f, 0.00086596457f,
	0.00092223983f, 0.00098217216f, 0.0010459992f,  0.0011139742f,
	0.0011863665f,  0.0012634633f,  0.0013455702f,  0.0014330129f,
	0.0015261382f,  0.0016253153f,  0.0017309374f,  0.0018434235f,
	0.0019632195f,  0.0020908006f,  0.0022266726f,  0.0023713743f,
	0.0025254795f,  0.0026895994f,  0.0028643847f,  0.0030505286f,
	0.0032487691f,  0.0034598925f,  0.0036847358f,  0.0039241906f,
	0.0041792066f,  0.0044507950f,  0.0047400328f,  0.0050480668f,
	0.0053761186f,  0.0057254891f,  0.0060975636f,  0.0064938176f,
	0.0069158225f,  0.0073652516f,  0.0078438871f,  0.0083536271f,
	0.0088964928f,  0.009474637f,   0.010090352f,   0.010746080f,
	0.011444421f,   0.012188144f,   0.012980198f,   0.013823725f,
	0.014722068f,   0.015678791f,   0.016697687f,   0.017782797f,
	0.018938423f,   0.020169149f,   0.021479854f,   0.022875735f,
	0.024362330f,   0.025945531f,   0.027631618f,   0.029427276f,
	0.031339626f,   0.033376252f,   0.035545228f,   0.037855157f,
	0.040315199f,   0.042935108f,   0.045725273f,   0.048696758f,
	0.051861348f,   0.055231591f,   0.058820850f,   0.062643361f,
	0.066714279f,   0.071049749f,   0.075666962f,   0.080584227f,
	0.085821044f,   0.091398179f,   0.097337747f,   0.10366330f,
	0.11039993f,    0.11757434f,    0.12521498f,    0.13335215f,
	0.14201813f,    0.15124727f,    0.16107617f,    0.17154380f,
	0.18269168f,    0.19456402f,    0.20720788f,    0.22067342f,
	0.23501402f,    0.25028656f,    0.26655159f,    0.28387361f,
	0.30232132f,    0.32196786f,    0.34289114f,    0.36517414f,
	0.38890521f,    0.41417847f,    0.44109412f,    0.46975890f,
	0.50028648f,    0.53279791f,    0.56742212f,    0.60429640f,
	0.64356699f,    0.68538959f,    0.72993007f,    0.77736504f,
	0.82788260f,    0.88168307f,    0.9389798f,     1.00000000f,
};
