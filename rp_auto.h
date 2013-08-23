#ifndef _RP_AUTO_H_
#define _RP_AUTO_H_

#if defined(__linux__)
#define RP_LINUX           1
#define RP_HAVE_EPOLL      1
#elif defined(__FreeBSD__)
#define RP_FREEBSD         1
#define RP_HAVE_KQUEUE     1
#else
#define RP_HAVE_SELECT     1
#endif

#endif /* RP_AUTO_H_ */
