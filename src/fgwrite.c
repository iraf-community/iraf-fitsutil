/* Copyright(c) 1986 Association of Universities for Research in Astronomy Inc.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>

#include "kwdb.h"

/*
 * FGWRITE -- Write a MEF files with FOREIGN Xtension type.
 *
 * Switches:
 *		f	write to named file, otherwise write to stdout
 *		d	print debug messages
 *		v	verbose; print full description of each file
 *		g       FG_GROUP name. The defualt is the root directory name
 *		t	select filetypes to include in output file
 *		o	skip filestypes from input files selection
 *		h	do not produce PHU
 *	        i       write Table Of Content in PHU. 
 *		s       Calculate CHECKSUM and DATASUM for the input file.
 *
 * Usage: "fgwrite [-t <tbdsfm>] [-o <tbdsfm>] [-vdih] [-g <group_name>]
 *		   [-f output_fits_file] [input_files]".
 */

#define ERR		-1
#define YES		1
#define NO		0
#define EOS		'\0'

#define SZ_PATHNAME	511
#define FBLOCK		2880
#define SLEN            68
#define TOCLEN          70
#define CARDLEN		80
#define NBLOCK		20
#define BYTELEN		8
#define NAMSIZ		100
#define MAX_TOC		100
#define SZ_OWNERSTR     48
#define	MAXENTRIES      500	

#define KB		1024
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
	long	size;
	long	mtime;
	long	ctime;
	long	chksum;
	int	linkflag;
	char	linkname[NAMSIZ];
};

/* Map file mode bits into characters for printed output.
 */
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

int	debug=NO;		/* Print debugging messages		*/
int	omittxt=NO;		/* omit text files	*/
int	omitbin=NO;		/* omit binary files    */
int	omitdir=NO;		/* omit directory files	*/
int	omitsymlink=NO;		/* omit symbolic links 	*/
int	omitfits=NO;		/* omit FITS files 	*/
int	omitfitsmef=NO;		/* omit FITS-MEF files 	*/
int	verbose=NO;		/* Print everything	*/

int	in;
int	out = EOF;
int	count = 0;
int	maxcount;
int	toc;
int	sums = NO;
int     hdr_off;
char    *slines;

char	group[SLEN],*gname();
char	*dname();
static  char *str();

/* MAIN -- "fgwrite [-t <tbdlfm>] [-o <tbflfm>] [-vd] [-f fitsfile] [files]".
 * If no files are listed the
 * current directory tree is used as input.  If no output file is specified
 * output is to the standard output.
 */
main (argc, argv)
int	argc;
char	*argv[];
{
	static	char	*def_flist[1] = {NULL};
	char	*argp, **flist, *arg, *ip;
	pointer kwdb, kwtoc;
	char    card[256];
	char    *sline;
	int	argno, ftype, i, ncards, level, phu;

	flist       = def_flist;
	verbose     = debug;
	group[0] = EOS;
	phu = YES;
	toc = NO;

	if (debug) {
	    printf ("fgwrite called with %d arguments:", argc);
	    for (argno=1;  (argp = argv[argno]) != NULL;  argno++)
		printf (" %s", argp);
	    printf ("\n");
	}

	/* Process the argument list.
	 */
	for (argno=1;  (argp = argv[argno]) != NULL;  argno++) {
	    if (*argp != '-') {
		flist = &argv[argno];
		break;

	    } else {
		for (argp++;  *argp;  argp++) {
		    switch (*argp) {
		    case 'd':
			debug++;
			break;
		    case 'v':
			verbose++;
			break;
		    case 'h':
			phu = NO;
			break;
		    case 'i':
			toc = YES;
			break;
		    case 'g':		/* Get GROUP name */
			if (argv[argno+1])
			    strcpy(group, argv[++argno]);
			break;
		    case 's':
			sums = YES;
			break;
		    case 'o':		/* Omit filetypes */
			if (argv[argno+1])
			    arg = argv[++argno];
	  	        else
			    break;
	 	        for (ip = &arg[0];  *ip != EOS;  ip++) {
			    if (*ip == 't')
				omittxt = YES;
			    if (*ip == 'b')
				omitbin = YES;
			    if (*ip == 'd')
				omitdir = YES;
			    if (*ip == 's')
				omitsymlink = YES;
			    if (*ip == 'f')
				omitfits = YES;
			    if (*ip == 'm')
				omitfitsmef = YES;
			}
			break;
		    case 't':		/* Include filetypes */
			if (argv[argno+1])
			    arg = argv[++argno];
	  	        else
			    break;
			omittxt = YES;
			omitbin = YES;
			omitdir = YES;
			omitsymlink = YES;
			omitfits = YES;
			omitfitsmef = YES;

	 	        for (ip = &arg[0];  *ip != EOS;  ip++) {
			    if (*ip == 't')
				omittxt = NO;
			    if (*ip == 'b')
				omitbin = NO;
			    if (*ip == 'd')
				omitdir = NO;
			    if (*ip == 's')
				omitsymlink = NO;
			    if (*ip == 'f')
				omitfits = NO;
			    if (*ip == 'm')
				omitfitsmef = NO;
			}
			break;
		    case 'f':
			if (argv[argno+1]) {
			    argno++;
			    if (debug)
				printf ("open output file `%s'\n", argv[argno]);
			    out = open (argv[argno], O_RDWR|O_CREAT|O_TRUNC,
				0644);
			    if (out == ERR) {
				fflush (stdout);
				fprintf (stderr,
				    "cannot open `%s'\n", argv[argno]);
				exit (1);
			    }
			}
			break;

		    default:
			fflush (stdout);
			fprintf (stderr,
			    "Warning: unknown switch -%c\n", *argp);
			fflush (stderr);
		    }
		}
	    }
	}

	/* Write to the standard output if no output file specified.
	 * The filename "stdin" is reserved.
	 */
	if (out == ERR) {
	    verbose = 0;
	    if (debug)
		printf ("output defaults to stdout\n");
	    out = 1;
	}
	
	/* if no GROUP name */
	if (!group[0]) {
	    getcwd (card, SZ_PATHNAME);
	    strcpy(group, gname(card));
	}
	 
	/* Write toc only of phu is not deselected */
	if (phu == NO)
	     toc = NO;

	/* Create Table Of Contents */
	if (toc == YES) {
	    slines = (char *) calloc (MAX_TOC, TOCLEN);
	    ip = slines;
	    maxcount = MAX_TOC;
	    hdr_off = 2880;
	    level = 1;
	    /* Put each directory and file listed on the command line to 
	     * the fitsfile.
	     */
	    for (i=0;  (argp = flist[i]) != NULL;  i++)
		if ((ftype = filetype (argp)) == LF_DIR)
		    putfiles (argp, out, "", &level);
		else
		    fgfileout (argp, out, ftype, "", level);
	}

	if (phu == YES) {
	    /* Write PHU
	    */
	    if (!(kwdb = kwdb_Open ("PHU")))
		goto done;
	    kwdb_AddEntry (kwdb, "SIMPLE", "T", "L", 
		"File conforms to FITS standard");
	    kwdb_AddEntry (kwdb, "BITPIX", "8", "N", 
		"Bits per pixel (not used)");
	    kwdb_AddEntry (kwdb, "NAXIS",  "0", "N",
		"PHU contains no image matrix");
	    kwdb_AddEntry (kwdb, "EXTEND", "T", "L", 
		"File contains extensions");
	    kwdb_AddEntry (kwdb, "ORIGIN", 
		"NOAO Fgwrite utility May 1999", "S", "");

	    /* Now add the Table of Content to this PHU */
	    if (toc == YES) {
		list_toc (kwdb);
		free (ip);
	    }

	    ncards = kwdb_WriteFITS (kwdb, out);
	    hdr_off = ((ncards + 1 + 35)/36)*36*80;
	    memset (card, ' ', CARDLEN);
	    for (i = (ncards+1) % 36;  i < 36;  i++)
		write (out, card, CARDLEN);
					  
	    strcpy (card, "END");
	    memset (card+3, ' ', CARDLEN-3);
	    write (out, card, CARDLEN);
					     
	    kwdb_Close (kwdb);
	}

	
	toc = NO;
	count = 0;
	level = 1;
	/* Put each directory and file listed on the command line to 
	 * the fitsfile.
	 */
	for (i=0;  (argp = flist[i]) != NULL;  i++)
	    if ((ftype = filetype (argp)) == LF_DIR){
		putfiles (argp, out, "", &level);
	    } else
		fgfileout (argp, out, ftype, "", level);

	/* Close the fitsfile.
	 */
done:
	close (out);
	exit (0);
}


