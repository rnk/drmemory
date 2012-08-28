/* **********************************************************
 * Copyright (c) 2010-2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; 
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Options list that is used for:
 * - usage message from front-end perl script
 * - Doxygen documentation
 * - usage message from client
 * - parsing of options, including default and min-max values, by client
 *
 * We have to split into two different macros in order to populate the
 * client struct: no conditionals there.
 *
 * OPTION_CLIENT and OPTION_FRONT, both take these args:
 * (scope, name, type, default_val, min, max, val_descr, short_descr, long_descr)
 *
 * scope values:
 * - front    = for front-end script for data gathering run
 * - side     = for front-end script to target application in progress
 * - post     = for front-end script for post-run analysis
 * - script   = for front-end script for any use
 * - client   = for client; a documented option
 * - internal = for client; not a documented option (developer use only)
 */

/* XXX: PR 487993: should we support file-private options?
 * Then we wouldn't need op_ globals, and could move midchunk_* options
 * inside leak.c, etc.
 */ 

/* Common min+max values */
#define OPTION_FRONT_BOOL(scope, name, defval, short, long) \
    OPTION_FRONT(scope, name, bool, defval, 0, 0, short, long)
#define OPTION_FRONT_STRING(scope, name, defval, short, long) \
    OPTION_FRONT(scope, name, opstring_t, defval, 0, 0, short, long)
#define OPTION_CLIENT_BOOL(scope, name, defval, short, long) \
    OPTION_CLIENT(scope, name, bool, defval, 0, 0, short, long)
#define OPTION_CLIENT_STRING(scope, name, defval, short, long) \
    OPTION_CLIENT(scope, name, opstring_t, defval, 0, 0, short, long)
#define OPTION_CLIENT_STRING_REPEATABLE(scope, name, defval, short, long) \
    OPTION_CLIENT(scope, name, multi_opstring_t, defval, 0, 0, short, long)

#ifndef TOOLNAME
# define TOOLNAME "Dr. Memory"
#endif
#ifndef drmemscope
# define drmemscope client
#endif
/* Extra reference to expand drmemscope */
#define OPTION_CLIENT_SCOPE(scope, name, type, defval, min, max, short, long) \
    OPTION_CLIENT(scope, name, type, defval, min, max, short, long)

#ifndef IF_WINDOWS_ELSE
# ifdef WINDOWS
#  define IF_WINDOWS_ELSE(x,y) x
# else
#  define IF_WINDOWS_ELSE(x,y) y
# endif
#endif

/****************************************************************************
 * Front-end-script options.  We present a unified list of options to users.
 */

OPTION_FRONT_BOOL(front, version, false,
                  "Display "TOOLNAME" version",
                  "Display "TOOLNAME" version")
OPTION_FRONT_STRING(front, dr, "",
                    "Path to DynamoRIO installation",
                    "The path to the DynamoRIO installation to use.  Not needed when using a released "TOOLNAME" package.")

#ifdef TOOL_DR_MEMORY
OPTION_FRONT_STRING(front, drmemory, "",
                    "Path to "TOOLNAME" installation",
                    "The path to the base of the "TOOLNAME" installation.  Not needed when invoking "TOOLNAME" from an unmodified installation tree.")
OPTION_FRONT_STRING(front, srcfilter, "",
                    "Only show errors referencing named file",
                    "Do not show errors that do not reference the named source file somewhere in their callstacks.")
# ifdef WINDOWS
OPTION_FRONT_BOOL(front, top_stats, false,
                  "Show time taken and memory usage of whole process",
                  "Primarily for use by developers of the tool.  Shows time taken and memory usage of the whole process at the end of the run")
# endif
#endif /* TOOL_DR_MEMORY */
OPTION_FRONT_BOOL(front, follow_children, true,
                  "Monitor child processes",
                  "Monitor child processes by following across execve on Linux or CreateProcess on Windows.  On Linux, monitoring always continues across a fork.")

OPTION_FRONT(side, nudge, uint, 0, 0, UINT_MAX,
             "Process id to nudge",
             "Use this option to 'nudge' an already-running process in order to request leak checking and other "TOOLNAME" actions that normally only occur when the process exits.")

OPTION_FRONT_BOOL(script, v, false,
                  "Display verbose information in the "TOOLNAME" front end",
                  "Display verbose information in the "TOOLNAME" front end")

/* FIXME i#614, i#446: add support for aggregate and post-process w/ USE_DRSYMS */
#if defined(LINUX) && defined(TOOL_DR_MEMORY) && !defined(USE_DRSYMS)
OPTION_FRONT_BOOL(front, skip_results, false,
                  "No results during run: use -results afterward",
                  "Do not produce results while running the application.  This can reduce resource usage if the symbol files for the application are large.  To obtain the results, after the application run is finished, use the -results option in a separate step.")
OPTION_FRONT_STRING(post, results, "",
                    "Produce results from specified -skip_results log dir",
                    "Use this option as the second step in a -skip_results run.  Pass the name of the log directory created by the -skip_results application run.  The results.txt will then be filled in.")
OPTION_FRONT_STRING(post, results_app, "",
                    "Use with -results: specify app from -skip_results run",
                    "Use this option when invoking -results on a different machine from where the application was run with -skip_results.  When -results is invoked without this option also specified, the path to the application that was used to run it with -skip_results is assumed.")
OPTION_FRONT_STRING(post, aggregate, "",
                    "Produce aggregate error report on log dirs",
                    "Pass a list of log directories to produce an aggregate error report.  Useful for applications that consist of a group of separate processes.")
# ifdef VMX86_SERVER
OPTION_FRONT_BOOL(front, use_vmtree, true,
                  "Use VMTREE and VMBLD env vars to locate symbols",
                  "See \\ref sec_setup_syms.")
