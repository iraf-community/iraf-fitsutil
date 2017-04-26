#{ FITSUTIL.CL -- The FITSUTIL package

print ("This is the initial release of the IRAF FITSUTIL package")
print ("to include support for FITS tile compression via 'fpack'.")
print ("Please send comments and questions to seaman@noao.edu.")
print ("")

cl < "fitsutil$/lib/zzsetenv.def"
package	fitsutil, bin = fitsutilbin$

task  fxheader,
      fxdummyh,
      fxinsert,
      fxdelete,
      fxextract,
      fxsplit,
      fxconvert,
      fxplf,
      fxcopy 	= "fitsutil$src/x_fitsutil.e"

task ricepack	= "fitsutil$src/ricepack.cl"
task fpack 	= "fitsutil$src/fpack.cl"
task funpack 	= "fitsutil$src/funpack.cl"
task sum32	= "fitsutil$src/sum32.cl"

task $t_fpack 	= ("$" // osfn("fitsutilbin$") // "fpack")
task $t_funpack = ("$" // osfn("fitsutilbin$") // "funpack")
task $t_sum32	= ("$" // osfn("fitsutilbin$") // "sum32")
hidetask	t_fpack
hidetask	t_funpack
hidetask	t_sum32

task fgwrite 	= "fitsutil$src/fgwrite.cl"
task fgread 	= "fitsutil$src/fgread.cl"

task $t_fgwrite = ("$" // osfn("fitsutilbin$") // "fgwrite.e $*")
task $t_fgread 	= ("$" // osfn("fitsutilbin$") // "fgread.e $*")
hidetask	t_fgwrite
hidetask	t_fgread

clbye()
;
