/*
Copyright (c) Nicholas Wilson
Some rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <assert.h>
#include <jni.h>

static jboolean error(const char* fn, int error) {
  fprintf(stderr, "%s error: %d\n", fn, error);
  return JNI_FALSE;
}
static jboolean error(const char* fn, const char* message) {
  fprintf(stderr, "%s error: %s\n", fn, message);
  return JNI_FALSE;
}

namespace {
  struct JniObject {
    JniObject(JNIEnv* env_, jobject obj_) : env(env_), obj(obj_) {}
    ~JniObject() { env->DeleteLocalRef(obj); }
    JNIEnv* env;
    jobject obj;
  };
  struct JniLongFieldSetter {
    JniLongFieldSetter(JNIEnv* env_, jobject obj_, jfieldID fieldID_,
      jlong value)
      : env(env_), obj(obj_), fieldID(fieldID_),
      oldValue(env->GetLongField(obj, fieldID))
    {
      env->SetLongField(obj, fieldID, value);
    }
    ~JniLongFieldSetter() { env->SetLongField(obj, fieldID, oldValue); }
    JNIEnv* env;
    jobject obj;
    jfieldID fieldID;
    jlong oldValue;
  };
  struct JniMonitor {
    JniMonitor(JNIEnv* env_, jobject obj_, bool enter_)
      : env(env_), obj(obj_), enter(enter_)
    {
      toggleMonitor(enter);
    }
    ~JniMonitor() { toggleMonitor(!enter); }
    void toggleMonitor(bool enter) {
      if (enter) {
        if (env->MonitorEnter(obj)) error("MonitorEnter", "unknown");
      }
      else {
        assert(!env->MonitorExit(obj));
      }
    }
    JNIEnv* env;
    jobject obj;
    bool enter;
  };
}

#ifdef WIN32
#include <Windows.h>
namespace {
  struct Handle {
    Handle(HANDLE h_) : h(h_) {}
    ~Handle() { if (h) CloseHandle(h); }
    HANDLE h;
  };
}
#else
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
namespace {
  struct Fd {
    Fd(int fd_) : fd(fd_) {}
    ~Fd() { close(fd); }
    int fd;
  };
}
#endif

static jfieldID eventHandleID = 0;
static bool loadEventHandleID(JNIEnv* env, jobject nativeTask) {
  if (!eventHandleID) {
    JniObject clazz(env, env->GetObjectClass(nativeTask));
    eventHandleID = env->GetFieldID((jclass)clazz.obj, "eventHandle", "J");
  }
  return eventHandleID != 0;
}

extern "C" JNIEXPORT void JNICALL
Java_NativeTask_wakeupTask0(JNIEnv* env, jobject this_)
{
  if (!loadEventHandleID(env, this_)) return;
  JniMonitor enter(env, this_, true);
#ifdef WIN32
  HANDLE eventHandle = reinterpret_cast<HANDLE>(env->GetLongField(this_, eventHandleID));
  if (eventHandle && !SetEvent(eventHandle))
    error("SetEvent", GetLastError());
#else
  int fd = (int)env->GetLongField(this_, eventHandleID) - 1;
  unsigned char dummy[1] = {};
  if (fd >= 0) {
    ssize_t rv;
    while ((rv = write(fd, dummy, sizeof(dummy))) < 0 && errno == EINTR);
    if (rv != sizeof(dummy)) error("write", errno);
  }
#endif
}

extern "C" JNIEXPORT jboolean JNICALL
Java_NativeTask_doTask0(JNIEnv* env, jobject this_)
{
  if (!loadEventHandleID(env, this_)) return JNI_FALSE;
#ifdef WIN32
  Handle eventHandle(CreateEvent(0, TRUE, FALSE, 0));
  if (!eventHandle.h) return error("CreateEvent", GetLastError());
  jlong jniHandle = reinterpret_cast<jlong>(eventHandle.h);
#else
  int fds[2];
  if (pipe(fds)) return error("pipe", errno);
  Fd readEnd(fds[0]), writeEnd(fds[1]);
  jlong jniHandle = writeEnd + 1;
#endif
  JniMonitor enter(env, this_, true);
  JniLongFieldSetter setter(env, this_, eventHandleID, jniHandle);
  if (setter.oldValue != 0) return error("logic", "doTask() is already running");
  JniMonitor exit(env, this_, false);

  for (int i = 0; i < 5; ++i) {
#ifdef WIN32
    switch (WaitForSingleObject(eventHandle.h, 5000)) {
    case WAIT_ABANDONED:
      return error("WaitForSingleObject", "wait abandonned");
    case WAIT_OBJECT_0:
      return JNI_TRUE;
      break;
    case WAIT_TIMEOUT:
      break;
    case WAIT_FAILED:
      return error("WaitForSingleObject", GetLastError());
    }
#else
    fd_set rfds; FD_ZERO(&rfds); FD_SET(readEnd.fd, &rfds);
    struct timeval tv = { 5, 0 };
    switch (select(readEnd.fd + 1, &rfds, 0, 0, &tv)) {
    case 1:
      {
        unsigned char single[1];
        ssize_t rv;
        while ((rv = read(readEnd.fd, single, sizeof(single))) < 0 && errno == EINTR) ;
        assert(rv == sizeof(single));
      }
      return JNI_TRUE;
    case 0:
      break;
    case -1:
      return error("select", errno);
    }
#endif
  }
  return JNI_FALSE;
}
