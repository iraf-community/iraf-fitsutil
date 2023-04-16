/* Copyright(c) 1986 Association of Universities for Research in Astronomy Inc.
 */

/*
 * KWDB -- Keyword Database interface.
 *
 * The KWDB is a simple keyword/value interface which buffers a list of
 * keywords in memory within the KWDB.
 *
 * KWDB routines:
 *
 *         kwdb = kwdb_Open (kwdbname)
 *         name = kwdb_Name (kwdb)
 *    	     nkw = kwdb_Len (kwdb)
 *    	         kwdb_Close (kwdb)
 *    
 *    	      kwdb_AddEntry (kwdb, keyword, value, type, comment)
 *    value = kwdb_GetValue (kwdb, keyword)
 *            kwdb_SetValue (kwdb, keyword, value)
 *    	    kwdb_SetComment (kwdb, keyword, comment)
 *    str = kwdb_GetComment (kwdb, keyword)
 *	       kwdb_SetType (kwdb, keyword, type)
 *	type = kwdb_GetType (kwdb, keyword)
 *    
 *         ep = kwdb_Lookup (kwdb, keyword, instance)
 *	     ep = kwdb_Head (kwdb)
 *	     ep = kwdb_Tail (kwdb)
 *	     ep = kwdb_Next (kwdb, ep)
 *    	   kwdb_DeleteEntry (kwdb, ep)
 *    	   kwdb_RenameEntry (kwdb, ep, newname)
 *           kwdb_CopyEntry (kwdb, o_kwdb, o_ep, newname)
 *    count = kwdb_GetEntry (kwdb, ep, keyword, value, type, comment)
 *       name = kwdb_KWName (kwdb, ep)
 *
 * Associated utility routines:
 *
 *     kwdb = kwdb_OpenFITS (filename, maxcards, nblank)
 *          kwdb_UpdateFITS (kwdb, filename, update, extend, npad)
 *    count = kwdb_ReadFITS (kwdb, fd, maxcards, nblank)
 *           kwdb_WriteFITS (kwdb, fd)
 *		 kwdb_SetIO (kwdb, readfcn, writefcn)
 *
 * FITS card types:
 *
 *	S=string L=logical N=numeric C=comment H=history T=text
 *
 * The kwdbname is a string identifying the database.  The value of the
 * string is arbitrary and is not used internally by KWDB.  kwdb_Name
 * returns the value of kwdbname entered in kwdb_Open.  kwdb_Len returns
 * the number of keywords (or other entries including null entries) in the
 * database.  kwdb_Close destroys a database and frees all resources used
 * by the KWDB.
 *
 * KWDB entries are maintained in a FIFO list and may be referenced either
 * by the entry reference EP or by the keyword name if the entry is a
 * keyword.  Multiple entries with the same keyword name are permitted, in
 * which case the most recent entry takes precedence.
 *
 * kwdb_AddEntry adds an entry to the database.  kwdb_GetValue returns the
 * value of a keyword (as a string) given its name.  kwdb_SetValue sets the
 * value string of a keyword; kwdb_SetComment sets the comment field.
 * kwdb_Lookup searches the database for the most recent entry for a keyword
 * and returns the entry reference: zero is returned if the keyword is not
 * found.
 * 
 * kwdb_CopyEntry copies an entry from one database to another.
 * kwdb_DeleteEntry deletes an entry.  kwdb_RenameEntry renames an entry:
 * either the old or new name can be nil.  Renaming an entry does not change
 * its position in the list but if there are redefinitions it becomes the 
 * most recent instance.  kwdb_GetEntry gets an entry given the symbol
 * reference.  kwdb_Head returns the entry reference EP of the first entry in
 * the list.  kwdb_Next returns the reference for the next entry after the
 * current one.
 *
 * All keyword values and other fields are stored and manipulated as strings.
 * There is an associated data type field however which can be used to guide
 * how the value field is interpreted or presented.  The contents of the type
 * field are up to the application (e.g., "bool", "int", "float", "string").
 *
 * The high level utility routines kwdb_OpenFITS and kwdb_UpdateFITS create
 * a new KWDB and load the header of a FITS file into it (OpenFITS), and
 * save a KWDB to a new FITS file or update the header of an existing file
 * (UpdateFITS).  kwdb_ReadFITS and kwdb_WriteFITS read and write FITS cards
 * to a file stream.  kwdb_SetIO can be used to enter private file read and
 * write functions to be used by ReadFITS/WriteFITS, allowing data other than
 * host files to be read and written.
 *
 * These are the only routines in KWDB which know anything about FITS, the
 * rest of the interface provides a general purpose symbol table with special
 * characteristics of FIFO ordering, non-keyword entries, and comment lines
 * on keywords.
 */

