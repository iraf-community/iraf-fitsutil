include <imio.h>
include <imhdr.h>
include <mef.h>
include <ctype.h>

define MAX_RANGES  100

         
# FINSERT -- Insert one or more extension into the output file.

procedure t_fxinsert()

char	input_file[SZ_FNAME]
char	output_file[SZ_FNAME]
char	imtlist[SZ_FNAME], lbrk		

char	group_list[SZ_LINE], temp[SZ_FNAME]
int	group_range[2*MAX_RANGES+1]

int	in, out, gn, ninfiles, nitems, list, i, outfd
int	ig, nblks, nchars, stat
int	imtopen(), imtgetim(), imtlen(), open(), read(), mef_rdhdr_gn()
int	lget_next_number(), ldecode_ranges(), stridx(), fnroot(), note()
     
pointer mefi, mefo, mef_open(), sp, buf
bool    clgetb(), ogrp
bool    verbose
errchk  fcopy

begin
	lbrk = '['

	call smark (sp)
	call salloc (buf, FITS_BLKSZ_CHAR, TY_CHAR)

	call clgstr ("input", imtlist, SZ_LINE)
	call clgstr ("output", output_file, SZ_FNAME)
	verbose = clgetb("verbose")

	mefo = mef_open (output_file, READ_WRITE, 0)
	if (MEF_ENUMBER(mefo) < 0)
	   call error (13,"Output extension number not given")
	out = MEF_FD(mefo)

	# Position output file to the insertion point. We endup with the file
	# pointer at the end of the group.

	stat = mef_rdhdr_gn (mefo, MEF_ENUMBER(mefo))
	nblks = (note(out) - 1) / FITS_BLKSZ_CHAR

	# Make a temporary filename.
	i = stridx(lbrk, output_file)
	if (i > 0)
	   output_file[i] = EOS
	i = fnroot (output_file, temp, SZ_FNAME)
	call mktemp (temp, temp, SZ_FNAME)
	outfd = open (temp, NEW_FILE, BINARY_FILE)
	MEF_FD(mefo) = outfd

	# Copy data into temporary file.
	call seek (out, BOF)
	do i = 1, nblks {
	    nchars = read (out, Memc[buf], FITS_BLKSZ_CHAR)
	    call write (outfd, Memc[buf], FITS_BLKSZ_CHAR)
	}
	call flush (outfd)

	list = imtopen (imtlist)
	ninfiles = imtlen (list)
	
        ogrp = false
        # Copy selected extensions from one input file to the output file.
        call clgstr ("groups", group_list, SZ_LINE)

        if (ldecode_ranges (group_list,group_range,MAX_RANGES,nitems) ==ERR)
	    call error (0, "Illegal file number list.")

	while (imtgetim (list, input_file, SZ_PATHNAME) != EOF) {
	    mefi = mef_open (input_file, READ_ONLY, 0)
	    in = MEF_FD(mefi)
	    
	    ogrp = (stridx (lbrk, input_file) > 0)
            if (verbose && (ogrp || group_list[1] == EOS)) {
	 	call eprintf ("%s -> %s\n")
		    call pargstr (input_file)
		    call pargstr (output_file)
	    }

            # If an extension is specified, copy that only.
            if (ogrp) {
		ig =  MEF_ENUMBER(mefi)
	        if (ig >= 0)
		    call mef_copy_extn (mefi, mefo, ig)
	 	else
		    call mef_app_file (mefi, mefo)
	    } else if (group_list[1] == EOS ) {
	        # No input group list is specified, copy the whole file.
		call mef_copy_extn (mefi, mefo, 0)
	        call fcopyo (in, outfd)
	    } else {
	        gn = -1
                while (lget_next_number (group_range, gn) != EOF) {
	            call mef_copy_extn (mefi, mefo, gn)
	            if (verbose) {
		        call eprintf ("%s[%d] -> %s\n")
		        call pargstr (input_file)
		        call pargi(gn)
		        call pargstr (output_file)
	            }
	        }
	    }	

	    call mef_close (mefi)
        } # End While

	call close (in)

	# Now append the rest of the old output file into the temp file
	call seek (out, nblks * FITS_BLKSZ_CHAR + 1)
	call fcopyo (out, outfd)
	call close (out)
        call mef_close (mefo)
	call delete (output_file)
	call rename (temp, output_file)
	call imtclose (list)

	call sfree(sp)
end
