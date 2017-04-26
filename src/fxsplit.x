include <imio.h>
include <imhdr.h>
include <mef.h>
include <mach.h>


# FXSPLIT --  Split a MEF file into individual single FITS files.

procedure t_fxsplit()

char	input_file[SZ_FNAME]
char	output_file[SZ_FNAME]
char	imtlist[SZ_FNAME]
char	root[SZ_FNAME], extn[4]

int	i, gn, ninfiles, list
int	imtopen(), imtgetim(), imtlen()
int	fnextn(), fnroot()
     
pointer mefi, mefo, mef_open()
bool    clgetb()
bool    verbose
define  err_ 99
errchk  fcopy

begin
	call clgstr ("input", imtlist, SZ_FNAME)
	verbose = clgetb("verbose")

	list = imtopen (imtlist)
	ninfiles = imtlen (list)

	while (imtgetim (list, input_file, SZ_FNAME) != EOF) {
	    mefi = mef_open(input_file, READ_ONLY, 0)

	    i = fnextn (input_file, extn, 4)
	    i = fnroot (input_file, root, SZ_FNAME)
	    if (extn[1] == EOS)
		call strcpy ("fits", extn, 4)

	    do gn = 0, MAX_INT {

	        call sprintf (output_file, SZ_FNAME, "%s%d.%s")
		    call pargstr(root)
		    call pargi(gn)
		    call pargstr(extn)

	        mefo = mef_open (output_file, NEW_FILE, 0)

	        iferr (call mef_copy_extn (mefi, mefo, gn)) {
		      call mef_close (mefo)
		      call delete (output_file)
		      break
	        }

                # If verbose print the operation.
	        if (verbose) {
	  	    call eprintf ("%s -> %s\n")
		       call pargstr (input_file)
		       call pargstr (output_file)
	        }

	        call mef_close (mefo)
	    }
            call mef_close (mefi)
	}

	call imtclose (list)
end