/* Data structure design.
 * ----------------------
 *	hash table
 *	    hash to thread table
 *	    thread points to head of linked-list of entries
 *	    search down list for first matching entry
 *	    link new entries at head of list
 *	descriptor array (indexed)
 *	    include hash table thread links
 *	sbuf
 *	    character storage
 *	    append new strings
 *	    strings referenced by index
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include "kwdb.h"

#define NTHREADS	797
#define DEF_SZSBUF	32768
#define	DEF_NKEYWORDS	512
#define	MAX_HASHCHARS	18
#define	SZ_FILEBUF	102400

#define	COM_EP(db,p)	((p)-db->itab)
#define	REF_EP(db,ep)	&db->itab[ep]

int primes[] = {
	101,103,107,109,113,127,131,137,139,
	149,151,157,163,167,173,179,181,191,
};

/* KWDB item descriptor. */
struct item {
	int name;		/* item name (optional) */
	int value, vallen;	/* item value (optional) */
	int type, typelen;	/* value type (optional) */
	int comment, comlen;	/* item comment (optional) */
	int nexthash;		/* next item in hash thread */
	int nextglob;		/* next item in global list */
};
typedef struct item Item;

typedef int  (*PFI)();

struct kwdb {
	int kwdbname;		/* database name */
	int maxitems;		/* capacity of itab */
	int nitems;		/* number of valid items */
	int itemsused;		/* number of item slots used */
	int sbuflen;		/* capacity of sbuf */
	int sbufused;		/* sbuf characters used */
	int head;		/* itab index of first item */
	int tail;		/* itab index of last item */
	Item *itab;		/* pointer to itab array */
	char *sbuf;		/* pointer to string buffer */
	int hashtbl[NTHREADS];	/* hash table */

	ssize_t (*read)();	/* private file read function */
	ssize_t (*write)();	/* private file write function */
};
typedef struct kwdb KWDB;

static int hash();
static int addstr();
static int streq();


/*
 * Public functions.
 * -----------------
 */

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

static int addstr (register KWDB *db, char *text);
static int streq (register char *s1, register char *s2);
static int hash (char *key);


/* KWDB_OPEN -- Open a new, empty keyword database.
 */
pointer
kwdb_Open (
    char *kwdbname
)
{
	register KWDB *db = NULL;
	Item *itab = NULL;
	char *sbuf = NULL;

	if (!(db = (KWDB *) calloc (1, sizeof(KWDB))))
	    goto cleanup;
	if (!(itab = (Item *) calloc (DEF_NKEYWORDS, sizeof(Item))))
	    goto cleanup;
	if (!(sbuf = (char *) calloc (DEF_SZSBUF, sizeof(char))))
	    goto cleanup;

	sbuf[0] = '\0';
	db->kwdbname = 1;
	strcpy (sbuf+1, kwdbname);
	db->sbufused = strlen(kwdbname) + 2;
	db->sbuflen = DEF_SZSBUF;
	db->sbuf = sbuf;

	db->maxitems = DEF_NKEYWORDS;
	db->nitems = 0;
	db->itemsused = 1;
	db->head = 0;
	db->tail = 0;
	db->itab = itab;
	db->read = read;
	db->write = write;

	return ((pointer) db);

cleanup:
	if (sbuf)
	    free (sbuf);
	if (itab)
	    free ((char *) itab);
	if (db)
	    free ((char *) db);

	return (NULL);
}


/* KWDB_CLOSE -- Destroy a KWDB database and free all resources.
 */
void
kwdb_Close (
    pointer kwdb
)
{
	register KWDB *db = (KWDB *) kwdb;

	if (db->sbuf)
	    free (db->sbuf);
	if (db->itab)
	    free ((char *) db->itab);
	if (db)
	    free ((char *) db);
}


/* KWDB_NAME -- Return the name of a KWDB database.
 */
char *
kwdb_Name (
    pointer kwdb
)
{
	register KWDB *db = (KWDB *) kwdb;
	return (db->sbuf + db->kwdbname);
}