# endif
#endif /* LINUX + TOOL_DR_MEMORY + !USE_DRSYMS */

/****************************************************************************
 * Public client options
 */

OPTION_CLIENT_BOOL(client, brief, false,
                   "Show simplified and easier-to-read error reports",
                   "Show simplified and easier-to-read error reports that hide STL and CRT source paths, remove executable path prefixes from source files, omit absolute addresses, omit instruction disassembly, and omit thread timestamps")
/* The client default is "c:\\|/tmp" but the front-end script uses install/logs */
OPTION_CLIENT_STRING(client, logdir, "<install>/logs",
                     "Base directory for result file subdirectories and symbol cache",
                     "Destination base directory for result files and the symbol cache (unless -symcache_dir is specified).  A subdirectory inside this base directory is created for each process that is run, along with a single shared symbol cache directory.  If you specify a separate base directory for every run, you will lose the benefits of symbol caching, unless you also specify a separate shared cache directory with the -symcache_dir option.")
OPTION_CLIENT(client, verbose, uint, 1, 0, 32,
              "Verbosity level in log files",
              "Verbosity level in log files: 0=none, 1=warnings, 2+=diagnostic.  Primarily for debugging of "TOOLNAME" itself.")
OPTION_CLIENT_BOOL(client, quiet, false,
                   "Suppress stderr messages and results",
                   "Suppress stderr messages and, on Windows, popup messages.  Overrides -results_to_stderr and -summary.")
#ifdef USE_DRSYMS
OPTION_CLIENT_BOOL(client, results_to_stderr, true,
                   "Print error reports to stderr in addition to results.txt",
                   "Print error reports to stderr in addition to results.txt, interleaving them with the application output.  The output will be prefixed by ~~Dr.M~~ for the main thread and by the thread id for other threads.  This interleaving can make it easier to see which part of an application run raised an error.")
OPTION_CLIENT_BOOL(client, log_suppressed_errors, false,
                   "Log suppressed error reports for postprocessing.",
                   "Log suppressed error reports for postprocessing.  Enabling this option will increase the logfile size, but will allow users to re-process suppressed reports with alternate suppressions or additional symbols.")
#endif
OPTION_CLIENT_BOOL(client, ignore_asserts, false,
                   "Do not abort on debug-build asserts",
                   "Display, but do not abort, on asserts in debug build (in release build asserts are automatically disabled).")
OPTION_CLIENT_BOOL(drmemscope, pause_at_error, false,
                   "Pause at each reported error of any type",
                   ""TOOLNAME" pauses at the point of each error that is identified.  On Windows, this pause is a popup window.  On Linux, the pause involves waiting for a keystroke, which may not work well if the application reads from stdin.  In that case consider -pause_via_loop as an additional option.")
OPTION_CLIENT_BOOL(drmemscope, pause_at_unaddressable, false,
                   "Pause at each unaddressable access",
                   ""TOOLNAME" pauses at the point of each unaddressable access error that is identified.  On Windows, this pause is a popup window.  On Linux, the pause involves waiting for a keystroke, which may not work well if the application reads from stdin.  In that case consider -pause_via_loop as an additional option.")
OPTION_CLIENT_BOOL(drmemscope, pause_at_uninitialized, false,
                   "Pause at each uninitialized read",
                   "Identical to -pause_at_unaddressable, but applies to uninitialized access errors.")
OPTION_CLIENT_BOOL(drmemscope, pause_at_exit, false,
                   "Pause at exit",
                   "Pauses at exit, using the same mechanism described in -pause_at_unaddressable.  Meant for examining leaks in the debugger.")
OPTION_CLIENT_BOOL(client, pause_at_assert, false,
                   "Pause at each debug-build assert",
                   ""TOOLNAME" pauses at the point of each debug-build assert.  On Windows, this pause is a popup window.  On Linux, the pause involves waiting for a keystroke, which may not work well if the application reads from stdin.  In that case consider -pause_via_loop as an additional option.")
OPTION_CLIENT_BOOL(client, pause_via_loop, false,
                   "Pause via loop (not wait for stdin)",
                   "Used in conjunction with -pause_at_uninitialized and -pause_at_uninitialized on Linux, this option causes "TOOLNAME" to pause via an infinite loop instead of waiting for stdin.  "TOOLNAME" will not continue beyond the first such error found.")

#ifdef TOOL_DR_MEMORY
OPTION_CLIENT(client, callstack_max_frames, uint, 12, 0, 4096,
              "How many call stack frames to record",
              "How many call stack frames to record for each error report.  A larger maximum will ensure that no call stack is truncated, but can use more memory if many stacks are large, especially if -check_leaks is enabled.")
#endif

OPTION_CLIENT(client, callstack_style, uint, 0x0301, 0, 0x3ff,
              "Set of flags that controls the callstack printing style",
              "Set of flags that controls the callstack printing style: @@<ul>"
              "<li>0x0001 = show frame numbers@@"
              "<li>0x0002 = show absolute address@@"
              "<li>0x0004 = show offset from library base@@"
              "<li>0x0008 = show offset from symbol start:"
              " @&library!symbol+offs@&@@"
              "<li>0x0010 = show offset from line start: @&foo.c:44+0x8@&@@"
              "<li>0x0020 = @&file:line@& on separate line@@"
              "<li>0x0040 = @&file @ line@& instead of @&file:line@&@@"
              "<li>0x0080 = @&symbol library@& instead of @&library!symbol@&@@"
              "<li>0x0100 = put fields in aligned columns@@"
              "<li>0x0200 = show symbol and module offset when symbols are"
              " missing@@"
              "<li>0x0400 = print unique module id@@"
              "</ul>@@")
              /* (when adding, update the max value as well!) */