/* PUTFILES -- Put the named directory tree to the output fitsfile.  We chdir
 * to each subdirectory to minimize path searches and speed up execution.
 */
putfiles (dir, out, path, level)
char	*dir;			/* directory name		*/
int	out;			/* output file			*/
char	*path;			/* pathname of curr. directory	*/
int	*level;			/* directory level              */
{
	char	newpath[SZ_PATHNAME+1];
	char	oldpath[SZ_PATHNAME+1];
	char	fname[SZ_PATHNAME+1];
	int	ftype, dirl;
	DIR	*dfd;
	struct dirent  *dp;

	if (debug)
	    printf ("putfiles (%s, %d, %s level: %d)\n", dir, out, path,*level);

	/* Put the directory file itself to the output as a file.
	 */
	fgfileout (dir, out, LF_DIR, path, *level);

	if ((dfd = opendir (dir)) == NULL) {
	    fflush (stdout);
	    fprintf (stderr, "cannot open subdirectory `%s%s'\n", path, dir);
	    fflush (stderr);
	    return (0);
	}

	getcwd (oldpath, SZ_PATHNAME);
	sprintf (newpath, "%s%s", dname(path), dir);
	strcpy (newpath, dname(newpath));

	if (debug)
	    printf ("change directory to %s\n", newpath);
	if (chdir (dir) == ERR) {
	    closedir (dfd);
	    fflush (stdout);
	    fprintf (stderr, "cannot change directory to `%s'\n", newpath);
	    fflush (stderr);
	    return (0);
	}

	/* Put each file in the directory to the output file.  Recursively
	 * read any directories encountered.
	 */
	dirl = *level + 1;
	while ((dp = readdir(dfd)) != NULL) {
	    if (strcmp (dp->d_name, ".") == 0 || strcmp (dp->d_name, "..") == 0)
		continue;    /* skip self and parent */

	    if ((ftype = filetype (dp->d_name)) == LF_DIR) {
		putfiles (dp->d_name, out, newpath, &dirl);
	    } else
		fgfileout (dp->d_name, out, ftype, newpath, dirl);
	}

	if (debug)
	    printf ("return from subdirectory %s\n", newpath);
	if (chdir (oldpath) == ERR) {
	    fflush (stdout);
	    fprintf (stderr, "cannot return from subdirectory `%s'\n", newpath);
	    fflush (stderr);
	} 

	closedir (dfd);
}