/* KWDB_LEN -- Return the number of items in a KWDB database.
 */
int
kwdb_Len (
    pointer kwdb
)
{
	register KWDB *db = (KWDB *) kwdb;
	return (db->nitems);
}


/* KWDB_ADDENTRY -- Append an entry to the keyword database.  Any of the
 * fields keyword, value, type, or comment can be NULL if these fields
 * have no value.
 */
int
kwdb_AddEntry (
    pointer kwdb,
    char *keyword, 
    char *value, 
    char *type, 
    char *comment
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp, *otp;
	int index, i;

	/* Make sure there is space in the item buffer. */
	if (db->itemsused >= db->maxitems) {
	    db->maxitems += DEF_NKEYWORDS;
	    itp = (Item *) realloc ((char *)db->itab,
		sizeof(Item) * db->maxitems);
	    if (!itp)
		return (-1);
	    db->itab = itp;
	}

	index = db->itemsused++;
	itp = &db->itab[index];
	itp->name = addstr (db, keyword);
	itp->value = addstr (db, value);
	itp->vallen = value ? strlen(value) : 0;
	itp->type = addstr (db, type);
	itp->typelen = type ? strlen(type) : 0;
	itp->comment = addstr (db, comment);
	itp->comlen = comment ? strlen(comment) : 0;
	itp->nexthash = 0;
	itp->nextglob = 0;

	/* Link item at head of global list if list empty. */
	if (!db->head)
	    db->head = index;

	/* Link item at tail of global list. */
	if (db->tail) {
	    otp = REF_EP(db,db->tail);
	    otp->nextglob = index;
	}
	db->tail = index;

	/* Enter item in hash table. */
	if (keyword && *keyword) {
	    int hashval = hash (keyword);
	    if ((i = db->hashtbl[hashval]))
		itp->nexthash = i;
	    db->hashtbl[hashval] = index;
	}

	db->nitems++;
	return (COM_EP(db,itp));
}


/* KWDB_LOOKUP -- Lookup a keyword in the database.  If there are multiple
 * values for the same keyword in the database then the instance number
 * determines which is returned.  An instance of zero returns the first 
 * (most recently entered) instance.  Successive instances return entries
 * earlier in the list.  Note that this is opposite of an ordered traversal
 * of the list using kwdb_Head/kwdb_Next, which accesses the list with 
 * oldest entries first.
 */
int
kwdb_Lookup (
    pointer kwdb,
    char *keyword,
    int instance
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp;
	register int i, j;
	int hashval;

	hashval = hash (keyword);
	if ((i = db->hashtbl[hashval]))
	    for (itp = REF_EP(db,i), j=0;  i;  itp = REF_EP(db,i=itp->nexthash))
		if (streq (db->sbuf + itp->name, keyword))
		    if (instance == j++)
			return (i);

	return (0);
}


/* KWDB_GETVALUE -- Lookup a keyword and returns its value.  NULL is returned
 * if the keyword is not found.
 */
char *
kwdb_GetValue (
    pointer kwdb,
    char *keyword
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp;
	register int ep;

	if ((ep = kwdb_Lookup (kwdb, keyword, 0))) {
	    itp = REF_EP(db,ep);
	    return (db->sbuf + itp->value);
	}

	return (NULL);
}


/* KWDB_SETVALUE -- Modify the value of a keyword.  Zero is returned for a
 * successul operation, otherwise -1.
 */
int
kwdb_SetValue (
    pointer kwdb,
    char *keyword,
    char *value
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp = NULL;
	char *oldval;
	int ep;

	if ((ep = kwdb_Lookup (kwdb, keyword, 0)))
	    itp = REF_EP(db,ep);
	else
	    return (-1);

	oldval = db->sbuf + itp->value;
	memset (oldval, 0, itp->vallen);
	if (strlen(value) > itp->vallen) {
	    itp->value = addstr (db, value);
	    itp->vallen = strlen (value);
	} else
	    strcpy (oldval, value);

	return (0);
}


/* KWDB_SETCOMMENT -- Modify the comment field of a keyword.  Zero is
 * returned for a successul operation, otherwise -1.
 */
