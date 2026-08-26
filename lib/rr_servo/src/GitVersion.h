#ifndef RR_GIT_VERSION_H
#define RR_GIT_VERSION_H

// Compile-time SNIP software version.
// Prefer RR_GIT_VERSION_LITERAL (a quoted string, so v2.06+ is safe).
// scripts/git_version.py write_inc() drops git_version.inc next to the sketch.
// Fallback when Arduino IDE compiles without that file: v0.01+
#ifndef RR_GIT_VERSION
#define RR_GIT_VERSION v0.01+
#endif

#define RR_GIT_VERSION_XSTR(x) #x
#define RR_GIT_VERSION_STR(x) RR_GIT_VERSION_XSTR(x)
#ifndef RR_GIT_VERSION_LITERAL
#define RR_GIT_VERSION_LITERAL RR_GIT_VERSION_STR(RR_GIT_VERSION)
#endif

#endif
