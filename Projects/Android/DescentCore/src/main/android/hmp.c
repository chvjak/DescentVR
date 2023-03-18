#include <linux/limits.h>
#include <jni.h>
#include <stdlib.h>
#include "hmp.h"
#include "cfile.h"

extern JavaVM *jvm;
extern jobject Activity;

extern void hmp2mid(hmp_file *hmp, unsigned char **midbuf, unsigned int *midlen);

void hmp_init() {

}

char *appClass = "com/chvjak/descentvr/MainActivity";
void hmp_stop(hmp_file *hmp) {
}

void hmp_setvolume(hmp_file *hmp, int volume) {
}

void StartMusic(const char* MIDIFILE, int FILELEN);
int hmp_play(hmp_file *hmp, bool loop) {
	const char *midbuf;
	const char path[PATH_MAX];
	unsigned int midlen;
	FILE *file;
	JNIEnv *env;
	jclass clazz;
	jmethodID method;
	jstring jpath;

	hmp2mid(hmp, &midbuf, &midlen);
	StartMusic(midbuf, midlen);

	return 0;
}
