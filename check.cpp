/*******************************************************************
**                                                                **
**  Knihovna pro kontrolu prace s alokovanou pameti a se soubory  **
**  Autor: Pavel Sabatka a Petr Petyovsky                         **
**  Implementovano v ramci bakalarske prace                       **
**  VUT v Brne, UAMT, FEKT v ak. roce 2007, 2008, 2010 - 2012     **
**  rel. 2012.11                                                  **
**                                                                **
*******************************************************************/
/* $Id: check.cpp 2 2013-10-21 17:59:37Z petyovsky $ */

#include "check.h"

#define CHECKER_TRUE 1
#define CHECKER_FALSE 0
#define CHECKER_BOOL short int

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef fopen
#undef fclose

#ifdef __cplusplus
	#undef new
	#undef delete
#endif /* __cplusplus */

/* include knihoven */
#include <string.h>

#define HIDDEN_FUNC_NAME "##"
#define HIDDEN_SOURCE_FILE_NAME "#HiddenSource#"

#define PAGE_WIDTH 79

/****************************
*      DATOVE TYPY          *
****************************/
enum TAllocType {EMalloc = 0, ECalloc, ERealloc, ENew, ENewA, ENewNothrow, ENewNothrowA, ECount};
static const char *alloc_type_str[ECount] = {"malloc", "calloc", "realloc", "new", "new[]", "new(nothrow)", "new(nothrow)[]"};

//  prvek seznamu
struct TNodeMem 
	{
	struct TNodeMem *ptr;		// ukazatel na dalsi prvek
	void *memory_address;	// adresa alokovane pameti
	size_t size;			// alokovana velikost
	unsigned int linenum;	// radek zdrojoveho kodu (makro __LINE__)
	unsigned int alloc_num;			// absolutni poradi alokace
	char file[__CHECKER_MAXLENGTH_FILENAME__];
	char func[__CHECKER_MAXLENGTH_FUNCNAME__];
	enum TAllocType type_func;
	};    //  linearni seznam

struct TListMem 
	{
	struct TNodeMem *act;
	struct TNodeMem *first;
		
	// statistiky programu
	size_t size_alloc_max_peak;
	size_t size_alloc;
	size_t size_free;
	unsigned int count_alloc;
	unsigned int count_realloc;
	unsigned int count_dealloc;
	};

struct TNodeFile 
	{
	struct TNodeFile *ptr;		// ukazatel na dalsi prvek
	FILE *memory_address;			// adresa handleru FILE
	unsigned int linenum;			// radek zdrojoveho kodu (makro __LINE__)
	char file[__CHECKER_MAXLENGTH_FILENAME__];		// soubor, ve kterem je volano fopen
	char func[__CHECKER_MAXLENGTH_FUNCNAME__];	// funkce, ve ktere je volano fopen
	char path[__CHECKER_MAXLENGTH_PATHNAME__];		// jmeno souboru
	char mode[__CHECKER_MAXLENGTH_FILEMODE__];	// parametry otevirani souboru
	};

struct TListFile 
	{
	struct TNodeFile *act;
	struct TNodeFile *first;

	// statistiky programu
	unsigned int count_fopen;
	unsigned int count_fclose;
	};

/****************************
*          FUNKCE           *
****************************/
//  prace se seznamem - funkce pro kontrolu alokace v C
static void allocc_init_list(struct TListMem *aLm);					// inicializace
static void allocc_insert(struct TListMem*, size_t, void*, unsigned int, const char[], const char [], enum TAllocType);	// vlozeni prvku
static void allocc_resize_act(struct TListMem*, size_t, void*, unsigned int, const char[], const char [], enum TAllocType);	// zmena parametru pouzite pro realloc()
static void allocc_dispose_act(struct TListMem *L);				// zrusi aktualni prvek
static void allocc_set_act(struct TListMem *L, void *address);		// nastavi prvek se zadanou adresou jako aktualni
static void allocc_set_last_act(struct TListMem *L);				// nastavi posledni prvek jako aktualni
static int  allocc_active(const struct TListMem *L);				// overi, jestli je seznam aktivni

// prace se seznamem - funkce pro kontrolu souboru v C
static void filec_init_list(struct TListFile *LS);				// inicializace
static void filec_insert(struct TListFile *, const char [], const char [], unsigned int, const char [], const char [], FILE *); // vlozeni prvku
static void filec_dispose_act(struct TListFile *L);			// zrusi aktualni prvek
static void filec_set_act(struct TListFile *L, FILE *adress);	// nastavi prvek se zadanou adresou jako aktualni
static void filec_set_last_act(struct TListFile *L);			// nastavi posledni prvek jako aktualni
static int  filec_active(const struct TListFile *L);			// overi, jestli je seznam aktivni

// prace se seznamem - pro oba seznamy
static void dispose_lists (struct TListMem*, struct TListFile *);		// zruseni vsech seznamu

// dalsi funkce
static void checkdel(void);								// tuto funkci pridavam do funkce exit
static void program_error(void);						// vyvola chybu, korektne uvolni pamet a soubory, ukonci program
static void print_lists(struct TListMem *aL, struct TListFile *aLF, int aPrint_alloc, int aPrint_file); // vypise statistiky programu
static void printchar(size_t from, size_t to, char ch); // opakovane vypisuje znak ch dokud from != to
static unsigned int chars_in_dec(size_t val);  		// Vraci pocet znaku nutnych k vytisknuti cisla v desitkove soustave
static unsigned int chars_in_hex(ptrdiff_t val);	// Vraci pocet znaku nutnych k vytisknuti cisla v sestnactkove soustave

/***************************************
*  IMPLEMENTACE STATICKYCH PROMENNYCH  *
****************************************/
static struct TListMem L;						// seznam alokovane pameti
static struct TListFile LF;						// seznam otevrenych souboru
static CHECKER_BOOL used_alloc =  CHECKER_FALSE;	// bool - byla jiz alokovana nejaka pamet?
static CHECKER_BOOL used_file_c = CHECKER_FALSE;	// bool - byl jiz otevren nejaky soubor?
static CHECKER_BOOL correct_exit = CHECKER_TRUE;	// bool - byl program ukoncen bez chyby

#ifdef __cplusplus
// Mailbox pro docasne ulozeni adresy dealokovane pameti pri volani operatoru delete
static void *chk_delete_err_ptr = NULL; 
#endif /* __cplusplus */

#ifdef __cplusplus
// Mailbox pro docasne ulozeni _LINE_ _FILE_ _FUNC_ hodnot pri volani operatoru new
static CHECKER_BOOL chk_new_record_full = CHECKER_FALSE;  // bool - Je ulozen v temp delete mailboxu nejaky zaznam?
static unsigned int chk_new_line = 0;
static char chk_new_soubor[__CHECKER_MAXLENGTH_FILENAME__] = {'\0'};
static char chk_new_func[__CHECKER_MAXLENGTH_FUNCNAME__] = {'\0'};
#endif /* __cplusplus */