#ifdef TOOL_DR_MEMORY
# ifdef USE_DRSYMS /* NYI for postprocess */
/* _REPEATABLE would take too much capacity and too much option string space to
 * specify more than one or two.
 */
OPTION_CLIENT_STRING(client, callstack_truncate_below, "main,wmain,WinMain,wWinMain,*RtlUserThreadStart,_threadstartex",
                     ",-separated list of function names at which to truncate callstacks",
                     "Callstacks will be truncated at any frame that matches any of these ,-separated function names.  The function names can contain * or ? wildcards.")
OPTION_CLIENT_STRING(client, callstack_modname_hide, "*drmemory*",
                     ",-separated list of module names to hide in callstack frames",
                     "Callstack frames will not list module names matching any of these ,-separated patterns.  The names can contain * or ? wildcards.  The module name will be displayed whenever the function name is uknown, however.  The module name will only be hidden for error display purposes: it will still be included when considering suppressions, and it will be included in the generated suppression callstacks.")
OPTION_CLIENT_BOOL(client, callstack_exe_hide, true,
                   "Whether to omit the executable name from callstack frames",
                   "Callstack frames will not list the executable name.  The executable name will be displayed whenever the function name is uknown, however.  The executable name will only be hidden for error display purposes: it will still be included when considering suppressions, and it will be included in the generated suppression callstacks.")
OPTION_CLIENT_STRING(client, callstack_srcfile_hide, "",
                     ",-separated list of source file paths to hide in callstack frames",
                     "Callstack frames will not list source file paths matching any of these ,-separated patterns.  The paths can contain * or ? wildcards.")
OPTION_CLIENT_STRING(client, callstack_srcfile_prefix, "",
                     ",-separated list of path prefixes to remove",
                     "Callstack frame source paths that match any of these ,-separated prefixes will be printed without the leading portion up to and including the match.")
# endif
#endif

OPTION_CLIENT_BOOL(client, callstack_use_top_fp, true,
              "Use the top-level ebp/rbp register as the first frame pointer",
              "Whether to trust the top-level ebp/rbp register to hold the next frame pointer.  If enabled, overridden when -callstack_use_top_fp_selectively is enabled.  Normally trusting the register is correct.  However, if a frameless function is on top of the stack, using the ebp register can cause a callstack to skip the next function.  If this option is set to false, the callstack walk will perform a stack scan at the top of every callstack.  This adds additional overhead in exchange for more accuracy, although in -light mode the additional accuracy has some tradeoffs and can result in incorrect frames.  It should not be necessary to disable this option normally, unless an application or one of its static libraries is built with optimizations that omit frame pointers.")
OPTION_CLIENT_BOOL(client, callstack_use_top_fp_selectively, true,
              "Use the top-level ebp/rbp register as the first frame pointer in certain situations",
              "Whether to trust the top-level ebp/rbp register to hold the next frame pointer in certain situations.  When enabled, this overrides -callstack_use_top_fp if it is enabled; but if -callstack_use_top_fp is disabled then the top fp is never used.  When this option is enabled, in full or -leaks_only modes then the top fp is not used for all non-leak errors, while in -light mode the top fp is only not used for non-leak errors where the top frame is in an application module.  See the -callstack_use_top_fp option for further information about the top frame pointer.")
/* by default scan forward a fraction of a page: good compromise bet perf (scanning
 * can be the bottleneck) and good callstacks
 */
OPTION_CLIENT(client, callstack_max_scan, uint, 2048, 0, 16384,
              "How far to scan to locate the first or next stack frame",
              "How far to scan to locate the first stack frame when starting in a frameless function, or to locate the next stack frame when crossing loader or glue stub thunks or a signal or exception frame.  Increasing this can produce better callstacks but may incur noticeable overhead for applications that make many allocation calls.")

#ifdef TOOL_DR_MEMORY
OPTION_CLIENT_BOOL(client, check_leaks, true,
                   /* Requires -count_leaks and -track_heap */
                   "List details on memory leaks detected",
                   "Whether to list details of each individual memory leak.  If this option is disabled and -count_leaks is enabled, leaks will still be detected, but only the count of leaks will be shown.")
OPTION_CLIENT_BOOL(client, count_leaks, true,
                   "Look for memory leaks",
                   "Whether to detect memory leaks.  Whether details on each leak are shown is controlled by the -check_leaks option.  Disabling this option can reduce execution overhead as less information must be kept internally, while disabling -check_leaks will not affect execution overhead.")
#endif /* TOOL_DR_MEMORY */

#ifdef USE_DRSYMS
OPTION_CLIENT_BOOL(client, symbol_offsets, false,
                   "Deprecated: use -callstack_style flag 0x4",
                   "Deprecated: use -callstack_style flag 0x4")
#endif
              
OPTION_CLIENT_BOOL(client, ignore_early_leaks, true,
                   "Ignore pre-app leaks",
                   "Whether to ignore leaks from memory allocated by system code prior to "TOOLNAME" taking over.")
OPTION_CLIENT_BOOL(client, check_leaks_on_destroy, true,
                   "Report leaks on heap destruction",
                   "If enabled, when a heap is destroyed (HeapDestroy on Windows), report any live allocations inside it as possible leaks.")
OPTION_CLIENT_BOOL(client, possible_leaks, true,
                   "Show possible-leak callstacks",
                   "Whether to list possibly-reachable allocations when leak checking.  Requires -check_leaks.")
#ifdef WINDOWS
OPTION_CLIENT_BOOL(client, check_encoded_pointers, true,
                   "Check for encoded pointers",
                   "Check for encoded pointers to eliminate false positives from pointers kept in encoded form.")
