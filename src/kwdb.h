/*
 * KWDB.H -- KWDB global definitions.
 */
typedef void *pointer;

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
