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

static bool error(const char* fn, int num) {
  fprintf(stderr, "%s error: %d\n", fn, num);
  return false;
}
static bool error(const char* fn, const char* message) {
  fprintf(stderr, "%s error: %s\n", fn, message);
  return false;
}
static jboolean jError(const char* fn, int num) {
  error(fn, num);
  return JNI_FALSE;
}
static jboolean jError(const char* fn, const char* message) {
  error(fn, message);
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
        oldValue(env->GetLongField(obj, fieldID)),
        error(env->ExceptionCheck() == JNI_TRUE)
    {
     if (error) {
       ::error("GetLongField()", "unknown");
       return;
     }
     env->SetLongField(obj, fieldID, value);
     if ((error = (env->ExceptionCheck() == JNI_TRUE)))
       ::error("SetLongField()", "unknown");
    }
    ~JniLongFieldSetter()
    { if (!error) env->SetLongField(obj, fieldID, oldValue); }
    JNIEnv* env;
    jobject obj;
    jfieldID fieldID;
    jlong oldValue;
    bool error;
  };
  struct JniMonitor {
    JniMonitor(JNIEnv* env_, jobject obj_, bool enter_)
      : env(env_), obj(obj_), enter(enter_), error(!toggleMonitor(enter)) {}
    ~JniMonitor() {
      bool restoredMonitor = error || toggleMonitor(!enter);
      assert(restoredMonitor);
    }
    bool toggleMonitor(bool enter) {
      jint rv = enter ? env->MonitorEnter(obj) : env->MonitorExit(obj);
      return (rv < 0) ? ::error(enter ? "MonitorEnter" : "MonitorExit", "unknown")
                      : true;
    }
    JNIEnv* env;
    jobject obj;
    bool enter, error;
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
    ~Fd() { if (fd >= 0) close(fd); }
    int fd;
  };
}
#endif

static bool loadEventHandleID(JNIEnv* env, jobject nativeTask,
                              jfieldID* eventHandleID) {
  jclass clazz = env->GetObjectClass(nativeTask);
  if (!clazz) return error("GetObjectClass(nativeTask)", "unknown");
  JniObject cleanup(env, clazz);
  jfieldID id = env->GetFieldID(clazz, "eventHandle", "J");
  *eventHandleID = id;
  return id ? true : error("GetFieldID(\"eventHandle\")", "unknown");
}

extern "C" JNIEXPORT void JNICALL
Java_NativeTask_wakeupTask0(JNIEnv* env, jobject this_)
{
  jfieldID eventHandleID = 0;
  if (!loadEventHandleID(env, this_, &eventHandleID)) return;
  JniMonitor enter(env, this_, true);
  if (enter.error) return;
  jlong eventHandle = env->GetLongField(this_, eventHandleID);
  if (env->ExceptionCheck() == JNI_TRUE) {
    error("GetLongField(eventHandleID)", "unknown");
    return;
  }
#ifdef WIN32
  HANDLE h = reinterpret_cast<HANDLE>(eventHandle);
  if (!h) return; // nothing to wake up
  if (!SetEvent(h)) error("SetEvent", GetLastError());
#else
  assert(eventHandle >= 0);
  if (eventHandle == 0) return; // nothing to wake up
  int fd = eventHandle - 1;
  unsigned char dummy[1] = {};
  ssize_t rv;
  while ((rv = write(fd, dummy, sizeof(dummy))) < 0 && errno == EINTR)
    ;
  if (rv != sizeof(dummy)) error("write", errno);
#endif
}

// Returns JNI_TRUE if and only if we returned early due to a wakeup.
extern "C" JNIEXPORT jboolean JNICALL
Java_NativeTask_doTask0(JNIEnv* env, jobject this_)
{
  jfieldID eventHandleID = 0;
  if (!loadEventHandleID(env, this_, &eventHandleID)) return JNI_FALSE;
#ifdef WIN32
  Handle eventHandle(CreateEvent(0, TRUE, FALSE, 0));
  if (!eventHandle.h) return jError("CreateEvent", GetLastError());
  jlong jniHandle = reinterpret_cast<jlong>(eventHandle.h);
#else
  int fds[2];
  if (pipe(fds)) return jError("pipe", errno);
  assert(fds[0] >= 0 && fds[1] >= 0);
  Fd readEnd(fds[0]), writeEnd(fds[1]);
  jlong jniHandle = writeEnd.fd + 1;
#endif
  JniMonitor enter(env, this_, true);
  if (enter.error) return JNI_FALSE;
  JniLongFieldSetter setter(env, this_, eventHandleID, jniHandle);
  if (setter.error) return JNI_FALSE;
  if (setter.oldValue != 0) return jError("logic", "doTask() is already running");
  JniMonitor exit(env, this_, false);
  if (exit.error) return JNI_FALSE;

  for (int i = 0; i < 5; ++i) {
#ifdef WIN32
    switch (WaitForSingleObject(eventHandle.h, 5000)) {
    case WAIT_ABANDONED:
      return jError("WaitForSingleObject", "wait abandonned");
    case WAIT_OBJECT_0:
      return JNI_TRUE;
    case WAIT_TIMEOUT:
      break;
    case WAIT_FAILED:
      return jError("WaitForSingleObject", GetLastError());
    }
#else
    fd_set rfds; FD_ZERO(&rfds); FD_SET(readEnd.fd, &rfds);
    struct timeval tv = { 5, 0 };
    switch (select(readEnd.fd + 1, &rfds, 0, 0, &tv)) {
    case 1:
      {
        unsigned char single[1];
        ssize_t rv;
        while ((rv = read(readEnd.fd, single, sizeof(single))) < 0 && errno == EINTR)
          ;
        assert(rv == sizeof(single));
      }
      return JNI_TRUE;
    case 0:
      break;
    case -1:
      // (If interrupted by a signal, let's say that our long-running native task
      // has finished successfully. We return JNI_FALSE since we weren't interrupted
      // by the Java wakeup() mechanism.)
      if (errno == EINTR)
        return JNI_FALSE;
      return jError("select", errno);
    default:
      assert(false);
      return jError("select", "bad return");
    }
#endif
  }
  return JNI_FALSE;
}