/********************************************
*  IMPLEMENTACE FUNKCI  - UZIVATELSKYCH     *
********************************************/
void my_stat(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	fprintf(CHECKER_OUT, "%s: %u :%s\n" ,soubor, line, func);
	// vypise alokovanou pamet a otevrene soubory
	print_lists(&L, &LF, CHECKER_TRUE, CHECKER_TRUE);
	}

void my_memory_stat (unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	fprintf(CHECKER_OUT, "%s: %u :%s\n" ,soubor, line, func);
	// vypise aktualni stav alokovane pameti
	print_lists(&L, &LF, CHECKER_TRUE, CHECKER_FALSE);
	}

void my_file_stat (unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	fprintf(CHECKER_OUT, "%s: %u: %s\n", soubor, line, func);
	//  vypise aktualni otevrene soubory
	print_lists(&L, &LF, CHECKER_FALSE, CHECKER_TRUE);
	}

/***************************************************
*  IMPLEMENTACE ALOKACNICH DEALOKACNICH FUNKCI - C *
***************************************************/
void* my_malloc(size_t size, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	void *novyPtr = NULL;
	if (used_alloc == CHECKER_FALSE) 
		{
		used_alloc = CHECKER_TRUE;
		allocc_init_list(&L);
		
		if (used_file_c == CHECKER_FALSE) 
			atexit(&checkdel);
		}
		
	novyPtr = (void *) malloc(size);

	if (novyPtr) 
		allocc_insert(&L, size, novyPtr, line, soubor, func, EMalloc);
	else 
		fprintf(CHECKER_OUT, "Pri alokaci pameti nastala chyba !\n"
				              "---------------------------------\n"
							  "Chyba pri volani funkce void* malloc(size_t)\n"
				);
	return novyPtr;
	}

void* my_calloc(size_t count, size_t size, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	void *novyPtr = NULL;
	if (used_alloc == CHECKER_FALSE) 
		{
		used_alloc = CHECKER_TRUE;
		allocc_init_list(&L);
		if (used_file_c == CHECKER_FALSE)
			atexit(&checkdel);
		}

	novyPtr = (void *) calloc(count, size);

	if (novyPtr) 
		allocc_insert (&L, size*count, novyPtr, line, soubor, func, ECalloc);
	else 
		fprintf(CHECKER_OUT, "Pri alokaci pameti nastala chyba !\n"
							  "------------------------------\n"
				              "Chyba pri volani funkce void* calloc(size_t, size_t)\n"
				);

	return novyPtr;
	}

void* my_realloc(void* realloc_ptr, size_t size, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	void *newPtr = NULL;

	if(size == 0) 		// tzn. jiny zapis funkce free
		{
		allocc_set_act(&L, realloc_ptr);

		if (allocc_active(&L)) 		// uvolnuji korektne pridelenou pamet
			{
#ifdef __cplusplus			
			if( (L.act->type_func == ENew) || (L.act->type_func == ENewA) || (L.act->type_func == ENewNothrow) || (L.act->type_func == ENewNothrowA) )
			   {
			   fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nFunkci realloc() kompletne uvolnujete pamet: %p, ktera byla alokovana operatorem %s(). Spravne by mela byt uvolnena operatorem delete%s(). Pozor, pamet nebyla uvolnena.\n\n", realloc_ptr, alloc_type_str[L.act->type_func], ( (L.act->type_func == ENewA) || (L.act->type_func == ENewNothrowA) )? ("[]") : (""));
			   fprintf(CHECKER_OUT, "------------------------------\n"
					   "Lokace varovani (odalokace adresy: %p):\n"
					   "Zdrojovy soubor:radek: %s:%u\n", realloc_ptr, soubor, line);
			   fprintf(CHECKER_OUT, "Volani funkce: %s()\n", func);
			   fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
			   return(realloc_ptr);
			   }
#endif
			allocc_dispose_act(&L);
			free(realloc_ptr);
			}
		else 					// uvolnuji pamet, ktera mi nebyla pridelena
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: CHYBA\n------------------------------\n");
			fprintf(CHECKER_OUT, "Volanim funkce\nvoid realloc(void*, size_t)\nse pokousite uvolnit pamet na adrese: %p, ktera Vam nebyla pridelena.\n", realloc_ptr);
			fprintf(CHECKER_OUT, "\n-------------------------------------\n"
					"Lokace chyby:\n"
					"Zdrojovy soubor:radek: %s:%u\n", soubor, line);
			fprintf(CHECKER_OUT, "Volani funkce: %s()\n", func);
			fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
			program_error();
			}
		}  // size == 0
	else 				// budeme pracovat s pameti (alokovat / realokovat)
		{
		if (realloc_ptr == NULL) // tj. jiny zapis funkce malloc
			{
			if (used_alloc == CHECKER_FALSE) 	// jestlize jeste nebylo alokovano
				{
				used_alloc = CHECKER_TRUE;
				allocc_init_list(&L);
				if (used_file_c == CHECKER_FALSE) 
					atexit(&checkdel);
				}
			newPtr = (void *) malloc(size);
			if(newPtr)
				allocc_insert (&L, size, newPtr, line, soubor, func, ERealloc);
			else 
				fprintf(CHECKER_OUT, "CHECKER: Pri alokaci pameti nastala chyba !\n------------------------------\nChyba pri volani funkce void* realloc(void *, size_t,)");
			
			} // if (realloc_ptr == NULL)
		else 				// volani funkce realloc
			{
			allocc_set_act (&L, realloc_ptr);
#ifdef __cplusplus			
			if( (L.act->type_func == ENew) || (L.act->type_func == ENewA) || (L.act->type_func == ENewNothrow) || (L.act->type_func == ENewNothrowA) )
				{
				fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nFunkci realloc() menite velikost pameti: %p, ktera byla alokovana operatorem %s(). Norma jazyka C++ tuto operaci nepovoluje. Spravne by mela byt uvolnena operatorem delete%s(). Pozor, pamet nebyla uvolnena ani zmenena jeji velikost.\n\n", realloc_ptr, alloc_type_str[L.act->type_func], ( (L.act->type_func == ENewA) || (L.act->type_func == ENewNothrowA) )? ("[]") : (""));
				fprintf(CHECKER_OUT, "------------------------------\n"
						"Lokace varovani (realokace adresy: %p):\n"
						"Zdrojovy soubor:radek: %s:%u\n", realloc_ptr, soubor, line);
				fprintf(CHECKER_OUT, "Volani funkce: %s()\n", func);
				fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
				return(realloc_ptr);
				}
#endif			
			if (!allocc_active(&L)) 
				{
				fprintf(CHECKER_OUT, "\n\nCHECKER: Chyba - prvni parametr funkce\n void* realloc (void*, size_t) \nzadavate ukazatel na misto v pameti, ktere vam nebylo prideleno ! !\n-----------------------------------------------------\n");
				return(NULL);
				}
			newPtr = (void *) realloc(realloc_ptr, size);
			if (newPtr) 
				allocc_resize_act(&L, size, newPtr, line, soubor, func, ERealloc);					
			else 
				fprintf(CHECKER_OUT, "CHECKER: Pri alokaci pameti nastala chyba !\n------------------------------\nChyba pri volani funkce void* realloc(void *, size_t,)");
			
			} // else - if (realloc_ptr != NULL)
		return newPtr;
		} // else - if (size > 0)
	return NULL;
	}

