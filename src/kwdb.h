/* Copyright(c) 1986 Association of Universities for Research in Astronomy Inc.
 */

/*
 * KWDB.H -- KWDB global definitions.
 */
typedef void *pointer;

int kwdb_AddEntry (/* kwdb, keyword, value, type, comment */);
void kwdb_Close (/* kwdb) */);
int kwdb_CopyEntry (/* kwdb, o_kwdb, o_ep) */);
int kwdb_DeleteEntry (/* kwdb, ep) */);
char *kwdb_GetComment (/* kwdb, keyword) */);
int kwdb_GetEntry (/* kwdb, ep, keyword, value, type, comment */);
char *kwdb_GetType (/* kwdb, keyword) */);
char *kwdb_GetValue (/* kwdb, keyword) */);
int kwdb_Head (/* kwdb) */);
int kwdb_Len (/* kwdb) */);
int kwdb_Lookup (/* kwdb, keyword) */);
char *kwdb_Name (/* kwdb) */);
char *kwdb_KWName (/* kwdb, ep) */);
int kwdb_Next (/* kwdb, ep */);
pointer kwdb_Open (/* kwdbname */);
int kwdb_SetComment (/* kwdb, keyword, comment */);
int kwdb_SetType (/* kwdb, keyword, type */);
int kwdb_SetValue (/* kwdb, keyword, value */);
int kwdb_Tail (/* kwdb */);

pointer kwdb_OpenFITS (/* filename, maxcards, nblank */);
int kwdb_UpdateFITS (/* kwdb, filename, update, extend, npad */);
int kwdb_ReadFITS (/* kwdb, fd, maxcards, nblank */);
int kwdb_WriteFITS (/* kwdb, fd */);

/* Compatibility garbage. */
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
