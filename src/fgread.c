/* Copyright(c) 1986 Association of Universities for Research in Astronomy Inc.
 */

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <pwd.h>
#include <utime.h>
/* #include <time.h> */

#ifdef SYSV
#include <time.h>
#else
#include <sys/time.h>
#include <sys/timeb.h>
#endif

#if defined(MACOSX) || defined(__linux)
#include <time.h>
#endif

#include "kwdb.h"
static  long get_timezone();
/*
 * FGREAD -- Read a MEF FITS file created by fgwrite.
 *
 * Switches:
 *		d	print debug messages
 *		f	read from named file rather than stdin
 *		n	get list of extension numbers to extract
 *		o	omit the indicated FITS types (tbdsfm)
 *			   t: text, b: binary, d: directory, s: symlink,
 *			   f: FITS, m: FITS-MEF.
 *		r	replace existing file at extraction
 *		s       check CHECKSUM if keywords are present
 *		t	include the indicated FITS types (tbdsfm) only
 *		v	verbose; print full description of each file
 *		x	extract files
 *
 * Usage: "fgread [-t <tbdsfm>] [-o <tbdsfm>] [-n ranges] [-vdxrf] 
 *			[-f fitsfile [files]".
 *	   where 'ranges' is of the form: 2,4,7-9,12-14,23 (no spaces
 *	   allow in between).
 *
 *	fgread -xvf fgfile.fits log12.txt kp01.fits rf_mos.hd
 *
 * would extract the listed files from input gffile.fits. If the list is
 * empty, all the FITS extensions from the input file will be extracted.
 *
 *	fgread -n 2,5,9-12,16 -xvf fgfile.fits
 *
 * would extract the FITS extension numbers indicated in the list. The list
 * should not have imbedded spaces.
 */

#define FBLOCK		2880
#define NBLOCK		20
#define NAMSIZ		100
#define CARDLEN		80
#define SZ_OWNERSTR     48
#define SLEN            68
#define	MAXLINELEN	256
#define	SZ_BUFFER	(FBLOCK * NBLOCK)
#define	EOS		'\0'
#define	ERR		(-1)
#define	OK		0
#define MAXENTRIES      500
#define	YES		1
#define NO		0
#define BYTELEN		8

#define	LF_LINK		1
#define	LF_SYMLINK	2
#define	LF_BIN		3
#define	LF_TXT		4
#define	LF_DIR		5
#define	FITS		6
#define	FITS_MEF	7
#define OTHER		8


/* Decoded file header.
 */
struct fheader {
	char	name[NAMSIZ];
	int	mode;
	int	uid;
	int	gid;
	int	isdir;
	int	dirlevel;
	long	size;
	long	hsize;
	long	mtime;
	long	chksum;
	int	linkflag;
	char	linkname[NAMSIZ];
};

struct _modebits {
	int	code;
	char	ch;
} modebits[] = {
	0400,	'r',
	0200,	'w',
	0100,	'x',
	040,	'r',
	020,	'w',
	010,	'x',
	04,	'r',
	02,	'w',
	01,	'x',
	0,	0
};


static int debug;		/* Print debugging messages		*/
static int extract;		/* Extract files from the tape		*/
static int replace;		/* Replace existing files		*/
static int printfnames;		/* Print file names			*/
static int verbose;		/* Print everything			*/

static int eof;
static int nerrs;
static char *file_list;
static int nblocks;
static int dlevel;		/* Directory level (1 == top) */
int	sums = NO;		/* Do not do checksum */
int	count;

int	omittxt=NO;		/* omit text files	*/
int	omitbin=NO;		/* omit binary files    */
int	omitdir=NO;		/* omit directory files	*/
int	omitsymlink=NO;		/* omit symbolic links 	*/
int	omitfits=NO;		/* omit FITS files 	*/
int	omitfitsmef=NO;		/* omit FITS-MEF files 	*/

unsigned short sum16;
unsigned int   sum32;