int
kwdb_SetComment (
    pointer kwdb,
    char *keyword,
    char *comment
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp = NULL;
	char *oldcom;
	int ep;

	if ((ep = kwdb_Lookup (kwdb, keyword, 0)))
	    itp = REF_EP(db,ep);
	else
	    return (-1);

	oldcom = db->sbuf + itp->comment;
	memset (oldcom, 0, itp->comlen);
	if (strlen(comment) > itp->comlen) {
	    itp->comment = addstr (db, comment);
	    itp->comlen = strlen (comment);
	} else
	    strcpy (oldcom, comment);

	return (0);
}


/* KWDB_GETCOMMENT -- Lookup a keyword and returns its comment field.  NULL
 * is returned if the keyword is not found.
 */
char *
kwdb_GetComment (
    pointer kwdb,
    char *keyword
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp;
	register int ep;

	if ((ep = kwdb_Lookup (kwdb, keyword, 0))) {
	    itp = REF_EP(db,ep);
	    return (db->sbuf + itp->comment);
	}

	return (NULL);
}


/* KWDB_SETTYPE -- Modify the data type field of a keyword.  Zero is
 * returned for a successul operation, otherwise -1.
 */
int
kwdb_SetType (
    pointer kwdb,
    char *keyword,
    char *type
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp = NULL;
	char *oldtype;
	int ep;

	if ((ep = kwdb_Lookup (kwdb, keyword, 0)))
	    itp = REF_EP(db,ep);
	else
	    return (-1);

	oldtype = db->sbuf + itp->type;
	memset (oldtype, 0, itp->typelen);
	if (strlen(type) > itp->typelen) {
	    itp->type = addstr (db, type);
	    itp->typelen = strlen (type);
	} else
	    strcpy (oldtype, type);

	return (0);
}


/* KWDB_GETTYPE -- Lookup a keyword and returns its type field.  NULL
 * is returned if the keyword is not found.
 */
char *
kwdb_GetType (
    pointer kwdb,
    char *keyword
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp;
	register int ep;

	if ((ep = kwdb_Lookup (kwdb, keyword, 0))) {
	    itp = REF_EP(db,ep);
	    return (db->sbuf + itp->type);
	}

	return (NULL);
}


/* KWDB_HEAD -- Return the entry pointer for the first entry in the database.
 */
int
kwdb_Head (
    pointer kwdb
)
{
	register KWDB *db = (KWDB *) kwdb;
	return (db->head);
}


/* KWDB_TAIL -- Return the entry pointer for the most recent entry in
 * the database.
 */
int
kwdb_Tail (
    pointer kwdb
)
{
	register KWDB *db = (KWDB *) kwdb;
	return (db->tail);
}


/* KWDB_NEXT -- Return the entry pointer for the next entry in a database,
 * given a pointer to the preceding entry.  Zero is returned at the end of
 * the database.
 */
int
kwdb_Next (
    pointer kwdb,
    int ep
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp = NULL;

	itp = REF_EP(db,ep);
	return (itp->nextglob);
}


/* KWDB_DELETEENTRY -- Delete an entry from the database given its entry
 * pointer.
 */
int
kwdb_DeleteEntry (
    pointer kwdb,
    int ep
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp = NULL, *otp;
	int hashval, i;
	char *name;

	if (ep <= 0 || ep > db->tail)
	    return (-1);

	itp = REF_EP(db,ep);
	name = db->sbuf + itp->name;

	/* If the entry is a keyword remove it from the hash table. */
	if (itp->name && *name) {
	    hashval = hash (name);
	    i = db->hashtbl[hashval];
	    if (ep == i)
		db->hashtbl[hashval] = itp->nexthash;
	    else {
		otp = REF_EP(db,i);
		for (i=otp->nexthash;  i;  otp=REF_EP(db,i), i=otp->nexthash)
		    if (ep == i) {
			otp->nexthash = itp->nexthash;
			break;
		    }
	    }
	}

	/* Remove the entry from the global list. */
	if (db->head == ep)
	    db->head = itp->nextglob;

	for (i=db->head;  i;  i = otp->nextglob) {
	    otp = REF_EP(db,i);
	    if (otp->nextglob == ep) {
		otp->nextglob = itp->nextglob;
		break;
	    }
	}

	if (db->tail == ep) {
	    db->tail = i;
	    db->itemsused--;
	}

	db->nitems--;
	return (0);
}


/* KWDB_RENAMEENTRY -- Rename an entry given its entry pointer.
 * Either the old or new name can be nil.  Renaming an entry does not change
 * its position in the list but if there are redefinitions it becomes the 
 * most recent instance.
 */