void my_free(void *Ptr, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char name_fce[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	allocc_set_act(&L, Ptr);

	if (allocc_active(&L)) 
		{
		if ( (L.act->type_func == ENew) || (L.act->type_func == ENewA) || (L.act->type_func == ENewNothrow) || (L.act->type_func == ENewNothrowA)) 
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nFunkci free() uvolnujete pamet: %p, ktera byla alokovana operatorem %s(). Spravne by mela byt uvolnena operatorem delete%s(). Pozor, pamet nebyla uvolnena.\n\n", Ptr, alloc_type_str[L.act->type_func], ( (L.act->type_func == ENewA) || (L.act->type_func == ENewNothrowA) )? ("[]") : (""));
			fprintf(CHECKER_OUT, "------------------------------\n"
								  "Lokace varovani (odalokace adresy: %p):\n"
								  "Zdrojovy soubor:radek: %s:%u\n", Ptr, soubor, line);
			fprintf(CHECKER_OUT, "Volani funkce: %s()\n", name_fce);
			fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
			}
		else 
			{
			allocc_dispose_act(&L);
			free(Ptr);
			}
		}
	else
		if(Ptr)
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: CHYBA\n------------------------------\n"
								 "Volanim funkce\nvoid free(void*)\nse pokousite uvolnit pamet na adrese: %p, ktera Vam nebyla pridelena.\n", Ptr);
		
			fprintf(CHECKER_OUT, "------------------------------\n"
								 "Lokace chyby:\n"
								  "Zdrojovy soubor:radek: %s:%u\n", soubor, line);
			fprintf(CHECKER_OUT, "Volani funkce: %s()\n", name_fce);
			fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
			program_error();
			}
	}


/****************************************************
*  IMPLEMENTACE ALOKACNI / DEALOKACNI OPERATORU - C++ *
****************************************************/
#ifdef __cplusplus

bool my_new_recorder(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	size_t len;
	if(chk_new_record_full == CHECKER_TRUE)
		fprintf(CHECKER_OUT, "New record mailbox isn't empty:\nnew(%s:%u:%s)\n<-\nnew(%s:%u:%s)\n", 
				chk_new_soubor, chk_new_line, chk_new_func, soubor, line, func);
	chk_new_line = line;
	len = strlen(soubor) + 1;
	if(len > __CHECKER_MAXLENGTH_FILENAME__ )
		strncpy(chk_new_soubor, soubor + len - __CHECKER_MAXLENGTH_FILENAME__, __CHECKER_MAXLENGTH_FILENAME__);
	else
		strncpy(chk_new_soubor, soubor, __CHECKER_MAXLENGTH_FILENAME__);
	
	strncpy(chk_new_func, func, __CHECKER_MAXLENGTH_FUNCNAME__ - 1);
	chk_new_func[__CHECKER_MAXLENGTH_FUNCNAME__ - 1] = '\0';
	chk_new_record_full = CHECKER_TRUE;
	return(false);
	}

void* operator new(size_t aSize) _CHECKER_BAD_ALLOC_EXCEPTION
	{
	void *addr = NULL;
	
	if (used_alloc == CHECKER_FALSE) 
		{
		used_alloc = CHECKER_TRUE;
		allocc_init_list(&L);
		if(used_file_c == CHECKER_FALSE) 
			atexit(&checkdel);
		}
	addr = (void *) malloc(aSize);
	if (addr) 
		{
		if(chk_new_record_full)
			allocc_insert (&L, aSize, addr, chk_new_line, chk_new_soubor, chk_new_func, ENew);
		else
			allocc_insert (&L, aSize, addr, 0, HIDDEN_SOURCE_FILE_NAME, HIDDEN_FUNC_NAME, ENew);
		}
	else 
		{
		fprintf(CHECKER_OUT, "Pri alokaci pameti nastala chyba !\n---------------------------------\nChyba pri alokaci pomoci operatoru void* operator new(size_t)");
		throw std::bad_alloc();
		}
	chk_new_record_full = CHECKER_FALSE;
	return(addr);
	}	

void* operator new(size_t aSize, const std::nothrow_t&) _CHECKER_NO_EXCEPTION
	{
	void *addr = NULL;
	
	if (used_alloc == CHECKER_FALSE) 
		{
		used_alloc = CHECKER_TRUE;
		allocc_init_list(&L);
		if (used_file_c == CHECKER_FALSE) 
			atexit(&checkdel);
		}
	addr = (void *) malloc(aSize);
	
	if(addr) 
		{
		if(chk_new_record_full)
			allocc_insert (&L, aSize, addr, chk_new_line, chk_new_soubor, chk_new_func, ENewNothrow);
		else
			allocc_insert (&L, aSize, addr, 0, HIDDEN_SOURCE_FILE_NAME, HIDDEN_FUNC_NAME, ENewNothrow);
		}
	else 
		{
		fprintf(CHECKER_OUT, "Pri alokaci pameti nastala chyba !\n---------------------------------\nChyba pri alokaci pomoci operatoru void* operator new(size_t,nothrow)");
		}
	chk_new_record_full = CHECKER_FALSE;
	return(addr);
	}

void* operator new[](size_t aSize) _CHECKER_BAD_ALLOC_EXCEPTION
	{
	void *addr = NULL;
	
	if (used_alloc == CHECKER_FALSE) 
		{
		used_alloc = CHECKER_TRUE;
		allocc_init_list(&L);
		if(used_file_c == CHECKER_FALSE) 
			atexit(&checkdel);
		}
	addr = (void *) malloc(aSize);
	if(addr) 
		{
		if(chk_new_record_full)
			allocc_insert (&L, aSize, addr, chk_new_line, chk_new_soubor, chk_new_func, ENewA);
		else
			allocc_insert (&L, aSize, addr, 0, HIDDEN_SOURCE_FILE_NAME, HIDDEN_FUNC_NAME, ENewA);
		}
	else 
		{
		fprintf(CHECKER_OUT, "Pri alokaci pameti nastala chyba !\n---------------------------------\nChyba pri alokaci pomoci operatoru void* operator new[](size_t)");
		throw std::bad_alloc();
		}
	chk_new_record_full = CHECKER_FALSE;
	return(addr);
	}	

void* operator new[](size_t aSize, const std::nothrow_t&) _CHECKER_NO_EXCEPTION
	{
	void *addr = NULL;
	
	if (used_alloc == CHECKER_FALSE) 
		{
		used_alloc = CHECKER_TRUE;
		allocc_init_list(&L);
		if(used_file_c == CHECKER_FALSE) 
			atexit(&checkdel);
		}
	addr = (void *) malloc(aSize);
	if (addr) 
		{
		if(chk_new_record_full)
			allocc_insert (&L, aSize, addr, chk_new_line, chk_new_soubor, chk_new_func, ENewNothrowA);
		else
			allocc_insert (&L, aSize, addr, 0, HIDDEN_SOURCE_FILE_NAME, HIDDEN_FUNC_NAME, ENewNothrowA);
		}
	else 
		{
		fprintf(CHECKER_OUT, "Pri alokaci pameti nastala chyba !\n---------------------------------\nChyba pri alokaci pomoci operatoru void* operator new[](size_t,nothrow)");
		}
	chk_new_record_full = CHECKER_FALSE;
	return(addr);
	}	