/* MAIN -- "rtar [xtvlef] [names]".  The default operation is to extract all
 * files from the tar format standard input in quiet mode.
 */
main (argc, argv)
int	argc;
char	*argv[];
{
	struct	fheader fh;
	char	**argp;
	char	*s, *ip, *p, type[20];
	char	ascii[161];
	int	in = 0, out, omitx;
	int	ftype, ch, ncards, xarr[100], xcount;
	int	epos,in_off,nbytes;
	pointer kwdb;

	int	stat, list;

	debug		= 0;
	extract		= 0;
	replace		= 0;
	printfnames	= 0;
	verbose		= 0;
	omitx		= NO;
	xarr[1]		= -1;

	/* Get parameters.  Argp is left pointing at the list of files to be
	 * extracted (default all if no files named).
	 */
	argp = &argv[1];
	if (argc <= 1)
	    extract++;
	else {
	    while (*argp && **argp == '-') {
		ip = *argp++ + 1;
		while ((ch = *ip++) != EOS) {
		    switch (ch) {
		    case 'x':
			extract++;
			break;
		    case 'd':
			debug++;
			break;
		    case 'r':
			replace++;
			break;
		    case 's':
			sums = YES;
			break;
		    case 'v':
			printfnames++;
			verbose++;
			break;
		    case 'o':		/* Omit filetypes */
			omitx = YES;
	 	        for (p = *argp;  *p != EOS;  p++) {
			    if (*p == 't')
				omittxt = YES;
			    if (*p == 'b')
				omitbin = YES;
			    if (*p == 'd')
				omitdir = YES;
			    if (*p == 's')
				omitsymlink = YES;
			    if (*p == 'f')
				omitfits = YES;
			    if (*p == 'm')
				omitfitsmef = YES;
			}
			*argp++;
			break;
		    case 't':		/* Include filetypes */
			omittxt = YES;
			omitbin = YES;
			omitdir = YES;
			omitsymlink = YES;
			omitfits = YES;
			omitfitsmef = YES;

			omitx = YES;
	 	        for (p = *argp;  *p != EOS;  p++) {
			    if (*p == 't')
				omittxt = NO;
			    if (*p == 'b')
				omitbin = NO;
			    if (*p == 'd')
				omitdir = NO;
			    if (*p == 's')
				omitsymlink = NO;
			    if (*p == 'f')
				omitfits = NO;
			    if (*p == 'm')
				omitfitsmef = NO;
			}
			*argp++;
			break;
		    case 'n':
			/* Get list of extension numbers to read */
			get_range (*argp, xarr);
			*argp++;
			break;
		    case 'f':
			if (*argp == NULL) {
			    fprintf (stderr, "missing filename argument\n");
			    exit (1);
			}
			in = open (*argp, 0);
			if (in == ERR) {
			    fprintf (stderr, "cannot open `%s'\n", *argp);
			    exit (1);
			}
			argp++;
			break;
		    default:
			fprintf (stderr, "Warning: unknown switch `%c'\n", ch);
			fflush (stderr);
			exit (1);
		    }
		}
	    }
	}
	/* read PHU first */
	kwdb = kwdb_Open("PHU");
	ncards = kwdb_ReadFITS (kwdb, in, MAXENTRIES, NULL);

	if (kwdb_Lookup (kwdb, "SIMPLE",0) == 0) {
	    /* We have a FITS file with no PHU, rewind */
	    lseek (in, 0L, SEEK_SET);
	}
	kwdb_Close (kwdb);

	file_list = *argp++;
	list = NO;
	if (file_list != NULL)
	    list = YES;

	/* Step along through the FG FITS file.  Read file header and if
	 * extension is in list and extraction is enabled, extract extension.
	 */
	dlevel = 1;
	xcount = 0;
	count = 0;
        for (;;) {
	    count++;

	    kwdb = kwdb_Open("EHU");
	    stat = getheader (in, &fh, &ftype, kwdb, type); 
	    if (stat == EOF) {
	        kwdb_Close (kwdb);
		break;
	    }

	    /* See if we need to omit types of extensions */
	    if (omitx) {
		if (omit_ftype (ftype) == YES) {
		    skipfile (in, &fh, kwdb);
	            kwdb_Close (kwdb);
		    if (xarr[1] > 0)
			xcount++;
		    continue;
		}
	    }
	    /* If there is a file list, look for it */
	    if (xarr[1] > 0) {
		if (xarr[xcount] < 0) break;
		if (xarr[xcount] != count) {
		    skipfile (in, &fh, kwdb);
	            kwdb_Close (kwdb);
		    continue;
		}
		xcount++;
	    } else if (file_list != NULL) {
		if (strcmp (fh.name, file_list) != 0) {
		    skipfile (in, &fh, kwdb);
	            kwdb_Close (kwdb);
		    continue;
		}
		file_list = *argp++;
	    } 
	    if (printfnames) {
		printheader (stdout, &fh, type);
		fflush (stdout);
	    }


	    if (ftype == LF_SYMLINK) {
		if (extract) {
		    if (replace)
			unlink (fh.name);
		    if (symlink (fh.linkname, fh.name) != 0) {
			fprintf (stderr,
			    "Cannot make symbolic link %s -> %s\n",
			    fh.name, fh.linkname);
		    }
		}
		continue;
	    }

	    if (extract) {
		out = newfile (fh.name, &fh, ftype);
		if (out == ERR) {
		    fprintf (stderr, "cannot create file `%s'\n", fh.name);
		    skipfile (in, &fh, kwdb);
		    continue;
		}
		if (!fh.isdir) {
		    if (ftype == FITS || ftype == FITS_MEF)
			copyheader (out, kwdb);
		    copyfile (in, out, &fh, ftype);
		    close (out);
		    if (sum32 > 0 && sums == YES) {
		        if (debug) 
			    printf("CHECKSUM: %d\n",sum32);
		        if (sum32 != -1 && ftype != FITS_MEF) { 
			    fprintf (stderr,
			       "**** Checksum error in extension %d (%s)\n",
			       count, fh.name);
		        }
		    }
		}
		chown (fh.name, fh.uid, fh.gid);

		/* set file mtime */
		{ struct utimbuf times;
		    times.actime = 0L;
		    times.modtime = fh.mtime;
		    utime (fh.name, &times);
		}
	    } else
		skipfile (in, &fh, kwdb);

	    kwdb_Close (kwdb);
	    if (list == YES && file_list == NULL)
		break;
	} 

	if (in)
	    close (in);

	exit(0);
}


