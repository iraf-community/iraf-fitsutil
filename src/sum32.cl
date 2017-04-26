#{ SUM32 - calculate the FITS checksum on a list of files

procedure sum32 (input)

string	input	    		{prompt="input files"}

bool	verbose = no		{prompt="verbose messages?"}
int	nimages			{prompt="number of images checksummed"}

string	*imlist

begin
	string	limages, img, tmpfile, junk, sumstr
	int	count, filsiz, insize

	cache sections

	limages = input

	tmpfile = mktemp ("tmp$sum32_")
	sections (limages, opt="fullname", > tmpfile)

	if (sections.nimages <= 0) {
	    printf ("nothing in input list\n")
	    return
	}

	imlist = tmpfile
	while (fscan (imlist, img) != EOF) {
	    # preflight check
	    if (! access (img)) {
		printf ("input image %s not found\n", img)
		return
	    }
	}

	count = 0
	insize = 0
	imlist = tmpfile
	while (fscan (imlist, img) != EOF) {
	    filsiz = 0
	    directory (img, long+) | scan (junk, junk, filsiz)

	    if (verbose) {
		t_sum32 ("-v " // osfn(img))

	    } else {
		sumstr = "NONE"
		t_sum32 (osfn(img)) | scan (sumstr)
		if (sumstr != "NONE")
		    printf ("%10s   %10d %s\n", sumstr, filsiz, img)
	    }

	    insize += int ((real (filsiz) / 1024.) + 0.5)
	    count += 1
	}

	imlist = ""; delete (tmpfile, ver-, >& "dev$null")

	nimages = count

	if (verbose && count > 0)
	    printf ("%d files, %.3f MB\n", count, real(insize)/1024.)
end