#endif
OPTION_CLIENT_BOOL(client, midchunk_size_ok, true,
                   "Consider mid-chunk post-size pointers legitimate",
                   "Consider allocations reached by a mid-allocation pointer that points past a size field at the head of the allocation to be reachable instead of possibly leaked.  Currently this option looks for a very specific pattern.  If your application's header is slightly different please contact the authors about generalizing this check.")
OPTION_CLIENT_BOOL(client, midchunk_new_ok, true,
                   "Consider mid-chunk post-new[]-header pointers legitimate",
                   "Consider allocations reached by a mid-allocation pointer that points past a size field at the head of the allocation that looks like a new[] header to be reachable instead of possibly leaked.  A heuristic is used for this identification that is not perfect.")
OPTION_CLIENT_BOOL(client, midchunk_inheritance_ok, true,
                   "Consider mid-chunk multi-inheritance pointers legitimate",
                   "Consider allocations reached by a mid-allocation pointer that points to a parent class instantiation to be reachable instead of possibly leaked.  A heuristic is used for this identification that is not perfect.")
OPTION_CLIENT_BOOL(client, midchunk_string_ok, true,
                   "Consider mid-chunk std::string pointers legitimate",
                   "Consider allocations reached by a mid-allocation pointer that points to a char array inside an instance of a std::string representation to be reachable instead of possibly leaked.  A heuristic is used for this identification that is not perfect.")
OPTION_CLIENT_BOOL(client, show_reachable, false,
                   "List reachable allocs",
                   "Whether to list reachable allocations when leak checking.  Requires -check_leaks.")
OPTION_CLIENT_STRING_REPEATABLE(client, suppress, "",
                     "File containing errors to suppress",
                     "File containing errors to suppress.  May be repeated.  See \\ref sec_suppress.")
OPTION_CLIENT_BOOL(client, default_suppress, true,
                   "Use the set of default suppressions",
                   "Use the set of default suppressions that come with "TOOLNAME".  See \\ref sec_suppress.")
OPTION_CLIENT_BOOL(client, gen_suppress_offs, true,
                   "Generate mod+offs suppressions in the output suppress file",
                   "Generate mod+offs suppressions in addition to mod!sym suppressions in the output suppress file")
OPTION_CLIENT_BOOL(client, gen_suppress_syms, true,
                   "Generate mod!syms suppressions in the output suppress file",
                   "Generate mod!syms suppressions in addition to mod+offs suppressions in the output suppress file")
OPTION_CLIENT_BOOL(client, show_threads, true,
                   "Print the callstack of each thread creation point referenced in an error",
                   "Whether to print the callstack of each thread creation point referenced in an error report to the global logfile, which can be useful to identify which thread was involved in the error report.  Look for 'NEW THREAD' in the global.pid.log file in the log directory where the results.txt file is found.")
OPTION_CLIENT_BOOL(client, show_all_threads, false,
                   "Print the callstack of each thread creation point",
                   "Whether to print the callstack of each thread creation point (whether referenced in an error report or not) to the global logfile.  This can be useful to identify which thread was involved in error reports, as well as general diagnostics for what threads were present during a run.  Look for 'NEW THREAD' in the global.pid.log file in the log directory where the results.txt file is found.")
OPTION_CLIENT_BOOL(client, conservative, false,
                   "Be conservative whenever reading application memory",
                   "Be conservative whenever reading application memory.  When this option is disabled, "TOOLNAME" may read return addresses and arguments passed to functions without fault-handling code, which gains performance but can sacrifice robustness when running hand-crafted assembly code")

/* Exposed for Dr. Memory only */
OPTION_CLIENT_BOOL(drmemscope, check_uninit_cmps, true,
                   /* If we check when eflags is written, we can mark the source
                    * undefined reg as defined (since we're reporting there)
                    * and avoid multiple errors on later jcc, etc.
                    */
                   "Check definedness of comparison instructions",
                   "Report definedness errors on compares instead of waiting for conditional jmps.")
OPTION_CLIENT_BOOL(drmemscope, check_uninit_non_moves, false,
                   /* XXX: should also support different checks on a per-module
                    * basis to be more stringent w/ non-3rd-party code?
                    */
                   "Check definedness of all non-move instructions",
                   "Report definedness errors on any instruction that is not a move.  Note: turning this option on may result in false positives, but can also help diagnose errors through earlier error reporting.")
OPTION_CLIENT_BOOL(drmemscope, check_uninit_all, false,
                   "Check definedness of all instructions",
                   "Report definedness errors on any instruction, rather than the default of waiting until something meaningful is done, which reduces false positives.  Note: turning this option on may result in false positives, but can also help diagnose errors through earlier error reporting.")
OPTION_CLIENT_BOOL(drmemscope, strict_bitops, false,
                   "Fully check definedness of bit operations",
                   "Currently, Dr. Memory's definedness granularity is per-byte.  This can lead to false positives on code that uses bitfields.  By default, Dr. Memory relaxes its uninitialized checking on certain bit operations that are typically only used with bitfields, to avoid these false positives.  However, this can lead to false negatives.  Turning this option on will eliminate all false negatives (at the cost of potential false positives).  Eventually Dr. Memory will have bit-level granularity and this option will go away.")
OPTION_CLIENT_SCOPE(drmemscope, stack_swap_threshold, int, 0x9000, 256, INT_MAX,
                    "Stack change amount to consider a swap",
                    "Stack change amount to consider a swap instead of an allocation or de-allocation on the same stack.  "TOOLNAME" attempts to dynamically tune this value unless it is changed from its default.")
