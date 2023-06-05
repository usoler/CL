func main ()
   var nz,a,b, i,j : int
   var sum: float
   var A : array [2] of int
   var M : matrix [10,20] of float

   read a;
   read b;

   i = 0;
   while i<10 do
      j = 0;
      while j<20 do
         M[i,j] = a;
         a = a - b;
         j = j+1;
      endwhile
      i = i+1;
   endwhile
  
   write M[4,10];

endfunc