/* GETHEADER -- Read the next file block and attempt to interpret it as a
 * file header.  A checksum error on the file header is fatal and usually
 * indicates that the file was not properly transferred.
 */
getheader (in, fh, ftype, kwdb, type)
int	in;			/* input file			*/
register struct	fheader *fh;	/* decoded file header (output)	*/
int	*ftype;			/* file type			*/
pointer  kwdb;
char	 *type;			/* Extension type */
{
	register char *ip, *op;
	register int n;
	char    smode[SLEN];
	int	ntrys, ncards, hsize, hpos, in_off;
	int	nbh, bks, i, recsize;
	char	record[FBLOCK*NBLOCK];
	char    *s, *p;
	register struct	_modebits *mp;
	struct tm tm;

	int   mode=0;
	/* get the current file position  */
	hpos = lseek (in, 0, SEEK_CUR); 

	ncards = kwdb_ReadFITS (kwdb, in, MAXENTRIES, NULL);
	if (ncards == 0)
	    return(EOF);
	
	sum16 = 0;
	sum32 = 0;
	fh->isdir = 0;
	fh->linkflag = 0;
	fh->hsize = 0;
	s = kwdb_GetValue (kwdb, "FG_FTYPE");	
	/* We could be reading a header from an IMAGE extension and
	 * we will not have any of the FG keywords.
	 */
        if (s == NULL) {
	    fh->name[0] = EOS;
	    return (FBLOCK);		
        }
	if (strncmp (s, "text", 4) ==  0)
  	    *ftype = LF_TXT;
	else if (strncmp (s, "symlink", 7) ==  0)
  	    *ftype = LF_SYMLINK;
	else if (strncmp (s, "binary", 6) ==  0)
  	    *ftype = LF_BIN;
	else if (strncmp (s, "directory", 9) ==  0) {
  	    *ftype = LF_DIR;
	    fh->isdir = 1;
	} else if (strncmp (s, "FITS-MEF", 8) ==  0)
  	    *ftype = FITS_MEF;
	else if (strncmp (s, "FITS", 4) ==  0)
  	    *ftype = FITS;
	else if (strncmp (s, "other", 5) ==  0)
  	    *ftype = OTHER;

	strcpy (type, s);
	
	s = kwdb_GetValue (kwdb, "FG_LEVEL");	
	fh->dirlevel = atoi(s);

	s = kwdb_GetValue (kwdb, "FG_FNAME");	
	if ((p=strchr(s,' ')) != NULL)
	    *p = '\0';
	strcpy (fh->name, s);
	if (*ftype == LF_SYMLINK) {
	    strcpy (fh->name, s);
	    strcpy (fh->linkname, p+4);
	    fh->linkflag = 1;
	}
	    
	s = kwdb_GetValue (kwdb, "FG_FSIZE");	
	fh->size = atoi(s);
	if (*ftype == FITS || *ftype == FITS_MEF) {
	    /* reduce the size by the size of the FITS header 
	     * since for FITS we need to write the header out and 
	     * its size is already included in fh.size.
	     */
	    hsize = ((ncards+35)/36)*2880;
	    fh->hsize = hsize;
	}

	s = kwdb_GetValue (kwdb, "FG_FMODE");	
	s++;
	for (mp=modebits;  mp->code;  mp++)
	    mode = mp->ch == *s++ ? mp->code | mode : mode;

	fh->mode = mode;

	s = kwdb_GetValue (kwdb, "FG_MTIME");	

	strptime (s, "%Y-%m-%dT%T",&tm);
	fh->mtime = mktime(&tm) - get_timezone();

	s = kwdb_GetValue (kwdb, "FG_FUOWN");	
	get_uid (s, fh);
	s = kwdb_GetValue (kwdb, "FG_FUGRP");	
	get_gid (s, fh);

	in_off = lseek (in, 0, SEEK_CUR); 
	/* Calculate header checksum only if the CHECKSUM keyword exists */
	if (kwdb_GetValue (kwdb, "CHECKSUM") != NULL && sums == YES) {
	    in_off = lseek (in, hpos, SEEK_SET); 
	    nbh = (ncards + 1 + 35)/36;    /* Fblocks of header */
	    bks = nbh/NBLOCK;
	    for (i=1; i<=bks; i++) {
		recsize = read (in, record, FBLOCK*NBLOCK);
		checksum (record, recsize, &sum16, &sum32);
	    }
	    if (nbh % NBLOCK != 0) {
		recsize = read (in, record, (nbh % 10)*FBLOCK);
		checksum (record, recsize, &sum16, &sum32);
	    }			
	}	
	return (FBLOCK);
}