OPTION_CLIENT_SCOPE(drmemscope, redzone_size, uint, 16, 0, 32*1024,
                    "Buffer on either side of each malloc",
                    "Buffer on either side of each malloc.  This should be a multiple of 8.")
OPTION_CLIENT_SCOPE(drmemscope, report_max, int, 20000, -1, INT_MAX,
                    "Maximum non-leak errors to report (-1=no limit)",
                    "Maximum non-leak errors to report (-1=no limit).")
OPTION_CLIENT_SCOPE(drmemscope, report_leak_max, int, 10000, -1, INT_MAX,
                    "Maximum leaks to report (-1=no limit)",
                    "Maximum leaks to report (-1=no limit).")
#ifdef USE_DRSYMS
OPTION_CLIENT_BOOL(drmemscope, batch, false,
                   "Do not invoke notepad at the end",
                   "Do not launch notepad with the results file at application exit.")
OPTION_CLIENT_BOOL(drmemscope, summary, true,
                   "Display a summary of results to stderr",
                   "Display process startup information and a summary of errors to stderr at app exit.")
OPTION_CLIENT_BOOL(drmemscope, use_symcache, true,
                   "Cache results of symbol lookups to speed up future runs",
                   "Cache results of symbol lookups to speed up future runs")
OPTION_CLIENT_STRING(drmemscope, symcache_dir, "<install>/logs/symcache",
                     "Directory for symbol cache files",
                     "Destination for symbol cache files.  When using a unique log directory for each run, symbols will not be shared across runs because the default cache location is inside the log directory.  Use this option to set a shared directory.")
OPTION_CLIENT(client, symcache_minsize, uint, 1000, 0, UINT_MAX,
                   "Minimum module size to cache symbols for",
                   "Minimum module size to cache symbols for.  Note that there's little downside to caching and it is pretty much always better to cache.")
OPTION_CLIENT_BOOL(drmemscope, use_symcache_postcall, true,
                   "Cache post-call sites to speed up future runs",
                   "Cache post-call sites to speed up future runs.  Requires -use_symcache to be true.")
# ifdef WINDOWS
OPTION_CLIENT_BOOL(drmemscope, preload_symbols, false,
                   "Preload debug symbols on module load",
                   "Preload debug symbols on module load.  Debug symbols cannot be loaded during leak reporting on Vista, so this option is on by default on Vista.  This option may cause excess memory usage from unneeded debugging symbols.")
OPTION_CLIENT_BOOL(drmemscope, skip_msvc_importers, true,
                   "Do not search for alloc routines in modules that import from msvc*",
                   "Do not search for alloc routines in modules that import from msvc*")
# endif /* WINDOWS */
#else
OPTION_CLIENT_BOOL(drmemscope, summary, false,
                   "Display a summary prior to symbol processing",
                   "Display process startup information and a summary of errors prior to symbol-based suppression and other processing.")
#endif
OPTION_CLIENT_BOOL(drmemscope, warn_null_ptr, false,
                   "Warn if NULL passed to free/realloc",
                   "Whether to warn when NULL is passed to free() or realloc().")
OPTION_CLIENT_SCOPE(drmemscope, delay_frees, uint, 2000, 0, UINT_MAX,
                    "Frees to delay before committing",
                    "Frees to delay before committing.  The larger this number, the greater the likelihood that "TOOLNAME" will identify use-after-free errors.  However, the larger this number, the more memory will be used.  This value is separate for each set of allocation routines.")
OPTION_CLIENT_SCOPE(drmemscope, delay_frees_maxsz, uint, 20000000, 0, UINT_MAX,
                    "Maximum size of frees to delay before committing",
                    "Maximum size of frees to delay before committing.  The larger this number, the greater the likelihood that "TOOLNAME" will identify use-after-free errors.  However, the larger this number, the more memory will be used.  This value is separate for each set of allocation routines.")
OPTION_CLIENT_BOOL(drmemscope, delay_frees_stack, false,
                   "Record callstacks on free to use when reporting use-after-free",
                   "Record callstacks on free to use when reporting use-after-free or other errors that overlap with freed objects.  There is a slight performance hit incurred by this feature for malloc-intensive applications.")
OPTION_CLIENT_BOOL(drmemscope, leaks_only, false,
                   "Check only for leaks and not memory access errors",
                   "Puts "TOOLNAME" into a leak-check-only mode that has lower overhead but does not detect other types of errors other than invalid frees.")

OPTION_CLIENT_BOOL(drmemscope, check_uninitialized, true,
                   "Check for uninitialized read errors",
                   "Check for uninitialized read errors.  When disabled, puts "TOOLNAME" into a mode that has lower overhead but does not detect definedness errors.  Furthermore, the lack of definedness information reduces accuracy of leak identification, resulting in potentially failing to identify some leaks.")
OPTION_CLIENT_BOOL(drmemscope, check_stack_bounds, false,
                   "For -no_check_uninitialized, whether to check for beyond-top-of-stack accesses",
                   "Only applies for -no_check_uninitialized.  Determines whether to check for beyond-top-of-stack accesses.")
OPTION_CLIENT_BOOL(drmemscope, check_stack_access, false,
                   "For -no_check_uninitialized, whether to check for errors on stack or frame references",
                   "Only applies for -no_check_uninitialized.  Determines whether to check for errors on memory references that use %esp or %ebp as a base.  These are normally local variable and function parameter references only, but for optimized or unusual code they could point elsewhere in memory.  Checking these incurs additional overhead.")
OPTION_CLIENT_BOOL(drmemscope, check_alignment, false,
                   "For -no_check_uninitialized, whether to consider alignment",
                   "Only applies for -no_check_uninitialized.  Determines whether to incur additional overhead in order to handle memory accesses that are not aligned to their size.  With this option off, the tool may miss bounds overflows that involve unaligned memory references.")
