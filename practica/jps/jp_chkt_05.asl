func main()
  var A,B : array[10] of bool 
  var pA, pB : pointer to array[10] of bool
  var ppA, ppB : pointer to pointer to array[10] of bool
  var i: int
  var b: bool
  var p, q: pointer to bool

  pA = &A;
  pB = &B;
  *pA = *pB;
  A[0] = not ((*pA)[1]);
  i = 3.5;
  A[b+1] = b[i] or 2;

  ppA = &pA;
  ppB = &&pB;
  (**ppA)[1] = (*pA)[0] or not B[1];
  **ppA[1] = *pA[i+1] or not B[2];
  (*ppA)[i-1] = true;
endfunc