/* GET_UID -- Get the uid for a user name .
*/ 
get_uid (s, fh)
char	*s;
register struct	fheader *fh;	/* decoded file header (output)	*/
{
	static      int uid;
	static      char owner[SZ_OWNERSTR+1]={'\0'};
	struct      passwd *pw;
	char 	    *ip;

	/* if the input file user name is not knowm get
	 * the current process uid.
	 */
	ip = strchr(s, ' ');
	if (ip != NULL)
	    *ip = '\0';

	if (!strncmp(s, "<unknown>", 9)) {
	    fh->uid = getuid();
	    return (0);
	}

	if (!strcmp(s,owner)) {;
	    fh->uid = uid;
	    return (0);
	} else {
	    /* setpwent();  */
	    pw = getpwnam (s);
	    /* endpwent(); */
	    if (pw == NULL)
	        fh->uid = getuid();
	    else {
		strncpy (owner, s, SZ_OWNERSTR);
	        fh->uid = pw->pw_uid;
		uid = pw->pw_uid;
	    }
	}
}

/* GET_GID -- Get the gid for a user name .
*/ 
get_gid (s, fh)
char	*s;
register struct	fheader *fh;	/* decoded file header (output)	*/
{
	static      int gid;
	static      char owner[SZ_OWNERSTR+1]={'\0'};
	struct      passwd *pw;
	char	    *ip;

	/* if the input file user name is not knowm get
	 * the current process uid.
	 */
	ip = strchr(s, ' ');
	if (ip != NULL)
	    *ip = '\0';

	if (!strncmp(s, "<unknown>", 9)) {
	    fh->uid = getgid();
	    return (0);
	}

	if (!strcmp(s,owner)) {;
	    fh->gid = gid;
	    return (0);
	} else {
	    /* setpwent();  */
	    pw = getpwnam (s);
	    /* endpwent(); */
	    if (pw == NULL)
	        fh->gid = getgid();
	    else {
		strncpy (owner, s, SZ_OWNERSTR);
	        fh->gid = pw->pw_gid;
	        gid = pw->pw_gid;
	    }
	}
}