OPTION_CLIENT_BOOL(drmemscope, fault_to_slowpath, true,
                   "For -no_check_uninitialized, use faults to exit to slowpath",
                   "Only applies for -no_check_uninitialized.  Determines whether to use faulting instructions rather than explicit jump-and-link to exit from fastpath to slowpath.")
#ifdef WINDOWS
OPTION_CLIENT_BOOL(internal, check_tls, true,
                   "Check for access to un-reserved TLS slots",
                   "Check for access to un-reserved TLS slots")
/* i#953: change warning to an error type instead */
OPTION_CLIENT_BOOL(drmemscope, check_gdi, true,
                   "Check for GDI API usage errors",
                   "Check for GDI API usage errors.  Any errors detected will be reported as errors of type WARNING.")
OPTION_CLIENT_BOOL(drmemscope, check_handle_leaks, false,
                   "Check for handle leak errors",
                   "Check for handle leak errors.  Any errors detected will be reported as errors of type WARNING.")
#endif
OPTION_CLIENT_BOOL(drmemscope, check_delete_mismatch, true,
                   "Whether to check for free/delete/delete[] mismatches",
                   "Whether to check for free/delete/delete[] mismatches")

OPTION_CLIENT_STRING(drmemscope, prctl_whitelist, "",
                     "Disable instrumentation unless PR_SET_NAME is on list",
                     "If this list is non-empty, when "TOOLNAME" sees prctl(PR_SET_NAME) and the name is not on the list, then "TOOLNAME" will disable its instrumentation for the rest of the process and for all of its child processes.  The list is ,-separated.")
OPTION_CLIENT_STRING(drmemscope, auxlib, "",
                     "Load auxiliary system call handling library",
                     "This option should specify the basename of an auxiliary system call handling library found in the same directory as the Dr. Memory client library.")
OPTION_CLIENT_BOOL(drmemscope, analyze_unknown_syscalls, true,
                   "For unknown syscalls use memory comparison to find output params",
                   "For unknown syscalls use memory comparison to find output params")
OPTION_CLIENT_BOOL(drmemscope, syscall_dword_granularity, true,
                   "For unknown syscall comparisons, use dword granularity",
                   "For unknown syscall comparisons (-analyze_unknown_syscalls), when changes are detected, consider the containing dword to have changed")
OPTION_CLIENT_BOOL(drmemscope, syscall_sentinels, false,
                   "Use sentinels to detect writes on unknown syscalls.",
                   "Use sentinels to detect writes on unknown syscalls and reduce false positives, in particular for uninitialized reads.  Can potentially result in incorrect behavior if definedness information is incorrect or application threads read syscall parameter info simultaneously.  This option requires -analyze_unknown_syscalls to be enabled.")
/* for chromium we need to ignore malloc_usable_size, and for most windows
 * uses it doesn't exist, so we have this on by default (xref i#314, i#320)
 */
OPTION_CLIENT_BOOL(drmemscope, prefer_msize, IF_WINDOWS_ELSE(true, false),
                   "Prefer _msize to malloc_usable_size when both are present",
                   "Prefer _msize to malloc_usable_size when both are present")

/* not supporting perturb with heapstat: can add easily later */
/* XXX: some of the other options here shouldn't be allowed for heapstat either */
OPTION_CLIENT_BOOL(drmemscope, perturb, false,
                   "Perturb thread scheduling to increase chances of catching races",
                   "Adds random delays to thread synchronization and other operations to try and increase the chances of catching race conditions.")
OPTION_CLIENT_BOOL(drmemscope, perturb_only, false,
                   "Perturb thread scheduling but disable memory checking",
                   "Adds random delays to thread synchronization and other operations to try and increase the chances of catching race conditions, but disables all memory checking to create a low-overhead tool that executes significantly faster.  However, without memory checking race conditions will only be detected if they result in an actual crash or other externally visible change in behavior.  When this option is enabled, "TOOLNAME" will not produce an error summary or results.txt.")
OPTION_CLIENT_SCOPE(drmemscope, perturb_max, uint, 50, 0, UINT_MAX,
                    "Maximum delay added by -perturb",
                    "This option sets the maximum delay added by -perturb, in milliseconds for thread operations and in custom units for instruction-level operations.  Delays added will be randomly selected from 0 up to -perturb_max.")
OPTION_CLIENT_SCOPE(drmemscope, perturb_seed, uint, 0, 0, UINT_MAX,
                    "Seed used for random delays added by -perturb",
                    "To reproduce the random delays added by -perturb, pass the seed from the logfile from the target run to this option.  There may still be non-determinism in the rest of the system, however.")
OPTION_CLIENT_BOOL(drmemscope, light, false,
                   "Enables a lightweight mode that detects only critical errors",
                   "This option enables a lightweight mode that detects only critical errors.  Currently these include only unaddressable accesses.")
OPTION_CLIENT_SCOPE(drmemscope, pattern, uint, 0, 0, USHRT_MAX,
                    "Enables pattern mode. A non-zero 2-byte value must be provided",
                    "Use sentinels to detect accesses on unaddressable regions around allocated heap objects.  When this option is enabled, checks for uninitialized read errors will be disabled.")
OPTION_CLIENT_BOOL(drmemscope, persist_code, false,
                   "Cache instrumented code to speed up future runs",
                   "Cache instrumented code to speed up future runs.  For short-running applications, this can provide a performance boost.  It may not be worth enabling for long-running applications.")
OPTION_CLIENT_STRING(drmemscope, persist_dir, "<install>/logs/codecache",
                     "Directory for code cache files",
                     "Destination for code cache files.  When using a unique log directory for each run, symbols will not be shared across runs because the default cache location is inside the log directory.  Use this option to set a shared directory.")