/* FGFILEOUT -- Write the named file to the output in FITS format.
*/
fgfileout (fname, out, ftype, path, level)
char	*fname;			/* file to be output	*/
int	out;			/* output stream	*/
int	ftype;			/* file type		*/
char	*path;			/* current path		*/
int	level;			/* directory level      */
{
	struct  stat   fst;
	struct	fheader fh;
	char	card[CARDLEN], type[20];
	char	sval[SLEN];
	register struct	_modebits *mp;
	char 	*tp, *fn, *get_owner(), *get_group();
	pointer kwdb;
	int	k, nbh, nbp, usize, in, get_checksum(), hdr_plus;
	long	in_off, out_off;
	unsigned int datasum;
	int	nkw, i, ep, status, ncards, pcount, hd_nlines, hd_cards;

	if (debug)
	    printf ("put file `%s', type %d\n", fname, ftype);

	switch(ftype) {
	case LF_SYMLINK:
	    if (omitsymlink) return (0);
	    break;
	case LF_BIN:
	    if (omitbin) return (0);
	    break;
	case LF_TXT:
	    if (omittxt) return (0);
	    break;
	case LF_DIR:
	    if (omitdir) return (0);
	    break;
	case FITS:
	    if (omitfits) return (0);
	    break;
	case FITS_MEF:
	    if (omitfitsmef) return (0);
	    break;
	default:
	    return (0);
	    break;
	}

	if ((in = open (fname, 0, O_RDONLY)) == ERR) {
	    fflush (stdout);
	    fprintf (stderr, "Warning: cannot open file `%s'\n", fname);
	    fflush (stderr);
	    return (0);
	}

	/* Format and output the file header.
	 */
	memset (&fh, 0, sizeof(fh));
	strcpy (fh.name, path);
	strcat (fh.name, fname);
	strcpy (fh.linkname, "");
	fh.linkflag = 0;
	fh.isdir = 0;

	/* Get info on file to make file header.
	 */
	if (fstat (in, &fst) == ERR) {
	    fflush (stdout);
	    fprintf (stderr, 
		"Warning: could not stat file `%s'\n", fname);
	    fflush (stderr);
	    return (0);
	}
	fh.uid = fst.st_uid;
	fh.gid = fst.st_gid;
	fh.mode  = fst.st_mode;
	fh.ctime = fst.st_ctime;
	fh.mtime = fst.st_mtime;
	fh.size  = fst.st_size;

	strcpy (sval, fname);
	if (ftype == LF_SYMLINK) {
	    struct  stat fi;
	    int n;
	    lstat (fname, &fi);

	    /* Set attributes of symbolic link, not file pointed to. */
	    fh.uid   = fi.st_uid;
	    fh.gid   = fi.st_gid;
	    fh.mode  = fi.st_mode;
	    fh.ctime = fi.st_ctime;
	    fh.mtime = fi.st_mtime;
	    fh.size  = 0;

	    fh.linkflag = LF_SYMLINK;
	    if ((n = readlink (fname, fh.linkname, NAMSIZ)) > 0)
		 fh.linkname[n] = '\0';
	    sprintf(sval, "%s -> %s",fname,fh.linkname);
	}

	/* Open keyword database
	*/
	if (!(kwdb = kwdb_Open ("EHU"))) {
	    fflush (stdout);
	    fprintf (stderr, 
		"Warning: Could not open EHU kwdb `%s'\n", fname);
	    fflush (stderr);
	    return (0);
	}

	hdr_plus = 0;
	if (fh.linkflag == LF_SYMLINK) {
	    tp = sval;
	    fn = fname;
	} else {
	    if (strcmp (fname, ".") == 0)
		tp = group;
	    else
		tp = gname(sval);
	    fn = tp;
	}
	if (ftype == FITS || ftype == FITS_MEF) {
	    if ((ncards = kwdb_ReadFITS (kwdb, in, MAXENTRIES, NULL)) < 0) {
		fflush (stdout);
		fprintf (stderr, "cannot read FITS header `%s'\n", fname);
		fflush (stderr);
	    }
	    /* If file is empty, treat as text */
	    if (ncards == 0) {
		ftype = LF_TXT;
	        goto emptyfile;
	    }
	    ep = kwdb_Lookup (kwdb, "SIMPLE", 0);
	    kwdb_RenameEntry (kwdb, ep, "XTENSION");
	    kwdb_SetValue (kwdb, "XTENSION", "IMAGE");
	    nkw = kwdb_Len (kwdb);
	    hd_nlines = nkw;
	    nbp = pix_block(kwdb);
	    if (toc)    /* Input file usize */
		usize = ((nkw+35)/36)*36*80 + nbp*FBLOCK;

	    if (sums == YES) {
		/* Check if the PHU has these keywords 1st */
		if (kwdb_Lookup (kwdb, "CHECKSUM", 0) == 0) {
		    kwdb_AddEntry (kwdb, "CHECKSUM", "0000000000000000", "S",
			    "ASCII 1's complement checksum");
		    hd_nlines++;
		} else    /* Reset the value */
		    kwdb_SetValue (kwdb, "CHECKSUM", "0000000000000000");
		if (kwdb_Lookup (kwdb, "DATASUM", 0) == 0) {
		    kwdb_AddEntry (kwdb, "DATASUM", "       0", "S",
			    "checksum of data records");
		    hd_nlines++;
		} else
		    kwdb_SetValue (kwdb, "DATASUM", "       0");
		if (kwdb_Lookup (kwdb, "CHECKVER", 0) == 0) {
		    kwdb_AddEntry (kwdb, "CHECKVER", "COMPLEMENT", "S",
			    "checksum version ID");
		    hd_nlines++;
		} else
		    kwdb_SetValue (kwdb, "CHECKVER", "COMPLEMENT");
   	    }		
	    /* Advance input file pointer to the end of the current FBLOCK
	     * mark. kwdb_ReadFITS only read as much as ncards.
	     */
	    in_off = lseek (in, 0, SEEK_CUR); 
	    in_off = ((in_off + 2879)/2880)*2880;
	    in_off = lseek (in, in_off, SEEK_SET); 

	    /* In case we need to strech the PHU  to accomodate the FG
	     * keywords set one extra FBLOCK.
	     */
	    k = (36-nkw) % 36;
	    if (k > 0 && k < 10)
		hdr_plus = 2880;
	} else {
emptyfile:
	    kwdb_AddEntry (kwdb, "XTENSION","FOREIGN", "S",
		"NOAO xtension type");
	    kwdb_AddEntry (kwdb, "BITPIX","8", "N", "Bits per pixel (byte)");
	    kwdb_AddEntry (kwdb, "NAXIS", "0", "N", "No Image matrix");
	    kwdb_AddEntry (kwdb, "GCOUNT", "1", "N", "One group");
	    pcount = fh.size;
	    if (ftype == LF_DIR || ftype == LF_SYMLINK)
		pcount = 0;
	    kwdb_AddEntry (kwdb, "PCOUNT", str(pcount), "N", 
		"File size in bytes");
	    kwdb_AddEntry (kwdb, "EXTNAME", fn, "S", "Filename");
	    kwdb_AddEntry (kwdb, "EXTVER","1", "N", "");
	    kwdb_AddEntry (kwdb, "EXTLEVEL", str(level), "N","Directory level");
	    hd_nlines = 8;
	    if (sums == YES) {
		kwdb_AddEntry (kwdb, "CHECKSUM", "0000000000000000", "S",
			"ASCII 1's complement checksum");
		kwdb_AddEntry (kwdb, "DATASUM", "       0", "S",
			"checksum of data records");
		kwdb_AddEntry (kwdb, "CHECKVER", "COMPLEMENT", "S",
			"checksum version ID");
		hd_nlines = 11;
	    }
	}
	kwdb_AddEntry (kwdb, "FG_GROUP", group, "S", "Group Name");
	kwdb_AddEntry (kwdb, "FG_FNAME", tp, "S", "Filename");
	switch(ftype) {
	case LF_SYMLINK:
	    strcpy (type, "symlink");
	    break;
	case LF_BIN:
	    strcpy (type, "binary");
	    break;
	case LF_TXT:
	    strcpy (type, "text");
	    break;
	case LF_DIR:
	    strcpy (type, "directory");
	    break;
	case FITS:
	    strcpy (type, "FITS");
	    break;
	case FITS_MEF:
	    strcpy (type, "FITS-MEF");
	    break;
	default:
	    strcpy (type, "other");
	    break;
	}
	kwdb_AddEntry (kwdb, "FG_FTYPE", type, "S", "File type");
	kwdb_AddEntry (kwdb, "FG_LEVEL", str(level), "N", "Directory level");
	pcount = fh.size + hdr_plus;
	if (ftype == LF_DIR || ftype == LF_SYMLINK)
	    pcount = 0;
	kwdb_AddEntry (kwdb, "FG_FSIZE", str(pcount), "N", "Data size (bytes)");
	tp = sval;
	*tp = '-';
	if (ftype == LF_DIR)
	    *tp++ = 'd';
	else if (ftype == LF_SYMLINK)
	    *tp++ = 'l';
	else
	    tp++;
	for (mp=modebits;  mp->code;  mp++)
	    *tp++ = mp->code & fh.mode ? mp->ch : '-';
	*tp=0;

	kwdb_AddEntry (kwdb, "FG_FMODE", sval, "S", "File mode");
	kwdb_AddEntry (kwdb, "FG_FUOWN", get_owner(fh.uid), "S", "File UID");
	kwdb_AddEntry (kwdb, "FG_FUGRP", get_group(fh.gid), "S", "File GID");
	{ struct tm *tm;
	    tm = gmtime(&fh.ctime);
	    sprintf(card,"%d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2d",tm->tm_year+1900,
		tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
	    kwdb_AddEntry (kwdb, "FG_CTIME", card, "S", "file ctime (GMT)");
	    tm = gmtime(&fh.mtime);
	    sprintf(card,"%d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2d",tm->tm_year+1900,
		tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
	    kwdb_AddEntry (kwdb, "FG_MTIME", card, "S", "file mtime (GMT)");
	}
	hd_cards = hd_nlines + 10 + 1;
	if (toc == NO) {
	    /* Get the current output file position */
	    out_off = lseek (out, 0, SEEK_CUR); 
	    ncards = kwdb_WriteFITS (kwdb, out);
	    nbh = (ncards + 1 + 35)/36;    /* Fblocks of header */
	    ncards = ncards % 36;

	    /* Blank fill the remainder of the header area. */
	    memset (card, ' ', CARDLEN);
	    for (i = ncards + 1;  i < 36;  i++)
		write (out, card, CARDLEN);
					  
	    /* Write the END card to mark the end of the header.  */
	    strcpy (card, "END");
	    memset (card+3, ' ', CARDLEN-3);
	    
	    write (out, card, CARDLEN);
        }					 
	kwdb_Close (kwdb);

	if (ftype == LF_DIR) {
	    strcpy (fh.name, dname(fh.name));
	    fh.size  = 0;
	    fh.isdir = 1;
	    fh.linkflag = LF_DIR;
	}

	/* Copy the file data.
	 */
	if ((toc==NO) && fh.size > 0 && !fh.isdir && !fh.linkflag)
	    copyfile (in, &fh, out, ftype, out_off, nbp, &datasum);

	if (verbose && !toc) {
	    printheader (stdout, &fh, type);
	    fflush (stdout); 
	}

	/* Generate one liner for TOC */
	if (toc)
	    toc_card (in, &fh, ftype, hd_cards, level, usize);

	/* Calculate the checksum now */
	if (sums == YES)
	    if ((toc==NO) && fh.size > 0 && !fh.isdir && !fh.linkflag)
		get_checksum(out, out_off, nbh, &datasum);
	

	close (in);
}

/* GET_OWNER -- Obtain user name for the password file given the uid.
*/
char *
get_owner(fuid)
int 	fuid;
{

	/* Get owner name.  Once the owner name string has been retrieved
	* for a particular (system wide unique) UID, cache it, to speed
	* up multiple requests for the same UID.
	*/

	static      int uid = 0;
	static      char owner[SZ_OWNERSTR+1];
	struct      passwd *pw;

	if (fuid == uid)
	    return(owner);
	else {
	    /* setpwent();  */
	    pw = getpwuid (fuid);
	    /* endpwent(); */
	    if (pw == NULL)
		strcpy(owner, "<unknown>");
	    else {
		strncpy (owner, pw->pw_name, SZ_OWNERSTR);
		uid =  fuid;
	    }
	}
	owner[SZ_OWNERSTR] = 0;
	return(owner);
}


/* GET_GROUP -- Obtain group name for the file given the uid.
*/
char *
get_group(fuid)
int 	fuid;
{

	/* Get owner name.  Once the owner name string has been retrieved
	* for a particular (system wide unique) UID, cache it, to speed
	* up multiple requests for the same UID.
	*/

	static      int gid = 0;
	static      char owner[SZ_OWNERSTR+1];
	struct      group *gp;

	if (fuid == gid)
	    return(owner);
	else {
	    /* setpwent();  */
	    gp = getgrgid (fuid);
	    /* endpwent(); */
	    if (gp == NULL)
		strcpy(owner, "<unknown>");
	    else {
		strncpy (owner, gp->gr_name, SZ_OWNERSTR);
		gid =  fuid;
	    }
	}
	owner[SZ_OWNERSTR] = 0;
	return(owner);
}


/* CHECKSUM -- Calculate the checksum for a FITS extension unit, including
 * header and data.
*/
get_checksum (fd, out_offset, nbh, datasum)
int	fd;			/* file descriptor */
long	out_offset;		/* offset of the beginning of FITS header */
int	nbh;			/* number of FBLOCK of header */
unsigned int *datasum;		/* datasum value */
{
	unsigned short	sum16;
	unsigned int	sum32;
	char	record[FBLOCK*NBLOCK];
	char	ascii[161];
	unsigned int add_1s_comp();
	int	i, bks, ncards, ep, pos, recsize, permute;
	pointer kwdb;

	sum16 = 0;
	sum32 = 0;
	permute = 1;

	/* Position the output file at the beginning of the EHDU to start
	 * reading data. Read blocks of FBLOCK*NBLOCK bytes, then read a last 
	 * partial block FBLOCK*nb bytes.
	 */
	pos = lseek (fd, out_offset, SEEK_SET);

	bks = nbh/NBLOCK;
	for (i=1; i<=bks; i++) {
	    recsize = read (fd, record, FBLOCK*NBLOCK);
	    checksum (record, recsize, &sum16, &sum32);
	}
	if (nbh % NBLOCK != 0) {
	    recsize = read (fd, record, (nbh % 10)*FBLOCK);
	    checksum (record, recsize, &sum16, &sum32);
	}			
	/* Now add datasum and checksum and put the result in
	 * 1's complement with permute in a string.
	 */
	char_encode (~add_1s_comp(*datasum,sum32), ascii, 4, permute);

	/* Position the output file at the beginning of the EHDU to 
	 * read FITS header
	 */
	pos = lseek (fd, out_offset, SEEK_SET);

	kwdb = kwdb_Open ("PHU");
	if ((ncards = kwdb_ReadFITS (kwdb, fd, MAXENTRIES, NULL)) < 0) {
	    fflush (stdout);
	    fprintf (stderr, "cannot read FITS header in checksum");
	    fflush (stderr);
	}

	kwdb_SetValue (kwdb, "CHECKSUM", ascii);

	/* Position the output file at the beginning of the EHDU to 
	 * write back the update FITS header
	 */
	pos = lseek (fd, out_offset, SEEK_SET);
	ncards = kwdb_WriteFITS (kwdb, out);
	kwdb_Close(kwdb);

	/* put file pointer to the EOF position */
	pos = lseek (fd, 0, SEEK_END);
}

/* CHECKSUM -- Increment the checksum of a character array.  The
 * calling routine must zero the checksum initially.  Shorts are
 * assumed to be 16 bits, ints 32 bits.
 */

/* Explicitly exclude those ASCII characters that fall between the
 * upper and lower case alphanumerics (<=>?@[\]^_`) from the encoding.
 * Which is to say that only the digits 0-9, letters A-Z, and letters
 * a-r should appear in the ASCII coding for the unsigned integers.
 */
#define	NX	13
unsigned exclude[NX] = { 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
			 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60 };

int offset = 0x30;		/* ASCII 0 (zero) character */

/* Internet checksum algorithm, 16/32 bit unsigned integer version:
 */
checksum (buf, length, sum16, sum32)
char		*buf;
int		length;
unsigned short	*sum16;
unsigned int	*sum32;
{
	unsigned short	*sbuf;
	int	 	len, remain, i;
	unsigned int	hi, lo, hicarry, locarry, tmp16;

	sbuf = (unsigned short *) buf;

	len = 2*(length / 4);	/* make sure len is even */
	remain = length % 4;	/* add remaining bytes below */

	/* Extract the hi and lo words - the 1's complement checksum
	 * is associative and commutative, so it can be accumulated in
	 * any order subject to integer and short integer alignment.
	 * By separating the odd and even short words explicitly, both
	 * the 32 bit and 16 bit checksums are calculated (although the
	 * latter follows directly from the former in any case) and more
	 * importantly, the carry bits can be accumulated efficiently
	 * (subject to short integer overflow - the buffer length should
	 * be restricted to less than 2**17 = 131072).
	 */
	hi = (*sum32 >> 16);
	lo = (*sum32 << 16) >> 16;

	for (i=0; i < len; i+=2) {
	    hi += sbuf[i];
	    lo += sbuf[i+1];
	}

	/* any remaining bytes are zero filled on the right
	 */
	if (remain) {
	    if (remain >= 1)
		hi += buf[2*len] * 0x100;
	    if (remain >= 2)
		hi += buf[2*len+1];
	    if (remain == 3)
		lo += buf[2*len+2] * 0x100;
	}

	/* fold the carried bits back into the hi and lo words
	 */
	hicarry = hi >> 16;
	locarry = lo >> 16;

	while (hicarry || locarry) {
	    hi = (hi & 0xFFFF) + locarry;
	    lo = (lo & 0xFFFF) + hicarry;
	    hicarry = hi >> 16;
	    locarry = lo >> 16;
	}

	/* simply add the odd and even checksums (with carry) to get the
	 * 16 bit checksum, mask the two to reconstruct the 32 bit sum
	 */
	tmp16 = hi + lo;
	while (tmp16 >> 16)
	    tmp16 = (tmp16 & 0xFFFF) + (tmp16 >> 16);

	*sum16 = tmp16;
	*sum32 = (hi << 16) + lo;
}


/* CHAR_ENCODE -- Encode an unsigned integer into a printable ASCII
 * string.  The input bytes are each represented by four output bytes
 * whose sum is equal to the input integer, offset by 0x30 per byte.
 * The output is restricted to alphanumerics.
 *
 * This is intended to be used to embed the complement of a file checksum
 * within an (originally 0'ed) ASCII field in the file.  The resulting
 * file checksum will then be the 1's complement -0 value (all 1's).
 * This is an additive identity value among other nifty properties.  The
 * embedded ASCII field must be 16 or 32 bit aligned, or the characters
 * can be permuted to compensate.
 *
 * To invert the encoding, simply subtract the offset from each byte
 * and pass the resulting string to checksum.
 */

char_encode (value, ascii, nbytes, permute)
unsigned int	value;
char		*ascii;	/* at least 17 characters long */
int		nbytes;
int		permute;
{
	int	byte, quotient, remainder, ch[4], check, i, j, k;
	char	asc[32];

	for (i=0; i < nbytes; i++) {
	    byte = (value << 8*(i+4-nbytes)) >> 24;

	    /* Divide each byte into 4 that are constrained to be printable
	     * ASCII characters.  The four bytes will have the same initial
	     * value (except for the remainder from the division), but will be
	     * shifted higher and lower by pairs to avoid special characters.
	     */
	    quotient = byte / 4 + offset;
	    remainder = byte % 4;

	    for (j=0; j < 4; j++)
		ch[j] = quotient;

	    /* could divide this between the bytes, but the 3 character
	     * slack happens to fit within the ascii alphanumeric range
	     */
	    ch[0] += remainder;

	    /* Any run of adjoining ASCII characters to exclude must be
	     * shorter (including the remainder) than the runs of regular
	     * characters on either side.
	     */
	    check = 1;
	    while (check)
		for (check=0, k=0; k < NX; k++)
		    for (j=0; j < 4; j+=2)
			if (ch[j]==exclude[k] || ch[j+1]==exclude[k]) {
			    ch[j]++;
			    ch[j+1]--;
			    check++;
			}

	    /* ascii[j*nbytes+(i+permute)%nbytes] = ch[j]; */
	    for (j=0; j < 4; j++)
		asc[j*nbytes+i] = ch[j];
	}

	for (i=0; i < 4*nbytes; i++)
	    ascii[i] = asc[(i+4*nbytes-permute)%(4*nbytes)];

	ascii[4*nbytes] = 0;
}

/* ADD_1S_COMP -- add two unsigned integer values using 1's complement
 * addition (wrap the overflow back into the low order bits).  Could do
 * the same thing using checksum(), but this is a little more obvious.
 * To subtract, just complement (~) one of the arguments.
 */
unsigned int add_1s_comp (u1, u2)
unsigned int	u1, u2;
{
	unsigned int	hi, lo, hicarry, locarry;

	hi = (u1 >> 16) + (u2 >> 16);
	lo = ((u1 << 16) >> 16) + ((u2 << 16) >> 16);

	hicarry = hi >> 16;
	locarry = lo >> 16;

	while (hicarry || locarry) {
	    hi = (hi & 0xFFFF) + locarry;
	    lo = (lo & 0xFFFF) + hicarry;
	    hicarry = hi >> 16;
	    locarry = lo >> 16;
	}

	return ((hi << 16) + lo);
}


/* PRINTHEADER -- Print one line of information per file.
 *
 */
printheader (fp, fh, type)
FILE	*fp;			/* output file			*/
register struct fheader *fh;	/* file header struct		*/
char	*type;			/* type of file */
{
	register struct	_modebits *mp;
	char	c, *tp, line[CARDLEN];
	int	k;
	long clk;

	clk = fh->mtime;
	tp = ctime (&fh->mtime);

	fprintf (fp, "%-4d %-10.10s %9d %-12.12s %-4.4s %s",
	    ++count,type, fh->size, tp + 4, tp + 20, fh->name);
	if (fh->linkflag && *fh->linkname) {
	    fprintf (fp, " -> %s ", fh->linkname);
	}
	fprintf(fp, "\n");
}


/* COPYFILE -- Copy bytes from the input file to the output file.  Each file
 * consists of a integral number of FBLOCK size blocks on the output file.
 */
copyfile (in, fh, out, ftype, out_off, nbp, datasum)
int	in;			/* input file descriptor	*/
struct	fheader *fh;		/* file header structure	*/
int	out;			/* output file descriptor	*/
int	ftype;			/* file type LF_TXT and others   */
int	out_off;		/* points to the beginning of EHU */
int	nbp;			/* number of Fblocks in data unit */
unsigned int *datasum;		/* output datasum value		*/
{
	register int	i;
	int	nbytes, ncards, pos;
	int	npad, nb, bks;
	char	buf[FBLOCK*10], ascii[161];
	int	ipos, epos, in_off; 
	unsigned short	sum16;
	unsigned int	sum32;
	pointer  kwdb;

	sum16 = 0;
	sum32 = 0;
	nb = 0;

	/* If we are reading a MEF file, compute the checksum for
	 * the PDU only
	 */
	if (ftype == FITS_MEF) {
	    bks = nbp/10;
	    for (i=1; i<=bks; i++) {
	        nbytes = read (in, buf, FBLOCK*10);
	        write (out, buf, nbytes);
		if (sums == YES)
		    checksum (buf, nbytes, &sum16, &sum32);
		nb = nb + nbytes;
	    }
	    if (nbp % 10 != 0) {
		nbytes = read (in, buf, (nbp % 10)*FBLOCK);
		write (out, buf, nbytes);
		if (sums == YES)
		    checksum (buf, nbytes, &sum16, &sum32);
		nb = nb + nbytes;
	    }			
	}
	/* Now read and write */
	ipos = lseek(out, 0, SEEK_CUR);
	while ((nbytes = read (in, buf, FBLOCK*10)) != 0) {
	    write (out, buf, nbytes);
	    if (ftype != FITS_MEF) {
		if (sums == YES)
		    checksum (buf, nbytes, &sum16, &sum32);
		nb = nb + nbytes;
	    }
	}

	/* Pad data unit with blanks for text data and zero for
	 * other datatype
	 */
	npad = 2880 - nb % 2880;
	i = 0;
	if (ftype == LF_TXT)
	    i = ' ';
	if ((npad % 2880) > 0) {
	    memset (buf, i, npad);
	    if (sums == YES)
		checksum (buf, npad, &sum16, &sum32);
	    write (out, buf, npad);
	}

	/* The checksum algorithm does not work for small text files
	 * that are not multiple or 4 bytes. Recalculate it for the
	 * output data extension since this one is multiple of 2880.
	 */
	if (sums == YES) {
	    if (ftype == LF_TXT) {
		epos = lseek(out, 0, SEEK_CUR);
		sum16=0; sum32=0;
		bks = (epos-ipos)/(FBLOCK*10);
		in_off = lseek(out, ipos, SEEK_SET);
		for (i=1; i<=bks; i++) {
		    nbytes = read (out, buf, FBLOCK*10);
		    checksum (buf, nbytes, &sum16, &sum32);
		}
		nbp = (epos-ipos) % (FBLOCK*10);
		if (nbp != 0) {
		    nbytes = read (out, buf, nbp);
		    checksum (buf, nbytes, &sum16, &sum32);
		}			
	    }


	    *datasum = sum32;
	    /* go to the start of EHU */
	    pos = lseek (out, out_off, SEEK_SET);

	    kwdb = kwdb_Open ("PHU");
	    if ((ncards = kwdb_ReadFITS (kwdb, out, MAXENTRIES, NULL)) < 0) {
		fflush (stdout);
		fprintf (stderr, "cannot read FITS header in copyfile");
		fflush (stderr);
	    }

	    if (sums == YES) {
		/* update DATASUM value */
		sprintf(ascii,"%-10lu",sum32);
		kwdb_SetValue (kwdb, "DATASUM", ascii);
	    }
	    /* Position the output file at the beginning of the EHDU to 
	     * write back the update FITS header
	     */
	    pos = lseek (out, out_off, SEEK_SET);
	    ncards = kwdb_WriteFITS (kwdb, out);
	    kwdb_Close(kwdb);
	}
}


/* DNAME -- Normalize a directory pathname.   For unix, this means convert
 * an // sequences into a single /, and make sure the directory pathname ends
 * in a single /.
 */
char *
dname (dir)
char	*dir;
{
	register char	*ip, *op;
	static	char path[SZ_PATHNAME+1];

	for (ip=dir, op=path;  *ip;  *op++ = *ip++)
	    while (*ip == '/' && *(ip+1) == '/')
		ip++;

	if (op > path && *(op-1) != '/')
	    *op++ = '/';
	*op = (char )NULL;

	return (path);
}


/*
 * FILETYPE -- Determine whether the named file is a text file, a binary
 * file, or a directory.
 */

char *binextn[] = {		/* Known binary file extensions */
	".o",
	".e",
	".a",
	".mip",
	".pl",
	".gif",
	".jpeg",
	".jpg",
	".tiff",
	".tif",
	".gz",
	NULL
};

char *srcextn[] = {		/* Known source file extensions */
	".x",
	".h",
	".f",
	".c",
	".s",
	".hlp",
	NULL
};

char *fitsextn[] = {		/* Known FITS file extensions */
	".fits",
	".fit",
	NULL
};

#define SZ_TESTBLOCK    1024            /* for TEXT/BINARY heuristic    */
#define MAX_LINELEN     256             /* when looking for newlines    */
#define R               04              /* UNIX access() codes          */
#define W               02
#define ctrlcode(c)     ((c) >= '\007' && (c) <= '\017')

/* FILETYPE -- Determine the type of a file.  If the file has one of the
 * known source file extensions we assume it is a text file; if it has a well
 * known binary file extension we assume it is a binary file; otherwise we call
 * os_access to determine the file type.
 */
filetype (fname)
char	*fname;			/* name of file to be examined	*/
{
	register char	*ip, *ep;
	register int	n, ch, i;
	int	 fd, nchars, newline_seen;
	char	*extn, buf[SZ_TESTBLOCK];
	int	fits_mef();
	struct stat fi;

	if (lstat(fname, &fi) == 0) {
	    if ((fi.st_mode & S_IFMT) == S_IFDIR)
		return(LF_DIR);
	    else if ((fi.st_mode & S_IFMT) == S_IFLNK)
		return(LF_SYMLINK);
	}
	/* Get filename extension.
	 */
	extn = NULL;
	for (ip=fname;  (ch = *ip);  ip++)
	    if (ch == '.')
		extn = ip;

	/* If the filename has a extension, check the list of known text and 
	 * binary file extensions to see if we can make a quick determination
	 * of the file type.
	 */
	if (extn) {
	    ch = *(extn + 1);

	    /* Known source file extension? */
	    for (i=0;  (ep = srcextn[i]);  i++)
		if (*(ep+1) == ch)
		    if (strcmp (ep, extn) == 0)
			return (LF_TXT);

	    /* Known binary file extension? */
	    for (i=0;  (ep = binextn[i]);  i++)
		if (*(ep+1) == ch)
		    if (strcmp (ep, extn) == 0)
			return (LF_BIN);

	    /* Known FITS file extension? */
	    for (i=0;  (ep = fitsextn[i]);  i++)
		if (*(ep+1) == ch)
		    if (strcmp (ep, extn) == 0)
			return (fits_mef(fname, fi.st_size));
	}


	/* Do NOT read from a special device (may block) */
	if ((fi.st_mode & S_IFMT) & S_IFREG) {
	    /* If we are testing for a text file the portion of the file
	     * tested must consist of only printable ascii characters or
	     * whitespace, with occasional newline line delimiters.
	     * Control characters embedded in the text will cause the
	     * heuristic to fail.  We require newlines to be present in
	     * the text to disinguish the case of a binary file containing
	     * only ascii data, e.g., a cardimage file.
	     */
	    fd = open ((char *)fname, 0);
	    if (fd >= 0) {
		nchars = read (fd, buf, SZ_TESTBLOCK); 
		if (nchars == 0)       /* Check for empty file */
		    return(LF_BIN);
		ip = buf;
		for (n=nchars, newline_seen=0;  --n >= 0;  ) {
		    ch = *ip++;
		    if (ch == '\n')
			newline_seen++;
		    else if (!isprint(ch) && !isspace(ch) && !ctrlcode(ch))
			break;
		}
		close (fd);

		if (n >= 0 || (nchars > MAX_LINELEN && !newline_seen))
		    return(LF_BIN);
		else
		    return(LF_TXT);
	    }
	} return (OTHER);
}


/* FITS_MEF -- Determines by reading the FITS file if is FITS (single unit)
* or a FITS-MEF (multiple units).
*/
fits_mef(fname, filesize)
char 	*fname;
int	filesize;	/* fname size */
{
	int     ncards;
	int	fd, datasize, size;
	char	*sval;
	pointer kwdb;

	if ((fd = open (fname, 0)) <= 0) {;
	    fflush (stdout);
	    fprintf (stderr, "cannot open FITS file `%s'\n", fname);
	    fflush (stderr);
	    return (0);
	}

	kwdb = kwdb_Open ("FITS");

	if ((ncards = kwdb_ReadFITS (kwdb, fd, MAXENTRIES, NULL)) < 0) {
	    fflush (stdout);
	    fprintf (stderr, "cannot read FITS header `%s'\n", fname);
	    fflush (stderr);
	    return (0);
	}
	close (fd);

	datasize = pix_block (kwdb);
	size = datasize * FBLOCK + (ncards+1) * CARDLEN;

	kwdb_Close (kwdb);

	if (filesize > size)
	    return (FITS_MEF);
	else
	    return (FITS);
}


static char *
str (n)
int	n;
{
        static char s[32];
	sprintf (s, "%d", n);
	return (s);
}


/* GNAME -- Return a filename with no pre or post '/' if it
 * has any.
 */
char *gname(name)
char *name;
{
	char *ip;
	
	ip = rindex(name,'/');
	if (ip != NULL && *(ip+1) == (char )NULL)
	    *ip = (char )'\0';

	ip = rindex(name,'/');
	if(ip == NULL)
	   return(name);
	else
	   return(ip+1);
}


/* TOC_CARD -- Format a Table of content card of the form:
 *
 * Count HDR_OFF FSIZE FTYPE FLEVEL FNAME
 *
 * e.g.
   1       1     0     fd    1   .
   2       2     4     ff    2   h36.fits
   3       6     0     fd    2   nza
 *
 * Count:   Extension counter
 * HDR_OFF: Extension header offset in 2880 bytes block units
 * FSIZE:   Extension unit size in 2880 bytes block units
 * FTYPE:   Type of FITS extension:
	    fd: directory
	    fb: binary file
	    ft: text file
	    fs: symbolic link
	    ff: FITS file
	    fm: FITS_MEF file
 * FLEVEL:  Directory level of the unit. 1 is top directory
 * FNAME:   File name for the extension unit
 */
toc_card (in, fh, ftype, hd_cards, level, usize)
register struct fheader *fh;	/* file header struct		*/
int	ftype;			/* type of file */
int	hd_cards;
int	level;			/* Directory level */
int	usize;			/* FITS unit size		*/
{
	char	type[3], *tp, line[CARDLEN];
	int	c, k, fsize;

	switch(ftype) {
	case LF_SYMLINK:
	    strcpy (type, "fl");
	    break;
	case LF_BIN:
	    strcpy (type, "fb");
	    break;
	case LF_TXT:
	    strcpy (type, "ft");
	    break;
	case LF_DIR:
	    strcpy (type, "fd");
	    break;
	case FITS:
	    strcpy (type, "ff");
	    break ;
	case FITS_MEF:
	    strcpy (type, "fm");
	    break;
	default:
	    strcpy (type, "??");
	    break;
	}

        fsize = (fh->size+2879)/2880;	
	c = level <= 9 ? level+'0' : level-10+'a';
	sprintf (slines, "  %-4d %4d %4d %s %c %s",
	    ++count, hdr_off/2880, fsize, type, c, gname(fh->name));
	slines = slines + TOCLEN;

	if (count >= maxcount) {
	    maxcount += MAX_TOC;
	    slines= (char *)realloc (maxcount, TOCLEN);
	}

	if (ftype == FITS_MEF || ftype == FITS) {
	    /* the MEF PHU can run of header space and go to next block on
	     * output. Account the offset for this effect 
	     */
	    k = hd_cards % 36;
	    if (k > 0 && k < 10)
		hdr_off = hdr_off + 2880;
	}
	if (ftype == FITS_MEF) {
	    list_mef(in, usize);
	} else if (ftype == FITS)
	    hdr_off = hdr_off + usize;
	else
	    hdr_off = hdr_off + fsize*2880 + ((hd_cards + 35)/36)*36*80;
}


/* LIST_TOC -- List and update if necessary the header offset 
 * column because we have gone over one or more FITS blocks with lines of TOC.
 */
list_toc (kwdb)
pointer kwdb;        /* Output db */
{

	char    *s, line[CARDLEN];
	int	i, k, hoff, nb;

	kwdb_AddEntry(kwdb, "TOCLEN", str(count), "N",
	    "Number of entries in TOC");
	kwdb_AddEntry(kwdb, "COMMENT", "Col 11-14:  Index","C","");
	kwdb_AddEntry(kwdb, "COMMENT", "Col 16-19:  Header Offset (FITS block)",
	    "C","");
	kwdb_AddEntry(kwdb, "COMMENT", "Col 21-24:  File size (FITS block)",
	    "C","");
	kwdb_AddEntry(kwdb, "COMMENT", 
	    "Col 26-27:  FITS type: f(tbdlfm) or (tbio)","C","");
	kwdb_AddEntry(kwdb, "COMMENT", "Col 29:     Directory Level","C","");
	kwdb_AddEntry(kwdb, "COMMENT", "Col 31-80:  Filename ","C","");
	kwdb_AddEntry(kwdb, "", "","T","");

	nb = count/36;   /* number of blocks to add */
	if (count % 36 > 21) nb++;   /* add one more if necessary */

	if (nb == 0) {
	    for (s=slines-TOCLEN*count, k=count; k > 0; s+=TOCLEN,k--) {
		kwdb_AddEntry (kwdb, "        ", s, "T","");
	        if (verbose)
	            printf("%s\n",s);
	    }
	} else {
	    for (s=slines-TOCLEN*count, k=count; k > 0; s+=TOCLEN,k--) {
	        sscanf (s, "%d %d", &i, &hoff);
		hoff = hoff + nb;
	        sprintf (line, "  %-4d %4d%s", i, hoff, s+11);
		kwdb_AddEntry (kwdb, "        ", line, "T","");
	        if (verbose)
	            printf("%s\n",line);
	    }
	}
}


/* LIST_MEF -- Lists the extension units of a mef file when TOC is
 * requested.
 */
list_mef(fd, usize)
int	fd;		/* input fd   */
int	usize;		/* size of MEF PHDU */
{
	int     ncards, bytepix, pcount, naxes, i, npix;
	int	stat, foff, datasize;
	char	ft, *sval;
	pointer kwdb;

	/* At this point we have read the header; we need to position
	 * at the beginning of the next EHU.
	 */
	foff = lseek (fd, usize, SEEK_SET);

	/* Point to the beginning of the 1st EHU */
        while(1) {
	    kwdb = kwdb_Open ("FITS");

	    ncards = kwdb_ReadFITS (kwdb, fd, MAXENTRIES, NULL);
	    if (ncards < 0) {
		fflush (stdout);
		fprintf (stderr, "cannot read FITS header in list_mef()");
		fflush (stderr);
		return (0);
	    }  else if (ncards == 0) {
	        hdr_off = hdr_off + foff;
		kwdb_Close (kwdb);
		return (0);
	    }

	    sval =  kwdb_GetValue (kwdb,"XTENSION");
	    if (sval != NULL) { 
		if (     !strcmp(sval, "IMAGE   "))
		    ft = 'i';
		else if (!strcmp(sval, "TABLE   "))
		    ft = 't';
		else if (!strcmp(sval, "BINTABLE"))
		    ft = 'b';
		else if (!strcmp(sval, "FOREIGN "))
		    ft = 'f';
		else
		    ft = 'o';
	    }

	    datasize =  pix_block (kwdb);
	    ncards = ((ncards + 35)/36)*36;

	    sprintf(slines,"  %-4d %4d %7.7c",++count, 
		(hdr_off+foff)/2880, ft);
	    foff = foff + datasize * FBLOCK + ncards * CARDLEN;
	    if (count >= maxcount) {
		maxcount += MAX_TOC;
		slines= (char *)realloc (maxcount, TOCLEN);
	    }
	    slines = slines + TOCLEN;

	    stat = lseek (fd, foff, SEEK_SET);
	    if (stat < 0) {
	      kwdb_Close (kwdb);
	      return (0);
	    }
       }
}


/* PIX_BLOCK -- Calculate the size of the pixel area for a FITS UNIT
 *  in blocks of 2880.
 */
pix_block (kwdb)
pointer	kwdb;

{
	char *sval, *spcount, kwname[8];
	int  bytepix, naxes, npix, i, pcount; 
	int  size;

	if ((sval = kwdb_GetValue (kwdb, "BITPIX")) == NULL) 
	    return(0);
			
	bytepix = abs(atoi(sval)) / BYTELEN;
	naxes = atoi(kwdb_GetValue (kwdb, "NAXIS"));
	npix = naxes ? 1 : 0;
	for (i=1;  i <= naxes;  i++) {
	    sprintf(kwname,"NAXIS%d",i);
	    npix *= atoi(kwdb_GetValue (kwdb, kwname));
	}

	spcount = kwdb_GetValue (kwdb, "PCOUNT");
	if (spcount == NULL)
	    pcount = 0;
	else
	    pcount = atoi(spcount);

	size = (((npix+pcount) * bytepix) + FBLOCK-1) / FBLOCK;
	return(size);
}
