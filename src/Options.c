#include "Options.h"
#include "ExtMath.h"
#include "Funcs.h"
#include "Platform.h"
#include "Stream.h"
#include "Chat.h"
#include "Errors.h"

const char* FpsLimit_Names[FPS_LIMIT_COUNT] = {
	"LimitVSync", "Limit30FPS", "Limit60FPS", "Limit120FPS", "LimitNone",
};
static StringsBuffer Options_Changed;

bool Options_HasAnyChanged(void) { return Options_Changed.Count > 0;  }

void Options_Free(void) {
	StringsBuffer_Clear(&Options_Keys);
	StringsBuffer_Clear(&Options_Values);
	StringsBuffer_Clear(&Options_Changed);
}

CC_NOINLINE static int Options_CaselessIndexOf(StringsBuffer* buffer, const String* str) {
	String entry;
	int i;

	for (i = 0; i < buffer->Count; i++) {
		entry = StringsBuffer_UNSAFE_Get(buffer, i);
		if (String_CaselessEquals(&entry, str)) return i;
	}
	return -1;
}

bool Options_HasChanged(const String* key) {
	return Options_CaselessIndexOf(&Options_Changed, key) >= 0;
}
static int Options_Find(const String* key) {
	return Options_CaselessIndexOf(&Options_Keys, key);
}

static bool Options_TryGetValue(const char* keyRaw, String* value) {
	int i, idx;
	String key = String_FromReadonly(keyRaw);
	*value     = String_Empty;

	i = Options_Find(&key);
	if (i >= 0) {
		*value = StringsBuffer_UNSAFE_Get(&Options_Values, i);
		return true; 
	}

	/* Fallback to without '-' (e.g. "hacks-fly" to "fly") */
	/* Needed for some very old options.txt files */
	idx = String_IndexOf(&key, '-', 0);
	if (idx == -1) return false;
	key = String_UNSAFE_SubstringAt(&key, idx + 1);

	i = Options_Find(&key);
	if (i >= 0) {
		*value = StringsBuffer_UNSAFE_Get(&Options_Values, i);
		return true;
	}
	return false;
}

void Options_Get(const char* key, String* value, const char* defValue) {
	String str;
	Options_TryGetValue(key, &str);
	value->length = 0;

	if (str.length) {
		String_AppendString(value, &str);
	} else {
		String_AppendConst(value, defValue);
	}
}

int Options_GetInt(const char* key, int min, int max, int defValue) {
	String str;
	int value;
	if (!Options_TryGetValue(key, &str))    return defValue;
	if (!Convert_TryParseInt(&str, &value)) return defValue;

	Math_Clamp(value, min, max);
	return value;
}

bool Options_GetBool(const char* key, bool defValue) {
	String str;
	bool value;
	if (!Options_TryGetValue(key, &str))     return defValue;
	if (!Convert_TryParseBool(&str, &value)) return defValue;

	return value;
}

float Options_GetFloat(const char* key, float min, float max, float defValue) {
	String str;
	float value;
	if (!Options_TryGetValue(key, &str))       return defValue;
	if (!Convert_TryParseFloat(&str, &value)) return defValue;

	Math_Clamp(value, min, max);
	return value;
}

int Options_GetEnum(const char* key, int defValue, const char** names, int namesCount) {
	String str;
	if (!Options_TryGetValue(key, &str)) return defValue;
	return Utils_ParseEnum(&str, defValue, names, namesCount);
}

static void Options_Remove(int i) {
	StringsBuffer_Remove(&Options_Keys, i);
	StringsBuffer_Remove(&Options_Values, i);
}

static int Options_Insert(const String* key, const String* value) {
	int i = Options_Find(key);
	if (i >= 0) Options_Remove(i);

	StringsBuffer_Add(&Options_Keys, key);
	StringsBuffer_Add(&Options_Values, value);
	return Options_Keys.Count;
}

void Options_SetBool(const char* keyRaw, bool value) {
	static String str_true  = String_FromConst("True");
	static String str_false = String_FromConst("False");
	Options_Set(keyRaw, value ? &str_true : &str_false);
}

void Options_SetInt(const char* keyRaw, int value) {
	String str; char strBuffer[STRING_INT_CHARS];
	String_InitArray(str, strBuffer);
	String_AppendInt(&str, value);
	Options_Set(keyRaw, &str);
}

void Options_Set(const char* keyRaw, const String* value) {
	String key = String_FromReadonly(keyRaw);
	Options_SetString(&key, value);
}

void Options_SetString(const String* key, const String* value) {
	int i;
	if (!value || !value->length) {
		i = Options_Find(key);
		if (i >= 0) Options_Remove(i);
	} else {
		i = Options_Insert(key, value);
	}

	if (i == -1 || Options_HasChanged(key)) return;
	StringsBuffer_Add(&Options_Changed, key);
}

void Options_Load(void) {	
	static String path = String_FromConst("options.txt");
	String line; char lineBuffer[768];

	String key, value;
	uint8_t buffer[2048];
	struct Stream stream, buffered;
	int i;
	ReturnCode res;	

	res = Stream_OpenFile(&stream, &path);
	if (res == ReturnCode_FileNotFound) return;
	if (res) { Chat_LogError2(res, "opening", &path); return; }

	/* Remove all the unchanged options */
	for (i = Options_Keys.Count - 1; i >= 0; i--) {
		key = StringsBuffer_UNSAFE_Get(&Options_Keys, i);
		if (Options_HasChanged(&key)) continue;
		Options_Remove(i);
	}

	/* ReadLine reads single byte at a time */
	Stream_ReadonlyBuffered(&buffered, &stream, buffer, sizeof(buffer));
	String_InitArray(line, lineBuffer);

	for (;;) {
		res = Stream_ReadLine(&buffered, &line);
		if (res == ERR_END_OF_STREAM) break;
		if (res) { Chat_LogError2(res, "reading from", &path); break; }

		if (!line.length || line.buffer[0] == '#') continue;
		if (!String_UNSAFE_Separate(&line, '=', &key, &value)) continue;

		if (!Options_HasChanged(&key)) {
			Options_Insert(&key, &value);
		}
	}

	res = stream.Close(&stream);
	if (res) { Chat_LogError2(res, "closing", &path); return; }
}

void Options_Save(void) {	
	static String path = String_FromConst("options.txt");
	String line; char lineBuffer[768];

	String key, value;
	struct Stream stream;
	int i;
	ReturnCode res;

	res = Stream_CreateFile(&stream, &path);
	if (res) { Chat_LogError2(res, "creating", &path); return; }
	String_InitArray(line, lineBuffer);

	for (i = 0; i < Options_Keys.Count; i++) {
		key   = StringsBuffer_UNSAFE_Get(&Options_Keys,   i);
		value = StringsBuffer_UNSAFE_Get(&Options_Values, i);
		String_Format2(&line, "%s=%s", &key, &value);

		res = Stream_WriteLine(&stream, &line);
		if (res) { Chat_LogError2(res, "writing to", &path); break; }
		line.length = 0;
	}

	StringsBuffer_Clear(&Options_Changed);
	res = stream.Close(&stream);
	if (res) { Chat_LogError2(res, "closing", &path); return; }
}