#ifdef WINDOWS
OPTION_CLIENT_BOOL(drmemscope, soft_kills, true,
                   "Ensure external processes terminated by this one exit cleanly",
                   "Ensure external processes terminated by this one exit cleanly.  Often applications forcibly terminate child processes, which can prevent proper leak checking and error and suppression summarization as well as generation of symbol and code cache files needed for performance.  When this option is enabled, every termination call to another process will be replaced with a directive to the Dr. Memory running in that process to perform a clean shutdown.  If there is no DynamoRIO-based tool in the target process, the regular termination call will be carried out.")
#endif

/****************************************************************************
 * Un-documented client options, for developer use only
 */

OPTION_CLIENT_SCOPE(internal, resfile, uint, 0, 0, UINT_MAX, 
                   "For the given pid, write the result file path to <logdir>/resfile.<pid>",
                   "Write the result file path to <logdir>/resfile.<pid> if the process id equals the passed-in option value")
OPTION_CLIENT_BOOL(internal, stderr, true,
                   "Print summary messages on stderr",
                   "Print summary messages on stderr")
OPTION_CLIENT_BOOL(internal, shadowing, true,
                   "Enable memory shadowing",
                   "For debugging and -leaks_only and -perturb_only modes: can disable all shadowing and do nothing but track mallocs")
OPTION_CLIENT_BOOL(internal, track_allocs, true,
                   "Enable malloc and alloc syscall tracking",
                   "for debugging and -leaks_only and -perturb_only modes: can disable all malloc and alloc syscall tracking")
OPTION_CLIENT_BOOL(internal, check_invalid_frees, true,
                   "Check for invalid frees",
                   "Check for invalid frees")
OPTION_CLIENT_BOOL(internal, track_heap, true,
                   "Track malloc and other library allocations",
                   "If false, "TOOLNAME" only tracks memory allocations at the system call level and does not delve into individual malloc units.  This is required to track leaks, even for system-call-only leaks.  Nowadays we use the heap info for other things, like thread stack identification (PR 418629), and don't really support turning this off.  Requires track_allocs.")
OPTION_CLIENT_BOOL(internal, size_in_redzone, true,
                   "Store alloc size in redzone",
                   "Store size in redzone.  This can only be enabled if redzone_size >= sizeof(size_t).")
OPTION_CLIENT_BOOL(internal, fastpath, true,
                   "Enable fastpath",
                   "Enable fastpath")
OPTION_CLIENT_BOOL(internal, esp_fastpath, true,
                   "Enable esp-adjust fastpath",
                   "Enable esp-adjust fastpath")
OPTION_CLIENT_BOOL(internal, shared_slowpath, true,
                   "Enable shared slowpath calling code",
                   "Enable shared slowpath calling code")
OPTION_CLIENT_BOOL(internal, loads_use_table, true,
                   "Use a table lookup to stay on fastpath",
                   "Use a table lookup to check load addressability and stay on fastpath more often")
OPTION_CLIENT_BOOL(internal, stores_use_table, true,
                   "Use a table lookup to stay on fastpath",
                   "Use a table lookup to check store addressability and stay on fastpath more often")
OPTION_CLIENT(internal, num_spill_slots, uint, 5, 0, 16,
              "How many of our own spill slots to use",
              "How many of our own spill slots to use")
OPTION_CLIENT_BOOL(internal, check_ignore_unaddr, true,
                   "Suppress instrumentation (vs dynamic exceptions) for heap code",
                   "Suppress instrumentation (vs dynamic exceptions) for heap code.  PR 578892: this is now done dynamically and is pretty safe")
OPTION_CLIENT_BOOL(internal, thread_logs, false,
                   /* PR 456181/PR 457001: on some filesystems we can't create a file per
                    * thread, so we support sending everything to the global log.
                    * For now, the only multi-write sequence we ensure is atomic is a
                    * reported error.
                    * FIXME: though for most development this option should be turned on,
                    * maybe we should also support:
                    *  - locking all sequences of writes to global log
                    *  - prepending all writes w/ the writing thread id
                    */
                   "Use per-thread logfiles",
                   "Use per-thread logfiles")
OPTION_CLIENT_BOOL(internal, statistics, false,
                   "Calculate stats in the fastpath",
                   "Calculate stats in the fastpath")
OPTION_CLIENT(internal, stats_dump_interval, uint, 500000, 1, UINT_MAX,
              "How often to dump statistics, in units of slowpath executions",
              "How often to dump statistics, in units of slowpath executions")
OPTION_CLIENT_BOOL(internal, define_unknown_regions, true,
                   "Mark unknown regions as defined",
                   "Handle memory allocated by other processes (or that we miss due to unknown system calls or other problems) by treating as fully defined.  Xref PR 464106.")
OPTION_CLIENT_BOOL(internal, replace_libc , true,
                   "Replace libc str/mem routines w/ our own versions",
                   "Replace libc str/mem routines w/ our own versions")
OPTION_CLIENT_STRING(internal, libc_addrs, "",
                     /* XXX: should we expose this option, or should users w/ custom
                      * or inlined versions be expected to use suppression?
                      * This option is only needed on Linux or on Windows when
                      * we don't have symbols.
                      */
                     "Addresses of statically-included libc routines for replacement.",
                     "Addresses of statically-included libc routines for replacement.  Must be a comma-separated list of hex addresses with 0x prefixes,  in this order: memset, memcpy, memchr, strchr, strrchr, strlen, strcmp, strncmp, strcpy, strncpy, strcat, strncat")
