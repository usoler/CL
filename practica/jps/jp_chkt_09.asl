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
             'X' : b = true;
             'Z'  : n = 2;
             'x'  : n = 0;
             'Z'  : b = false;
          endcase

      12 : b = true;
          while not x or i>4 do
             n = 22;
             case n-1 of
                0 : n = n+1;
                9 : n = n-1;
                '<' : b = false;
             endcase
          endwhile
             
     default:
         b = true;
         x = false;
         i = x;
  endcase

endfunc