/*
struct _modebits {
	int	code;
	char	ch;
} modebits[] = {
	040000,	'd',
	0400,	'r',
	0200,	'w',
	0100,	'x',
	040,	'r',
	020,	'w',
	010,	'x',
	04,	'r',
	02,	'w',
	01,	'x',
	0,	0
};
*/

/* PRINTHEADER -- Print the file header in either short or long (verbose)
 * format, e.g.:
 *		drwxr-xr-x  9 tody         1024 Nov  3 17:53 .
 */
printheader (out, fh, type)
FILE	*out;			/* output file			*/
register struct fheader *fh;	/* file header struct		*/
char    *type;			/* Foreign extension type, (bin, text..) */
{
	char	*tp, *ctime();

	tp = ctime (&fh->mtime);

	fprintf (out, "%-4d %-10.10s %9d %-12.12s %-4.4s %s",
	    count,type, fh->size, tp + 4, tp + 20, fh->name);
	if (fh->linkflag && *fh->linkname) {
	    fprintf (out, " -> %s ", fh->linkname);
	}
	fprintf(out, "\n");
}


/* NEWFILE -- Try to open a new file for writing, creating the new file
 * with the mode bits given.  Create all directories leading to the file if
 * necessary (and possible).
 */
newfile (fname, fh, ftype)
char	*fname;			/* pathname of file		*/
register struct fheader *fh;	/* file header struct		*/
int	ftype;			/* text, binary, director, etc	*/
{
	int	fd;
	char	*cp, *cwd, dirname[MAXLINELEN];
	int	i, mode, fdirlevel;
	
	mode = fh->mode;
	fdirlevel = fh->dirlevel;	
	if (debug)
	    fprintf (stderr, "newfile `%s':\n", fname);

	if (ftype == LF_DIR && fdirlevel >= 1) {
	    if (fdirlevel < dlevel) {
		chdir ("../");
	        dlevel = fdirlevel;
            }
	    /* See if directory has been created */
	    cwd = getcwd (dirname, MAXLINELEN);
	    strcat (dirname, "/");
	    strcat (dirname, fname);
	    if (access (dirname, F_OK)) {
		/* directory does not exist, create */
	        fd = mkdir (fname, mode);
		fd = OK;
	        chdir (fname);
		dlevel++;
	    }
/*	    if (dlevel != fdirlevel || dlevel == 1) {   
	        chdir (fname);
		dlevel++;
printf("chdir to '%s', fdirlevel: %d, dlevel: %d\n", fname, fdirlevel, dlevel);
*/
            
	   /* dlevel = fdirlevel; */
	} else if (fdirlevel < dlevel) {
	    /* we need to travel upwards */
	    for (i=fdirlevel; i < dlevel; i++)
		chdir ("../");
	    dlevel = fdirlevel;
	}

	if (ftype != LF_DIR) {
	    if (replace)
		unlink (fname);
	    fd = creat (fname, mode);
	}
	return (fd);
}

