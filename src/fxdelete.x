include <imio.h>
include <imhdr.h>
include <ctype.h>
include <fset.h>
include <mach.h>
include <mef.h>

define MAX_RANGES  100

         
# FDELETE -- Delete one or more extension in place from a given list 
#	     of FITS files.

procedure t_fxdelete()

char	input_file[SZ_FNAME]
char	imtlist[SZ_FNAME], lbrk		

char	group_list[SZ_LINE], temp[SZ_FNAME]
int	group_range[2*MAX_RANGES+1]

int	nch, in, gn, ninfiles, nitems, list, i, outfd
int	ig, k, fsize, lbrkdx, ngroups, stat
bool    clgetb()
int	imtopen(), imtgetim(), imtlen(), open(), read()
int	lget_next_number(), ldecode_ranges(), stridx(), fnroot(), mef_totpix()
int	fstatl(), mef_rdhdr_gn()
     
pointer mef, mef_open(), sp, buf, gp, bp, gsk, gnb
bool    verbose
errchk  fcopy

begin
	lbrk = '['

	call smark (sp)
	call salloc (buf, FITS_BLKSZ_CHAR, TY_CHAR)

	call clgstr ("input", imtlist, SZ_FNAME)
	verbose = clgetb("verbose")

	list = imtopen (imtlist)
	ninfiles = imtlen (list)
	
	# Find out if we have a bracket indicating an extension number.
	while (imtgetim (list, input_file, SZ_PATHNAME) != EOF) {
	    lbrkdx = stridx (lbrk, input_file)
	    if (lbrkdx <= 0)
		break
	}
	call imtrew (list)

	# Read group list
	nitems = 1
	if (lbrkdx <= 0) {
            call clgstr ("groups", group_list, SZ_LINE)
            if (ldecode_ranges (group_list,group_range,MAX_RANGES,nitems) ==ERR)
	        call error (0, "Illegal file number list.")
	    
	    if (group_list[1] == EOS)
		call error (0, "cannot delete PHU (group 0) from file.")

	    call salloc (gp, nitems+1, TY_INT)
	    gn = -1
	    k = 0
	    # If group is zero, ignore; we are not going to delete 
	    # group zero.

            while (lget_next_number (group_range, gn) != EOF) {
	         Memi[gp+k] = gn
		 k = k + 1
            }			
	}
	call salloc (gsk, nitems+1, TY_INT)
	call salloc (gnb, nitems+1, TY_INT)

	while (imtgetim (list, input_file, SZ_PATHNAME) != EOF) {

	    mef = mef_open (input_file, READ_WRITE, 0)
	    in = MEF_FD(mef)
	    lbrkdx = stridx(lbrk, input_file)
	    ig =  MEF_ENUMBER(mef)
	        
	    # Do not delete PHU
	    if (ig == 0) {
		call mef_close (mef)
		next
	    }

	    if (lbrkdx > 0)
	        input_file[lbrkdx] = EOS
	    i = fnroot (input_file, temp, SZ_FNAME)
	    call mktemp (temp, temp, SZ_FNAME)
	    call strcat (".fits", temp, SZ_FNAME)
	    outfd = open (temp, NEW_FILE, BINARY_FILE)
	    
	    if (verbose) {
		call eprintf ("File: %s, deleting extension numbers: ")
		    call pargstr (input_file)
	    }				
	    fsize = fstatl (MEF_FD(mef), F_FILESIZE)

	    bp = 0
	    if (lbrkdx > 0)
		ngroups = 0
	    else
		ngroups = nitems - 1

	    # Preread the file and mark the groups to delete, the offset
	    # points and the number of blocks to copy to the temporary file.

	    do i = 0, ngroups {
		if (lbrkdx > 0)
		    gn = ig
	        else
		    gn = Memi[gp+i]
		    
		stat =  mef_rdhdr_gn (mef, gn)
		Memi[gsk+i] = bp
		Memi[gnb+i] = (MEF_HOFF(mef) - bp)/1440
		bp = MEF_POFF(mef) 

		# If we have a header with no pixels, the pixel offset will
		# be at the end of the last header block.

		if (bp == INDEFI)
		    bp = MEF_HOFF(mef) + ((MEF_HSIZE(mef)+2879)/2880)*1440
	        bp = bp + mef_totpix (mef)
	    }

	    # Now copy those groups that are not being deleted into
	    # the temporary file.

	    do i = 0, ngroups {
		if (lbrkdx > 0)
		    gn = ig
	        else
		    gn = Memi[gp+i]
	         if (verbose) {
		     call eprintf ("%d ")
		         call pargi(gn)
	         }
		 if (Memi[gsk+i] == 0)
		     call seek (in, BOF)
	         else
		     call seek (in, Memi[gsk+i])

		 do k = 1,  Memi[gnb+i] {
		     nch = read (in, Memc[buf], 1440)
		     call write (outfd, Memc[buf], 1440)
	 	 }
	    } #end do

	    # Skip the last group data we want to delete
	    call seek (in, bp)

	    # Copy the rest of the file if necessary
	    if (bp < fsize)
		call fcopyo (in, outfd)
	    	
	    if (verbose)
		call eprintf ("\n")

	    call mef_close (mef)
	    call close (outfd)

	    call delete (input_file)
	    call rename (temp, input_file)
	} #end while

	call sfree(sp)
end
