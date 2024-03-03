#include <jni.h>
#include <unistd.h>
#include <stdlib.h>
#include <android/native_window_jni.h>
#include <sys/timerfd.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>

#define  FRAME_RATE 60

/**
  This is missing in headers for some reason.
 **/
int pipe2(int pipefd[2], int flags);


typedef struct
{
  int fd_in, fd_out;
} cpipe_t;


enum {
  JNB_QUIT, JNB_PAUSE, JNB_RESUME, JNB_SURFACE_NEW, JNB_SURFACE_DEL, 
  JNB_ACK_SURFACE_DEL
};

typedef struct
{
  cpipe_t *worker2main, *main2worker;
  pthread_t worker;
 
  JNIEnv *env;
  JavaVM *vm;
  jobject activity, activity_ref;

  ANativeWindow *window;
  int window_w, window_h, window_format;
} app_t;

static app_t app;

typedef struct
{
  int type;
  union
  {
    struct {
      ANativeWindow *window;
    } surface_new;
  };
} aevent_t;

static int cpipe_create(cpipe_t **CP)
{
  int fd[2];
  *CP= NULL;
  if (pipe2(fd,O_DIRECT)) { kill(getpid(), SIGPIPE); return 1; }
  *CP= malloc(sizeof(**CP));
  (*CP)->fd_in= fd[0];
  (*CP)->fd_out= fd[1];
  return 0;
}

static void cpipe_destroy(cpipe_t **CP)
{
  if (!*CP) return ;
  if ((*CP)->fd_out>=0) close((*CP)->fd_out);
  if ((*CP)->fd_in>=0) close((*CP)->fd_in);
  free(*CP);
  *CP= NULL;
}

static int cpipe_send(cpipe_t *CP, aevent_t *cmd)
{
  if (!CP) return 1;
  return write(CP->fd_out, cmd, sizeof(*cmd))!=sizeof(*cmd) ? 1 : 0;
}

static int cpipe_recv(cpipe_t *CP, aevent_t *cmd)
{
  if (!CP) return 1;
  return read(CP->fd_in, cmd, sizeof(*cmd))!=sizeof(*cmd) ? 1 : 0;
}

/**
  returns 1     if there is an I/O error. ie. there is something wrong
                with the pipe. almost never happens, the pipe is between
                the threads of the same process, its write end gets never
                closed unless Android does some extreme fd-closing before
                the whole process has terminated.

  returns 0     normal operation
 **/
static int worker_get_event(aevent_t *cmd)
{
#define RL_SURFACE 1
#define RL_PAUSE   2
  static int initial_surface_received;
  int reloop;
  aevent_t ocmd;
  reloop= 0;
  if (!initial_surface_received) reloop = RL_SURFACE;
again:
  if (cpipe_recv(app.main2worker, cmd)) return 1;
  switch(cmd->type)
  {
  case JNB_QUIT: 
    return 0;
  case JNB_PAUSE:
    reloop |= RL_PAUSE;
    // notify the user.
    break;
  case JNB_RESUME:
    reloop &= ~RL_PAUSE;
    break;
  case JNB_SURFACE_NEW: // we got the surface we can start drawing
    if (app.window)
       ANativeWindow_release(app.window);
    app.window= cmd->surface_new.window;
    app.window_w= ANativeWindow_getWidth(app.window);
    app.window_h= ANativeWindow_getHeight(app.window);
    app.window_format= ANativeWindow_getFormat(app.window);
    initial_surface_received= 1;
    reloop &= ~RL_SURFACE;
    break;
  case JNB_SURFACE_DEL:
    if (app.window)
    {
      ANativeWindow_release(app.window);
    }
    app.window= NULL;
    reloop |= RL_SURFACE;
    ocmd.type= JNB_ACK_SURFACE_DEL;
    if (cpipe_send(app.worker2main, &ocmd)) return 1;
    break;
  }
  if (reloop) goto again;
  return 0;
#undef RL_SURFACE
#undef RL_PAUSE
}


static void fill_rect(ANativeWindow_Buffer *B, ARect *R, uint32_t color)
{
  uint16_t wolor;
  int x,y;
  uint32_t   *dpix;
  uint16_t   *wpix;
 
  switch(B->format)
  {
  case WINDOW_FORMAT_RGBX_8888:
    dpix= B->bits;
    for(y=R->top;y<R->bottom;y++)
      for(x=R->left;x<R->right;x++)
        dpix[y*B->stride+x]= color;
    break;
  case WINDOW_FORMAT_RGBA_8888:
    dpix= B->bits;
    for(y=R->top;y<R->bottom;y++)
      for(x=R->left;x<R->right;x++)
        dpix[y*B->stride+x]= color | 0xff000000;
    break;
  case WINDOW_FORMAT_RGB_565:
    wpix= B->bits;
    wolor= (((0xff&(color>>16))>>3)<<11) |
           (((0xff&(color>>8))>>2)<<5)    |
           ((0xff&color)>>3);
    for(y=R->top;y<R->bottom;y++)
      for(x=R->left;x<R->right;x++)
        wpix[y*B->stride+x]= wolor; 
    break;
  }
}

static int SX=100, SY=100, SW=100, SH=100;
static int SdX= 6, SdY= 15;

static int imin2(int a, int b) { return a<b?a:b; }
static int imax2(int a, int b) { return a>b?a:b; }

