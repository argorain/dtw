/*******************************************************************
**                                                                **
**  Knihovna pro kontrolu prace s alokovanou pameti a se soubory  **
**  Autor: Pavel Sabatka a Petr Petyovsky                         **
**  Implementovano v ramci bakalarske prace                       **
**  VUT v Brne, UAMT, FEKT v ak. roce 2007, 2008, 2010 - 2012     **
**  rel. 2012.11                                                  **
**                                                                **
*******************************************************************/
/* $Id: check.h 2 2013-10-21 17:59:37Z petyovsky $ */

/**
** NAVOD K POUZITI:
**	Pro pouziti teto knihovny je treba:
**	  1)  pripojit soubory check.h a check.c k projektu
**	  2)  pripojit check.h directivou #include. Tj. do 
**		  hlavickoveho souboru uvest
**			#include "check.h"
**
** UZIVATELSKA MAKRA:
**	Jsou definovany 3 uzivatelske makra, ktere lze volat z programu:
**		- void memory_stat(void); - aktualni vypis alokovane pameti
**		- void file_stat(void); - vypis otevrenych souboru v dane chvili
**		- void stat(void); - slucuje obe predchozi funkce
**/

#ifndef __CHECK_H__
#define __CHECK_H__

#if defined(WIN32) && !defined(_CRT_SECURE_NO_DEPRECATE)
	#define _CRT_SECURE_NO_DEPRECATE 1  /* Due to MSVC */
#endif

/* include knihoven */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#ifdef __cplusplus
	#include <new>
#endif /* __cplusplus */

/*******************************
*   definice maker pro check   *
*******************************/
/* presmerovani vystupu checkeru standardni / chybovy vystup */
#define CHECKER_OUT stderr

/* delka nazvu funkci a souboru */
#define __CHECKER_MAXLENGTH_FILENAME__ 25
#define __CHECKER_MAXLENGTH_FUNCNAME__ 20
#define __CHECKER_MAXLENGTH_PATHNAME__ 25
#define __CHECKER_MAXLENGTH_FILEMODE__ 10

/* sloupce tabulky vypisu - pri odstraneni definice makra se sloupec nezobrazi */
#define __CHECKER_TAB_INDEX__
#define __CHECKER_TAB_FILE_LINENUM__
#define __CHECKER_TAB_FUNCTION__
#define __CHECKER_TAB_SIZE__
#define __CHECKER_TAB_MEM_ADDR__ 

/* makro __func__  je definovano pouze v C99 a C++11 */
/* ISO/IEC N3242=11-0012 [dcl.fct.def.general]
 * It is unspecified whether such a variable __func__has an address distinct from 
 * that of any other object in the program.
 */ 
#if (__STDC_VERSION__ >= 199901L) || (__cplusplus >= 201103L)
	#define __CHECK_FUNC__ __func__
#elif defined(WIN32)
	#define __CHECK_FUNC__ __FUNCTION__
#else
	#define __CHECK_FUNC__ ""
#endif /* (__STDC_VERSION__ >= 199901L) || (__cplusplus >= 201103L) */

/* definice maker pro funkce */
#define malloc(x)     my_malloc((x), __LINE__, __FILE__, __CHECK_FUNC__)
#define calloc(x, y)  my_calloc((x), (y), __LINE__, __FILE__, __CHECK_FUNC__)
#define realloc(x, y) my_realloc((x), (y), __LINE__, __FILE__, __CHECK_FUNC__)
#define free(x)       my_free((x), __LINE__, __FILE__, __CHECK_FUNC__)
#define fopen(x, y)   my_fopen((x), (y), __LINE__, __FILE__, __CHECK_FUNC__)
#define fclose(x)     my_fclose((x), __LINE__, __FILE__, __CHECK_FUNC__)

/* uzivatelske makra */
/* vypise aktualni alokovanou pamet a otevrene soubory */
#define stat()        my_stat(__LINE__, __FILE__, __CHECK_FUNC__)
/* vypise aktualni alokovanou pamet */
#define memory_stat() my_memory_stat(__LINE__, __FILE__, __CHECK_FUNC__)
/* vypise aktualni otevrene soubory */
#define file_stat()   my_file_stat(__LINE__, __FILE__, __CHECK_FUNC__)

#ifdef __cplusplus
	extern "C" {
#endif /* __cplusplus */

/* uzivatelske funkce */
/* vypise aktualni alokovanou pamet a otevrene soubory */
extern void my_stat(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* vypise aktualni alokovanou pamet */
extern void my_memory_stat(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* vypise aktualni otevrene soubory */
extern void my_file_stat(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);

/*  kontrolni funkce */
extern void* my_malloc(size_t size, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
extern void* my_calloc(size_t count, size_t size, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
extern void* my_realloc(void* realloc_ptr, size_t size, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
extern void my_free(void *Ptr, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char name_fce[__CHECKER_MAXLENGTH_FUNCNAME__]);

extern FILE* my_fopen(const char file[__CHECKER_MAXLENGTH_PATHNAME__], const char mode[__CHECKER_MAXLENGTH_FILEMODE__], unsigned int line, const char filesource[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
extern int my_fclose(FILE* streamPtr, unsigned int line, const char file[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);

#ifdef __cplusplus
	 }
#endif /* __cplusplus */

/* KONTROLA PAMETI PRO C++ */
#ifdef __cplusplus

#if (__cplusplus >= 201103L)
	#define _CHECKER_BAD_ALLOC_EXCEPTION
	#define _CHECKER_NO_EXCEPTION			noexcept
#elif defined(WIN32)
	#define _CHECKER_BAD_ALLOC_EXCEPTION
	#define _CHECKER_NO_EXCEPTION			throw()
#else
	#define _CHECKER_BAD_ALLOC_EXCEPTION	throw(std::bad_alloc)
	#define _CHECKER_NO_EXCEPTION			throw()
#endif

	/* kontrolni operatory	*/
 	extern void* operator new(size_t) _CHECKER_BAD_ALLOC_EXCEPTION;
	extern void* operator new[](size_t) _CHECKER_BAD_ALLOC_EXCEPTION;
	extern void* operator new(size_t, const std::nothrow_t&) _CHECKER_NO_EXCEPTION;
	extern void* operator new[](size_t, const std::nothrow_t&) _CHECKER_NO_EXCEPTION;

	extern void operator delete(void *) _CHECKER_NO_EXCEPTION;
	extern void operator delete[](void *) _CHECKER_NO_EXCEPTION;
	extern bool my_new_recorder(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);

	/* pomocna trida pro ulozeni zaznamu pri chybe v delete */
	class CMyDeleteRecorder
		{
		const char * const iFile;
		const char * const iFunc;
		const unsigned iLine;
		
	public:
		CMyDeleteRecorder(const unsigned aLine, const char * const aFile, const char * const aFunc): iFile(aFile), iFunc(aFunc), iLine(aLine)
			{}
		~CMyDeleteRecorder();
		operator bool() const
			{return((!iFile) && (!iFunc));} /* Always return false! No so easy to understand for compiler optimizer. I hope. */
		};

	/* definice maker pro operatory */
	#define new my_new_recorder(__LINE__, __FILE__, __CHECK_FUNC__) ? 0 : new
	#define delete CMyDeleteRecorder(__LINE__, __FILE__, __CHECK_FUNC__) ? (void)0 : (void) delete

#endif /* __cplusplus */

#endif /* __CHECK_H__ */
