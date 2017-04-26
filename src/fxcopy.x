include <imio.h>
include <imhdr.h>
include <mef.h>
include <ctype.h>

define MAX_RANGES  100

         
# FITSCOPY --  Accumulate 2 or more input FITS file into one multiple
# extension file or append several FITS units to another FITS file

procedure t_fxcopy()

char	input_file[SZ_PATHNAME]
char	output_file[SZ_PATHNAME]
char	imtlist[SZ_LINE], lbrk		

char	group_list[SZ_LINE]
int	group_range[2*MAX_RANGES+1]

int	nch, in, out, gn, ninfiles, nitems, list
int	imtopen(), imtgetim(), imtlen()
int	lget_next_number(), ldecode_ranges(), stridx()
     
pointer mefi, mefo, fco_open_output(), mef_open()
bool    clgetb(), new_file, ogrp
bool    verbose
define  err_ 99
errchk  fcopy

begin

	call clgstr ("input", imtlist, SZ_LINE)
	call clgstr ("output", output_file, SZ_LINE)
	new_file = clgetb("new_file")
	verbose = clgetb("verbose")

	mefo =  fco_open_output (output_file, new_file)

	out = MEF_FD(mefo)

	list = imtopen (imtlist)
	ninfiles = imtlen (list)
	
	lbrk = '['
	if (ninfiles == 1)
	   ogrp = (stridx (lbrk, imtlist) > 0)

	# Accumulate 2 or more input FITS files into one mef file or copy
	# one group from one input file.

	if (ninfiles > 1 || ogrp) {
	    while (imtgetim (list, input_file, SZ_PATHNAME) != EOF) {
		mefi = mef_open(input_file, READ_ONLY, 0)

        	# If verbose print the operation.
		if (verbose) {
		    call eprintf ("%s -> %s\n")
		      call pargstr (input_file)
		      call pargstr (output_file)
		}

        	# Copy input file to either new or existent file.
		if (new_file) {
		   if (MEF_ENUMBER(mefi) >= 0)
		       call mef_copy_extn (mefi, mefo, MEF_ENUMBER(mefi))
	 	   else
		       call fcopyo (MEF_FD(mefi), MEF_FD(mefo))
		   
		   MEF_ACMODE(mefo) = APPEND
		   new_file = false
	        } else {
		   if (MEF_ENUMBER(mefi) >= 0)
		       call mef_copy_extn (mefi, mefo, MEF_ENUMBER(mefi))
		   else
		       call mef_app_file (mefi, mefo)
	        }
		call mef_close (mefi)
	    }
            call mef_close (mefo)
	    call imtclose (list)

	} else {
	    # Copy selected extensions from one input file to the output file.
            call clgstr ("groups", group_list, SZ_LINE)

	    # Since ranges handles only positive numbers, see if we need
	    # to look for zero group (PHDU).

            if (ldecode_ranges (group_list,group_range,MAX_RANGES,nitems) ==ERR)
	        call error (0, "Illegal file number list.")

	    list = imtopen (imtlist)

	    nch = imtgetim (list, input_file, SZ_PATHNAME) 

	    mefi = mef_open (input_file, READ_ONLY, 0)

	    in = MEF_FD(mefi)
	    
	    # If no input group list is specified, copy the whole file.
	    if (group_list[1] == EOS) {
	       if (verbose) {
		  call eprintf ("%s -> %s\n")
		    call pargstr (input_file)
		    call pargstr (output_file)
	       }
	       call fcopyo (in, out)
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
err_
	    call close (in)
	    call close (out)

	    call imtclose (list)
	}
end


# FCOP_OPEN_OUTPUT -- Open output file and return the mef descriptor.

pointer procedure fco_open_output (output_file, new_file)

char	output_file[ARB]	#I, output filename
bool	new_file			#I, true if file already exists

pointer mef, mef_open()
int	access(), acmode
errchk  mef_open

begin
	if (!new_file) {
	   # See if the file exists else change mode.
	   if (access (output_file, 0, 0) == NO)
	      new_file = true
	}
	
	acmode = APPEND
	if (new_file) {
           call fclobber (output_file)
	   acmode = NEW_FILE
	}

	mef = mef_open (output_file, acmode, 0)

	if (MEF_ENUMBER(mef) != -1)
	   call error(13, "Extension number not allowed in filename")

	return (mef)
end