/* COPYHEADER -- Copy the PHU of a FITS or FITS-MEF extension from a 
 * file created by fgwrite. We need to get rid of the FG_ keywords
 * before leaving.
 */
copyheader (out, kwdb)
int	out;			/* output file			*/
pointer kwdb;
{
	int ep, ncards, hdr_off, i;
	char	card[CARDLEN];
	
	/* Change extension to SIMPLE */
	ep = kwdb_Lookup (kwdb, "XTENSION", 0);
	kwdb_RenameEntry (kwdb, ep, "SIMPLE");
	kwdb_SetType (kwdb, "SIMPLE", "L");
	kwdb_SetValue (kwdb, "SIMPLE", "T");

	ep = kwdb_Lookup (kwdb, "FG_GROUP", 0);
	kwdb_DeleteEntry (kwdb, ep);
	ep = kwdb_Lookup (kwdb, "FG_FNAME", 0);
	kwdb_DeleteEntry (kwdb, ep);
	ep = kwdb_Lookup (kwdb, "FG_FTYPE", 0);
	kwdb_DeleteEntry (kwdb, ep);
	ep = kwdb_Lookup (kwdb, "FG_LEVEL", 0);
	kwdb_DeleteEntry (kwdb, ep);
	ep = kwdb_Lookup (kwdb, "FG_FSIZE", 0);
	kwdb_DeleteEntry (kwdb, ep);
	ep = kwdb_Lookup (kwdb, "FG_FMODE", 0);
	kwdb_DeleteEntry (kwdb, ep);
	ep = kwdb_Lookup (kwdb, "FG_FUOWN", 0);
	kwdb_DeleteEntry (kwdb, ep);
	ep = kwdb_Lookup (kwdb, "FG_FUGRP", 0);
	kwdb_DeleteEntry (kwdb, ep);
	ep = kwdb_Lookup (kwdb, "FG_CTIME", 0);
	kwdb_DeleteEntry (kwdb, ep);
	ep = kwdb_Lookup (kwdb, "FG_MTIME", 0);
	kwdb_DeleteEntry (kwdb, ep);

	/* Delete CHECKSUM set */
	ep = kwdb_Lookup (kwdb, "CHECKSUM", 0);
	kwdb_DeleteEntry (kwdb, ep);
	ep = kwdb_Lookup (kwdb, "DATASUM", 0);
	kwdb_DeleteEntry (kwdb, ep);
	ep = kwdb_Lookup (kwdb, "CHECKVER", 0);
	kwdb_DeleteEntry (kwdb, ep);

	ncards = kwdb_WriteFITS (kwdb, out);
	hdr_off = ((ncards + 1 + 35)/36)*36*80;
	memset (card, ' ', CARDLEN);
	for (i = (ncards+1) % 36;  i < 36;  i++)
	    write (out, card, CARDLEN);
				      
	strcpy (card, "END");
	memset (card+3, ' ', CARDLEN-3);
	write (out, card, CARDLEN);
}					 

/* COPYFILE -- Copy bytes from the input (tar) file to the output file.
 * Each file consists of a integral number of FBLOCK size blocks on the
 * input file.
 */
