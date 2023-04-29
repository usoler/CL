func f(x :int) : bool
   return x!=0;
endfunc

func main()
  var i,j : int
  var y: float 
  var p : pointer to int
  var q : pointer to pointer to int
  if p == null then
    p = &i;
  endif
  j = *p + (x>0);
  *p = *p/(*p+1);
  *p = *p*(*p);
  *p = *p * *p;
  q = &p;
  i = &&q;
  q = &f;
  p = q + 1;
  p = *q;
  p = *q + 1;
  j = *q;
  i = 3 + **q;
  j = 5 * **q;
  i = ***q;
endfunc