int
kwdb_RenameEntry (
    pointer kwdb,
    int ep,
    char *newname
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp = NULL, *otp;
	int hashval, i;
	char *name;

	if (ep <= 0 || ep > db->tail)
	    return (-1);

	itp = REF_EP(db,ep);
	name = db->sbuf + itp->name;

	/* If the entry is a keyword remove it from the hash table. */
	if (itp->name && *name) {
	    hashval = hash (name);
	    i = db->hashtbl[hashval];
	    if (ep == i)
		db->hashtbl[hashval] = itp->nexthash;
	    else {
		otp = REF_EP(db,i);
		for (i=otp->nexthash;  i;  otp=REF_EP(db,i), i=otp->nexthash)
		    if (ep == i) {
			otp->nexthash = itp->nexthash;
			break;
		    }
	    }
	}

	/* Reenter the item in the hash table using the new name. */
	if (newname && *newname) {
	    int hashval = hash (newname);
	    if ((i = db->hashtbl[hashval]))
		itp->nexthash = i;
	    db->hashtbl[hashval] = ep;
	    itp->name = addstr (db, newname);
	} else
	    itp->name = 0;

	return (0);
}


/* KWDB_COPYENTRY -- Copy an entry from one database to another.  If newname
 * is not not null it will be the name of the new entry.  The input and output
 * datbases can be the same.
 */
int
kwdb_CopyEntry (
    pointer kwdb,
    pointer o_kwdb,
    int o_ep,
    char *newname
)
{
	register KWDB *o_db = (KWDB *) o_kwdb;
	register Item *itp = REF_EP(o_db,o_ep);
	char *name;

	if (newname && *newname)
	    name = newname;
	else
	    name = itp->name + o_db->sbuf;

	return (kwdb_AddEntry (kwdb, name,
	    itp->value + o_db->sbuf,
	    itp->type + o_db->sbuf,
	    itp->comment + o_db->sbuf));
}


/* KWDB_GETENTRY -- Get an entry given its entry pointer.  A count of the
 * number of nonempty strings is returned (i.e. zero is returned if the value
 * is a blank line, 3 for a full keyword with value and comment).
 */
int
kwdb_GetEntry (
    pointer kwdb,
    int ep,
    char **keyword,
    char **value,
    char **type,
    char **comment
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp;

	if (ep <= 0 || ep > db->tail)
	    return (-1);
	else
	    itp = REF_EP(db,ep);

	if (keyword)
	    *keyword = itp->name + db->sbuf;
	if (value)
	    *value = itp->value + db->sbuf;
	if (type)
	    *type = itp->type + db->sbuf;
	if (comment)
	    *comment = itp->comment + db->sbuf;

	return (itp->name + itp->value + itp->type + itp->comment);
}


/* KWDB_KWNAME -- Return a pointer to the name of a keyword given its entry
 * pointer.
 */
char *
kwdb_KWName (
    pointer kwdb,
    int ep
)
{
	register KWDB *db = (KWDB *) kwdb;
	register Item *itp;

	if (ep <= 0 || ep > db->tail)
	    return (NULL);
	else
	    itp = REF_EP(db,ep);

	return (itp->name + db->sbuf);
}


/*
 * Utility routines.
 * -------------------
 */


/* KWDB_OPENFITS -- Open a FITS file and read the file header into a KWDB
 * keyword database.  Each 80 character card image in the input file 
 * corresponds to one entry in the output KWDB.  Values of the type field
 * generated are L (logical), S (string), N (numeric), H (history),
 * C (comment), T (text other than history or comment), and B (blank).
 * Any trailing blank lines at the end of the header are omitted, but a
 * count of the number of blank trailers can be returned as an argument if
 * desired.  The KWDB pointer is returned as the function value.
 */
pointer
kwdb_OpenFITS (
    char *filename,	/* file to be opened */
    int maxcards,	/* maximum FITS cards to be read */
    int *nblank		/* if not NULL receives count of blank lines at end */
)
{
	register KWDB *kwdb;
	int fd;

	/* Open FITS file. */
	if ((fd = open (filename, O_RDONLY)) < 0)
	    return (NULL);

	/* Open new, empty KWDB. */
	if (!(kwdb = (KWDB *) kwdb_Open (filename))) {
	    close (fd);
	    return (NULL);
	}

	/* Scan the file into the KWDB. */
	if (kwdb_ReadFITS (kwdb, fd, maxcards, nblank) < 0) {
	    close (fd);
	    return (NULL);
	}

	close (fd);
	return ((pointer) kwdb);
}


