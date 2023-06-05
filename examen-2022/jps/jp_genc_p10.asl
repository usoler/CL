func main ()
   var M: matrix [10,15] of float

   M[6,0] = 9999;
   write "OK and now crash:\n";
   write M[5,15]; write "\n";
   write "End without crash!!\n";
endfunc
