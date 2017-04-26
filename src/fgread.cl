#{ FGREAD - Procedure to start the foreign task fgread.

procedure fgread (input, list, file_list)

string input	    {prompt="Input MEF file"}
string list	    {prompt="Extension list to extract"}
string file_list    {prompt="Output file list"}
bool   verbose=yes  {prompt="verbose"}
string types=""     {prompt="Select filetypes (tbdsfm)"}
string exclude=""   {prompt="Exclude filetypes  (tbdsfm)"}
bool   extract=yes  {prompt="Extract files?"}
bool   checksum=no  {prompt="Checksums?"}
bool   replace=yes  {prompt="replace existing files?"}

begin
	string lis, olis
	string inf=""
	string out=""
	string sel=""
	string excl=""
	string flags=""
	string groupn


	if (input != "")
	    inf = "-f "//input

	lis = list 
	olis = file_list
	# Look if the output is a list of files.
	i = 1
	j = stridx (",", olis)
	if (j == 0)
	    out = olis
	while (j > 0) {
	    out = out//" "// substr(olis,i,j-1)
	    olis = substr (olis, j+1,file_list.p_len)
	    j = stridx (",",olis)
	    if (j == 0)
		out = out//" "// substr(olis,i,j-1)
	}

	if (verbose == yes)
	    flags = flags//"v"
	if (extract == yes)
	    flags = flags//"x"
	if (replace == no)
	    flags = flags//"r"
	if (checksum == yes)
	    flags = flags//"s"

	if (flags != "")
	    flags = "-"//flags
        if (types != "")
	    sel = "-t "//types
	
	if (exclude != "") 
	    excl = "-o "//exclude

	if (lis != "") 
	    lis = "-n "//lis
	
	t_fgread (sel, excl, lis, flags, inf, out)
end
