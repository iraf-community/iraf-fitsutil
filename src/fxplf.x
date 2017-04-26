# Copyright(c) 1986 Association of Universities for Research in Astronomy Inc.

include	<syserr.h>
include	<imhdr.h>
include	<imio.h>
include	<pmset.h>
include	<plio.h>
include <mef.h>

define  INC_HDRMEM      8100

# FXPLF -- Procedure to convert a 'pl' file into a 
# BINTABLE extension into a FITS file.

procedure t_fxplf()

char    output[SZ_LINE], stitle[SZ_LINE], input[SZ_LINE]
pointer	sp, fname, hp, pl, bp, mef
pointer  mef_open(), pl_open()

int	masklen, buflen, ctime,mtime, limtime
int	naxes, axlen[7], depth, list, ninfiles, oldfile
int     imtopen(), imtgetim(), imtlen(), access()
int	flags, pl_save()
real    minval, maxval
bool	verbose, clgetb()

begin
	call smark (sp)
	call salloc (fname, SZ_PATHNAME, TY_CHAR)
	call salloc (hp, INC_HDRMEM, TY_CHAR)

	call clgstr ("input", Memc[fname], SZ_LINE)
	call clgstr ("output", output, SZ_LINE)
	verbose = clgetb("verbose")

	list = imtopen (Memc[fname])
	ninfiles = imtlen (list)

	oldfile = YES
	if (access(output, 0,0) == NO) {
	    mef = mef_open(output, NEW_FILE, NULL)
	    call mef_dummyhdr (MEF_FD(mef), NULL)
	    oldfile = NO
	} else {
	    mef = mef_open(output, APPEND, NULL)
	}
	MEF_ACMODE(mef) = APPEND

	while (imtgetim (list, input, SZ_PATHNAME) != EOF) {
	    # Open an empty mask.
	    pl = pl_open (NULL)

	    # Load the named mask if opening an existing mask image.
	    iferr (call pl_loadf (pl, input, Memc[hp], INC_HDRMEM)) {
		call pl_close (pl)
		call sfree (sp)
		call eprintf("Error loading input pl file\n")
		return
	    }


	    bp = NULL
	    masklen = pl_save (pl, bp, buflen, flags)

	    call pl_gsize (pl, naxes, axlen, depth)

            if (verbose) {
		if (oldfile == NO) {
		    call eprintf ("%s -> %s\n")
		    oldfile = YES
	        } else
		    call eprintf ("%s -> %s[append]\n")
		call pargstr (input)
		call pargstr (output)
	    }
	    call mef_setpl (PLIO_SVMAGIC, masklen, Memc[hp], stitle, ctime, 
		    mtime, limtime, minval, maxval, mef)
	    call mef_wrpl (mef, stitle, ctime,mtime, limtime, minval, 
		  maxval,Mems[bp], naxes, axlen)

	    call mfree (bp, TY_SHORT)

	}
	call mef_close (mef)
	call sfree (sp)
end