static void draw_frame()
{
  ARect rect;
  ANativeWindow_Buffer buffer;
  int nx, ny;

  rect.left= SX;
  rect.right= SX+SW;
  rect.top= SY;
  rect.bottom= SY+SH;

  nx= SX + SdX; if (nx<0 || nx+SW>app.window_w) { SdX= -SdX; nx= SX+SdX; }
  ny= SY + SdY; if (ny<0 || ny+SH>app.window_h){ SdY= -SdY; ny= SY+SdY; }
  
  rect.left= imin2(nx, SX);
  rect.right= imax2(SX+SW, nx+SW);
  rect.top= imin2(ny, SY);
  rect.bottom= imax2(SY+SH, ny+SH);

  if (ANativeWindow_lock(app.window, &buffer, &rect)<0) return ;
  fill_rect(&buffer, &rect, 0);
  rect.left= nx;
  rect.right= nx+SW;
  rect.top= ny;
  rect.bottom= ny+SH;
  fill_rect(&buffer, &rect, 0xff0000);
  ANativeWindow_unlockAndPost(app.window);
  
  SX= nx;
  SY= ny;
}

static void* worker_entry(void *_arg)
{
  char rbuf[8];
  int timefd;
  int pipefd;
  fd_set rset, xset;
  aevent_t cmd;
  int running;
  int R;

  R= (*app.vm)->GetEnv(app.vm, (void**) &app.env, JNI_VERSION_1_6); 
  if (R==JNI_EDETACHED)
  {
    R= (*app.vm)->AttachCurrentThread(app.vm, &app.env, NULL);
    if (R) return NULL;
  }

  pipefd= app.main2worker->fd_in;
  timefd= timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);

  struct itimerspec tm;
  tm.it_interval.tv_sec= 0;
  tm.it_interval.tv_nsec= 1e9 / FRAME_RATE;
  tm.it_value= tm.it_interval;

  timerfd_settime(timefd, 0, &tm, NULL);

  int maxfd;
  running= 1;
  
  while(running)
  {
    maxfd= 0;
    FD_ZERO(&rset);
    FD_ZERO(&xset);
    FD_SET(pipefd, &rset);
    FD_SET(pipefd, &xset);
    maxfd= imax2(maxfd, pipefd);
    if (timefd>=0)
    {
      FD_SET(timefd, &rset); 
      FD_SET(timefd, &xset);
      maxfd= imax2(maxfd, timefd);
    }
    if (select(maxfd+1, &rset, NULL, &xset, NULL)<0)
    {
      if (errno!=EINTR) break;
      continue;
    }
    if (FD_ISSET(pipefd, &xset)) break;
    if (timefd>=0 && FD_ISSET(timefd, &xset))
    {
      close(timefd);
      timefd= -1;
      // FIXME: timer is gone, either reopen it or advise the user
      continue;
    }
    if (FD_ISSET(pipefd, &rset))
    {
      if (worker_get_event(&cmd)) break;
      if (cmd.type==JNB_QUIT)
      {
        // FIXME: do the quit stuff
        break;
      }
    }
    if (FD_ISSET(timefd, &rset))
    {
       if (!app.window) continue;
       read(timefd, rbuf, 8);
       draw_frame();
       // do the animation
    }
  }
  kill(getpid(), SIGQUIT);
  return NULL; // FIXME: do the cleanup.
}

static jboolean jnbSetup(JNIEnv* env, jobject thiz)
{
  pthread_attr_t attr;

  if (cpipe_create(&app.worker2main) ||
      cpipe_create(&app.main2worker))
     return JNI_FALSE;

  app.activity= thiz;
  app.activity_ref= (*env)->NewGlobalRef(env, thiz);
  (*env)->GetJavaVM(env, &app.vm);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&app.worker, &attr, worker_entry, NULL);

  return JNI_TRUE;
}

void jnbSurfaceNew(JNIEnv *env, jobject thiz, jobject surface)
{
  aevent_t cmd;
  cmd.type= JNB_SURFACE_NEW;
  cmd.surface_new.window= ANativeWindow_fromSurface(env, surface); 
  cpipe_send(app.main2worker, &cmd);
}

void jnbSurfaceDel(JNIEnv *env, jobject thiz)
{
  aevent_t cmd;
  cmd.type= JNB_SURFACE_DEL;
  cpipe_send(app.main2worker, &cmd);
  while(!cpipe_recv(app.worker2main, &cmd))
    if (cmd.type==JNB_ACK_SURFACE_DEL) break;
}



jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
  static JNINativeMethod methods[]=
  {
   { "setup", "()Z", (void*) jnbSetup },
   { "surface_new", "(Landroid/view/Surface;)V", (void*) jnbSurfaceNew },
   { "surface_del", "()V", (void*) jnbSurfaceDel },
  };
  JNIEnv* env;
  int R;
  R= (*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6); 
  if (R==JNI_EDETACHED)
  {
    R= (*vm)->AttachCurrentThread(vm, &env, NULL);
    if (R) return -1;
  }
  jclass cls= (*env)->FindClass(env, "com/dodo/android_test/MainActivity");
  if (cls)
  {
     (*env)->RegisterNatives(env, cls, methods,
                             sizeof(methods)/sizeof(methods[0]));
     return JNI_VERSION_1_6;
  }
  else
     return -1;
}