/* KWDB_READFITS -- Scan a file stream and load successive FITS cards into
 * a keyword database.  Each 80 character card image in the input file 
 * corresponds to one entry in the output KWDB.  Values of the type field
 * generated are L (logical), S (string), N (numeric), H (history),
 * C (comment), T (text other than history or comment), and B (blank).
 * Any trailing blank lines at the end of the header are omitted, but a
 * count of the number of blank trailers can be returned as an argument if
 * desired.  A count of the number of cards read is returned as the function
 * value, or -1 if an error occurs.
 */
int
kwdb_ReadFITS (
    pointer kwdb,
    int fd,
    int maxcards,
    int *nblank
)
{
	register KWDB *db = (KWDB *) kwdb;
	register char *ip, *op;
	char keyword[9], value[80], type[2], comment[80];
	int ncards, istext, isstring, iscomment, ishistory, ep, nb, n;
	char card[80], *lop;

	for (ncards=0;
	    (!maxcards || ncards < maxcards) && db->read(fd,card,80)==80;
	    ncards++) {

	    istext = isstring = 0;

	    /* Get keyword field (if any). */
	    for (ip=card, n=8, op=keyword;  --n >= 0 && *ip != ' ';  )
		*op++ = *ip++;
	    *op = '\0';

	    /* Quit when we see the END card, or if we miss the END card and
	     * run into some binary data.
	     */
	    for (ip=card, n=8;  --n >= 0;  ip++)
		if (!isprint (*ip)) {
		    strcpy (keyword, "END     ");
		    break;
		}
	    if (streq (keyword, "END"))
		break;

	    iscomment = (streq (keyword, "COMMENT"));
	    ishistory = (streq (keyword, "HISTORY"));

	    /* Get value field if any. */
	    if (!iscomment && !ishistory && card[8] == '=') {
		istext = 0;
		op = value;

		for (ip=card+9, n=21;  --n >= 0 && isspace(*ip);  )
		    ip++;
		if (*ip == '\'') {
		    ip++;  isstring = 1;
		    while (*ip != '\'' && ip < card+80)
			*op++ = *ip++;
		    ip++;
		} else {
		    while (*ip != ' ' && *ip != '/' && ip < card+80)
			*op++ = *ip++;
		}
		*op = '\0';

		/* Get comment field if any.  Trailing whitespace is trimmed.
		 */
		while ((*ip == '/' || *ip == ' ') && ip < card+80)
		    ip++;
		for (op=lop=comment;  ip < card+80 && *ip != '\n';  ip++) {
		    *op++ = *ip;
		    if (*ip != ' ')
			lop = op;
		}
		*lop = '\0';
	    } else {
		istext = 1;

		/* Process history or comment (text) card.  The text is placed
		 * in the value field.  Trailing whitespace is trimmed.
		 */
		op = lop = value;
		for (ip=card+8;  ip < card+80 && *ip != '\n';  ip++) {
		    *op++ = *ip;
		    if (*ip != ' ')
			lop = op;
		}
		*lop = '\0';
	    }

	    /* Determine the card type. */
	    if (istext) {
		if (iscomment)
		    strcpy (type, "C");
		else if (ishistory)
		    strcpy (type, "H");
		else
		    strcpy (type, "T");
	    } else {
		if (isstring)
		    strcpy (type, "S");
		else if (streq(value,"T") || streq(value,"F"))
		    strcpy (type, "L");
		else
		    strcpy (type, "N");
	    }

	    /* Enter the card into the KWDB. */
	    if (kwdb_AddEntry(kwdb, keyword, value, type, comment) < 0) {
		return (-1);
	    }
	}

	/* Trim any blank lines at the end of the header list. */
	for (ep=kwdb_Tail(kwdb), nb=0;  ep > 0;  ep=kwdb_Tail(kwdb), nb++) {
	    char *keyword, *value, *type;
	    if (kwdb_GetEntry (kwdb, ep, &keyword, &value, &type, NULL) >= 0) {
		if (*type == 'T' && *value == '\0') {
		    kwdb_DeleteEntry (kwdb, ep);
		} else
		    break;
            }
	}
	if (nblank)
	    *nblank = nb;

	return (ncards);
}


