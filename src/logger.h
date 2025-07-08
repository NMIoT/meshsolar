
/**
 * @file logger.h
 * @brief This file contains the definition of the logger namespace and related macros for debugging purposes.
 * @anchor [bitpony]
 * @date 2024.04.25
 * @version 1.0.1
 */

#ifndef _LOGGER_H_
#define _LOGGER_H_
#include <Arduino.h>
#include <cstring>

//TODO : disable in Arduino IDE
#define LOG_COLOR_ENABLE

#ifdef log_i
#undef log_i
#endif

#ifdef log_d
#undef log_d
#endif

#ifdef log_w
#undef log_w
#endif

#ifdef log_e
#undef log_e
#endif


namespace dbg
{
	/**
	 * @brief Prints the hexadecimal representation of an array of bytes.
	 * 
	 * @param pary Pointer to the array of bytes.
	 * @param len Length of the array.
	 * @param tag Tag to be printed before the hexadecimal representation.
	 */
	void hex_print(uint8_t *pary, uint16_t len, const char *tag);

	/**
	 * @brief Macro to extract the filename from the full file path.
	 */
	#define FILENAME (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

	/**
	 * @brief Name of the debug section.
	 */
	#define DBG_SECTION_NAME  "NM"

	/**
	 * @brief Macro to print the debug log header with the specified level name and color.
	 */

	#ifdef LOG_COLOR_ENABLE
		#define _DBG_LOG_HDR(new_line, lvl_name, color_n) \
			do { \
				if(new_line) { \
					Serial2.printf("\033["#color_n"m[" lvl_name "/" DBG_SECTION_NAME "] "); \
				} else { \
					Serial2.printf("\033["#color_n"m"); \
				} \
			} while(0)
	#else
		#define _DBG_LOG_HDR(new_line, lvl_name, color_n) \
			do { \
				if(new_line) { \
					Serial2.printf("[" lvl_name "/" DBG_SECTION_NAME "] "); \
				} else { \
					Serial2.printf(""); \
				} \
			} while(0)
	#endif


	/**
	 * @brief Macro to print the debug log end with a new line.
	 */
	
	#ifdef LOG_COLOR_ENABLE
		#define _DBG_LOG_X_END_NEWLINE     		 Serial2.printf("\033[0m\r\n")
	#else
		#define _DBG_LOG_X_END_NEWLINE     		 Serial2.printf("\r\n")
	#endif

	/**
	 * @brief Macro to print the debug log end without a new line.
	 */

	#ifdef LOG_COLOR_ENABLE
		#define _DBG_LOG_X_END_NONE      		 Serial2.printf("\033[0m")
	#else
		#define _DBG_LOG_X_END_NONE      		 Serial2.printf("")
	#endif

	/**
	 * @brief Macro to print the debug log line with the specified level, color, format, and arguments.
	 * Enhanced to handle long strings by using Serial2.print() instead of printf to avoid buffer limitations.
	 * 
	 * @param auto_new_line Flag indicating whether to print a new line after the log line.
	 * @param lvl Debug level.
	 * @param color_n Color code for the log line.
	 * @param fmt Format string for the log line.
	 * @param ... Arguments for the format string.
	 */
	#define dbg_log_line(auto_new_line, lvl, color_n, fmt, ...) \
							do \
							{ \
								_DBG_LOG_HDR(auto_new_line, lvl, color_n);\
								char _temp_buf[512]; \
								snprintf(_temp_buf, sizeof(_temp_buf), fmt, ##__VA_ARGS__); \
								Serial2.print(_temp_buf); \
								if (auto_new_line) \
								{ \
									_DBG_LOG_X_END_NEWLINE; \
								} \
								else \
								{ \
									_DBG_LOG_X_END_NONE; \
								} \
							} \
							while (0)

	/**
	 * @brief Debug level for error messages.
	 */
	#define ERROR           0
	#define WARNING         1
	#define INFO            2
	#define LOG             3
	#define DEBUG           4
	/**
	 * @brief Default debug level.
	 */
	#ifndef DBG_LEVEL
	#define DBG_LEVEL       LOG
	#endif

	/**
	 * @brief Macro to print a debug log line with the specified format and arguments, only if the debug level is set to DBG_LOG or higher.
	 */
	#if (DBG_LEVEL >= DEBUG)
	#define LOG_D(fmt, ...)      dbg_log_line(true,"D", 0, fmt, ##__VA_ARGS__)
	#define log_d(fmt, ...)      dbg_log_line(false, "D", 0, fmt, ##__VA_ARGS__)
	#else
	#define LOG_D(...)
	#define log_d(...)
	#endif
	
	
	#if (DBG_LEVEL >= LOG)
	#define LOG_L(fmt, ...)      dbg_log_line(true,"L", 36, fmt, ##__VA_ARGS__)
	#define log_l(fmt, ...)      dbg_log_line(false, "L", 36, fmt, ##__VA_ARGS__)
	#else
	#define LOG_L(...)
	#define log_l(...)
	#endif
	
	/**
	 * @brief Macro to print an informational log line with the specified format and arguments, only if the debug level is set to DBG_INFO or higher.
	 */
	#if (DBG_LEVEL >= INFO)
	#define LOG_I(fmt, ...)      dbg_log_line(true,"I", 32, fmt, ##__VA_ARGS__)
	#define log_i(fmt, ...)      dbg_log_line(false,"I", 32, fmt, ##__VA_ARGS__)
	#else
	#define LOG_I(...)
	#define log_i(...)
	#endif

	/**
	 * @brief Macro to print a warning log line with the specified format and arguments, only if the debug level is set to DBG_WARNING or higher.
	 */
	#if (DBG_LEVEL >= WARNING)
	#define LOG_W(fmt, ...)      dbg_log_line(true,"W", 33, fmt, ##__VA_ARGS__)
	#define log_w(fmt, ...)      dbg_log_line(false,"W", 33, fmt, ##__VA_ARGS__)
	#else
	#define LOG_W(...)
	#define log_w(...)
	#endif

	/**
	 * @brief Macro to print an error log line with the specified format and arguments, only if the debug level is set to DBG_ERROR or higher.
	 */
	#if (DBG_LEVEL >= ERROR)
	#define LOG_E(fmt, ...)      dbg_log_line(true,"E", 31, fmt, ##__VA_ARGS__)
	#define log_e(fmt, ...)      dbg_log_line(false,"E", 31, fmt, ##__VA_ARGS__)
	#define LOG_E_LOC(fmt, ...)  do {LOG_E("%s:%d:[%s]=> " fmt "\r\n", FILENAME, __LINE__, __func__, ##__VA_ARGS__);} while(0)
	#else
	#define LOG_E(...)
	#define log_e(...)
	#define LOG_E_LOC(...)
	#endif

} 

#endif