OPTION_CLIENT_BOOL(internal, check_push, true,
                   "Check that pushes are writing to unaddressable memory",
                   "Check that pushes are writing to unaddressable memory")
OPTION_CLIENT_BOOL(internal, single_arg_slowpath, false,
                   /* XXX: PR 494769: this feature is not yet finished */
                   "Use single arg for jmp-to-slowpath and derive second",
                   "Use single arg for jmp-to-slowpath and derive second")
OPTION_CLIENT_BOOL(internal, repstr_to_loop, true,
                   "Add fastpath for rep string instrs by converting to normal loop",
                   "Add fastpath for rep string instrs by converting to normal loop")
OPTION_CLIENT_BOOL(internal, replace_realloc, true,
                   "Replace realloc to avoid races and non-delayed frees",
                   "Replace realloc to avoid races and non-delayed frees")
OPTION_CLIENT_BOOL(internal, share_xl8, true,
                   "Share translations among adjacent similar references",
                   "Share translations among adjacent similar references")
OPTION_CLIENT(internal, share_xl8_max_slow, uint, 5000, 0, UINT_MAX/2,
              "How many slowpaths before abandoning sharing for an individual instr",
              "Sharing does not work across 64K boundaries, and if we get this many slowpaths we flush and re-instrument the at-fault instr without sharing")
OPTION_CLIENT(internal, share_xl8_max_diff, uint, 2048, 0, SHADOW_REDZONE_SIZE*4,
              "Maximum displacement difference to share translations across",
              "Maximum displacement difference to share translations across")
OPTION_CLIENT(internal, share_xl8_max_flushes, uint, 64, 0, UINT_MAX,
              "How many flushes before abandoning sharing altogether",
              "How many flushes before abandoning sharing altogether")
OPTION_CLIENT_BOOL(internal, check_memset_unaddr, true,
                   "Check for in-heap unaddr in memset",
                   "Check for in-heap unaddr in memset")
#ifdef WINDOWS
OPTION_CLIENT_BOOL(internal, disable_crtdbg, true,
                   "Disable debug CRT checks",
                   "Disable debug CRT checks")
#endif

OPTION_CLIENT_BOOL(internal, zero_stack, true,
                   "When detecting leaks but not keeping definedness info, zero old stack frames",
                   "When detecting leaks but not keeping definedness info, zero old stack frames in order to avoid false negatives from stale stack values.  This is potentially unsafe.")
OPTION_CLIENT_BOOL(internal, zero_retaddr, true,
                   "Zero stale return addresses for better callstacks",
                   "Zero stale return addresses for better callstacks.  When enabled, zeroing is performed in all modes of Dr. Memory.  This is theoretically potentially unsafe.  If your application does not work correctly because of this option please let us know.")

#ifdef SYSCALL_DRIVER
OPTION_CLIENT_BOOL(internal, syscall_driver, false,
                   "Use a syscall-info driver if available",
                   "Use a syscall-info driver if available")
#endif
OPTION_CLIENT_BOOL(internal, verify_sysnums, false,
                   "Check system call numbers at startup",
                   "Check system call numbers at startup")
OPTION_CLIENT_BOOL(internal, leave_uninit, false,
                   "Do not mark an uninitialized value as defined once reported",
                   "Do not mark an uninitialized value as defined once reported.  This may result in many reports for the same error.")
OPTION_CLIENT_BOOL(client, leak_scan, true,
                   "Perform leak scan",
                   "Whether to perform the leak scan.  For performance measurement purposes only.")
OPTION_CLIENT_BOOL(internal, pattern_use_malloc_tree, false,
                   "Use red-black tree for tracking malloc/free",
                   "Use red-black tree for tracking malloc/free to reduce the overhead of maintaining the malloc tree on every memory allocation and free, but we have to do expensive hashtable walk to check if an address is in the redzone.")
OPTION_CLIENT_BOOL(internal, replace_malloc, false,
                   "Replace malloc rather than wrapping existing routines",
                   "Replace malloc with custom routines rather than wrapping existing routines.  Replacing is more efficient but can be less transparent.")
OPTION_CLIENT_SCOPE(internal, pattern_max_2byte_faults, int, 0x1000, -1, INT_MAX,
                    "The max number of faults caused by 2-byte pattern checks we could tolerate before switching to 4-byte checks only",
                    "The max number of faults caused by 2-byte pattern checks we could tolerate before switching to 4-byte checks only. 0 means do not use 2-byte checks, and negative value means always use 2-byte checks")
OPTION_CLIENT(internal, callstack_dump_stack, uint, 0, 0, 512*1024,
              "How much of the stack to dump to the logfile",
              "How much of the stack to dump to the logfile prior to each callstack walk.  Debug-build only.")
OPTION_CLIENT_BOOL(internal, pattern_opt_repstr, false,
                   "For pattern mode, optimize each loop expanded from a rep string instruction",
                   "For pattern mode, optimize each loop expanded from a rep string instruction by using an inner loop to avoid unnecessary aflags save/restore.")
OPTION_CLIENT_BOOL(internal, pattern_opt_elide_overlap, false,
                   "For pattern mode, remove redundant checks",
                   "For pattern mode, remove redundant checks if they overlap with other existing checks. This can result in not reporting an error in favor of reporting another error whose memory reference is adjacent. Thus, this gives up the property of reporting any particular error before it happens: a minor tradeoff in favor of performance.")
OPTION_CLIENT_BOOL(internal, track_origins_unaddr, false,
                   "Report possible origins of unaddressable errors caused by using uninitialized variables as pointers",
                   "Report possible origins of unaddressable errors caused by using uninitialized variables as pointers by reporting the alloc context of the memory being referenced by uninitialized pointers. This can result in additional overhead.")