CMyDeleteRecorder::~CMyDeleteRecorder()
	{
	if(chk_delete_err_ptr)
		{
		fprintf(CHECKER_OUT, "------------------------------\n"
			"Lokace chyby (odalokace adresy: %p):\n"
			"Zdrojovy soubor:radek: %s:%u\n", chk_delete_err_ptr, iFile, iLine);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", iFunc);
		fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
		}
	chk_delete_err_ptr = NULL;
	}

void operator delete(void *Ptr) _CHECKER_NO_EXCEPTION
	{
	if(chk_delete_err_ptr)
		{
		fprintf(CHECKER_OUT, "------------------------------\n"
				"Lokace chyby (odalokace adresy: %p):\n"
				"Zdrojovy soubor:radek: %s:%u\n", chk_delete_err_ptr, HIDDEN_SOURCE_FILE_NAME, 0);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", HIDDEN_FUNC_NAME);
		fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
		}
	chk_delete_err_ptr = NULL;

	allocc_set_act(&L, Ptr);

	if (allocc_active(&L)) 
		{
		if( (L.act->type_func != ENew) && (L.act->type_func != ENewNothrow) ) 
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nOperatorem delete() uvolnujete pamet: %p, ktera byla alokovana pomoci %s(). Spravne by mela byt uvolnena pomoci %s().\nPozor, pamet nebyla uvolnena.\n", Ptr, alloc_type_str[L.act->type_func], ((L.act->type_func == ENewA) || (L.act->type_func == ENewNothrowA)) ? ("delete[]") : ("free") );
			chk_delete_err_ptr = Ptr;
			}
		else 
			{
			allocc_dispose_act(&L);
			free(Ptr);
			}
		}
	else 
		if(Ptr)
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: CHYBA\n------------------------------\n"
					"Volanim operatoru \ndelete(void*)\nse pokousite uvolnit pamet na adrese: %p, ktera Vam nebyla pridelena.\n", Ptr);
			chk_delete_err_ptr = Ptr;
			program_error();
			}
	}

void operator delete[](void *Ptr) _CHECKER_NO_EXCEPTION
	{
	if(chk_delete_err_ptr)
		{
		fprintf(CHECKER_OUT, "------------------------------\n"
				"Lokace chyby (odalokace adresy: %p):\n"
				"Zdrojovy soubor:radek: %s:%u\n", chk_delete_err_ptr, HIDDEN_SOURCE_FILE_NAME, 0);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", HIDDEN_FUNC_NAME);
		fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
		}
	chk_delete_err_ptr = NULL;
	
	allocc_set_act(&L, Ptr);

	if (allocc_active(&L))
		{
		if( (L.act->type_func != ENewA) && (L.act->type_func != ENewNothrowA) )
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nOperatorem delete[]() uvolnujete pamet: %p, ktera byla alokovana pomoci %s(). Spravne by mela byt uvolnena pomoci %s().\nPozor, pamet nebyla uvolnena.\n", Ptr, alloc_type_str[L.act->type_func], ( (L.act->type_func == ENew) || (L.act->type_func == ENewNothrow)) ? ("delete") : ("free") );
			chk_delete_err_ptr = Ptr;			
			}
		else 
			{
			allocc_dispose_act(&L);
			free(Ptr);
			chk_delete_err_ptr = NULL;
			}
		}
	else
		if(Ptr)
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: CHYBA\n------------------------------\n"
					"Volanim operatoru \ndelete[](void*)\nse pokousite uvolnit pamet na adrese: %p, ktera Vam nebyla pridelena.\n", Ptr);
			chk_delete_err_ptr = Ptr;		
			program_error();
			}
	}

#endif /* __cplusplus */

