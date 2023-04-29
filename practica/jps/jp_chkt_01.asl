func f(a:int): bool
 return true;
endfunc

func main()
  var i,j : int
  var y: float 
  var p : pointer to int
  if p == null then
    p = &i;
  endif
  x = 3;
  j = *p + 1;
  y = 'a' or not 23;
  if not f(y) then
     write "meeeck!"   // syntax error!! no TypeCheck is done
  endif
endfunc