/* KWDB_UPDATEFITS -- Update the contents of a FITS file header from a KWDB
 * database to a FITS header.  If update=0 a new FITS file is written
 * consisting of only a header (from the KWDB) and no data.  If update=1 the
 * header of an existing file is updated in place, replacing the entire header
 * with the contents of the KWDB.  If extend=1 in update mode, the file will
 * automatically be extended if the KWDB will not fit in the existing header
 * area.
 */
int
kwdb_UpdateFITS (
    register KWDB *kwdb,
    char *filename,
    int update,
    int extend,
    int npad
)
{
	register char *ip, *op, *cp;
	int nblocks, nbytes, lastone, maxcards, new, fd, i;
	char card[256], tmpfile[512];
	char block[SZ_FILEBUF];
	char *lop;

	/* Prepare the output file.  If we are updating an existing file it
	 * must exist and must have sufficient space to hold the contents of
	 * the KWDB.  If we are writing to a new file we must be able to
	 * create the file.
	 */
	if (update) {
	    /* Update an existing file.
	     */
	    if ((fd = open (filename, O_RDWR)) < 0)
		return (-1);

	    /* Determine the capacity of the file header. */
	    lastone = 0;
	    maxcards = 0;
	    while (!lastone && read (fd, block, 2880) == 2880) {
		for (i=0, cp=block;  i < 36;  i++, cp += 80) {
		    cp[8] = '\0';
		    if (streq (cp, "END")) {
			lastone++;
			break;
		    }
		}
		maxcards += (lastone ? 35 : 36);
	    }

	    /* Extend the file if there is insufficient space and extension
	     * is requested.
	     */
	    if (kwdb_Len(kwdb) > maxcards) {
		if (!extend) {
		    close (fd);
		    return (-1);
		}

		/* Get a scratch file. */
		for (ip=filename, op=lop=tmpfile;  *ip;  ip++)
		    if ((*op++ = *ip) == '/')
			lop = op;
		for (ip="kwdb.XXXXXX", op=lop; (*op++ = *ip++);  )
		    ;
		*op = '\0';
		if (!mkstemp(tmpfile)) {
		    close (fd);
		    return (-1);
		}
		if ((new = open (tmpfile, O_CREAT|O_TRUNC|O_RDWR, 0644)) < 0) {
		    close (fd);
		    return (-1);
		}

		/* Write the header area of the new file. */
		nblocks = (kwdb_Len(kwdb) + npad + 1) + 35 / 36;
		memset (block, 0, 2880);
		while (--nblocks >= 0)
		    if (write (new, block, 2880) != 2880) {
abort:			close (fd);
			close (new);
			unlink (tmpfile);
			return (-1);
		    }

		/* Copy the data area of the target file. */
		while ((nbytes = read (fd, block, SZ_FILEBUF)) > 0)
		    if (write (new, block, nbytes) != nbytes)
			goto abort;

		/* Replace the original file with the extended version. */
		if (close (new) < 0)
		    goto abort;
		if (close (fd) < 0)
		    goto abort;
		if (rename (tmpfile, filename) < 0)
		    goto abort;

		/* Ready the newly extended file for header updating. */
		if ((fd = open (filename, O_RDWR)) < 0)
		    return (-1);

	    } else
		lseek (fd, 0L, SEEK_SET);

	} else {
	    /* Write a new file.
	     */
	    if ((fd = open (filename, O_CREAT|O_WRONLY, 0644)) < 0)
		return (-1);
	}

	/* Output the KWDB as a FITS file header.
	 */
	kwdb_SetIO (kwdb, read, write);
	if (kwdb_WriteFITS (kwdb, fd) < 0) {
	    close (fd);
	    return (-1);
	}

	/* Write the END card to mark the end of the header.
	 */
	strcpy (card, "END");
	memset (card+3, ' ', 80-3);
	if (write (fd, card, 80) != 80) {
	    close (fd);
	    return (-1);
	}

	return (close (fd));
}


/* KWDB_WRITEFITS -- Write the contents of a KWDB database to a file stream.
 * The KWDB entries are written out one per FITS card without any checks.
 * No END card is written.  WriteFITS may be called to write all or part of
 * a FITS header.
 */
