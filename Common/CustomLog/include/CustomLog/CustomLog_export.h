#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#  ifdef CustomLog_EXPORTS
#    define CUSTOMLOG_API __declspec(dllexport)
#  else
#    define CUSTOMLOG_API __declspec(dllimport)
#  endif
#else
#  define CUSTOMLOG_API
#endif

#ifdef __cplusplus
}
#endif