copyfile (in, out, fh, ftype)
int	in;			/* input file			*/
int	out;			/* output file			*/
struct	fheader *fh;		/* file header structure	*/
int	ftype;			/* text or binary file		*/
{
	long	nbytes, bsize;
	int	i, nblocks;
	char	buffer[SZ_BUFFER], *p;

	/* Link files are zero length on the tape. */
	if (fh->linkflag)
	    return (0);
	
	/* hsize is different from zero for FITS(-MEF) ftype only.
	 * Also we copy the entire MEF file since its size
	 * information is given in FG_FSIZE.
	 */
	nblocks = (fh->size - fh->hsize)/SZ_BUFFER;
	for (i=1; i <= nblocks; i++) {
	    nbytes = read (in, buffer, SZ_BUFFER);
	    if (sum32 > 0 && sums == YES) 
		checksum (buffer, nbytes, &sum16, &sum32);
	    if (write (out, buffer, nbytes) == ERR) {
		fprintf (stderr, "Warning: file write error on `%s'\n",
		    fh->name);
		    exit (1);
	    }
	}
	/* read the remainder in unit of FBLOCK to leave the file
	 * pointer correctly positioned for the next unit read.
	 */
	bsize = (fh->size - fh->hsize) % SZ_BUFFER;
	nbytes = ((bsize + 2879)/2880)*2880;
	if (bsize != 0) {
	    nbytes = read (in, buffer, nbytes);
	    if (sum32 > 0 && sums == YES)
		checksum (buffer, nbytes, &sum16, &sum32);
	    if (write (out, buffer, bsize) == ERR) {
		fprintf (stderr, "Warning: file write error on `%s'\n",
		    fh->name);
	    }
	}

}


/* SKIPFILE -- Skip the current FITS unit up to the beginning of the
 * next.
 */
skipfile (in, fh, kwdb)
int	in;			/* input file			*/
struct	fheader *fh;		/* file header			*/
pointer kwdb;
{
	int	datasize;
	int     in_off;

	in_off = lseek (in, 0, SEEK_CUR); 
	in_off = ((in_off + 2879)/2880)*2880;
	datasize = ((fh->size - fh->hsize + 2879)/2880)*2880;
	in_off = lseek (in, in_off+datasize, SEEK_SET); 
}


/* GET_RANGE -- Parse a string with a list of ranges; e.g. 1,4,5-8,12
 * and put the expanded values in the integer array.
 */
get_range (list, xarr)
char *list;
int  *xarr;
{
	int   i,j,k,l, n;
	char  *p;

	p= list;
	while (*p) {
	   if (isdigit(*p) ||
	       *p == ','   ||
	       *p == '-')
	       p++;
	   else {
	       fprintf (stderr,
	       "Only digits, '-' and ',' allowed in list.\n");
	       fflush (stderr);
	       exit (1);
	   }
	}
	p= list;
	j=0;
	for(;;) {
	   n = strtol(p, &p, 10);
	   if (*p == '\0') {
	       xarr[j++] = n;
	       break;
	   } else if (*p == '-') {
	       i=n;
	       n = strtol(++p, &p, 10);
	       k = n;
	       for(l=i; l<= k; l++)
		  xarr[j++] = l;
	       if (*p == '\0') break;
	       p++;
	   } else {
	       xarr[j++] = n;
	       p++; 
	   }

	}
	xarr[j] = -1;
}

/* OMIT_FTYPE --  Omit the FITS extension of type ftype */
int 
omit_ftype (ftype)
int	ftype;
{
	switch(ftype) {
	case LF_SYMLINK:
	    if (omitsymlink) return(YES);
	    break;
	case LF_BIN:
	    if (omitbin) return(YES);
	    break;
	case LF_TXT:
	    if (omittxt) return(YES);
	    break;
	case LF_DIR:
	    if (omitdir) return(YES);
	    break;
	case FITS:
	    if (omitfits) return(YES);
	    break;
	case FITS_MEF:
	    if (omitfitsmef) return(YES);
	    break;
	default:
	    return(NO);
	    break;
	}
	return(NO);
}


/* _TIMEZONE -- Get the local timezone, measured in seconds westward
 * from Greenwich, ignoring daylight savings time if in effect.
 */
static long
get_timezone()
{
#ifdef SYSV
        extern  long timezone;
        tzset();
        return (timezone);
#else
#ifdef MACOSX
        struct tm *tm;
        time_t clock = time(NULL);
        tm = localtime (&clock);
        return (-(tm->tm_gmtoff));
#else
        struct timeb time_info;
        ftime (&time_info);
        return (time_info.timezone * 60);
#endif
#endif
}