int
kwdb_WriteFITS (
    KWDB *kwdb,
    int fd
)
{
	register KWDB *db = (KWDB *) kwdb;
	register char *ip, *op;
	int ncards=0, ep, n, ch;
	char card[256];

	for (ep=kwdb_Head(kwdb);  ep;  ep=kwdb_Next(kwdb,ep), ncards++) {
	    char *keyword, *value, *type, *comment;

	    /* Get entry from database. */
	    if ((kwdb_GetEntry(kwdb,ep,&keyword,&value,&type,&comment)) < 0)
		return (-1);

	    /* Format the FITS card. */
	    memset (card, ' ', 80);
	    for (ip=keyword, op=card, n=8;  *ip && --n >= 0;  ) {
		ch = *ip++;
		if (isprint (ch))
		    *op++ = ch;
	    }

	    switch (*type) {
	    case 'S':
	    case 'N':
	    case 'L':
		card[8] = '=';
		op = card + 10;

		/* Output value field. */
		if (*type == 'S') {
		    *op++ = '\'';
		    for (ip=value;  *ip && op < card+80;  ) {
			ch = *ip++;
			if (isprint (ch))
			    *op++ = ch;
		    }
		    while (op < card+19)
			op++;
		    *op++ = '\'';
		    while (op < card+30)
			op++;
		} else {
		    if ((n = 20 - strlen(value)) > 0)
			op += n;
		    for (ip=value;  *ip && op < card+80;  ) {
			ch = *ip++;
			if (isprint (ch))
			    *op++ = ch;
		    }
		}

		/* Output comment. */
		if (*comment && op < card+80) {
		    op++;  *op++ = '/';  op++;
		    for (ip=comment;  *ip && op < card+80;  ) {
			ch = *ip++;
			if (isprint (ch))
			    *op++ = ch;
		    }
		}
		break;

	    default:
		for (ip=value, op=card+8;  *ip && op < card+80;  )
		    *op++ = *ip++;
		break;
	    }

	    /* Write card to the output file. */
	    if (db->write (fd, card, 80) != 80)
		return (-1);
	}

	return (ncards);
}


/* KWDB_SETIO -- Set the read and write functions to be used by kwdb_ReadFITS
 * and kwdb_WriteFITS.
 */
void
kwdb_SetIO (
    register KWDB *kwdb,
    ssize_t (*readfcn)(),
    ssize_t (*writefcn)()
)
{
	register KWDB *db = (KWDB *) kwdb;
	db->read = readfcn;
	db->write = writefcn;
}


/*
 * Internal functions.
 * -------------------
 */

/* ADDSTR -- Add a string to the database string buffer.  The sbuf index of
 * the string is returned.  If the string is NULL or empty zero is returned.
 * Element zero of sbuf is always the null/empty string.
 */
static int
addstr (
    register KWDB *db,
    char *text
)
{
	int offset, sbuflen, nchars;
	char *sbuf;

	if (!text || !*text)
	    return (0);
	nchars = strlen(text) + 1;

	/* Get more space if the string buffer fills. */
	if (db->sbufused + nchars >= db->sbuflen) {
	    sbuflen = db->sbuflen + DEF_SZSBUF;
	    if (!(sbuf = (char *) realloc (db->sbuf, sbuflen)))
		return (0);
	    db->sbuf = sbuf;
	    db->sbuflen = sbuflen;
	}

	strcpy (db->sbuf + db->sbufused, text);
	offset = db->sbufused;
	db->sbufused += nchars;

	return (offset);
}


/* HASH -- Compute the (case insensitive) hash value of a string.
 */
static int
hash (char *key)
{
	register char *ip;
	register int sum, i, ch;

	for (ip=key, sum=i=0;  (ch = *ip) && i < MAX_HASHCHARS;  i++)
	    sum += (isupper(ch) ? tolower(ch) : ch) * primes[i];

	return (sum % NTHREADS);
}


/* STREQ -- Case insensitive string compare.
 */
static int
streq (
    register char *s1,
    register char *s2
)
{
	register int c1, c2;

	for (;;) {
	    c1 = *s1++;
	    c2 = *s2++;
	    if (!c1 || !c2)
		break;

	    if (isupper(c1))
		c1 = tolower(c1);
	    if (isupper(c2))
		c2 = tolower(c2);
	    if (c1 != c2)
		break;
	}

	return (!c1 && !c2);
}
