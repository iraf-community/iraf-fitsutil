/* Copyright(c) 1986 Association of Universities for Research in Astronomy Inc.
 */

/*
 * KWDB.H -- KWDB global definitions.
 */
typedef void *pointer;


#define NTHREADS        797
#define DEF_SZSBUF      32768
#define DEF_NKEYWORDS   512
#define MAX_HASHCHARS   18
#define SZ_FILEBUF      102400

/* KWDB item descriptor.
 */
struct item {
        int name;               /* item name (optional) */
        int value, vallen;      /* item value (optional) */
        int type, typelen;      /* value type (optional) */
        int comment, comlen;    /* item comment (optional) */
        int nexthash;           /* next item in hash thread */
        int nextglob;           /* next item in global list */
};
typedef struct item Item;

typedef int  (*PFI)();

struct kwdb {
        int kwdbname;           /* database name */
        int maxitems;           /* capacity of itab */
        int nitems;             /* number of valid items */
        int itemsused;          /* number of item slots used */
        int sbuflen;            /* capacity of sbuf */
        int sbufused;           /* sbuf characters used */
        int head;               /* itab index of first item */
        int tail;               /* itab index of last item */
        Item *itab;             /* pointer to itab array */
        char *sbuf;             /* pointer to string buffer */
        int hashtbl[NTHREADS];  /* hash table */

        ssize_t (*read)();      /* private file read function */
        ssize_t (*write)();     /* private file write function */
};
typedef struct kwdb KWDB;


pointer kwdb_Open (char *kwdbname);
void    kwdb_Close (pointer kwdb);
char   *kwdb_Name (pointer kwdb);
int     kwdb_Len (pointer kwdb);
int     kwdb_AddEntry (pointer kwdb, char *keyword, char *value,
                       char *type, char *comment);
int     kwdb_Lookup (pointer kwdb, char *keyword, int instance);
char   *kwdb_GetValue (pointer kwdb, char *keyword);
int     kwdb_SetValue (pointer kwdb, char *keyword, char *value);
int     kwdb_SetComment (pointer kwdb, char *keyword, char *comment);
char   *kwdb_GetComment (pointer kwdb, char *keyword);
int     kwdb_SetType (pointer kwdb, char *keyword, char *type);
char   *kwdb_GetType (pointer kwdb, char *keyword);
int     kwdb_Head (pointer kwdb);
int     kwdb_Tail (pointer kwdb);
int     kwdb_Next (pointer kwdb, int ep);
int     kwdb_DeleteEntry (pointer kwdb, int ep);
int     kwdb_RenameEntry (pointer kwdb, int ep, char *newname);
int     kwdb_CopyEntry (pointer kwdb, pointer o_kwdb, int o_ep, char *newname);
int     kwdb_GetEntry (pointer kwdb, int ep, char **keyword, char **value,
                       char **type, char **comment);
char   *kwdb_KWName (pointer kwdb, int ep);

pointer kwdb_OpenFITS (char *filename, int maxcards, int *nblank);
int     kwdb_ReadFITS (pointer kwdb, int fd, int maxcards, int *nblank);
int     kwdb_UpdateFITS (register KWDB *kwdb, char *filename,
                         int update, int extend, int npad);
int     kwdb_WriteFITS (KWDB *kwdb, int fd);
void    kwdb_SetIO (register KWDB *kwdb,
                    ssize_t (*readfcn)(), ssize_t (*writefcn)());

/* Compatibility garbage. */
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
