//
// Created by devin on 4/19/16.
//
#include <jni.h>
#include "mouse.h"

extern void mouse_handler(short x, short y, bool down);

void Java_tuchsen_descent_DescentView_mouseHandler(JNIEnv *env, jclass thiz, jshort x,
												   jshort y, jboolean down) {
	mouse_handler(x, y, down);
}

void Java_tuchsen_descent_DescentView_mouseSetPos(JNIEnv *env, jclass thiz, jshort x, jshort y) {
	mouse_set_pos(x, y);
}
