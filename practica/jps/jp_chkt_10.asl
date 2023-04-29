func main()
  var b : bool
  var n, i: int
  var a : char
  var x : float  

  i = 4;
  read n;

  case not b or x>1 of
     12 : n = n+1;
          write n;

      6 : i = 0;
          case a of
             'X','Y' : b = true;
             'Z'     : n = 2;
             'x','X' : n = 0;
          endcase

     4,6,'a',12 : b = true;
          while not x or i>4 do
             n = 22;
             case n-1 of
                0,2,4,6,8 : n = n+1;
                1,3,false,7,9 : n = n-1;
                8,10,11 : b = false;
             endcase
          endwhile
             
     default:
         b = true;
         x = false;
         i = x;
  endcase

endfunc