/***************************************
*  IMPLEMENTACE SOUBOROVYCH FUNKCI - C *
****************************************/
FILE* my_fopen(const char file[__CHECKER_MAXLENGTH_PATHNAME__], const char mode[__CHECKER_MAXLENGTH_FILEMODE__], unsigned int line, const char filesource[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	FILE *novyPtr = NULL;
	if (used_file_c == CHECKER_FALSE) 
		{
		used_file_c = CHECKER_TRUE;
		filec_init_list(&LF);
		if (used_alloc == CHECKER_FALSE) atexit(&checkdel);
		}
		
	novyPtr = fopen(file, mode);

	if (novyPtr != NULL)
		{
		filec_insert(&LF, file, mode, line, filesource, func, novyPtr);
		LF.count_fopen++;
		}
	else
		{
		fprintf(CHECKER_OUT, "CHECKER: CHYBA\nSoubor %s  se nepodarilo otevrit", file);
		}
	return novyPtr;
	}

int my_fclose(FILE* streamPtr, unsigned int line, const char file[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	int succ=0;

	filec_set_act(&LF, streamPtr);

	if (filec_active(&LF))
		{
		LF.count_fclose++;
		succ = fclose (streamPtr);
		filec_dispose_act(&LF);
		}
	else
		{
		fprintf(CHECKER_OUT, "\n\nCHECKER: CHYBA\n------------------------------\n"
							 "Volanim funkce\nint fclose(FILE*)\nse pokousite uzavrit soubor, ktery nebyl otevren\n\n");
		printchar(0, PAGE_WIDTH, '-');
		fprintf(CHECKER_OUT, "\nLokace chyby:\n"
		                     "Zdrojovy soubor:radek: %s:%u\n", file, line);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", func);
		program_error();
		}
	return succ;
	}


/*********************************************************/
/*                  PRACE SE SEZNAMEM                    */
/*********************************************************/

void checkdel(void)
	{
	/*
	tato funkce je pridavana do funkce exit
	v pripade, ze v programu nenastane fatalni chyba (uvolneni nealokovane pameti apod.),
	vypise statistiky programu pomoci print_list() a nasledne uvolni nevolnenou pamet
	a uzavre neuzavrene soubory pomoci funkce dispose_lists()
	*/
#ifdef __cplusplus
	if(chk_delete_err_ptr)
		{
		fprintf(CHECKER_OUT, "------------------------------\n"
				"Lokace chyby (odalokace adresy: %p):\n"
				"Zdrojovy soubor:radek: %s:%u\n", chk_delete_err_ptr, HIDDEN_SOURCE_FILE_NAME, 0);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", HIDDEN_FUNC_NAME);
		fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
		}
	chk_delete_err_ptr = NULL;	
#endif /* __cplusplus */
	
	if (correct_exit == CHECKER_TRUE) 
		print_lists(&L, &LF, CHECKER_TRUE, CHECKER_TRUE);
	dispose_lists(&L, &LF);
	}

void allocc_init_list(struct TListMem *aLm) 
	{
	/* inicializuje seznam pro alokace*/
	
	aLm->act = NULL;
	aLm->first = NULL;
	aLm->count_alloc = 0;
	aLm->count_realloc = 0;
	aLm->count_dealloc = 0;
	aLm->size_alloc_max_peak = 0;
	aLm->size_alloc = 0;
	aLm->size_free = 0;
	}

void filec_init_list (struct TListFile *aLf) 
	{
	/* inicializuje seznam pro soubory*/
	
	aLf->act = NULL;
	aLf->first = NULL;
	aLf->count_fopen = 0;
	aLf->count_fclose = 0;
	}

void allocc_insert(struct TListMem *L, size_t size, void* address, unsigned int line, const char file[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__], enum TAllocType type_func) 
	{
	/* vlozi na konec seznamu prvek se zadanymi parametry
	   nastavi vlozeny prvek (tzn. posledni) jako aktualni
	*/
	
	size_t len;
	struct TNodeMem *insPtr;
		
	insPtr = (struct TNodeMem *) malloc(sizeof(struct TNodeMem));

	if (!insPtr) 
		program_error();
	else	
		{
		insPtr->size = size;			// alokovana velikost
		insPtr->linenum = line;			// radek zdrojoveho kodu (makro __LINE__)

		len = strlen(file) + 1;
		if(len > __CHECKER_MAXLENGTH_FILENAME__ )
			strncpy(insPtr->file, file + len - __CHECKER_MAXLENGTH_FILENAME__, __CHECKER_MAXLENGTH_FILENAME__);
		else
			strncpy(insPtr->file, file, __CHECKER_MAXLENGTH_FILENAME__);
			
		insPtr->alloc_num = L->count_alloc + 1;	// kolikate volani mallocu to je
		insPtr->memory_address = address;	// adresa alokovane pameti
		strncpy(insPtr->func, func, __CHECKER_MAXLENGTH_FUNCNAME__ - 1);		// jmeno nadrazene funkce (makro __CHECK_FUNC__)
		insPtr->func[__CHECKER_MAXLENGTH_FUNCNAME__ - 1] = '\0';
		
		insPtr->type_func = type_func;

		insPtr->ptr = NULL;

		// vlozeni do seznamu
		if (L->first) 
			{
			// vlozeni na konec
			allocc_set_last_act(L);
			L->act->ptr = insPtr;
			L->act = insPtr;
			}
		else 
			{
			L->first = insPtr;
			L->act = insPtr;
			}
		}
	
	/* aktualizace statistik */
	L->count_alloc++;
	L->size_alloc += size;
	if(L->size_alloc_max_peak < (L->size_alloc - L->size_free))
		L->size_alloc_max_peak = (L->size_alloc - L->size_free);
	}

void filec_insert(struct TListFile *L, const char aPath[__CHECKER_MAXLENGTH_PATHNAME__], const char mode[__CHECKER_MAXLENGTH_FILEMODE__], unsigned int line, const char file[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FILENAME__], FILE *stream) {  // ok
/*
vlozi na konec seznamu prvek se zadanymi parametry
nastavi vlozeny prvek (tzn. posledni) jako aktualni
*/
	size_t len;
	struct TNodeFile *insPtr;
	
	insPtr = (struct TNodeFile *) malloc(sizeof(struct TNodeFile));

	if (!insPtr)
		program_error();
	else	
		{
		len = strlen(file) + 1;       // jmeno souboru (makro __FILE__)
		if(len > __CHECKER_MAXLENGTH_FILENAME__ )
			strncpy(insPtr->file, file + len - __CHECKER_MAXLENGTH_FILENAME__, __CHECKER_MAXLENGTH_FILENAME__);
		else
			strncpy(insPtr->file, file, __CHECKER_MAXLENGTH_FILENAME__);

		strncpy(insPtr->func, func, __CHECKER_MAXLENGTH_FUNCNAME__ - 1);  // jmeno nadrazene funkce (makro __CHECK_FUNC__)
		insPtr->func[__CHECKER_MAXLENGTH_FUNCNAME__ - 1] = '\0';

		insPtr->linenum = line;				// radek zdrojoveho kodu (makro __LINE__)

		len = strlen(aPath) + 1; 		// jmeno oteviraneho souboru
		if(len > __CHECKER_MAXLENGTH_PATHNAME__ )
			strncpy(insPtr->path, aPath + len - __CHECKER_MAXLENGTH_PATHNAME__, __CHECKER_MAXLENGTH_PATHNAME__);
		else
			strncpy(insPtr->path, aPath, __CHECKER_MAXLENGTH_PATHNAME__);
				
		strncpy(insPtr->mode, mode, __CHECKER_MAXLENGTH_FILEMODE__ - 1);// mod otvirani souboru
		insPtr->mode[__CHECKER_MAXLENGTH_FILEMODE__ - 1] = '\0';
		
		insPtr->memory_address = stream;			   // adresa streamu

		insPtr->ptr = NULL;

		filec_set_last_act(L);			// vlozeni do seznamu
		if (L->act != NULL) 
			{
			L->act->ptr = insPtr;
			L->act = insPtr;
			}
		else 
			{
			L->first = insPtr;
			L->act = insPtr;
			}
		}
	}

void allocc_resize_act(struct TListMem *L, size_t size, void* address, unsigned int line, const char file[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__], enum TAllocType type_func) 
	{
/* 
Aktualni prvek je naplnen novymi parametry vyuzivano pro funkci realloc()
ponecha aktivitu na zvolenem prvku
*/
	size_t len;
	struct TNodeMem *insPtr = L->act;

	if(allocc_active(L))
		{
		/* aktualizace statistik */
		L->count_realloc++;
		L->size_free  += L->act->size;
		L->size_alloc += size;
		if(L->size_alloc_max_peak < (L->size_alloc - L->size_free))
			L->size_alloc_max_peak = (L->size_alloc - L->size_free);
		
		insPtr->size = size;			// alokovana velikost
		insPtr->linenum = line;			// radek zdrojoveho kodu (makro __LINE__)
		
		len = strlen(file) + 1;								// jmeno souboru (makro __FILE__)
		if(len > __CHECKER_MAXLENGTH_FILENAME__ )
			strncpy(insPtr->file, file + len - __CHECKER_MAXLENGTH_FILENAME__, __CHECKER_MAXLENGTH_FILENAME__);
		else
			strncpy(insPtr->file, file, __CHECKER_MAXLENGTH_FILENAME__);

		insPtr->alloc_num = L->count_alloc + 1;	// kolikate volani mallocu to je
		insPtr->memory_address = address;	// adresa alokovane pameti
		strncpy(insPtr->func, func, __CHECKER_MAXLENGTH_FUNCNAME__ - 1);		// jmeno nadrazene funkce (makro __CHECK_FUNC__)
		insPtr->func[__CHECKER_MAXLENGTH_FUNCNAME__ - 1] = '\0';

		insPtr->type_func = type_func;				
		}
	else 
		program_error();

	}

void allocc_dispose_act(struct TListMem *aLm) 
	{
	/*
	zrusi aktivni prvek. 
	Vazba z predchoziho se navaze na nasledujici prvek seznamu
	nastavi aktivitu na prvni prvek
	*/	
	if(allocc_active(aLm))
		{
		/* aktualizace statistik */
		aLm->count_dealloc++;
		aLm->size_free += aLm->act->size;
		
		if(aLm->act == aLm->first)	// jestli-ze je prvni
			aLm->first = aLm->act->ptr;
		else
			{						// jestli neni prvni, resim vazby
			struct TNodeMem *pom = aLm->first;

			while(pom->ptr != aLm->act) pom = pom->ptr;

			pom->ptr = aLm->act->ptr;
			}
		free(aLm->act);
		aLm->act = aLm->first;
		}
	else
		program_error();

	}

void filec_dispose_act(struct TListFile *aLf) 
	{
	/*
	zrusi aktivni prvek. 
	Vazba z predchoziho se navaze na nasledujici prvek seznamu
	nastavi aktivitu na prvni prvek
	*/	
	if(filec_active(aLf))
		{
		if(aLf->act == aLf->first)  // jestli-ze je prvni
			aLf->first = aLf->act->ptr;
		else
			{						// jestli neni prvni, resim vazby
			struct TNodeFile *pom = aLf->first;

			while(pom->ptr != aLf->act) pom = pom->ptr;

			pom->ptr = aLf->act->ptr;
			}
		free(aLf->act);
		aLf->act = aLf->first;
		}
	}

void allocc_set_last_act(struct TListMem *aLm) 
	{
	/*
	nastavi jako aktualni prvek posledni prvek
	jestli-ze nastane chyba, nastavi NULL
	*/
	if(aLm->first) 
		{
		if (!allocc_active(aLm)) aLm->act = aLm->first;
		while(aLm->act->ptr) aLm->act = aLm->act->ptr;
		}
	else
		aLm->act = NULL;
	}

void filec_set_last_act (struct TListFile *aLf) 
	{
	/*
	nastavi jako aktualni prvek posledni prvek
	jestli-ze nastane chyba, nastavi NULL
	*/
	if (aLf->first) 
		{	
		if (!filec_active(aLf)) aLf->act = aLf->first;
		while(aLf->act->ptr) aLf->act = aLf->act->ptr;
		}
	else 
		aLf->act = NULL;
	}

void allocc_set_act(struct TListMem *aLm, void *aAddress) 
	{
	/*
	najde prvek se zadanou adresou. V pripade, 
	ze takovy prvek nenajde, nastavi aLm->act = NULL
	*/	
	for(aLm->act = aLm->first; (aLm->act) && (aLm->act->memory_address != aAddress);  aLm->act = aLm->act->ptr) ;	
	}

void filec_set_act(struct TListFile *aLf, FILE *aStream) 
	{
	/*
	najde prvek se zadanou adresou. V pripade, 
	ze takovy prvek nenajde, nastavi aLf->act = NULL
	*/	
	for(aLf->act = aLf->first; (aLf->act) && (aLf->act->memory_address != aStream);  aLf->act = aLf->act->ptr) ;
	}

int allocc_active(const struct TListMem *aLm)
	{
	/* jestli-ze je seznam aktivni, vraci 1, jinak 0 */
	return ((aLm->act) ? (CHECKER_TRUE) : (CHECKER_FALSE) );
	}

int filec_active(const struct TListFile *aLf) 
	{
	/* jestli-ze je seznam aktivni, vraci 1, jinak 0 */
	return ((aLf->act) ? (CHECKER_TRUE) : (CHECKER_FALSE) );
	}

void dispose_lists(struct TListMem* aLm, struct TListFile *aLf) 
	{		
	struct TNodeMem *pom;
	struct TNodeFile *pomf;

	while(aLm->first) 
		{
		pom = aLm->first;
		aLm->first = aLm->first->ptr;
		free(pom->memory_address); 	// uvolnim pamet alokovanou v programu
		free(pom);			// zrusim zaznam ze seznamu
		}
	aLm->first = NULL;
	aLm->act = NULL;

	while(aLf->first) 
		{
		pomf = aLf->first;
		aLf->first = aLf->first->ptr;
		fclose(pomf->memory_address);	// uzavru soubor
		free(pomf);			// zrusim zaznam ze seznamu
		}
	aLf->first = NULL;
	aLf->act = NULL;
	}

void program_error(void) 
	{
	fprintf(CHECKER_OUT, "\nProgram musel byt ukoncen\n");
	fflush(CHECKER_OUT);
	correct_exit = CHECKER_FALSE;
	exit(EXIT_FAILURE);
	}

void allocc_print_list(struct TListMem *L)
	{
	size_t a = 0, b = 0, c = 0, e = 0, g = 0, h=0;
	size_t mezer_mezi_sloupci, x = 0;
	size_t pflen = 0; /* printf length */

	fprintf(CHECKER_OUT, "\n\nCHECKER: KONTROLNI VYPIS \n");
	printchar(0, PAGE_WIDTH, '=');
	putc('\n', CHECKER_OUT);

	if (!used_alloc)
		{
		fprintf(CHECKER_OUT, "\nZadna pamet nebyla dynamicky alokovana\n\n");
		return;
		}

	fprintf(CHECKER_OUT, "VYPIS ALOKOVANE PAMETI\n");
	fprintf(CHECKER_OUT, "Celkovy pocet alokaci pameti v programu: \t%u\n", L->count_alloc);
	if(L->count_realloc)
		fprintf(CHECKER_OUT, "Celkovy pocet realokaci pameti v programu: \t%u\n", L->count_realloc);
	
	fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: \t%u\n", L->count_dealloc);
#if (__STDC_VERSION__ >= 199901L) || (__cplusplus >= 201103L)
       // velikost v C99 jiz definovan modifikator velikosti (%z) pro typ size_t
		fprintf(CHECKER_OUT, "Spickove mnozstvi alokovane pameti pri behu programu:  %zu  bytu.\n", L->size_alloc_max_peak);	
		fprintf(CHECKER_OUT, "Alokovano celkem  %zu  bytu, uvolneno  %zu  bytu\n", L->size_alloc, L->size_free);
#else
		fprintf(CHECKER_OUT, "Spickove mnozstvi alokovane pameti pri behu programu:  %lu  bytu.\n", (unsigned long)L->size_alloc_max_peak);
		fprintf(CHECKER_OUT, "Alokovano celkem  %lu  bytu, uvolneno  %lu  bytu\n", 
			   (unsigned long)L->size_alloc, (unsigned long)L->size_free);
#endif /* __STDC_VERSION__ */
	
	if (!L->first)
		{
		fprintf(CHECKER_OUT, "Vsechna dynamicka pamet programu byla bez chyby uvolnena. :-)\n\n");
		return;
		}
	
#if (__STDC_VERSION__ >= 199901L) || (__cplusplus >= 201103L)
	// velikost v C99 jiz definovan modifikator velikosti (%z) pro typ size_t
	fprintf(CHECKER_OUT, "NEODALOKOVANO ZUSTAVA  %zd  BYTU !!!\n\n", L->size_alloc - L->size_free);
#else
	fprintf(CHECKER_OUT, "NEODALOKOVANO ZUSTAVA  %ld  BYTU !!!\n\n", (long) L->size_alloc - (long) L->size_free);
#endif /* __STDC_VERSION__ */

	/* ziskam delky nejdelsich udaju */
	L->act = L->first;
	while(L->act) 
		{
		#ifdef __CHECKER_TAB_INDEX__
		/* poradi alokace */
			x = chars_in_dec(L->act->alloc_num);
			if(x > a) a = x;
		#endif
		
		#ifdef __CHECKER_TAB_FILE_LINENUM__
		/* filename */
			x = strlen(L->act->file);
			if(x > b) b = x;
		/* line number starting from 0!!!!*/
			x = chars_in_dec(L->act->linenum);
			if(x > c) c = x;
		#endif
		
		#ifdef __CHECKER_TAB_FUNCTION__
		/* function name */
			x = strlen(L->act->func);
		/* allocation type (malloc/new/realloc) */
			x += strlen(alloc_type_str[L->act->type_func % ECount]);
			if(x > h) h = x;
		#endif
		
		#ifdef __CHECKER_TAB_SIZE__
		/* allocated memory block size */
			x = chars_in_dec(L->act->size);
			if (x > e) e = x;
		#endif
	
		#ifdef __CHECKER_TAB_MEM_ADDR__ 
		/* Address can be 0!!!! (So, addr + 1) */
			x = 1 + 4; /* pro pomlcku a dvakrat znaky 0x */
			x += chars_in_hex((ptrdiff_t) L->act->memory_address);
			x += chars_in_hex((ptrdiff_t) ((char*)L->act->memory_address + L->act->size) );
			if (x > g) g = x;
		#endif
		
		L->act = L->act->ptr;
		} /* while(L->act) */

	x = a + b + c + e + h + g + 6;				/* prizpusobeni sirky tabulky */
	if (x < 35) mezer_mezi_sloupci = 8;
	else if (x < 40) mezer_mezi_sloupci = 7;
	else if (x < 45) mezer_mezi_sloupci = 6;
	else if (x < 50) mezer_mezi_sloupci = 5;
	else if (x < 55) mezer_mezi_sloupci = 4;
	else if (x < 60) mezer_mezi_sloupci = 3;
	else mezer_mezi_sloupci = 2;
				
	
	/* Zahlavi tabulky */
	fprintf(CHECKER_OUT, "VYPIS ALOKOVANE PAMETI\n");
	
	#ifdef __CHECKER_TAB_INDEX__
	/* Poradi alokace */
		pflen = fprintf(CHECKER_OUT, "#");
		if(a < pflen)
			a = pflen;
		a+= mezer_mezi_sloupci;
		printchar(pflen, a, ' ');
	#endif
	
	#ifdef __CHECKER_TAB_FILE_LINENUM__
	/* Soubor:Radek */
		pflen = fprintf(CHECKER_OUT, "Soubor:Radek");
		if(b + c + 1 < pflen)
			b = pflen;
		else
			b += c + 1;
	
		b += mezer_mezi_sloupci;
		printchar(pflen, b, ' ');
	#endif
	
	#ifdef __CHECKER_TAB_FUNCTION__
	/* Funkce */
		pflen = fprintf(CHECKER_OUT, "Funkce");

		if(h + 3 + 2 < pflen)
			h = pflen;
		else
			h += 3 + 2;
	
		h += mezer_mezi_sloupci;
		printchar(pflen, h, ' ');
	#endif
	
	#ifdef __CHECKER_TAB_SIZE__
	/* allocated memblock size */
		pflen = fprintf(CHECKER_OUT, "B");
		if(e < pflen)
			e = pflen;
		e += mezer_mezi_sloupci;
		printchar(pflen, e, ' ');
	#endif
	
	#ifdef __CHECKER_TAB_MEM_ADDR__
		pflen = fprintf(CHECKER_OUT, "Adresa pameti");
		if(g < pflen)
			g = pflen;
		printchar(pflen, g, ' ');
	#endif
	
	/* Title break line */
	putc('\n', CHECKER_OUT);
	#ifdef __CHECKER_TAB_INDEX__
		printchar(mezer_mezi_sloupci, a, '-');
		printchar(0, mezer_mezi_sloupci, ' ');
	#endif
	#ifdef __CHECKER_TAB_FILE_LINENUM__
		printchar(mezer_mezi_sloupci, b, '-');
		printchar(0, mezer_mezi_sloupci, ' ');
	#endif
	#ifdef __CHECKER_TAB_FUNCTION__
		printchar(mezer_mezi_sloupci, h, '-');
		printchar(0, mezer_mezi_sloupci, ' ');
	#endif
	#ifdef __CHECKER_TAB_SIZE__
		printchar(mezer_mezi_sloupci, e, '-');
		printchar(0, mezer_mezi_sloupci, ' ');
	#endif
	#ifdef __CHECKER_TAB_MEM_ADDR__
		printchar(0, g, '-');
	#endif	
	putc('\n', CHECKER_OUT);
	
	/* Telo tabulky */
	L->act = L->first;
	while(L->act) 
		{
		#ifdef __CHECKER_TAB_INDEX__
		/* poradi alokace */
			pflen = fprintf(CHECKER_OUT, "%u", L->act->alloc_num);
			printchar(pflen, a, ' ');
		#endif
		
		#ifdef __CHECKER_TAB_FILE_LINENUM__
		/* file: line */
			pflen = fprintf(CHECKER_OUT, "%s:%u", L->act->file, L->act->linenum);
			printchar(pflen, b, ' ');
		#endif
		
		#ifdef __CHECKER_TAB_FUNCTION__
		/* function name and allocation type */
			pflen = 0;
			if(strlen(L->act->func))
				{
				pflen = fprintf(CHECKER_OUT, "%s()", L->act->func);
				}
			pflen += fprintf(CHECKER_OUT, ":%s()", alloc_type_str[L->act->type_func]);

			printchar(pflen, h, ' ');
		#endif
		
		#ifdef __CHECKER_TAB_SIZE__
			/* velikost */
			#if (__STDC_VERSION__ >= 199901L) || (__cplusplus >= 201103L)
				pflen = fprintf(CHECKER_OUT, "%zu", L->act->size);		/* velikost v C99, definovan modifikator velikosti (%z) pro typ size_t */
			#else
				pflen = fprintf(CHECKER_OUT, "%lu", (unsigned long)L->act->size);
			#endif /* __STDC_VERSION__ */
			printchar(pflen, e, ' ');
		#endif
		
		#ifdef __CHECKER_TAB_MEM_ADDR__
#ifdef WIN32
			pflen = fprintf(CHECKER_OUT, "0x%lx-0x%lx", L->act->memory_address, (void*)((char*)L->act->memory_address + L->act->size));
#else
			pflen = fprintf(CHECKER_OUT, "%p-%p", L->act->memory_address, (void*)((char*)L->act->memory_address + L->act->size));
#endif
			printchar(pflen, g, ' ');
		#endif
		
		putc('\n', CHECKER_OUT);
		L->act = L->act->ptr;
		} /* while(L->act) */
	
	putc('\n', CHECKER_OUT);
	} /* allocc_print_lists() */


void filec_print_lists(struct TListFile *LF)
	{
	size_t b = 0, c = 0, d = 0, e = 0, f = 0;
	size_t mezer_mezi_sloupci, x = 0;
	size_t pflen = 0; /* printf length */
	
	if(!used_file_c)
		{
		fprintf(CHECKER_OUT, "\nZadne soubory nebyly v programu zpracovavany.\n\n");
		return;
		}

	fprintf(CHECKER_OUT, "PREHLED OTEVRENYCH SOUBORU\n");
	fprintf(CHECKER_OUT, "Pocet otevrenych souboru: \t%u\n", LF->count_fopen);
	fprintf(CHECKER_OUT, "Pocet uzavrenych souboru: \t%u\n", LF->count_fclose);
	fprintf(CHECKER_OUT, "NEUZAVRENYCH ZUSTAVA  %d  SOUBORU\n\n", LF->count_fopen - LF->count_fclose);
	
	if (!LF->first)
		{
		fprintf(CHECKER_OUT, "Vsechny soubory byly bez chyby uzavreny\n\n");
		return;
		}
		
	/* ziskam delky nejdelsich udaju */
	LF->act = LF->first;

	while(LF->act)
		{
		#ifdef __CHECKER_TAB_FILE_LINENUM__
		/* soubor se zdrojovym kodem */
			x = strlen(LF->act->file);			
			if(x > b) b = x;
		/* cislo radku */
			x = chars_in_dec(LF->act->linenum);	
			if(x > c) c = x;
		#endif
		
		#ifdef __CHECKER_TAB_FUNCTION__
		/* funkce */
			x = strlen(LF->act->func);			
			if(x > d) d = x;
		#endif
		
		/* otevirany soubor */
		x = strlen(LF->act->path);			
		if(x > e) e = x;
		
		/* mod otevreni */
		x = strlen(LF->act->mode);			
		if(x > f) f = x;
		
		LF->act = LF->act->ptr;
		} /* while(LF->act) */

	x = b + c + d + e + f;						/* prizpusobeni sirky tabulky */
	if(x < 25) mezer_mezi_sloupci = 8;
	else if(x < 30) mezer_mezi_sloupci = 7;
	else if(x < 35) mezer_mezi_sloupci = 6;
	else if(x < 40) mezer_mezi_sloupci = 5;
	else if(x < 45) mezer_mezi_sloupci = 4;
	else if(x < 50) mezer_mezi_sloupci = 3;
	else mezer_mezi_sloupci = 2;
	
	/*  zahlavi tabulky */
	fprintf(CHECKER_OUT, "VYPIS OTEVRENYCH SOUBORU\n");

	#ifdef __CHECKER_TAB_FILE_LINENUM__
		pflen = fprintf(CHECKER_OUT, "Misto otevreni");		/* soubor: radek */
		if(b + c + 1 < pflen)
			b = pflen;
		else
			b += c + 1;
		b += mezer_mezi_sloupci;
		printchar(pflen, b, ' ');
	#endif
			
	#ifdef __CHECKER_TAB_FUNCTION__
		if(d)
			{
			pflen = fprintf(CHECKER_OUT, "Funkce");			/* funkce */
			d += 2 + mezer_mezi_sloupci;
			printchar(pflen, d, ' ');
			}
	#endif
			
	pflen = fprintf(CHECKER_OUT, "Otevreny soubor");		/* Jmeno souboru */
	if(e < pflen)
		e = pflen;
	e += mezer_mezi_sloupci;
	printchar(pflen, e, ' ');
		
	pflen = fprintf(CHECKER_OUT, "Mod otevreni");		/* Mod otevreni */
	if(f < pflen)
		f = pflen;
	printchar(pflen, f, ' ');

	/* Title break line */
	putc('\n', CHECKER_OUT);
	#ifdef __CHECKER_TAB_FILE_LINENUM__
		printchar(mezer_mezi_sloupci, b, '-');
		printchar(0, mezer_mezi_sloupci, ' ');
	#endif
	#ifdef __CHECKER_TAB_FUNCTION__
		if(d)
			{
			printchar(mezer_mezi_sloupci, d, '-');
			printchar(0, mezer_mezi_sloupci, ' ');
			}
	#endif
	printchar(mezer_mezi_sloupci, e, '-');
	printchar(0, mezer_mezi_sloupci, ' ');

	printchar(0, f, '-');
	putc('\n', CHECKER_OUT);
	
	/* Telo tabulky */
	LF->act = LF->first;
			
	while(LF->act)
		{
		#ifdef __CHECKER_TAB_FILE_LINENUM__
		/* file: line */
			pflen = fprintf(CHECKER_OUT, "%s:%u", LF->act->file, LF->act->linenum);
			printchar(pflen, b, ' ');
		#endif
				
		#ifdef __CHECKER_TAB_FUNCTION__
		/* funkce */
		if(d)
			{
			pflen = fprintf(CHECKER_OUT, "%s()", LF->act->func);
			printchar(pflen, d, ' ');
			}
		#endif
				
		/* nazev souboru */
		pflen = fprintf(CHECKER_OUT, "%s", LF->act->path);
		printchar(pflen, e, ' ');
				
		/* mod otevreni */
		pflen = fprintf(CHECKER_OUT, "%s", LF->act->mode);
		printchar(pflen, f, ' ');

		putc('\n', CHECKER_OUT);
		LF->act = LF->act->ptr;
		} /* while(LF->act) */
			
	putc('\n', CHECKER_OUT);
	} /* filec_print_lists() */


void print_lists(struct TListMem *aL, struct TListFile *aLF, int aPrint_alloc, int aPrint_file)
	{
	if(aPrint_alloc)
		allocc_print_list(aL);
	
	if(aPrint_alloc && aPrint_file && used_file_c)
		{
		putc('\n', CHECKER_OUT);
		printchar(0, PAGE_WIDTH, '-');
		putc('\n', CHECKER_OUT);
		}

	if(aPrint_file)
		filec_print_lists(aLF);

	fflush(CHECKER_OUT);
	} /* print_lists */

void printchar(size_t from, size_t to, char ch)
	{
	for(; from < to; from++) putc(ch, CHECKER_OUT);
	} /* printchar() */

unsigned int chars_in_dec(size_t val)
	{
	unsigned int i = 0;
	do
		{
		val /= 10;
		i++;
		}
	while(val);
	return(i);
	} /* chars_in_dec() */

unsigned int chars_in_hex(ptrdiff_t val)
	{
	unsigned int i = 0;
	do
		{
		val >>= 4;
		i++;
		}
	while(val);
	return(i);
	} /* chars_in_hex() */

/* End of file check.c */
