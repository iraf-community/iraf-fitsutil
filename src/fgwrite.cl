#{ FGWRITE - Procedure to start the foreign task fgwrite.

procedure fgwrite (input, output)

string input	    {prompt="Input FITS files"}
string output       {prompt="Output MEF file"}
bool   verbose=yes  {prompt="verbose"}
string group=""     {prompt="FG_GROUP name"}
string types=""     {prompt="Select filetypes (tbdsfm)"}
string exclude=""   {prompt="Exclude filetypes  (tbdsfm)"}
bool   phu=yes      {prompt="Creates output PHU"}
bool   checksum=no  {prompt="Checksums?"}
bool   toc=no       {prompt="write Table Of Content"}

begin
	string out, inf
	string in=""
	string sel=""
	string excl=""
	string flags=""
	string groupn

	inf = input
	# Look if the input is a list of files.
	i = 1
	j = stridx (",", inf)
	if (j == 0)
	    in = inf
	while (j > 0) {
	    in = in//" "// substr(inf,i,j-1)
	    inf = substr (inf, j+1,input.p_len)
	    j = stridx (",",inf)
	    if (j == 0)
		in = in//" "// substr(inf,i,j-1)
	}
	out = "-f "//output
	if (verbose == yes)
	    flags = flags//"v"
	if (toc == yes)
	    flags = flags//"i"
	if (phu == no)
	    flags = flags//"h"
	if (checksum == yes)
	    flags = flags//"s"

	if (flags != "")
	    flags = "-"//flags
        if (types != "")
	    sel = "-t "//types
	if (exclude != "")
	    excl = "-o "//exclude

	t_fgwrite (flags, sel, excl, out, in)
end
