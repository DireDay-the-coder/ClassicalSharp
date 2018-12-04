#ifndef CC_UTILS_H
#define CC_UTILS_H
#include "String.h"
#include "Bitmap.h"
/* Implements various utility functions.
   Copyright 2014-2017 ClassicalSharp | Licensed under BSD-3
*/

/* Represents a particular instance in time in some timezone. Not necessarily UTC time. */
/* NOTE: This is not an efficiently sized struct. Store DateTime_TotalMs instead for that. */
struct DateTime {
	int Year;  /* Year,   ranges from 0 to 65535 */
	int Month;  /* Month,  ranges from 1 to 12 */
	int Day;    /* Day,    ranges from 1 to 31 */
	int Hour;   /* Hour,   ranges from 0 to 23 */
	int Minute; /* Minute, ranges from 0 to 59 */
	int Second; /* Second, ranges from 0 to 59 */
	int Milli; /* Milliseconds, ranges from 0 to 999 */
};

#define DATETIME_MILLIS_PER_SEC 1000
int DateTime_TotalDays(const struct DateTime* time);
TimeMS DateTime_TotalMs(const struct DateTime* time);
void DateTime_FromTotalMs(struct DateTime* time, TimeMS ms);
void DateTime_HttpDate(TimeMS ms, String* str);

CC_NOINLINE int Utils_ParseEnum(const String* text, int defValue, const char** names, int namesCount);
bool Utils_IsValidInputChar(char c, bool supportsCP437);
bool Utils_IsUrlPrefix(const String* value, int index);

/* Creates the directory if it doesn't exist. (logs failure in chat) */
bool Utils_EnsureDirectory(const char* dirName);
/* Gets the filename portion of a path. (e.g. "dir/file.txt" -> "file.txt") */
void Utils_UNSAFE_GetFilename(STRING_REF String* path);
int Utils_AccumulateWheelDelta(float* accumulator, float delta);
#define Utils_AdjViewDist(value) ((int)(1.4142135f * (value)))

uint8_t Utils_GetSkinType(const Bitmap* bmp);
uint32_t Utils_CRC32(const uint8_t* data, uint32_t length);
extern const uint32_t Utils_Crc32Table[256];
CC_NOINLINE void* Utils_Resize(void* buffer, uint32_t* maxElems, uint32_t elemSize, uint32_t defElems, uint32_t expandElems);
CC_NOINLINE bool Utils_ParseIP(const String* ip, uint8_t* data);

struct EntryList {
	const char* Folder;
	const char* Filename;
	char Separator;
	StringsBuffer Entries;
};
typedef bool (*EntryList_Filter)(const String* entry);

/* Loads the entries from disc. */
CC_NOINLINE void EntryList_Load(struct EntryList* list, EntryList_Filter filter);
/* Saves the entries to disc. */
CC_NOINLINE void EntryList_Save(struct EntryList* list);
/* Removes the entry whose key caselessly equals the given key. */
CC_NOINLINE int  EntryList_Remove(struct EntryList* list, const String* key);
/* Replaces the entry whose key caselessly equals the given key, or adds a new entry. */
CC_NOINLINE void EntryList_Set(struct EntryList* list, const String* key, const String* value);
/* Returns the value of the entry whose key caselessly equals the given key. */
CC_NOINLINE STRING_REF String EntryList_UNSAFE_Get(struct EntryList* list, const String* key);
/* Finds the index of the entry whose key caselessly equals the given key. */
CC_NOINLINE int EntryList_Find(struct EntryList* list, const String* key);
/* Initialises the EntryList and loads the entries from disc. */
void EntryList_Init(struct EntryList* list, const char* folder, const char* file, char separator);
#endif
